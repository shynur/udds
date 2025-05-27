#include "udds.hpp"
#include <chrono>
#include <format>
#include <ranges>
#include <iostream>
#include <thread>
#include "../protos/UddsJsonProto.hpp"
#include "../protos/UddsJsonProtoPubSubTypes.hpp"

constexpr auto NUM_MSGS = 10u;

int main() {
    auto sender = rbk::udds::Publisher<
       UddsJsonProto, UddsJsonProtoPubSubType, [] {return "UddsJsonProto";}
    >{1, "发布者的名字", "给 topic 取的名字"};

    auto msg = UddsJsonProto{};
    for (const auto i : std::views::iota(0u, NUM_MSGS)) {
        msg.json(std::format("第 {} 条消息 嘻嘻", i));
        msg.timestamp(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count() / 1e9
        );

        while (!sender.publish(msg))
            std::this_thread::sleep_for(100ms);
        std::cerr << std::format("发布了第 {} 条数据\n", i);
        std::this_thread::sleep_for(100ms);
    }
}
