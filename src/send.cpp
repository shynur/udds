#define SHYNUR_UDDS_USED_BY_SEER_RBK 30408UL
#include "broadcast.hpp"
#include <iostream>

int main(int, const char *argv[]) {
    rbk::udds::broadcast::init(argv[1]);  // 设置 robot_id

    auto msg = UddsJsonProto{};

    while (std::cin >> msg.json())
        if (msg.json() == "q")
            break;
        else
            rbk::udds::broadcast::send(msg);
}
