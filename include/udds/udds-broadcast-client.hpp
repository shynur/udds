#pragma once
#include <errno.h>
#include <signal.h>  // kill
#include <unistd.h>  // dup2, close, fork, pipe
#include <sys/time.h>  // select
#include <sys/wait.h>  // waitpid
#include <bits/stdc++.h>
#include <ext/stdio_filebuf.h>

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
         * @brief Server 程序及其参数.
         * @note Server 需要能在 PATH 中查找到.
         */
        static const char inline
            cli_program[] = "udds-broadcast-cli",
            cli_option_end_of_json[] = "shynur.udds.json.end";

        const std::array<int, 2> to_cli, from_cli;
        const decltype(::fork()) cli_pid;  // cli_pid 一定要在 to_cli 和 from_cli 之后声明!!!
        class Broadcast_Server_IO {
            #if __cplusplus < 201703L
                __gnu_cxx::stdio_filebuf<char> *_tmp_var_within_init;
            #endif

            std::pair<
                const std::unique_ptr<__gnu_cxx::stdio_filebuf<char>>,
                std::basic_ostream<char>
            > to_cli;
            std::pair<
                const std::unique_ptr<__gnu_cxx::stdio_filebuf<char>>,
                std::basic_istream<char>
            > from_cli;
          public:
            __attribute__((fd_arg_write(2), fd_arg_read(3)))
            Broadcast_Server_IO(
                const int to_cli, const int from_cli
            ): to_cli{
                #if __cplusplus >= 201703L
                    [&, this] {
                        auto buf = std::make_unique<__gnu_cxx::stdio_filebuf<char>>(
                            to_cli, std::ios::out
                        );
                        return std::decay_t<decltype(this->to_cli)>{
                            std::move(buf), buf.get()
                        };
                    }()
                #else
                    _tmp_var_within_init = new __gnu_cxx::stdio_filebuf<char>{to_cli, std::ios::out},
                    _tmp_var_within_init
                #endif
            }, from_cli{
                #if __cplusplus >= 201703L
                    [&, this] {
                        auto buf = std::make_unique<__gnu_cxx::stdio_filebuf<char>>(
                            from_cli, std::ios::in
                        );
                        return std::decay_t<decltype(this->from_cli)>{
                            std::move(buf), buf.get()
                        };
                    }()
                #else
                    _tmp_var_within_init = new __gnu_cxx::stdio_filebuf<char>{from_cli, std::ios::in},
                    _tmp_var_within_init
                #endif
            } {
                this->from_cli.second.tie(&this->to_cli.second);
            }

        #if __cplusplus >= 202002L
            auto operator<<(auto&& i)
        #else
            template <typename T>
            auto operator<<(T&& i)
        #endif
            -> auto& {
                this->to_cli.second << std::forward<decltype(i)>(i);
                return *this;
            }

            auto& operator<<(
                std::basic_ostream<char>& (*endl_like)(std::basic_ostream<char>&)
            ) {
                this->to_cli.second << endl_like;
                return *this;
            }

        #if __cplusplus >= 202002L
            auto operator>>(auto&& o)
        #else
            template <typename T>
            auto operator>>(T&& o)
        #endif
            -> auto& {
                this->from_cli.second >> std::forward<decltype(o)>(o);
                return *this;
            }

            friend decltype(auto) getline(Broadcast_Server_IO& io, std::string& line) {
                return std::getline(io.from_cli.second, line);
            }

            bool operator!() const {
                return !(
                    !!this->from_cli.first and !!this->to_cli.second
                );
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
            std::vector<std::string>{
                "--robot_id="s + robot_id,
                "--fastdds_domain="s + std::to_string(fastdds_domain),
                "--end_of_json="s + cli_option_end_of_json,
                "--development_mode"s,
            }
        } {}

        Broadcast_Client(const std::vector<std::string>& options)
        : to_cli{
            []() -> std::decay_t<decltype(this->to_cli)> {
                int fd[2];
                ::pipe(fd);
                std::clog << "to_cli: "s + std::to_string(fd[0])
                             + " <- " + std::to_string(fd[1]) + '\n'
                          << std::flush;
                return {fd[0], fd[1]};
            }()
        }, from_cli{
            []() -> std::decay_t<decltype(this->from_cli)> {
                int fd[2];
                ::pipe(fd);
                std::clog << "from_cli: "s + std::to_string(fd[0])
                             + " <- " + std::to_string(fd[1]) + '\n'
                          << std::flush;
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
                            [](auto c) {return std::isspace(c);}
                        )
                    );

                std::clog << "Forking...\n" << std::flush;
                const auto cli_pid = ::fork();
                std::clog << "Forked, cli_pid=" + std::to_string(cli_pid) + '\n'
                          << std::flush;

                if (cli_pid == 0) {
                    this->close_my_fd();
                    ::dup2(  this->to_cli[0], 0), ::close(  this->to_cli[0]);
                    ::dup2(this->from_cli[1], 1), ::close(this->from_cli[1]);

                    std::clog << "exec "s + cli_program + " ...\n"
                              << std::flush;
                    ::execvp(
                        cli_program,
                        [options=options]() mutable {
                            auto argv = std::vector<char *>{};

                            static auto arg0 = cli_program + " (referer=udds-broadcast-client)"s;
                            argv.push_back(
                                #if __cplusplus < 201703L
                                    (char *)
                                #endif
                                arg0.data()
                            );

                            for (auto& option : options)
                                argv.push_back(
                                    #if __cplusplus < 201703L
                                        (char *)
                                    #endif
                                    option.data()
                                );

                            argv.push_back(nullptr);
                            return argv;
                        }().data()
                    );
                    std::cerr << "Failed to exec `"s + cli_program + "'!!!\n";
                    std::_Exit(EXIT_FAILURE);
                }

                ::close(this->to_cli[0]), ::close(this->from_cli[1]);

                if (cli_pid == -1) {
                    this->close_my_fd();
                    throw std::runtime_error{
                        "Failed to fork sub-process for "s + cli_program
                    };
                }

                // 等待 CLI 发送一个 whitespace 字符, 如果超时则说明有问题:
                if (
                    ::select(
                        this->from_cli[0]+1, [this, rfds=::fd_set{}]() mutable {
                            FD_ZERO(&rfds);
                            const auto ifd = this->from_cli[0];
                            FD_SET(ifd, &rfds);
                            return &rfds;
                        }(),
                        nullptr, nullptr,
                        [] {
                            static auto wait_time = ::timeval{
                                .tv_usec=40'000
                            };
                            return &wait_time;
                        }()
                    )
                ) {
                    this->close_my_fd();
                    if (int cli_stat; ::waitpid(cli_pid, &cli_stat, WNOHANG) && WEXITSTATUS(cli_stat)) {
                        if (WIFEXITED(cli_stat))
                            throw std::runtime_error{
                                "Failed to exec `"s + cli_program + "'!!!"
                            };
                        throw std::runtime_error{
                            "子进程 `"s + cli_program + "' 异常退出!!!"
                        };
                    } else {
                        ::kill(cli_pid, SIGINT);
                        ::waitpid(cli_pid, nullptr, 0);
                        throw std::runtime_error{
                            "子进程 `"s + cli_program + "' 初始化超时!!!"
                        };
                    }
                }

                return cli_pid;
            }()
        }, cli_io{this->to_cli[1], this->from_cli[0]} {
            std::clog << "Client 创建完成!\n" << std::flush;
        }

        ~Broadcast_Client() {
            this->close_my_fd();
            ::kill(this->cli_pid, SIGINT);
            ::waitpid(this->cli_pid, nullptr, 0);
            std::clog << "Udds Server 已经跟随 Client 被关闭.\n" << std::flush;
        }

        void close_my_fd() {
            ::close(this->to_cli[1]);
            ::close(this->from_cli[0]);
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

                        this->client.cli_io << "received_from.operator[] "
                                            << *this->probot << ' ';

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
                            else if (field_name == "json")
                                this->current_message.json = [this] {
                                    auto json = std::string{};
                                    for (
                                        auto line = std::string{};
                                        getline(this->client.cli_io, line),
                                        line != cli_option_end_of_json;
                                    )
                                        json += line + '\n';

                                    return std::string{
                                        std::find_if_not(
                                            json.cbegin(), json.cend(),
                                            [](auto c) {return std::isspace(c);}
                                        ),
                                        std::find_if_not(
                                            json.crbegin(), json.crend(),
                                            [](auto c) {return std::isspace(c);}
                                        ).base()
                                    };
                                }();
                            else
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

            return Range{
                *this,
                [this] {
                    this->cli_io << "received_from.keys" << ' ';

                    std::size_t num_cars;
                    this->cli_io >> num_cars;
                    std::clog << "num_cars=" + std::to_string(num_cars) + '\n'
                              << std::flush;

                    auto robots = std::vector<std::string>{};
                    for (auto _i = 0u; _i < num_cars; ++_i)
                        robots.push_back([this] {
                            std::string robot_id;
                            this->cli_io >> robot_id;
                            return robot_id;
                        }());
                    return robots;
                }()
            };
        }
    };
}

#ifdef SHYNUR_UDDS_USED_BY_SEER_RBK
namespace rbk {
    namespace udds = shynur::udds;
}
#endif
