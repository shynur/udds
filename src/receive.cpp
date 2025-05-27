#include "broadcast.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>

int main(int, const char *argv[]) {
    rbk::udds::broadcast::init(argv[1]);  // 设置 robot_id

    while (std::getchar()) {
        std::cout << "收到了 " << std::size(rbk::udds::broadcast::received_from)
                  << " 个订阅者的消息" << '\n';


    }
}
