#include <bits/stdc++.h>
#include "urpc.hpp"

struct Server: ::shynur::udds_rpc::Application::ServerImpl {
    auto addition(const ::ShynurUrpcProcessorServer_ClientContext&,
        std::int32_t x, std::int32_t y
    ) -> std::int32_t override {
        const auto result = x + y;
        return result;
    }
    auto floor(const ::ShynurUrpcProcessorServer_ClientContext&,
        const UddsTwoDouble& args
    ) -> UddsTwoInt override {
        UddsTwoInt result;
        result.x(args.x());
        result.y(args.y());
        return result;
    }
};
struct AdditionClient: ::shynur::udds_rpc::ClientApp::Operation {
    const std::int32_t x, y;
    AdditionClient(const std::int32_t x, const std::int32_t y): x{x}, y{y} {}

    auto execute() -> OperationStatus override {
        std::int32_t result;
        const auto op_status = this->call_rpc(
            &::ShynurUrpcProcessor::addition, result, this->x, this->y
        );

        assert(op_status == SUCCESS);
        ::shynur::utils::Logger{"INFO"}
            << "ClientApp"
            << "Addition result == " << result << "!!!";

        return op_status;
    }
};
struct FloorClient: ::shynur::udds_rpc::ClientApp::Operation {
    const double x, y;
    FloorClient(const double x, const double y): x{x}, y{y} {}

    auto execute() -> OperationStatus override {
        UddsTwoInt result;
        UddsTwoDouble arg;
        arg.x(this->x), arg.y(this->y);

        const auto op_status = this->call_rpc(
            &::ShynurUrpcProcessor::floor,
            result, arg
        );

        assert(op_status == SUCCESS);
        ::shynur::utils::Logger{"INFO"}
            << "ClientApp"
            << "Floor result: " << result.x() << result.y() << "!!!";

        return op_status;
    }
};

int main(const int argc, const char *const argv[]) {
    auto options = ::shynur::udds_rpc::Application::Options{};
    for (auto i = 1; i != argc; i++)
        if (const auto arg = std::string{argv[i]}; arg == "-s")
            options.entity = "server";
        else if (arg == "-c")
            options.entity = "client";
        else if (arg.starts_with("--thread_pool_size="))
            options.thread_pool_size = std::stoul(arg.substr(19));

    auto app = ::shynur::udds_rpc::Application::make_app<Server>(options);
    if (options.entity == "client") {
        std::int32_t x, y;
        std::cin >> x >> y;
        std::dynamic_pointer_cast<::shynur::udds_rpc::ClientApp>(app)->call(AdditionClient{x, y});

        double p, q;
        std::cin >> p >> q;
        std::dynamic_pointer_cast<::shynur::udds_rpc::ClientApp>(app)->call(FloorClient{p, q});
    } else {
        const auto _ = std::jthread{&::shynur::udds_rpc::Application::run, app};
        static std::function<void()> stop_app_handler = [=] {app->stop();};
        signal(SIGINT, +[](int) {stop_app_handler();});
    }
}
