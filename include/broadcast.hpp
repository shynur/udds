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
                auto contains(const std::string& robot_id) const {
                    auto _ = std::shared_lock{this->mutex};
                    return this->messages.contains(robot_id);
                }
                auto operator[](const std::string& robot_id) const {
                    auto _ = std::shared_lock{this->mutex};
                    return this->messages.at(robot_id);
                }
                auto& operator[](const std::string& robot_id) {
                    auto _ = std::unique_lock{this->mutex};
                    return this->messages[robot_id];
                }
                auto size() const {
                    auto _ = std::shared_lock{this->mutex};
                    return std::size(this->messages);
                }
                auto keys() const {
                    auto _ = std::shared_lock{this->mutex};
                    return std::forward_list<const std::string *>{
                        std::from_range,
                        this->messages | std::views::keys | std::views::transform(
                            [](const auto& k) { return std::addressof(k); }
                        )
                    };
                }

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
     *        对于同一订阅者, 只保留它最新一次发布的消息.
     * @example
     *
     * 拿特定小车的消息:
     *
     * ```
     * auto msg = received_from["Some Robot ID"]
     * ```
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
    }

    /**
     * @brief 向局域网中目前已经被发现的订阅者广播消息.
     * @param message 要广播的消息.
     *                message 的 timestamp / robot_id 字段会被自动设置.
     * @return 仅当没有订阅者时返回 false.
     */
    auto send(auto&& message) requires std::same_as<UddsJsonProto, std::decay_t<decltype(message)>> {
        message.robot_id(profile::self_robot_id);

        message.timestamp(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );

        return publisher->publish(message);
    }
}
