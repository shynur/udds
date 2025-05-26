#include "udds.hpp"
#include <ranges>
#include <vector>
#include <thread>
#include <iostream>
#include <format>
#include "../protos/ExampleMessage.hpp"
#include "../protos/ExampleMessagePubSubTypes.hpp"

constexpr auto NUM_MSGS = 10u;

int main() {
    auto messages = std::vector(NUM_MSGS, ExampleMessage{});
    auto received = std::atomic_uint{0};

    auto receiver = rbk::udds::Subscriber<
      ExampleMessage, ExampleMessagePubSubType, [] {return "ExampleMessage";}
      // 没错, 这三个模板参数必须你手写出来, 虽然它们长得几乎一样.
    >{
        1, "订阅者的名字", "给 topic 取的名字",
        [iter=std::begin(messages)] mutable -> auto& { return *iter++; },
        [&](const ExampleMessage& msg) {
            std::thread{
                [&] {
                    std::cerr << std::format(
                        "{{ 时间: {}, 文本: \"{}\" }}\n",
                        msg.timestamp(), msg.txt()
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
