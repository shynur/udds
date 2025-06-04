#include "broadcast.hpp"
#include <iostream>

int main(int, const char *argv[]) {
    shynur::udds::broadcast::init(argv[1]);  // 设置 robot_id

    auto msg = UddsJsonProto{};

    while (std::cin >> msg.json())
        if (msg.json() == "q")
            break;
        else
            shynur::udds::broadcast::send(msg);
}
