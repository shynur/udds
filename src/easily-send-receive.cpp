// 使用独立进程进行消息传递的示例.
#include "udds/udds-broadcast-client.hpp"
#include <iostream>
#include <string>

int main(int, const char *const argv[2]) {
    shynur::udds::Broadcast_Client c{ /* Robot ID: */ argv[1], 1};

    while (true) {
        std::cout << "\n\n-----------------------------------------\n\n";

        std::cout << "send or check?  ";
        std::string do_what;
        std::cin >> do_what;

        if (do_what == "send") {
            auto msg = shynur::udds::UddsJsonStruct{};

            std::string text;
            std::cout << "send what?  ";
            std::cin >> msg.json;

            c.send(msg);
        } else if (do_what == "check")
            for (auto [car, msg] : c.received_from())
                std::cout << "小车 ID: " << car << '\n'
                          << "发送时间: " << msg.send_timestamp_ns << '\n'
                          << "接收时间: " << msg.received_timestamp_ns << '\n'
                          << "JSON: " << msg.json << "\n\n";
    }
}
