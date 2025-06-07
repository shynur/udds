#pragma once
#include <unistd.h>  // dup2, close, fork, pipe
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <string>

namespace shynur::udds {
    struct [[gnu::weak]] Broadcast_Client {
        static constexpr char *const cli_program = "udds-broadcast-cli";

        Broadcast_Client(
            const std::string robot_id,
            const std::uint8_t fastdds_domain
        ): Broadcast_Client{
            std::vector{
                "--robot_id=" + robot_id,
                "--fastdds_domain=" + std::to_string(fastdds_domain),
            }
        } {}
        Broadcast_Client(const std::vector<std::string> options) {
            int to_cli[[indeterminate]][2], from_cli[[indeterminate]][2];

            ::pipe(to_cli), ::pipe(from_cli);

            switch (::fork()) {
                case 0:
                    ::close(to_cli[1]), ::close(from_cli[0]);
                    ::dup2(  to_cli[0], 0), ::close(  to_cli[0]);
                    ::dup2(from_cli[1], 1), ::close(from_cli[1]);

                    [[fallthrough]];
                case -1:
                    throw std::runtime_error{
                        "Failed to create sub-process " + std::string{cli_program}
                    };
                default:
                    ::close(to_cli[0]), ::close(from_cli[1]);

            }

        }
    };
}
