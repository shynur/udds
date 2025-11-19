#include <bits/stdc++.h>
#include "urpc.hpp"

struct Server: ::Application::ServerImpl {
    auto addition(const ShynurUrpcProcessorServer_ClientContext&,
        std::int32_t x, std::int32_t y
    ) -> std::int32_t override {
        const auto result = x + y;
        return result;
    }
    auto subtraction(const ShynurUrpcProcessorServer_ClientContext&,
        std::int32_t x, std::int32_t y
    ) -> std::int32_t override {
        const auto result = x - y;
        return result;
    }
};
struct Addition: ::Operation {
    Addition(const std::int32_t x, const std::int32_t y): x{x}, y{y} {}
    const std::int32_t x, y;
    auto execute() -> OperationStatus override {
        std::int32_t result;
        ::shynur::utils::Logger{"DEBUG"}
            << "ClientApp"
            << "Calling addition with x =" << this->x << "  y =" << this->y;
        const auto op_status = this->call_rpc(
            &::ShynurUrpcProcessor::addition, result, this->x, this->y
        );
        ::shynur::utils::Logger{"INFO"}
            << "ClientApp"
            << "Addition result == " << result;
        return op_status;
    }
};
int main(const int argc, const char *const argv[]) {
    auto options = ::Application::Options{};
    for (auto i = 1; i != argc; i++)
        if (const auto arg = std::string{argv[i]}; arg == "-s")
            options.entity = "server";
        else if (arg == "-c")
            options.entity = "client";
        else if (arg.starts_with("--thread_pool_size="))
            options.thread_pool_size = std::stoul(arg.substr(19));

    auto app = ::Application::make_app<Server>(options);
    if (options.entity == "client")
        for (auto i = 0; i != 3; i++) {
            std::int32_t x, y;
            std::cin >> x >> y;
            std::dynamic_pointer_cast<::ClientApp>(app)->call(::Addition{x, y});
        }
    else {
        const auto _ = std::jthread{&::Application::run, app};
        static std::function<void()> stop_app_handler = [=] {app->stop();};
        signal(SIGINT, +[](int) {stop_app_handler();});
    }
}
