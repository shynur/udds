#include "urpc.hpp"

int main(const int argc, const char *const argv[]) {
    auto options = ::shynur::udds_rpc::Application::Options{};
    for (auto i = 1; i != argc; i++)
        if (const auto arg = std::string{argv[i]}; arg == "-s")
            options.entity = "server";
        else if (arg == "-c")
            options.entity = "client";
        else if (arg.rfind("--thread_pool_size=", 0) == 0)
            options.thread_pool_size = std::stoul(arg.substr(19));

    ::shynur::utils::Logger::enabled = true;
    if (options.entity == "client")
        rbk::urpc::call(
            "Service1",
            std::function{[](const std::exception *err, std::string result) noexcept {
                if (err != nullptr)
                    return;
                std::cout << "===== Result =====> " << result << std::endl;
            }},
            2, "ppppppp"
        );
    else {
        auto ptr = rbk::urpc::serve(
            "Service1",
            std::function{[](int i, std::string s) noexcept {
                return std::to_string(i) + ": " + s;
            }}
        );
        std::this_thread::sleep_for(1min);
    }
}
