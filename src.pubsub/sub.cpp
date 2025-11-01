#define SEER_ROBOTICS_UDDS
#include "../include/udds/udds.hpp"

#include "UddsPersonProtoPubSubTypes.hpp"  // <-- FastDDS 命令行工具自动生成的 proto 头文件.
auto _ = new RBK_UDDS_SUBSCRIBER(
    123,  // <- 频道
    "订阅者的名字",
    UddsPersonProto,  // <- 订阅的消息类型, 必须先 include "UddsPersonProtoPubSubTypes.hpp" 头文件.
    [](std::shared_ptr<UddsPersonProto> person) {
        std::printf("Get {name: %s, age: %u}\n", person->name().c_str(), person->age());
    }  // <- 注册回调
);  // 若要取消订阅, delete 被 new 出来的指针即可.

int main() {
    std::this_thread::sleep_for(10000s);
}
