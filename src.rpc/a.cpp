#include "urpc.hpp"

int main(const int argc, const char *const argv[]) {
    auto options = ::shynur::udds_rpc::Application::Options{};
    for (auto i = 1; i != argc; i++)
        if (const auto arg = std::string{argv[i]}; arg == "-s")
            options.entity = "server";
        else if (arg == "-c")
            options.entity = "client";
        else if (arg.starts_with("--thread_pool_size="))
            options.thread_pool_size = std::stoul(arg.substr(19));

    if (options.entity == "client") {
        seer::urpc::call("Service1", "{}", [](auto err, auto result) noexcept {
            if (err != nullptr)
                return;
            std::cout << "===== Result =====> " << result << std::endl;
        });
    } else {
        seer::urpc::serve("Service1", [](auto json) noexcept {
            return "[" + json + "]";
        });
    }
}
