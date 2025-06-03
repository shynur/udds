#include "broadcast.hpp"
namespace rbk = shynur;
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
