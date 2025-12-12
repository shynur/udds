#include "urpc.hpp"

int main(const int argc, const char *const argv[]) {
    char method_num_to_call;
    auto options = ::shynur::udds_rpc::Application::Options{};
    for (auto i = 1; i != argc; i++)
        if (const auto arg = std::string{argv[i]}; arg == "-s")
            options.entity = "server";
        else if (arg == "-c")
            options.entity = "client";
        else if (arg.rfind("--thread_pool_size=", 0) == 0)
            options.thread_pool_size = std::stoul(arg.substr(19));
        else if (arg.rfind("--m=", 0) == 0)
            method_num_to_call = arg[4];

    if (options.entity == "client")
        rbk::urpc::call(
            "服务器S", "方法"s+method_num_to_call,
            std::function{[](const std::exception *err) noexcept {
                if (err)
                    std::println(
                        std::cerr,
                        "===== Error =====> {}",
                        err->what()
                    );
                else
                    std::println(
                        "===== Result =====>"
                    );
            }}, 1
        );
    else {
        auto p1 = rbk::urpc::serve("服务器S", "方法i", std::function{[](int i) {return i*2;}});
        auto p2 = rbk::urpc::serve("服务器S", "方法d", std::function{[](double d) {return d*2;}});
        assert(p1 == p2);
        std::this_thread::sleep_for(1min);
    }
}
