#define SEER_ROBOTICS_UDDS
#include "../include/udds/udds.hpp"

#include "UddsPersonProtoPubSubTypes.hpp"  // <-- FastDDS 命令行工具自动生成的 proto 头文件.
const auto publisher = new RBK_UDDS_PUBLISHER(
    123, // <- 频道
    "发布者的名字",
    UddsPersonProto  // <- 订阅的消息类型, 必须先 include "UddsPersonProtoPubSubTypes.hpp" 头文件.
);

int main(int, const char *const argv[]) {
    auto person = UddsPersonProto{};
    while (true) {
	person.name(argv[1]);
        person.age(std::rand());

	publisher->publish(person);
	std::printf("Pub {name: %s, age: %u}\n", person.name().c_str(), person.age());

	std::this_thread::sleep_for(2s);
    }
}
