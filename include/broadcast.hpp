#include "udds.hpp"
#include <string>
#include <format>
#include <cstdint>
#include <type_traits>
#include <chrono>
#include <ranges>
#include <iterator>
#include <utility>
#include <cassert>
#include <shared_mutex>
#include <unordered_map>
#include <forward_list>
#include <concepts>
#include <memory>
#include "../protos/UddsJsonProto.hpp"
#include "../protos/UddsJsonProtoPubSubTypes.hpp"

namespace rbk::udds::broadcast {

    constexpr auto DOMAIN_ID = 1;
    constexpr auto TOPIC_NAME = "broadcast";

    Publisher<
        UddsJsonProto, UddsJsonProtoPubSubType, [] {return "UddsJsonProto";}
    > *publisher [[indeterminate]];

    Subscriber<
        UddsJsonProto, UddsJsonProtoPubSubType, [] {return "UddsJsonProto";}
    > *subscriber [[indeterminate]];

    inline auto _received_from
        = [] {
            class Messages_From_Robots {
                std::unordered_map<std::string, std::shared_ptr<UddsJsonProto>> messages;
                mutable std::shared_mutex mutex;
              public:
                /**
                 * @brief 是否包含特定小车的消息.
                 */
                auto contains(const std::string& robot_id) const {
                    auto _ = std::shared_lock{this->mutex};
                    return this->messages.contains(robot_id);
                }
                /**
                 * @brief 获取 最近一次接收到的 来自特定小车的 消息.
                 * @param robot_id 小车的 ID.  `this->contains(robot_id)` 必须为 true.
                 * @return 返回指向消息的 shared_ptr.
                 * @note 请将 shared_ptr 拷贝给其它变量后再通过 shared_ptr 访问消息,
                 *       以避免数据竞争.
                 */
                auto operator[](const std::string& robot_id) const {
                    auto _ = std::shared_lock{this->mutex};
                    return this->messages.at(robot_id);
                }
                auto& operator[](const std::string& robot_id) {
                    auto _ = std::unique_lock{this->mutex};
                    return this->messages[robot_id];
                }
                /**
                 * @brief 收到了几台小车的消息, or 目前持有几条消息.
                 */
                auto size() const {
                    auto _ = std::shared_lock{this->mutex};
                    return std::size(this->messages);
                }
                /**
                 * @brief 列出目前已经收到的消息的发件人的 robot_id
                 *        (其实是指向 robot_id 的指针).
                 */
                auto keys() const {
                    auto _ = std::shared_lock{this->mutex};
                    return std::forward_list<const std::string *>{
                        std::from_range,
                        this->messages | std::views::keys | std::views::transform(
                            [](const auto& k) { return std::addressof(k); }
                        )
                    };
                }

                /**
                 * @brief 截至调用该函数时, 我们已知有哪些发件人.
                 *        在返回的迭代器上迭代, 每次解引用得到一个 pair
                 *        (指向发件人 robot_id 的指针, 指向消息的 shared_ptr).
                 *        如果在迭代过程中, 有已知的小车更新了消息, 迭代器会同步更新;
                 *        如果在迭代过程中, 有新的小车发送了消息, 这台新车不会被考虑加入到当前的迭代范围中.
                 */
                auto begin() const {
                    class const_iterator {
                        const Messages_From_Robots& messages_from;
                        const std::forward_list<const std::string *> keys_snapshot;
                        std::forward_list<const std::string *>::const_iterator key_iter;

                        friend Messages_From_Robots;
                        const_iterator(const Messages_From_Robots& messages_from)
                        : messages_from{messages_from},
                          keys_snapshot{messages_from.keys()},
                          key_iter{std::cbegin(keys_snapshot)} {}

                      public:
                        auto& operator++() {
                            assert(this->key_iter != std::cend(this->keys_snapshot));
                            ++this->key_iter;
                            return *this;
                        }
                        auto operator*() const {
                            assert(this->key_iter != std::cend(this->keys_snapshot));
                            return std::pair{
                                *this->key_iter,
                                this->messages_from[**this->key_iter],
                            };
                        }
                        auto operator!=(const std::default_sentinel_t&) const {
                            return this->key_iter != std::cend(this->keys_snapshot);
                        }
                    };
                    return const_iterator{*this};
                }
                static auto end() { return std::default_sentinel; }
            };
            return Messages_From_Robots{};
        }();
    /**
     * @brief 目前已接收的所有订阅者的消息.
     *        对于同一订阅者, 只保留最近一次来自它的消息.
     */
    inline const auto& received_from = _received_from;

    namespace profile {
        inline std::string self_robot_id;
    }

    /**
     * @brief 初始化广播系统.  要使用 udds::broadcast, 必须首先调用此函数.
     * @warning 应当仅调用一次.
     */
    inline auto init(const std::string& self_robot_id) {
        profile::self_robot_id = self_robot_id;

        static auto publisher_singleton = std::decay_t<decltype(*publisher)>{
            DOMAIN_ID,
            std::format(
                "{} (as publisher)",
                self_robot_id
            ),
            TOPIC_NAME
        };
        publisher = &publisher_singleton;
        {
            static struct publisher_resetter {
                ~publisher_resetter() { publisher = nullptr; }
            } _;
        }

        static auto subscriber_singleton = std::decay_t<decltype(*subscriber)>{
            DOMAIN_ID,
            std::format(
                "{} (as subscriber)",
                self_robot_id
            ),
            TOPIC_NAME,
            [] -> UddsJsonProto& {
                return *new UddsJsonProto;
            },
            [&](UddsJsonProto& message) {
                _received_from[message.robot_id()]
                    = std::shared_ptr<UddsJsonProto>{&message};
            }
        };
        subscriber = &subscriber_singleton;
        {
            static struct subscriber_resetter {
                ~subscriber_resetter() { subscriber = nullptr; }
            } _;
        }
    }

    /**
     * @brief 向局域网中目前已经被发现的订阅者广播消息.
     * @param message 要广播的消息.
     *                message 的 timestamp / robot_id / delay 字段会被自动设置.
     */
    auto send(auto&& message) requires std::same_as<UddsJsonProto, std::decay_t<decltype(message)>> {
        message.robot_id(profile::self_robot_id);

        message.timestamp(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );

        publisher->publish(message);
    }
}
