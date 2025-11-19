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

int main(const int argc, const char *const argv[]) {
    std::int32_t x, y;

    auto options = ::Application::Options{};
    for (auto i = 1; i != argc; i++)
        if (const auto arg = std::string{argv[i]}; arg == "-s")
            options.entity = "server";
        else if (arg == "-c")
            options.entity = "client";
        else if (arg.starts_with("--thread_pool_size="))
            options.thread_pool_size = std::stoul(arg.substr(19));
        else
            if (static auto x_set = false; !x_set)
                x_set = true, x = std::stoi(arg);
            else
                y = std::stoi(arg);

    auto app = ::Application::make_app<Server>(options);
    if (options.entity == "client") {
        std::dynamic_pointer_cast<::ClientApp>(app)->call(::Addition{x, y});
    } else {
        const auto _ = std::jthread{&::Application::run, app};
        static std::function<void()> stop_app_handler = [=] {app->stop();};
        signal(SIGINT, +[](int) {stop_app_handler();});
    }
}
