/* source code: <https://github.com/shynur/udds> */
#pragma once
#include <bits/stdc++.h>
#include <execution>
#include "./udds.hpp"
#include "../../protos/UddsJsonProto.hpp"
#include "../../protos/UddsJsonProtoPubSubTypes.hpp"
#include "../../protos/UddsClkSyncPackProto.hpp"
#include "../../protos/UddsClkSyncPackProtoPubSubTypes.hpp"

namespace shynur::udds::broadcast {

    constexpr auto DOMAIN_ID = 1;
    constexpr auto TOPIC_NAME = "broadcast";

    namespace profile {
        inline std::string self_robot_id;
    }

    inline Publisher<
        UddsJsonProto, UddsJsonProtoPubSubType, [] {return "UddsJsonProto";}
    > *publisher [[indeterminate]];

    inline Subscriber<
        UddsJsonProto, UddsJsonProtoPubSubType, [] {return "UddsJsonProto";}
    > *subscriber [[indeterminate]];

    inline auto _received_from
        = [] {
            class Messages_From_Robots {
                std::unordered_map<std::string, std::shared_ptr<const UddsJsonProto>> messages;
                mutable std::shared_mutex messages_mutex;
              public:
                /**
                 * @brief 是否包含特定小车的消息.
                 */
                auto contains(const std::string& robot_id) const {
                    auto _ = std::shared_lock{this->messages_mutex};
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
                    auto _ = std::shared_lock{this->messages_mutex};
                    return this->messages.at(robot_id);
                }
                auto& operator[](const std::string& robot_id) {
                    auto _ = std::unique_lock{this->messages_mutex};
                    return this->messages[robot_id];
                }
                /**
                 * @brief 收到了几台小车的消息, or 目前持有几条消息.
                 */
                auto size() const {
                    auto _ = std::shared_lock{this->messages_mutex};
                    return std::size(this->messages);
                }
                /**
                 * @brief 列出目前已经收到的消息的发件人的 robot_id
                 *        (其实是指向 robot_id 的指针).
                 */
                auto keys() const {
                    auto _ = std::shared_lock{this->messages_mutex};
                    return std::forward_list<const std::string *>{
                        std::from_range,
                        this->messages | std::views::keys | std::views::transform(
                            [](const auto& key) {return std::addressof(key);}
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
                        auto operator!=(std::default_sentinel_t) const {
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

    /**
     * @brief 向局域网中目前已经被发现的订阅者广播消息.
     * @param message 要广播的消息.
     *                message 的 send_timestamp_ns / received_timestamp_ns / robot_id 字段会被自动设置;
     *                你可设置: x / y / theta / json.
     */
    auto send(auto&& message) requires std::same_as<UddsJsonProto, std::decay_t<decltype(message)>> {
        message.robot_id(profile::self_robot_id);

        message.send_timestamp_ns(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );

        publisher->publish(message);
    }

    /**
     * @brief 初始化广播系统.  要使用 `udds::broadcast`, 必须首先调用此函数.
     * @warning 应当仅调用一次.
     */
    inline auto init(const std::string& self_robot_id) {
        profile::self_robot_id = self_robot_id;

        static auto publisher_singleton = std::decay_t<decltype(*publisher)>{
            DOMAIN_ID,
            std::format(
                "{} (as publisher)",
                profile::self_robot_id
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
                profile::self_robot_id
            ),
            TOPIC_NAME,
            [] noexcept {
                return std::make_unique<UddsJsonProto>();
            },
            [](UddsJsonProto& message) {
                message.received_timestamp_ns(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count()
                );
                std::clog << std::format(
                    "Received broadcast message from {}\n",
                    message.robot_id()
                ) << std::flush;

                _received_from[message.robot_id()]
                    = std::shared_ptr<std::decay_t<decltype(message)>>{&message};
            }
        };
        subscriber = &subscriber_singleton;
        {
            static struct subscriber_resetter {
                ~subscriber_resetter() { subscriber = nullptr; }
            } _;
        }
    }
}
