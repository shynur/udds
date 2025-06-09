#include "udds/broadcast.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
using namespace shynur;

int main(int, const char *argv[]) {
    udds::broadcast::init(argv[1]);  // 设置 robot_id

    while (true) {
        char choice;
        std::cout << "\n"
                     "1) 打印 size\n"
                     "2) 打印所有消息的发件人\n"
                     "3) 是否收到了某个小车的消息\n"
                     "4) 打印来自某个小车的消息\n"
                     "5) 遍历\n"
                     "6) 获取某个小车相对于我的时钟偏移量\n";
        std::cin >> choice;
        std::cout << '\n';
        switch (choice) {
            case '1':
                std::cout << std::size(udds::broadcast::received_from) << '\n';
                break;
            case '2':
                for (const auto& robot_id : udds::broadcast::received_from.keys())
                    std::cout << *robot_id << '\n';
                break;
            case '3': {
                    std::string robot_id;
                    std::cout << "请输入小车 ID: ";
                    std::cin >> robot_id;
                    std::cout << (udds::broadcast::received_from.contains(robot_id)
                                  ? "是的, 收到了.\n"
                                  : "没有收到.\n");
                }
                break;
            case '4': {
                    std::string robot_id;
                    std::cout << "请输入小车 ID: ";
                    std::cin >> robot_id;
                    std::cout << "JSON: "
                              << udds::broadcast::received_from[robot_id]->json() << '\n';
                }
                break;
            case '5':
                for (auto [robot_id, message] : udds::broadcast::received_from)
                    std::cout << "小车 ID: " << *robot_id << '\n'
                              << "发送时间: " << message->send_timestamp_ns() << '\n'
                              << "延迟: " << message->received_timestamp_ns() - message->send_timestamp_ns() << " ns\n"
                              << "JSON: " << message->json() << "\n\n";
                break;
            case '6': {
                std::string robot_id;
                std::cout << "请输入小车 ID: ";
                std::cin >> robot_id;
                std::cout << udds::broadcast::clock_offset_of.ns(robot_id) << " ns\n";
                break;
            }
            default:
                return {};
        }
        std::cout << '\n';
    }
}
