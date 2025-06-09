#pragma once
#include <unistd.h>  // dup2, close, fork, pipe
#include <iostream>
#include <stdexcept>
#include <array>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <csignal>
#include <cassert>
#include <memory>
#include <utility>
#include <istream>
#include <ostream>
#include <signal.h>  // kill
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
        std::string robot_id = "";
        double x = 0.0, y = 0.0, theta = 0.0;
        std::string json = "";
    };

    struct [[gnu::weak]] Broadcast_Client {
        /**
         * @brief Server 程序.
         * @note 需要能在 PATH 中查找到.
         */
        static constexpr char cli_program[] = "udds-broadcast-cli";
        static const inline std::string cli_option_end_of_json = "shynur.udds.json.end"s;

        const std::array<int, 2> to_cli, from_cli;
        const decltype(::fork()) cli_pid;  // cli_pid 一定要在 to_cli 和 from_cli 之后声明!!!
        class Broadcast_Server_IO {
            std::pair<
                const std::unique_ptr<__gnu_cxx::stdio_filebuf<char>>,
                std::basic_ostream<char>
            > to_cli;
            std::pair<
                const std::unique_ptr<__gnu_cxx::stdio_filebuf<char>>,
                std::basic_istream<char>
            > from_cli;
          public:
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
            auto& operator<<(
                std::decay_t<decltype(Broadcast_Server_IO::to_cli)::second_type>&
                (* endl_like)(std::decay_t<decltype(Broadcast_Server_IO::to_cli)::second_type>&)
            ) {
                this->to_cli.second << endl_like;
                return *this;
            }
            auto& operator>>(auto&& o) {
                this->from_cli.second >> std::forward<decltype(o)>(o);
                return *this;
            }
            friend decltype(auto) getline(Broadcast_Server_IO& io, std::string& line) {
                return std::getline(io.from_cli.second, line);
            }
        } cli_io;

        /**
         * @brief 创建一个 client, 并连接到一个新建的 server 上.
         * @param fastdds_domain 暂时只能设置为 1.
         */
        Broadcast_Client(
            const std::string robot_id,
            const std::uint8_t fastdds_domain
        ): Broadcast_Client{
            std::vector{
                "--robot_id=" + robot_id,
                "--fastdds_domain=" + std::to_string(fastdds_domain),
                "--end_of_json=" + cli_option_end_of_json,
                "--development_mode"s,
            }
        } {}

        Broadcast_Client(const std::vector<std::string>& options)
        : to_cli{
            [] -> std::decay_t<decltype(this->to_cli)> {
                int fd[2];
                ::pipe(fd);
                std::cerr << "to_cli: " << fd[0] << " <- " << fd[1] << '\n';
                return {fd[0], fd[1]};
            }()
        }, from_cli{
            [] -> std::decay_t<decltype(this->from_cli)> {
                int fd[2];
                ::pipe(fd);
                std::cerr << "from_cli: " << fd[0] << " <- " << fd[1] << '\n';
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
        }, cli_io{this->to_cli[1], this->from_cli[0]} {
            std::cerr << "Client 创建完成!\n";
        }

        ~Broadcast_Client() {
            ::close(this->to_cli[1]), ::close(this->from_cli[0]);
            ::kill(this->cli_pid, SIGINT);
        }

        /**
         * @brief 委托 server 向广播系统发布消息.
         * @param message x / y / theta / json 以外的字段会被忽略.
         */
        auto send(const UddsJsonStruct& message) {
            this->cli_io << "send\n"
                         << "x " << message.x << '\n'
                         << "y " << message.y << '\n'
                         << "theta " << message.theta << '\n'
                         << "json " << message.json << '\n'
                         << cli_option_end_of_json << std::endl;
        }

        /**
         * @brief 获取所有已接收的消息.
         *        通过 `for (auto [robot_id, message] : received_from()) {}` 遍历.
         */
        auto received_from() {
            this->cli_io << "received_from.keys" << std::endl;
            unsigned num_cars;
            this->cli_io >> num_cars;
            std::cerr << "num_cars=" << num_cars << '\n';

            auto robots = std::vector<std::string>{};
            for (auto i = 0u; i != num_cars; ++i) {
                std::string robot_id;
                this->cli_io >> robot_id;
                robots.push_back(std::move(robot_id));
            }

            struct Range {
                Broadcast_Client& client;
                const std::vector<std::string> robot_ids;

                struct iterator {
                    Broadcast_Client& client;
                    decltype(Range::robot_ids)::const_iterator probot;
                    mutable UddsJsonStruct current_message{};

                    auto& operator++() {
                        ++this->probot;
                        return *this;
                    }
                    auto operator*() const -> std::pair<std::string, UddsJsonStruct> {
                        if (this->current_message.robot_id == *this->probot)
                            return {*this->probot, this->current_message};

                        this->client.cli_io << "received_from.operator[]\n"
                                            << *this->probot << std::endl;

                        for (auto _ : std::array<char, /* UddsJson 字段数量: */ 7>{}) {
                            std::string field_name;
                            this->client.cli_io >> field_name;
                            if (field_name == "send_timestamp_ns")
                                this->client.cli_io >> this->current_message.send_timestamp_ns;
                            else if (field_name == "received_timestamp_ns")
                                this->client.cli_io >> this->current_message.received_timestamp_ns;
                            else if (field_name == "robot_id")
                                this->client.cli_io >> this->current_message.robot_id;
                            else if (field_name == "x")
                                this->client.cli_io >> this->current_message.x;
                            else if (field_name == "y")
                                this->client.cli_io >> this->current_message.y;
                            else if (field_name == "theta")
                                this->client.cli_io >> this->current_message.theta;
                            else if (field_name == "json") {
                                std::string json;
                                for (
                                    std::string line;
                                    getline(this->client.cli_io, line), line != cli_option_end_of_json;
                                )
                                    json += line + '\n';
                                this->current_message.json = std::move(json);
                            } else
                                throw std::runtime_error{
                                    "未知字段: {}" + field_name
                                };
                        }

                        return **this;
                    }
                    auto operator!=(const iterator& other) const {
                        return this->probot != other.probot;
                    }
                };

                auto begin() const -> iterator {return {this->client, this->robot_ids.cbegin()};}
                auto   end() const -> iterator {return {this->client, this->robot_ids.cend()  };}
            };

            return Range{*this, std::move(robots)};
        }
    };
}

#ifdef SHYNUR_UDDS_USED_BY_SEER_RBK
namespace rbk {
    namespace udds = shynur::udds;
}
#endif
