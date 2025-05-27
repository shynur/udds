#include "udds.hpp"
#include <ranges>
#include <vector>
#include <thread>
#include <iostream>
#include <format>
#include "../protos/UddsJsonProto.hpp"
#include "../protos/UddsJsonProtoPubSubTypes.hpp"

constexpr auto NUM_MSGS = 10u;

int main() {
    auto messages = std::vector(NUM_MSGS, UddsJsonProto{});
    auto received = std::atomic_uint{0};

    auto receiver = rbk::udds::Subscriber<
      UddsJsonProto, UddsJsonProtoPubSubType, [] {return "UddsJsonProto";}
    >{
        1, "订阅者的名字", "给 topic 取的名字",
        [iter=std::begin(messages)] mutable -> auto& { return *iter++; },
        [&](const UddsJsonProto& msg) {
            std::thread{
                [&] {
                    std::cerr << std::format(
                        "{{ 时间: {}, 文本: \"{}\" }}\n",
                        msg.timestamp(), msg.json()
                    );
                    received++;
                }
            }.detach();
        }
    };

    while (received < NUM_MSGS)
        std::this_thread::sleep_for(100ms);
    std::this_thread::sleep_for(100ms);
}
