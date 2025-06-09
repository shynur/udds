#include "udds/broadcast.hpp"
#include <iostream>
using namespace shynur;

int main(int, const char *argv[]) {
    udds::broadcast::init(argv[1]);  // 设置 robot_id

    auto msg = UddsJsonProto{};

    while (std::cin >> msg.json())
        if (msg.json() == "q")
            break;
        else
            udds::broadcast::send(msg);
}
