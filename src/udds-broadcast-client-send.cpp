#define SHYNUR_UDDS_USED_BY_SEER_RBK
#include "udds-broadcast-client.hpp"
#include <iostream>

int main(int, const char *const argv[]) {
    rbk::udds::Broadcast_Client c{argv[1], 1};

    auto msg = rbk::udds::UddsJsonStruct{};

    while (true) {
        std::cerr << "检测 stderr 是否可用...\n";
        std::cin >> msg.json;
        if (msg.json == "q")
            break;
        else {
            std::cerr << "即将发布消息...\n";
            c.send(msg);
            std::cerr << "消息已发布.\n";
        }
    }
}
