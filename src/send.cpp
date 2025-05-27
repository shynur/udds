#include "broadcast.hpp"
#include <iostream>

int main(int, const char *argv[]) {
    rbk::udds::broadcast::init(argv[1]);  // 设置 robot_id

    auto msg = UddsJsonProto{};

    while (std::cin >> msg.json())
        rbk::udds::broadcast::send(msg);
}
