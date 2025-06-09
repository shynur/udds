#pragma once
#include <unistd.h>  // dup2, close, fork, pipe
#include <iostream>
#include <stdexcept>
#include <array>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <memory>
#include <utility>
#include <istream>
#include <ostream>
#include <algorithm>
#include <cstdio>
#include <ext/stdio_filebuf.h>
#include <string>
using namespace std::literals;

namespace shynur::udds {
    struct [[gnu::weak]] UddsJsonStruct {
        std::uint64_t
            send_timestamp_ns = 0,
            received_timestamp_ns = 0;
        std::string robot_id;
        double x = 0.0, y = 0.0, theta = 0.0;
        std::string json;
    };

    struct [[gnu::weak]] Broadcast_Client {
        /**
         * @brief Server 程序.
         * @note 需要能在 PATH 中查找到.
         */
        static constexpr char cli_program[] = "udds-broadcast-cli";
        const decltype(::fork()) cli_pid;

        const std::array<int, 2> to_cli, from_cli;
        struct Broadcast_Server_IO {
            std::pair<
                const std::unique_ptr<__gnu_cxx::stdio_filebuf<char>>,
                std::basic_ostream<char>
            > to_cli;
            std::pair<
                const std::unique_ptr<__gnu_cxx::stdio_filebuf<char>>,
                std::basic_istream<char>
            > from_cli;

            Broadcast_Server_IO(const int to_cli, const int from_cli)
            : to_cli{
                [&, this] {
                    auto buf = std::make_unique<__gnu_cxx::stdio_filebuf<char>>(to_cli, std::ios::out);
                    return std::decay_t<decltype(this->to_cli)>{
                        std::move(buf), buf.get()
                    };
                }()
            }, from_cli{
                [&, this] {
                    auto buf = std::make_unique<__gnu_cxx::stdio_filebuf<char>>(from_cli, std::ios::in);
                    return std::decay_t<decltype(this->from_cli)>{
                        std::move(buf), buf.get()
                    };
                }()
            } {}

            auto& operator<<(auto&& i) {
                this->to_cli.second << std::forward<decltype(i)>(i);
                return *this;
            }
            auto& operator>>(auto&& o) {
                this->from_cli.second >> std::forward<decltype(o)>(o);
                return *this;
            }
        } cli_io;

        /**
         * @brief 创建一个 client, 并连接到一个新建的 server 上.
         * @param fastdds_domain 暂时不生效, 随便指定一个值即可.
         */
        Broadcast_Client(
            const std::string robot_id,
            const std::uint8_t fastdds_domain
        ): Broadcast_Client{
            std::vector{
                "--robot_id=" + robot_id,
                "--fastdds_domain=" + std::to_string(fastdds_domain),
            }
        } {}

        Broadcast_Client(const std::vector<std::string>& options)
        : to_cli{
            [] -> std::decay_t<decltype(this->to_cli)> {
                int fd[2];
                ::pipe(fd);
                return {fd[0], fd[1]};
            }()
        }, from_cli{
            [] -> std::decay_t<decltype(this->from_cli)> {
                int fd[2];
                ::pipe(fd);
                return {fd[0], fd[1]};
            }()
        }, cli_pid{
            [&] {
                /* pre contrast */
                for (const auto& option : options)
                    assert(
                        "CLI 程序不支持含空白字符的参数"
                        && std::none_of(
                            option.cbegin(), option.cend(),
                            [](const auto& c) {return std::isspace(c);}
                        )
                    );

                const auto cli_pid = ::fork();

                if (cli_pid == 0) {
                    ::close(this->to_cli[1]), ::close(this->from_cli[0]);
                    ::dup2(  this->to_cli[0], 0), ::close(  this->to_cli[0]);
                    ::dup2(this->from_cli[1], 1), ::close(this->from_cli[1]);

                    ::execvp(
                        cli_program,
                        [options=options] mutable {
                            auto argv = std::vector<char *>{};

                            static auto arg0 = cli_program + " (referer=udds-broadcast-client)"s;
                            argv.push_back(arg0.data());

                            for (auto& option : options)
                                argv.push_back(option.data());

                            argv.push_back(nullptr);
                            return argv;
                        }().data()
                    );
                    std::_Exit(EXIT_FAILURE);
                }

                ::close(this->to_cli[0]), ::close(this->from_cli[1]);

                if (cli_pid == -1) {
                    ::close(this->to_cli[1]), ::close(this->from_cli[0]);
                    throw std::runtime_error{
                        "Failed to create sub-process " + std::string{cli_program}
                    };
                }

                return cli_pid;
            }()
        }, cli_io{this->to_cli[1], this->from_cli[0]} {}

        ~Broadcast_Client() {
            ::close(this->to_cli[1]), ::close(this->from_cli[0]);
        }

        auto send(const UddsJsonStruct& message) {

        }
    };
}
