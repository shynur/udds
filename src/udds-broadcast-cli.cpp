// 该文件编译后放到 PATH 目录中.
#include <atomic>
#include <cctype>
#include <format>
#include <ranges>
#include <string>
#include <vector>
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <string_view>
#include <unordered_map>
#include "udds/broadcast.hpp"

struct Cin_With_Check {
    static struct Flags {
        static std::atomic_flag inline received_sigint = ATOMIC_FLAG_INIT;
        Flags() {
            std::signal(
                SIGINT,
                [](int) static {
                    Flags::received_sigint.test_and_set();
                }
            );
        }
        static auto check() {
            if (received_sigint.test()) {
                std::cerr << std::format(
                    "\n*** [{}] Interrupt\n",
                    __FILE__
                );
                std::exit(130);
            }
        }
    } flags;

    auto& operator>>(auto&& s) {
        std::cin >> std::forward<decltype(s)>(s);
        std::decay_t<decltype(*this)>::flags.check();
        return *this;
    }
    friend auto& getline(auto&& in, auto& s) {
        std::getline(std::cin, s);
        flags.check();
        return in;
    }
} cin_with_check;  // 本质上是一个 singleton, 因为它没有 data member.

auto parse_args [[gnu::unsequenced]] (const std::vector<std::string_view> args) {
    struct {
        std::string_view program;
        std::string_view robot_id;
        std::uint8_t fastdds_domain;
        bool development_mode = false;
        std::string_view end_of_json = "shynur.udds.json.end";
    } options;

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
        else
            throw std::runtime_error{
                std::format("未知参数: {}", arg)
            };

    return options;
}

int main(const int argc, const char *const argv[]) {
    const auto args
        = std::ranges::subrange{argv, argv + argc}
        | std::views::transform(
            [](const auto arg) -> std::string_view {return arg;}
        )
        | std::ranges::to<std::vector>();

    if (parse_args(args).development_mode)
        std::clog << std::format(
            "program: {}\n"
            "robot_id: {}\n"
            "fastdds_domain: {}\n"
            "development_mode: {}\n"
            "end_of_json: {}\n",
            parse_args(args).program,
            parse_args(args).robot_id,
            parse_args(args).fastdds_domain,
            parse_args(args).development_mode,
            parse_args(args).end_of_json
        );

    if (parse_args(args).development_mode)
        std::clog << "Start initializing broadcast server...\n";
    shynur::udds::broadcast::init(std::string{parse_args(args).robot_id});
    if (parse_args(args).development_mode)
        std::clog << "Broadcast server initialized.\n";

    while (true) {
        std::string fn;
        cin_with_check >> fn;

        if (fn == "received_from.contains") {
            std::string robot_id;
            cin_with_check >> robot_id;  // 假设 robot_id 中没有空白字符.

            std::cout << (
                shynur::udds::broadcast::received_from.contains(robot_id)
                ? "true" : "false"
            ) << std::endl;
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
                "json {}\n{}\n",
                message->send_timestamp_ns(),
                message->received_timestamp_ns(),
                message->robot_id(),
                message->x(),
                message->y(),
                message->theta(),
                message->json(),
                parse_args(args).end_of_json
            ) << std::endl;
        } else if (fn == "received_from.keys") {
            std::cout << std::size(shynur::udds::broadcast::received_from)
                      << std::endl;

            for (const auto& robot_id : shynur::udds::broadcast::received_from.keys()) {
                assert(
                    std::ranges::none_of(
                        *robot_id,
                        [](const auto c) {return std::isspace(c);}
                    )
                    && "CLI 无法处理含有 空白字符 的小车名"
                );
                std::cout << *robot_id << std::endl;
            }
        } else if (fn == "send") {
            if (parse_args(args).development_mode)
                std::clog << "will send...\n";

            auto message = UddsJsonProto{};
            for (auto _ : std::views::iota(0, /* number of fields: */ 4)) {
                std::string field_name;
                cin_with_check >> field_name;

                if (field_name == "x")
                    cin_with_check >> message.x();
                else if (field_name == "y")
                    cin_with_check >> message.y();
                else if (field_name == "theta")
                    cin_with_check >> message.theta();
                else if (field_name == "json") {
                    std::string json;
                    for (
                        std::string line;
                        getline(cin_with_check, line),
                        line != parse_args(args).end_of_json;
                    )
                        json += line + '\n';
                    message.json() = std::move(json);
                } else
                    throw std::runtime_error{
                        std::format("未知字段: {}", field_name)
                    };
            }

            shynur::udds::broadcast::send(message);
        } else if (fn == "clock_offset_of.ns") {
            std::string robot_id;
            cin_with_check >> robot_id;  // 假设 robot_id 中没有空白字符.

            std::cout << shynur::udds::broadcast::clock_offset_of.ns(robot_id)
                      << std::endl;
        } else
            throw std::runtime_error{
                std::format("未知指令: {}", fn)
            };
    }
}
