#include "udds.hpp"
#include <string>
#include <format>
#include <cstdint>
#include <type_traits>
#include <chrono>
#include <cassert>
#include <unordered_map>
#include <atomic>
#include <concepts>
#include <memory>
#include "../protos/UddsJsonProto.hpp"
#include "../protos/UddsJsonProtoPubSubTypes.hpp"

namespace rbk::udds::broadcast {

    constexpr auto DOMAIN_ID = 360u;
    constexpr auto TOPIC_NAME = "broadcast";

    inline auto publisher = std::unique_ptr<
        Publisher<
            UddsJsonProto, UddsJsonProtoPubSubType, [] {return "UddsJsonProto";}
        >
    >{};

    inline auto subscriber = std::unique_ptr<
        Subscriber<
            UddsJsonProto, UddsJsonProtoPubSubType, [] {return "UddsJsonProto";}
        >
    >{};

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
    inline auto& received_from
        = [MAX_ROBOTS_UNDER_LAN=10u] -> auto& {
            static auto received_from = std::unordered_map<
                std::string,
                std::atomic<std::shared_ptr<UddsJsonProto>>
            >{};
            received_from.reserve(MAX_ROBOTS_UNDER_LAN);
            return received_from;
        }();

    namespace profile {
        inline std::string self_robot_id;
    }

    /**
     * @brief 初始化广播系统.  要使用 udds::broadcast, 必须首先调用此函数.
     * @warning 应当仅调用一次.
     */
    inline auto init(const std::string& self_robot_id) {
        profile::self_robot_id = self_robot_id;

        publisher.reset(
            new std::decay_t<decltype(*publisher)>{
                DOMAIN_ID,
                std::format(
                    "{} (as publisher)",
                    self_robot_id
                ),
                TOPIC_NAME
            }
        );
        subscriber.reset(
            new std::decay_t<decltype(*subscriber)>{
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
                    received_from[message.robot_id()]
                        = std::shared_ptr<UddsJsonProto>{&message};
                }
            }
        );
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
