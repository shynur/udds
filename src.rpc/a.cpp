#include "urpc.hpp"

int main(const int argc, const char *const argv[]) {
    unsigned method_num_to_call;
    auto options = ::shynur::udds_rpc::Application::Options{};
    for (auto i = 1; i != argc; i++)
        if (const auto arg = std::string{argv[i]}; arg == "-s")
            options.entity = "server";
        else if (arg == "-c")
            options.entity = "client";
        else if (arg.rfind("--thread_pool_size=", 0) == 0)
            options.thread_pool_size = std::stoul(arg.substr(19));
        else if (arg.rfind("--m=", 0) == 0)
            method_num_to_call = std::stoul(arg.substr(4));

    if (options.entity == "client")
        rbk::urpc::call(
            "服务器S", std::format("方法{}", method_num_to_call),
            std::function{[](const std::exception *err, std::string result) noexcept {
                if (err)
                    std::println(
                        std::cerr,
                        "===== Error =====> {}",
                        err->what()
                    );
                else
                    std::println(
                        "===== Result =====> {}",
                        result
                    );
            }},
            2, "ppppppp"
        );
    else {
        const auto handler = std::function{[](int i, std::string s) noexcept {
            return std::to_string(i) + ": " + s;
        }};
        auto p1 = rbk::urpc::serve("服务器S", "方法1", handler);
        auto p2 = rbk::urpc::serve("服务器S", "方法2", handler);
        for (auto i = 3u; i < 1000; ++i)
            rbk::urpc::serve("服务器S", std::format("方法{}", i), handler);
        rbk::urpc::Logger{"INFO"} << "__main__" << "全部注册完成";
        assert(p1 == p2);
        std::this_thread::sleep_for(1min);
    }
}
