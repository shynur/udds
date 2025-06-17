// 使用独立进程进行消息传递的示例.
#include "udds/udds-broadcast-client.hpp"

int main(int, const char *const argv[2]) {
    shynur::udds::Broadcast_Client c{ /* Robot ID: */ argv[1], 1};

    for (
        std::string do_what;
        std::cout << "[send], send [N] characters, or [check]?  ", std::cin >> do_what;
        std::cout << "\n\n-----------------------------------------\n\n"
    ) {
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
                          << "JSON: " << (
                                msg.json.length() <= 40 ? msg.json : msg.json.substr(0, 40) + "..."
                            ) << "\n"
                          << "长度: " << msg.json.length() << '\n'
                          << "\n";
        else if (do_what == "N") {
            unsigned cnt;
            std::cout << "send N characters, N = ";
            std::cin >> cnt;

            char ch;
            std::cout << "send character: ";
            std::cin >> ch;

            auto msg = shynur::udds::UddsJsonStruct{};
            msg.json = std::string(cnt, ch);
            c.send(msg);
        }
    }
}
