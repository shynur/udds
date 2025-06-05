/* source code: <https://github.com/shynur/udds> */
#pragma once
#include "udds.hpp"
#include <chrono>
#include <format>
#include <memory>
#include <ranges>
#include <string>
#include <thread>
#include <cassert>
#include <cstdint>
#include <numeric>
#include <utility>
#include <concepts>
#include <iostream>
#include <iterator>
#include <execution>
#include <functional>
#include <type_traits>
#include <forward_list>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#if SHYNUR_UDDS_USED_BY_SEER_RBK == 30408UL
    #include "UddsJsonProto.hpp"
    #include "UddsJsonProtoPubSubTypes.hpp"
    #include "UddsClkSyncPackProto.hpp"
    #include "UddsClkSyncPackProtoPubSubTypes.hpp"
#else
    #include "../protos/UddsJsonProto.hpp"
    #include "../protos/UddsJsonProtoPubSubTypes.hpp"
    #include "../protos/UddsClkSyncPackProto.hpp"
    #include "../protos/UddsClkSyncPackProtoPubSubTypes.hpp"
#endif

namespace shynur::udds::broadcast {

    constexpr auto DOMAIN_ID = 1;
    constexpr auto TOPIC_NAME = "broadcast";
    constexpr auto DISCOVERY_DELAY = 400ms;

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
                            [](const auto& key) { return std::addressof(key); }
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
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );

        publisher->publish(message);
    }

    inline struct {
        const std::uint8_t DOMAIN_ID = 2;
        const unsigned NUM_PACKS = 10;

        std::unordered_map<std::string, std::vector<UddsClkSyncPackProto>> packs;
        mutable std::shared_mutex packs_mutex;

        /**
         * @brief 获取 ROBOT_ID 车辆的时钟 减去 自身时钟 的 值.
         * @note 只能查询 **向本机发送过消息的小车** 的时钟.
         * @warning 如果校对失败则返回 0.
         */
        auto ns [[gnu::reproducible]] (const std::string& robot_id) const {
            if (
                static auto robots_passed_before = std::unordered_set<std::string>{};
                !robots_passed_before.contains(robot_id)
            ) {
                robots_passed_before.insert(robot_id);
                std::this_thread::sleep_for(
                    2 * DISCOVERY_DELAY
                    + 40ms * (this->NUM_PACKS * this->NUM_PACKS /* 只是为了保证等待足够久 */)
                );
            }

            if (std::shared_lock{this->packs_mutex}, !this->packs.contains(robot_id))
                return 0.0;

            std::shared_lock{this->packs_mutex};
            return std::transform_reduce(
                std::execution::par_unseq,
                std::cbegin(this->packs.find(robot_id)->second),
                std::cend(this->packs.find(robot_id)->second),
                0.0,
                std::plus{},
                [](const UddsClkSyncPackProto& pack) {
                    std::cerr << std::format(
                        "ClkSyncPack {{\"my latency\":{},\t\"its latency\":{}}}\n",
                        pack.latency(), pack.received_timestamp() - pack.send_timestamp()
                    );
                    return (
                        pack.latency() - (pack.received_timestamp() - pack.send_timestamp())
                    ) / 2;
                }
            ) / std::size(this->packs.find(robot_id)->second) * 1e9;
        }

        auto init() {
            static auto receiver = Subscriber<
                UddsClkSyncPackProto, UddsClkSyncPackProtoPubSubType,
                [] {return "UddsClkSyncPackProto";}
            >{
                this->DOMAIN_ID,
                std::format(
                    "{} (as clock sync receiver)",
                    profile::self_robot_id
                ),
                profile::self_robot_id,
                [] noexcept {
                    return std::make_unique<UddsClkSyncPackProto>();
                },
                [this](UddsClkSyncPackProto& pack) {
                    pack.received_timestamp(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()
                        ).count() / 1e9
                    );

                    std::cerr << "Received clock sync pack.\n";
                    if (pack.latency()) {
                        std::cerr << std::format(
                            "Clock sync pack returned from {}\n",
                            pack.sender()
                        );
                        const auto _ = std::unique_lock{this->packs_mutex};
                        this->packs[pack.sender()].push_back(std::move(pack));
                    } else
                        std::thread{
                            [this, pack = std::move(pack)] mutable {
                                const auto sender = pack.sender();
                                this->reply(sender, std::move(pack));
                            }
                        }.detach();

                    delete &pack;
                }
            };
        }

        void reply(const std::string sender, UddsClkSyncPackProto pack) const {
            pack.sender(profile::self_robot_id);
            pack.latency(pack.received_timestamp() - pack.send_timestamp());

            static auto replier_for = std::unordered_map<
                std::string,
                std::unique_ptr<
                    Publisher<
                        UddsClkSyncPackProto, UddsClkSyncPackProtoPubSubType,
                        [] {return "UddsClkSyncPackProto";}
                    >
                >
            >{};
            static auto repliers_mutex = std::shared_mutex{};

            if (const auto _ = std::unique_lock{repliers_mutex}; !replier_for.contains(sender)) {
                replier_for[sender].reset(
                    new decltype(replier_for)::mapped_type::element_type{
                        this->DOMAIN_ID,
                        std::format(
                            "{} (as clock sync replier to {})",
                            profile::self_robot_id, sender
                        ),
                        sender
                    }
                );
                std::this_thread::sleep_for(DISCOVERY_DELAY);  // 等待被发现.
            }

            {
                const auto _ = std::shared_lock{repliers_mutex};
                std::cerr << std::format(
                    "Replying clock sync pack to {}...\n",
                    sender
                );
                pack.send_timestamp(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    ).count() / 1e9
                );
                replier_for[sender]->publish(std::move(pack));
            }
        }

        auto send_test_packs(const std::string& json_sender) const {
            auto sender = Publisher<
                UddsClkSyncPackProto, UddsClkSyncPackProtoPubSubType,
                [] {return "UddsClkSyncPackProto";}
            >{
                this->DOMAIN_ID,
                std::format(
                    "{} (as clock sync sender)",
                    profile::self_robot_id
                ),
                json_sender
            };
            std::this_thread::sleep_for(DISCOVERY_DELAY);  // 等待被发现.

            auto pack = UddsClkSyncPackProto{};
            pack.sender(profile::self_robot_id);

            for (const auto i : std::views::iota(0u, this->NUM_PACKS)) {
                std::this_thread::sleep_for(40ms * this->NUM_PACKS);
                std::cerr << std::format(
                    "Sending clock sync pack {}/{} to {}...\n",
                    i + 1, this->NUM_PACKS, json_sender
                );
                pack.send_timestamp(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    ).count() / 1e9
                );
                sender.publish(pack);
            }
        }
    } clock_offset_of;

    /**
     * @brief 初始化广播系统.  要使用 `udds::broadcast`, 必须首先调用此函数.
     * @warning 应当仅调用一次.
     */
    inline auto init(const std::string& self_robot_id) {
        profile::self_robot_id = self_robot_id;

        clock_offset_of.init();

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
                        std::chrono::steady_clock::now().time_since_epoch()
                    ).count()
                );

                if (!received_from.contains(message.robot_id()))
                    std::thread{
                        [sender=message.robot_id()] {
                            clock_offset_of.send_test_packs(std::move(sender));
                        }
                    }.detach();

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

        std::this_thread::sleep_for(DISCOVERY_DELAY);  // 等待被发现.
    }
}
