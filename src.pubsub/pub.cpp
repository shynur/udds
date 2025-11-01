#include "udds.hpp"

#include "UddsPersonProto.hpp"
#include "UddsPersonProtoPubSubTypes.hpp"

const auto publisher = new rbk::udds::Publisher<UddsPersonProto, UddsPersonProtoPubSubType>{
  123,  // <- 频道
  "publisher name",
  "UddsPersonProto",  // <- 必须和类型名一样
};

int main(int, const char *const argv[]) {
    auto person = UddsPersonProto{};
    while (true) {
	person.name(argv[1]);
        person.age(std::rand());

	publisher->publish(person);

	std::this_thread::sleep_for(2s);
    }
}
