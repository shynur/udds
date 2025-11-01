#include "udds.hpp"

#include "UddsPersonProto.hpp"
#include "UddsPersonProtoPubSubTypes.hpp"

const auto subscriber = new rbk::udds::Subscriber<
   UddsPersonProto, UddsPersonProtoPubSubType
>{
  123,  // <- 频道
  "subscriber name",
  "UddsPersonProto",  // <- 必须和类型名一致
  [](std::shared_ptr<UddsPersonProto> person) {
       std::printf("Get {name: %s, age: %ul}\n", person->name().c_str(), person->age());
  }
};

int main() {
    std::this_thread::sleep_for(10000s);
}
