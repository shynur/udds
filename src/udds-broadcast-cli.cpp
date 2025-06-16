// 该文件编译后放到 PATH 目录中.
#include <cctype>
#include <format>
#include <ranges>
#include <string>
#include <vector>
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <unistd.h>  // pipe, write, close, STDIN_FILENO
#include <algorithm>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include "udds/broadcast.hpp"

using namespace std::literals;

struct {
    static const auto& operator() [[gnu::nonnull_if_nonzero(2, 1)]] (
        const int argc = 0, const char *const argv[] = nullptr
    ) {
        static const auto options = [&] {
            assert(argc >= 1);

            class {
                const std::string_view default_end_of_json = "shynur.udds.json.end";
              public:
                std::string_view program;
                std::string_view robot_id;
                std::uint8_t fastdds_domain = 1;
                bool development_mode = false;
                std::string_view end_of_json = default_end_of_json;

                operator std::string() const {
                    auto cmdline = std::format(
                        "'{}' --robot_id={} --fastdds_fomain={}",
                        program, robot_id, fastdds_domain
                    );
                    if (development_mode)
                        cmdline += " --development_mode";
                    if (end_of_json != default_end_of_json)
                        cmdline += std::format(" --end_of_json={}", end_of_json);
                    return cmdline;
                }
            } options;

            const auto args
              = std::ranges::subrange{argv, argv + argc}
                | std::views::transform(
                    [](const auto arg) -> std::string_view {return arg;}
                )
                | std::ranges::to<std::vector>();

            options.program = args[0];
            for (const auto arg : args | std::views::drop(1))
                if (const auto param = "--robot_id="sv; arg.starts_with(param))
                    options.robot_id = arg.substr(param.length());
                else if (const auto param = "--fastdds_domain="sv; arg.starts_with(param))
                    options.fastdds_domain = std::stoul(std::string{arg.substr(param.length())});
                else if (arg == "--development_mode"sv)
                    options.development_mode = true;
                else if (const auto param = "--end_of_json="sv; arg.starts_with(param))
                    options.end_of_json = arg.substr(param.length());
                else if (arg == "-h" || arg == "--help") {
                    constexpr unsigned char script[] = {
                        #ifdef __cpp_pp_embed
                            #embed "./udds-broadcast-cli.help.py"  \
                                suffix(,)
                            0
                        #else
                            R"(print("打印帮助信息的功能需要更新的 C++ 编译器编译."))"
                        #endif
                    };
                    int rw[2];
                    ::pipe(rw);
                    ::write(rw[1], script, std::strlen((const char *)script));
                    ::close(rw[1]);
                    std::system(
                        std::format(
                            "bash -c 'python3 /dev/fd/{} -h'",
                            rw[0]
                        ).c_str()
                    );
                    std::exit(0);
                } else
                    throw std::runtime_error{
                        std::format("未知参数: {}", arg)
                    };

            if (options.development_mode)
                std::clog << std::format(
                    "program: {}\n"
                    "robot_id: {}\n"
                    "fastdds_domain: {}\n"
                    "development_mode: {}\n"
                    "end_of_json: {}\n",
                    options.program,
                    options.robot_id,
                    options.fastdds_domain,
                    options.development_mode,
                    options.end_of_json
                );

            return options;
        }();
        return options;
    }
    static const auto& get_options() {
        return operator()();
    }
} arg_parser;

struct cin_with_check_t {
    static volatile std::sig_atomic_t inline no_received_signal = 0;
    static void init() {
        static auto sigint_handler_set [[maybe_unused]]
          = std::signal(
            SIGINT,
            [](int) {
                no_received_signal = SIGINT;
                ::close(STDIN_FILENO);
            }
        );
        static auto cstdio_desynced [[maybe_unused]]
          = std::ios_base::sync_with_stdio(false);
    }
    static auto check() {
        if (!std::cin) [[unlikely]] {
            if (no_received_signal == SIGINT) {
                std::cerr << std::format(
                    "\n*** [{}] Interrupt\n",
                    std::string(arg_parser.get_options())
                );
                std::exit(130);
            }

            std::cerr << std::format(
                "\n*** [{}] EOF is read\n",
                std::string(arg_parser.get_options())
            );
            std::exit(0);
        }
    }

