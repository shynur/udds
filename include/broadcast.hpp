#include "udds.hpp"
#include <string>
#include <format>
#include <cstdint>
#include <type_traits>
#include <chrono>
#include <array>
#include <vector>
#include <unordered_map>
#include <concepts>
#include <unistd.h>
#include "../protos/UddsJsonProto.hpp"
#include "../protos/UddsJsonProtoPubSubTypes.hpp"

namespace rbk::udds::broadcast {

    constexpr auto DOMAIN_ID = 360u;
    constexpr auto TOPIC_NAME = "broadcast";
    auto get_participant_name [[gnu::reproducible]] () -> std::string;

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

    namespace detail_for_subscriber {
        constexpr auto BUFFER_SIZE = 4096u;

        alignas(BUFFER_SIZE) inline auto ring_buffer
            = std::array<UddsJsonProto, BUFFER_SIZE / sizeof(UddsJsonProto)>{};

        auto messages_by_seq_num
            = std::unordered_map<std::uint32_t, std::vector<UddsJsonProto *>>{};
    }
    inline auto subscriber = Subscriber<
        UddsJsonProto, UddsJsonProtoPubSubType, [] {return "UddsJsonProto";}
    >{
        DOMAIN_ID,
        std::format(
            "{} (as subscriber) (PID #{})",
            get_participant_name(), ::getpid()
        ),
        TOPIC_NAME,
        [i=0u, &ring_buffer=detail_for_subscriber::ring_buffer] mutable -> UddsJsonProto& {
            return ring_buffer[i++ % std::size(ring_buffer)];
        },
        [&messages_by_seq_num=detail_for_subscriber::messages_by_seq_num](
            UddsJsonProto& message
        ) {
            messages_by_seq_num;
        }
    };


    template <typename std_string>
    requires std::same_as<std::string, std::decay_t<std_string>>
    auto send(
        const std::uint32_t seq_num, std_string&& json
    ) {
        auto message = UddsJsonProto{};

        message.seq_num(seq_num);
        message.json(std::forward<decltype(json)>(json));
        message.host(get_participant_name());
        message.timestamp(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count() / 1e9
        );

        return publisher.publish(message);
    }


    inline auto receive_all(const std::uint32_t seq_num) {

    }
}
