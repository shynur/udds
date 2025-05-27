#include "udds.hpp"
#include <string>
#include <format>
#include <unistd.h>
#include "../protos/UddsJsonProto.hpp"
#include "../protos/UddsJsonProtoPubSubTypes.hpp"

namespace rbk::udds::broadcast {
    constexpr auto DOMAIN_ID = 360u;
    constexpr auto TOPIC_NAME = "broadcast";
    auto get_participant_name() -> std::string;

    inline auto publisher = Publisher<
        UddsJsonProto, UddsJsonProtoPubSubType, [] {return "UddsJsonProto";}
    >{
        DOMAIN_ID,
        std::format(
            "{} (as publisher) (PID #{})",
            get_participant_name(), ::getpid()
        ),
        TOPIC_NAME
    };

    inline auto subscriber = Subscriber<
        UddsJsonProto, UddsJsonProtoPubSubType, [] {return "UddsJsonProto";}
    >{
        DOMAIN_ID,
        std::format(
            "{} (as subscriber) (PID #{})",
            get_participant_name(), ::getpid()
        ),
        TOPIC_NAME,
        [] -> UddsJsonProto& {},
        [](UddsJsonProto& message) {}
    };


}