    auto& operator>>(auto&& s) {
        init();
        std::cin >> std::forward<decltype(s)>(s);
        check();
        return *this;
    }
    friend auto& getline(auto&& in, auto& s) {
        init();
        std::getline(std::cin, s);
        check();
        return in;
    }
} cin_with_check;  // singleton

int main(const int argc, const char *const argv[]) {
    arg_parser(argc, argv);

    if (arg_parser.get_options().development_mode)
        std::clog << "Start initializing broadcast server...\n";
    shynur::udds::broadcast::init(std::string{arg_parser.get_options().robot_id});
    if (arg_parser.get_options().development_mode)
        std::clog << "Broadcast server initialized." << std::endl;

    while (true) {
        std::string fn;
        cin_with_check >> fn;

        if (fn == "received_from.contains") {
            std::string robot_id;
            cin_with_check >> robot_id;  // 假设 robot_id 中没有空白字符.

            std::cout << (
                shynur::udds::broadcast::received_from.contains(robot_id)
                ? "true" : "false"
            ) << '\n';
        } else if (fn == "received_from.operator[]") {
            std::string robot_id;
            cin_with_check >> robot_id;  // 假设 robot_id 中没有空白字符.

            const auto message = shynur::udds::broadcast::received_from[robot_id];

            std::cout << std::format(
                "send_timestamp_ns {}\n"
                "received_timestamp_ns {}\n"
                "robot_id {}\n"
                "x {}\n"
                "y {}\n"
                "theta {}\n"
                "json {}\n"
                "{}\n",
                message->send_timestamp_ns(),
                message->received_timestamp_ns(),
                message->robot_id(),
                message->x(),
                message->y(),
                message->theta(),
                message->json(),
                arg_parser.get_options().end_of_json
            ) << '\n';
            std::clog << std::format(
                "刚才查询的消息 len(json)={}\n",
                message->json().length()
            );
        } else if (fn == "received_from.keys") {
            std::cout << std::size(shynur::udds::broadcast::received_from)
                      << '\n';

            for (const auto& robot_id : shynur::udds::broadcast::received_from.keys()) {
                assert(
                    std::ranges::none_of(
                        *robot_id,
                        [](const auto c) {return std::isspace(c);}
                    )
                    && "CLI 无法处理含有 空白字符 的小车名"
                );
                std::cout << *robot_id << ' ';
            }
            std::cout << '\n';
        } else if (fn == "send") {
            if (arg_parser.get_options().development_mode)
                std::clog << "will send...\n";

            auto message = UddsJsonProto{};
            for (auto _ : std::views::iota(0, /* number of fields: */ 4)) {
                const auto field_name = [] {
                    std::string field_name;
                    cin_with_check >> field_name;
                    return field_name;
                }();

                if (field_name == "x")
                    cin_with_check >> message.x();
                else if (field_name == "y")
                    cin_with_check >> message.y();
                else if (field_name == "theta")
                    cin_with_check >> message.theta();
                else if (field_name == "json")
                    message.json() = [] {
                        std::string json;
                        for (
                            std::string line;
                            getline(cin_with_check, line),
                            line != arg_parser.get_options().end_of_json;
                        )
                            json += line + '\n';
                        std::clog << std::format(
                            "即将发送 len(json)={}\n",
                            json.length()
                        );
                        return json;
                    }();
                else
                    throw std::runtime_error{
                        std::format("未知字段: {}", field_name)
                    };
            }

            shynur::udds::broadcast::send(message);
        } else if (fn == "clock_offset_of.ns") {
            std::string robot_id;
            cin_with_check >> robot_id;  // 假设 robot_id 中没有空白字符.

            std::cout << shynur::udds::broadcast::clock_offset_of.ns(robot_id)
                      << '\n';
        } else
            throw std::runtime_error{
                std::format("未知指令: {}", fn)
            };
    }
}
