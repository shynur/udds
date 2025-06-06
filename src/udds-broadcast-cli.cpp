#include "broadcast.hpp"
#include <string_view>
#include <vector>
#include <unordered_map>
#include <ranges>
#include <cctype>
#include <iostream>
#include <format>
#include <csignal>
#include <cassert>
#include <algorithm>
#include <string>
#include <cstdlib>
#include <cstdint>

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

    if (options.development_mode)
        std::cerr << std::format(
            "program: {}\n"
            "robot_id: {}\n"
            "fastdds_domain: {}\n"
            "development_mode: {}\n",
            options.program,
            options.robot_id,
            options.fastdds_domain,
            options.development_mode
        );


    return options;
}

int main(const int argc, const char *const argv[]) {
    const auto args
        = std::ranges::subrange{argv, argv + argc}
        | std::views::transform(
            [](const auto arg) -> std::string_view {return arg;}
        )
        | std::ranges::to<std::vector>();

    std::signal(SIGINT, [](int) {std::exit(0);});
    shynur::udds::broadcast::init(std::string{parse_args(args).robot_id});

    while (true) {
        std::string fn;
        std::cin >> fn;

        if (fn == "received_from.contains") {
            std::string robot_id;
            std::cin >> robot_id;  // 假设 robot_id 中没有空白字符.

            std::cout << shynur::udds::broadcast::received_from.contains(robot_id)
                      << std::endl;
        } else if (fn == "received_from.operator[]") {
            std::string robot_id;
            std::cin >> robot_id;  // 假设 robot_id 中没有空白字符.

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
        } else if (fn == "received_from.size") {
            std::cout << shynur::udds::broadcast::received_from.size()
                      << std::endl;
        } else if (fn == "received_from.keys") {
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
            auto message = UddsJsonProto{};
            for (auto _ : std::views::iota(0, /* number of fields: */ 7)) {
                std::string field_name;
                std::cin >> field_name;
                if (field_name == "send_timestamp_ns")
                    std::cin >> message.send_timestamp_ns();
                else if (field_name == "received_timestamp_ns")
                    std::cin >> message.received_timestamp_ns();
                else if (field_name == "robot_id")
                    std::cin >> message.robot_id();
                else if (field_name == "x")
                    std::cin >> message.x();
                else if (field_name == "y")
                    std::cin >> message.y();
                else if (field_name == "theta")
                    std::cin >> message.theta();
                else if (field_name == "json") {
                    std::string json;
                    for (
                        std::string line;
                        std::getline(std::cin, line), line != parse_args(args).end_of_json;
                    )
                        json += line + '\n';
                    message.json() = std::move(json);
                }
                else
                    throw std::runtime_error{
                        std::format("未知字段: {}", field_name)
                    };
            }
        } else if (fn == "clock_offset_of.ns") {
            std::string robot_id;
            std::cin >> robot_id;  // 假设 robot_id 中没有空白字符.

            std::cout << shynur::udds::broadcast::clock_offset_of.ns(robot_id)
                      << std::endl;
        } else
            throw std::runtime_error{
                std::format("未知指令: {}", fn)
            };
    }
}
