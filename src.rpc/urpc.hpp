/*
 * Author: 谢骐 <shynur@outlook.com>
 * URL: https://github.com/shynur/udds
 */
#pragma once
#include <bits/stdc++.h>
using namespace std::literals;
#include "shynur-polyfill.hpp"
#include "shynur-utils-logger.hpp"

#include <fastdds/dds/rpc/exceptions.hpp>
#include <fastdds/dds/domain/qos/ReplierQos.hpp>
#include <fastdds/dds/domain/qos/RequesterQos.hpp>
#include <fastdds/dds/rpc/interfaces/RpcFuture.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantExtendedQos.hpp>
#include "ShynurUrpcProcessor.hpp"
#include "ShynurUrpcProcessorClient.hpp"
#include "ShynurUrpcProcessorServer.hpp"
#include "ShynurUrpcProcessorServerImpl.hpp"

namespace shynur::udds_rpc {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
struct [[gnu::weak]] Logger: ::shynur::utils::Logger<Logger> {
#pragma GCC diagnostic pop
    using ::shynur::utils::Logger<Logger>::Logger;
    static constexpr const char *env_switch = "URPC_LOG";
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
struct [[gnu::weak]] Application {
#pragma GCC diagnostic pop
    virtual ~Application() = default;
    virtual void run()     = 0;
    virtual void stop()    = 0;
    struct ServerImpl: ::ShynurUrpcProcessorServerImplementation,
                       std::enable_shared_from_this<ServerImpl> {
        auto ptr() -> std::shared_ptr<ServerImpl>{
            return shared_from_this();
        }
      private:
        auto ping_(
            const ::ShynurUrpcProcessorServer_ClientContext&,
            const std::string&
        ) -> std::string override {
            return "";
        }
    };

    struct Options {
        std::string server;  // 服务器名
        std::string entity;  // server|client
        std::size_t thread_pool_size = [] {
            static const auto val = [] {
                const auto var = "URPC_SERVER_DEFAULT_NUM_THREADS"s;
                const auto val = std::string{std::getenv(var.c_str()) ? std::getenv(var.c_str()) : ""};
                Logger{"INFO"} << "export" << var + "=" + val;
                return val.empty() ? "0"s : val;
            }();
            return std::stoull(val);
        }();
        std::shared_ptr<ServerImpl> server_impl = nullptr;
    };
    template <
    #ifdef __cpp_lib_concepts
        std::derived_from<ServerImpl>
    #else
        typename
    #endif
        UserDefinedServerImpl
    >
    static auto make_app(const Options& options) -> std::shared_ptr<Application>;
};

template <
    #ifdef __cpp_lib_concepts
        std::derived_from<Application::ServerImpl>
    #else
        typename
    #endif
        UserDefinedServerImpl
>
struct ServerApp: Application {
    const struct Config {
        const std::size_t thread_pool_size = 0;
        std::weak_ptr<UserDefinedServerImpl> server_impl;
    } config;

    ServerApp(
        const std::string_view service_name, const Config& config
    ): config{config},
       server_impl_{this->config.server_impl},
       server_{[&, this] {
        const auto server = ::create_ShynurUrpcProcessorServer(
            *this->participant_,
            std::string{service_name}.c_str(),
            [] {
                auto qos = ::eprosima::fastdds::dds::ReplierQos{};
                return qos;
            }(),
            [this] {
                Logger{"DEBUG"} << "Initializing Server"
                                << std::format(
                                    "thread_pool_size={}",
                                    this->config.thread_pool_size
                                );
                return this->config.thread_pool_size;
            }(),
            this->server_impl_
        );
        if (!server)
            throw std::runtime_error{"Server initialization failed"};
        return server;
    }()} {
        Logger{"DEBUG"}
            << "Server (" + std::string{service_name} + ") Initialized"
            << this->participant_->guid().guidPrefix;
    }
    ~ServerApp() override {
        // As a precautionary measure, delete the server here because
        // `this->participant_->delete_contained_entities()` does not
        // automatically disable the service.  This line can be removed
        // once the RPC internal API supports service disabling.
        this->server_.reset();

        if (this->participant_) {
            this->participant_->delete_contained_entities();
            ::eprosima::fastdds::dds::DomainParticipantFactory::get_shared_instance()
                ->delete_participant(this->participant_);
        }
    }
    void run() override {
        if (this->stopped_)
            return;

        Logger{"INFO"}
            << "Server Running"
            << this->participant_->guid().guidPrefix;
        this->server_->run();
    }
    void stop() override {
        this->stopped_ = true;
        this->server_->stop();

        Logger{"INFO"}
            << "Server Stopped"
            << this->participant_->guid().guidPrefix;
    }
  private:
    std::atomic_bool                                   stopped_     = false;
    ::eprosima::fastdds::dds::DomainParticipant *const participant_ = [] {
        const auto factory = ::eprosima::fastdds::dds::DomainParticipantFactory::get_shared_instance();
        if (!factory)
            throw std::runtime_error{"shynur.urpc Failed to get participant factory instance"};

        const auto participant = factory->create_participant(
            []() -> ::eprosima::fastdds::dds::DomainId_t {
                static const auto val = [] {
                    const auto var = "URPC_DEFAULT_DOMAIN_ID"s;
                    const auto val = std::string{std::getenv(var.c_str()) ? std::getenv(var.c_str()) : ""};
                    Logger{"INFO"} << "export" << var + "="s + val;
                    return val.empty() ? "0"s : val;
                }();
                return std::stoull(val);
            }(),
            [] {
                auto qos = ::eprosima::fastdds::dds::DomainParticipantQos{};

                static const auto URPC_DEFAULT_LEASE_DURATION = [] {
                    const auto var = "URPC_DEFAULT_LEASE_DURATION"s;
                    const auto val = std::string{std::getenv(var.c_str()) ? std::getenv(var.c_str()) : ""};
                    Logger{"INFO"} << "export" << var + "="s + val;
                    return val;
                }();
                if (!URPC_DEFAULT_LEASE_DURATION.empty()) {
                    static const auto default_val = std::stod(URPC_DEFAULT_LEASE_DURATION);
                    qos.wire_protocol().builtin.discovery_config.leaseDuration = {
                        std::int32_t(default_val),
                        std::uint32_t((default_val - std::floor(default_val)) * 1'000'000'000)
                    };
                }

                static const auto URPC_DEFAULT_ANNOUNCEMENT_PERIOD = [] {
                    const auto var = "URPC_DEFAULT_ANNOUNCEMENT_PERIOD"s;
                    const auto val = std::string{std::getenv(var.c_str()) ? std::getenv(var.c_str()) : ""};
                    Logger{"INFO"} << "export" << var + "="s + val;
                    return val;
                }();
                if (!URPC_DEFAULT_ANNOUNCEMENT_PERIOD.empty()) {
                    static const auto default_val = std::stod(URPC_DEFAULT_ANNOUNCEMENT_PERIOD);
                    qos.wire_protocol().builtin.discovery_config.leaseDuration_announcementperiod = {
                        std::int32_t(default_val),
                        std::uint32_t((default_val - std::floor(default_val)) * 1'000'000'000)
                    };
                }

                static const auto URPC_DEFAULT_INITIAL_ANNOUNCEMENT_COUNT = [] {
                    const auto var = "URPC_DEFAULT_INITIAL_ANNOUNCEMENT_COUNT"s;
                    const auto val = std::string{std::getenv(var.c_str()) ? std::getenv(var.c_str()) : ""};
                    Logger{"INFO"} << "export" << var + "="s + val;
                    return val;
                }();
                if (!URPC_DEFAULT_INITIAL_ANNOUNCEMENT_COUNT.empty()) {
                    static const auto default_val = std::stoull(URPC_DEFAULT_INITIAL_ANNOUNCEMENT_COUNT);
                    qos.wire_protocol().builtin.discovery_config.initial_announcements.count = default_val;
                }

                static const auto URPC_DEFAULT_INITIAL_ANNOUNCEMENT_PERIOD = [] {
                    const auto var = "URPC_DEFAULT_INITIAL_ANNOUNCEMENT_PERIOD"s;
                    const auto val = std::string{std::getenv(var.c_str()) ? std::getenv(var.c_str()) : ""};
                    Logger{"INFO"} << "export" << var + "="s + val;
                    return val;
                }();
                if (!URPC_DEFAULT_INITIAL_ANNOUNCEMENT_PERIOD.empty()) {
                    static const auto default_val = std::stod(URPC_DEFAULT_INITIAL_ANNOUNCEMENT_PERIOD);
                    qos.wire_protocol().builtin.discovery_config.initial_announcements.period = {
                        std::int32_t(default_val),
                        std::uint32_t((default_val - std::floor(default_val)) * 1'000'000'000)
                    };
                }

                return qos;
            }()
        );
        if (!participant)
            throw std::runtime_error{"shynur.urpc Participant initialization failed"};

        return participant;
    }();
    std::shared_ptr<ServerImpl> server_impl_;
    std::shared_ptr<::ShynurUrpcProcessorServer> server_;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
struct [[gnu::weak]] ClientApp: Application {
#pragma GCC diagnostic pop
    struct Operation {
        enum class OperationStatus {SUCCESS, TIMEOUT, ERROR};
        #ifdef __cpp_using_enum
            using enum OperationStatus;
        #else
            static constexpr OperationStatus SUCCESS = OperationStatus::SUCCESS;
            static constexpr OperationStatus TIMEOUT = OperationStatus::TIMEOUT;
            static constexpr OperationStatus ERROR   = OperationStatus::ERROR;
        #endif
        virtual auto execute() -> OperationStatus = 0;
        virtual ~Operation()              = default;
      protected:
        #ifndef __cpp_concepts
            template <typename T, typename R, typename... Args>
        #endif
        auto call_rpc(
            const
            #ifdef __cpp_concepts
                auto
            #else
                T
            #endif
            rpc,
            #ifdef __cpp_concepts
                auto
            #else
                R
            #endif
            && result,
            #ifdef __cpp_concepts
                auto
            #else
                Args
            #endif
            &&... args
        ) /* final */ {
            if (auto client = this->client_.lock()) {
                auto future = std::mem_fn(rpc)(
                    client, std::forward<decltype(args)>(args)...
                );
                if (future.wait_for([] {
                    static const auto val = [] {
                        const auto var = "URPC_TIMEOUT"s;
                        const auto val = std::string{std::getenv(var.c_str()) ? std::getenv(var.c_str()) : ""};
                        Logger{"INFO"} << "export" << var + "="s + val;
                        return val.empty() ? "15"s : val;
                    }();
                    static const auto timeout = std::chrono::duration<double>{std::stod(val)};
                    return timeout;
                }()) != std::future_status::ready) {
                    Logger{"ERROR"}
                        << "Client RPC"
                        << "Timed Out";
                    return TIMEOUT;
                }
                try {
                    result = future.get();
                    Logger{"INFO"}
                        << "Client RPC"
                        << "Success";
                    return SUCCESS;
                } catch (const ::eprosima::fastdds::dds::rpc::RpcBrokenPipeException&) {
                    Logger{"ERROR"}
                        << "Client RPC"
                        << "Server Disconnected";
                    return ERROR;
                } catch (const ::eprosima::fastdds::dds::rpc::RpcException& e) {
                    Logger{"ERROR"}
                        << "Client RPC"
                        << "Exception:" << e.what();
                    return ERROR;
                }
            }
            throw std::runtime_error{"Client reference expired"};
        }
    private:
        mutable std::weak_ptr<::ShynurUrpcProcessor> client_;
        friend class ClientApp;
    };

    const struct Config {
        const std::size_t connection_attempts = 10;
    } config;

    ClientApp(
        const std::string_view service_name, const Config& config
    ): config{config}, client_{
        [&, this] {
            const auto client = ::create_ShynurUrpcProcessorClient(
                *this->participant_,
                std::string{service_name}.c_str(),
                [] {
                    auto qos = ::eprosima::fastdds::dds::RequesterQos{};
                    return qos;
                }()
            );
            if (!client)
                throw std::runtime_error{"Failed to create client"};
            return client;
        }()
    } {
        Logger{"DEBUG"}
            << "Client Initialized"
            << this->participant_->guid().guidPrefix;
    }
    ~ClientApp() override {
        Logger{"DEBUG"}
            << "Client Destroying"
            << this->participant_->guid().guidPrefix;
        // As a precautionary measure, delete the server here because
        // `this->participant_->delete_contained_entities()` does not
        // automatically disable the service.  This line can be removed
        // once the RPC internal API supports service disabling.
        this->client_.reset();

        if (this->participant_) {
            this->participant_->delete_contained_entities();
            ::eprosima::fastdds::dds::DomainParticipantFactory::get_shared_instance()
                ->delete_participant(this->participant_);
        }
    }
    void stop() override {
        this->stopped_ = true;
        Logger{"INFO"}
            << "Client Stopped"
            << this->participant_->guid().guidPrefix;
    }
    #ifndef __cpp_concepts
        template <typename T>
    #endif
    auto call(
        #ifdef __cpp_lib_concepts
            std::derived_from<Operation> auto
        #else
            T
        #endif
        op
    ) {
        this->set_operation(std::move(op));
        this->run();
    }
  protected:
    void run() override {
        if (this->stopped_)
            return;

        if (!this->ping_server()) {
            if (!this->stopped_) {
                 Logger{"ERROR"}
                    << "Client RPC"
                    << "Server not reachable.  Aborting this rpc...";
                throw std::runtime_error{"Server not reachable"};
            }
        }

        if (!this->stopped_)
            this->operation_->execute();
    }
    #ifndef __cpp_concepts
        template <typename T>
    #endif
    void set_operation(
        #ifdef __cpp_lib_concepts
            std::derived_from<Operation> auto
        #else
            T
        #endif
        op
    ) {
        op.Operation::client_ = this->client_;
        this->operation_ = std::unique_ptr<Operation>{
            new auto{std::move(op)}
        };
    }
    bool ping_server() {
        struct Ping: Operation {
            auto execute() -> OperationStatus override {
                const auto op_status = this->call_rpc(
                    &::ShynurUrpcProcessor::ping_, ""s, ""s
                );
                Logger{"DEBUG"} << "Client" << "Tried ping server";
                return op_status;
            }
        };

        auto original_operation = std::move(this->operation_);
        this->set_operation<Ping>({});

        auto reachable = false;
        for (auto i = 0u; i < this->config.connection_attempts; i++)
            if (!this->stopped_) {
                Logger{"DEBUG"}
                   << "Client"
                   << "Trying to ping server"
                   << '(' << (i + 1) << "/" << this->config.connection_attempts << ')';

                std::this_thread::sleep_for(1s);

                reachable = this->operation_->execute() == Operation::SUCCESS;
                if (reachable)
                    break;

                if (i == this->config.connection_attempts - 1)
                    Logger{"ERROR"}
                        << "Client"
                        << "All ping failed";
            }

        this->operation_ = std::move(original_operation);
        return reachable;
     }

    std::atomic_bool                             stopped_     = false;
    ::eprosima::fastdds::dds::DomainParticipant *participant_ = [] {
        const auto factory = ::eprosima::fastdds::dds::DomainParticipantFactory::get_shared_instance();
        if (!factory)
            throw std::runtime_error{"shynur.urpc Failed to get participant factory instance"};

        const auto participant = factory->create_participant(
            []() -> ::eprosima::fastdds::dds::DomainId_t {
                static const auto val = [] {
                    const auto var = "URPC_DEFAULT_DOMAIN_ID"s;
                    const auto val = std::string{std::getenv(var.c_str()) ? std::getenv(var.c_str()) : ""};
                    Logger{"INFO"} << "export" << var + "="s + val;
                    return val.empty() ? "0"s : val;
                }();
                return std::stoull(val);
            }(),
            [] {
                auto qos = ::eprosima::fastdds::dds::DomainParticipantQos{};

                static const auto URPC_DEFAULT_LEASE_DURATION = [] {
                    const auto var = "URPC_DEFAULT_LEASE_DURATION"s;
                    const auto val = std::string{std::getenv(var.c_str()) ? std::getenv(var.c_str()) : ""};
                    Logger{"INFO"} << "export" << var + "="s + val;
                    return val;
                }();
                if (!URPC_DEFAULT_LEASE_DURATION.empty()) {
                    static const auto default_val = std::stod(URPC_DEFAULT_LEASE_DURATION);
                    qos.wire_protocol().builtin.discovery_config.leaseDuration = {
                        std::int32_t(default_val),
                        std::uint32_t((default_val - std::floor(default_val)) * 1'000'000'000)
                    };
                }

                static const auto URPC_DEFAULT_ANNOUNCEMENT_PERIOD = [] {
                    const auto var = "URPC_DEFAULT_ANNOUNCEMENT_PERIOD"s;
                    const auto val = std::string{std::getenv(var.c_str()) ? std::getenv(var.c_str()) : ""};
                    Logger{"INFO"} << "export" << var + "="s + val;
                    return val;
                }();
                if (!URPC_DEFAULT_ANNOUNCEMENT_PERIOD.empty()) {
                    static const auto default_val = std::stod(URPC_DEFAULT_ANNOUNCEMENT_PERIOD);
                    qos.wire_protocol().builtin.discovery_config.leaseDuration_announcementperiod = {
                        std::int32_t(default_val),
                        std::uint32_t((default_val - std::floor(default_val)) * 1'000'000'000)
                    };
                }

                static const auto URPC_DEFAULT_INITIAL_ANNOUNCEMENT_COUNT = [] {
                    const auto var = "URPC_DEFAULT_INITIAL_ANNOUNCEMENT_COUNT"s;
                    const auto val = std::string{std::getenv(var.c_str()) ? std::getenv(var.c_str()) : ""};
                    Logger{"INFO"} << "export" << var + "="s + val;
                    return val;
                }();
                if (!URPC_DEFAULT_INITIAL_ANNOUNCEMENT_COUNT.empty()) {
                    static const auto default_val = std::stoull(URPC_DEFAULT_INITIAL_ANNOUNCEMENT_COUNT);
                    qos.wire_protocol().builtin.discovery_config.initial_announcements.count = default_val;
                }

                static const auto URPC_DEFAULT_INITIAL_ANNOUNCEMENT_PERIOD = [] {
                    const auto var = "URPC_DEFAULT_INITIAL_ANNOUNCEMENT_PERIOD"s;
                    const auto val = std::string{std::getenv(var.c_str()) ? std::getenv(var.c_str()) : ""};
                    Logger{"INFO"} << "export" << var + "="s + val;
                    return val;
                }();
                if (!URPC_DEFAULT_INITIAL_ANNOUNCEMENT_PERIOD.empty()) {
                    static const auto default_val = std::stod(URPC_DEFAULT_INITIAL_ANNOUNCEMENT_PERIOD);
                    qos.wire_protocol().builtin.discovery_config.initial_announcements.period = {
                        std::int32_t(default_val),
                        std::uint32_t((default_val - std::floor(default_val)) * 1'000'000'000)
                    };
                }

                return qos;
            }()
        );
        if (!participant)
            throw std::runtime_error{"shynur.urpc Participant initialization failed"};

        return participant;
    }();
    std::shared_ptr<ShynurUrpcProcessor> client_;
    std::unique_ptr<Operation>    operation_;
};

template <
    #ifdef __cpp_lib_concepts
        std::derived_from<Application::ServerImpl>
    #else
        typename
    #endif
    UserDefinedServerImpl
>
auto Application::make_app(const Options& options) -> std::shared_ptr<Application> {
    const auto service_name = options.server + "_Service"s;
    Application *app;
    if (options.entity == "server"s)
        app = new ServerApp<UserDefinedServerImpl>{
            service_name,
            {
                .thread_pool_size = options.thread_pool_size,
                .server_impl = options.server_impl
                               ? std::dynamic_pointer_cast<UserDefinedServerImpl>(options.server_impl)
                               : std::shared_ptr<UserDefinedServerImpl>{new UserDefinedServerImpl},
            }
        };
    else
        app = new ClientApp{
            service_name,
            {}
        };
    return std::shared_ptr<Application>{app};
}
} // namespace shynur::udds_rpc

/* SEER RBK */
#include "nlohmann/json.hpp"

namespace rbk {
    template <typename... Ts>
    using LittleLogger = ::shynur::utils::Logger<Ts...>;
}

namespace rbk::urpc {

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wattributes"
    struct [[gnu::weak]] Logger: LittleLogger<Logger> {
    #pragma GCC diagnostic pop
        using LittleLogger<Logger>::Logger;
        static constexpr const char *env_switch = "URPC_LOG";
    };

    namespace _detail {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wattributes"
        struct [[gnu::weak]] Server: ::shynur::udds_rpc::Application::ServerImpl {
        #pragma GCC diagnostic pop
            std::unordered_map<std::string, std::function<std::string(std::string)>> methods{};
            std::shared_mutex methods_mutex{};

            auto f(const ::ShynurUrpcProcessorServer_ClientContext&,
                const std::string& m, const std::string& x
            ) -> std::string override {
                return [this, m]() -> auto& {
                    const auto lock [[maybe_unused]] = std::shared_lock{this->methods_mutex};
                    return this->methods.at(m);
                }()(x);
            }
        };
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wattributes"
        struct [[gnu::weak]] Client: ::shynur::udds_rpc::ClientApp::Operation {
        #pragma GCC diagnostic pop
            const std::string m, x;
            const std::function<void(const std::exception *, std::string)> callback;
            Client(
                const std::string& m,
                const std::string& x,
                std::function<void(const std::exception *, std::string)> callback
            ): m{m}, x{x}, callback{std::move(callback)} {}

            auto execute() -> OperationStatus override {
                std::string result;
                OperationStatus op_status;

                try {
                    op_status = this->call_rpc(
                        &::ShynurUrpcProcessor::f, result, this->m, this->x
                    );
                } catch (const std::exception& e) {
                    callback(&e, "");
                    return OperationStatus::ERROR;
                }

                switch (op_status) {
                    case OperationStatus::TIMEOUT: {
                        const auto e = std::runtime_error{"Client.RPC Timed Out"};
                        callback(&e, "");
                        break;
                    }
                    case OperationStatus::ERROR: {
                        const auto e = std::runtime_error{"Client.RPC Error"};
                        callback(&e, "");
                        break;
                    }
                    case OperationStatus::SUCCESS: {
                        Logger{"INFO"}
                            << "Client RPC Result"
                            << (
                                result.length() <= 40
                                ? result
                                : std::format(
                                    "{} /*...*/ {}",
                                    result.substr(0, 15),
                                    result.substr(result.size() - 15)
                                )
                            );
                        callback(nullptr, result);
                    }
                }
                return op_status;
            }
        };

        inline auto serve(
            const std::string& server, const std::string& method,
            std::function<std::string(std::string)> handler
        ) {
            struct AppRunner {
                const std::shared_ptr<::shynur::udds_rpc::Application> app;
                std::thread                                    thread;

                AppRunner(
                    const std::shared_ptr<::shynur::udds_rpc::Application> app
                ): app{app}, thread{&::shynur::udds_rpc::Application::run, this->app} {}

                ~AppRunner() {
                    this->app->stop();
                    if (this->thread.joinable())
                        this->thread.join();
                }
            };

            // static const auto disabled_services = [](const char *const var) {
            //     const auto val = std::string{std::getenv(var) ? std::getenv(var) : ""};
            //     Logger{"INFO"} << "export" << var + "="s + val;
            //     auto services = std::unordered_set<std::string>{};
            //     std::istringstream in{val};
            //     for (std::string service; std::getline(in, service, ','); )
            //         if (!service.empty())
            //             services.insert(service);
            //     return services;
            // }("RBK_URPC_DISABLED_SERVICES");
            // if (disabled_services.find(service_name) != disabled_services.cend()) {
            //     Logger{"DEBUG"}
            //         << "serve "s + service_name
            //         << "is disabled via RBK_URPC_DISABLED_SERVICES";
            //     return std::shared_ptr<AppRunner>{};
            // }

            static auto server_impls = std::unordered_map<
                std::string,
                std::shared_ptr<Server>
            >{};
            if (server_impls.find(server) == server_impls.cend())
                server_impls[server] = std::make_shared<Server>();
            [&](const auto& server_impl) {
                const auto lock [[maybe_unused]] = std::unique_lock{server_impl->methods_mutex};
                server_impl->methods[method] = std::move(handler);
            }(server_impls.at(server));

            static auto server_apps = std::unordered_map<
                std::string,
                std::weak_ptr<AppRunner>
            >{};
            if (server_apps.find(server) == server_apps.cend() or server_apps.at(server).expired()) {
                const auto app = std::make_shared<AppRunner>(
                    shynur::udds_rpc::Application::make_app<Server>({
                        .server = server,
                        .entity  = "server",
                        .server_impl = server_impls.at(server),
                    })
                );
                server_apps[server] = app;
                return app;
            } else
                return server_apps.at(server).lock();
        }

        inline auto call(
            const std::string& server, const std::string& method,
            std::function<void(const std::exception *, std::string)> callback,
            const std::string& json
        ) {
            auto app = ::shynur::udds_rpc::Application::make_app<Server>({
                .server = server,
                .entity  = "client",
            });

            std::dynamic_pointer_cast<::shynur::udds_rpc::ClientApp>(app)->call(
                Client{method, json, callback}
            );
        }
    } // namespace _detail

    template <typename R, typename... Args>
    auto serve(
        const std::string& server, const std::string& method, std::function<R(Args...)> handler
    ) {
        return _detail::serve(
            server, method,
            [handler=std::move(handler)](const std::string& json) -> std::string {
                const auto args = ::nlohmann::json::parse(json)
                                  .get<std::tuple<std::decay_t<Args>...>>();
                if constexpr (std::is_same_v<R, void>) {
                    std::apply(handler, args);
                    Logger{"INFO"} << "serve" << "==> void";
                    return "";
                } else {
                    const auto result = std::apply(handler, args);
                    const auto result_json = ::nlohmann::json(result).dump();
                    Logger{"INFO"} << "serve" << "==>" << (
                        result_json.length() <= 40
                        ? result_json
                        : std::format(
                            "{} /*...*/ {}",
                            result_json.substr(0, 15),
                            result_json.substr(result_json.size() - 15)
                        )
                    );
                    return result_json;
                }
            }
        );
    }

    template </*del*/typename R, typename... Args>
    void call(
        const std::string& server, const std::string& method,
        std::function<void(const std::exception *, /*del*/R)> callback,
        Args... args
    ) {
        const auto callback_ = std::shared_ptr<std::decay_t<decltype(callback)>>{
            new std::decay_t<decltype(callback)>{
                std::move(callback)
            }
        };

        const auto json = sizeof...(args) == 0 ? "[]" : ::nlohmann::json{args...}.dump();
        Logger{"INFO"} << "call" << (
            json.length() <= 40
            ? json
            : std::format(
                "{} /*...*/ {}",
                json.substr(0, 15),
                json.substr(json.size() - 15)
            )
        );

        try {
            _detail::call(
                server, method,
                [callback_](
                    const std::exception *const e, const std::string& json
                ) {
                    (*callback_)(e, /*del*/!e ? ::nlohmann::json::parse(json).get<R>() : R{});
                },
                json
            );
        } catch (const std::exception& e) {
            (*callback_)(&e, /*del*/R{});
        }
    }
    template <typename... Args>
    void call(
        const std::string& server, const std::string& method,
        std::function<void(const std::exception *)> callback,
        Args... args
    ) {
        const auto callback_ = std::shared_ptr<std::decay_t<decltype(callback)>>{
            new std::decay_t<decltype(callback)>{
                std::move(callback)
            }
        };

        const auto json = sizeof...(args) == 0 ? "[]" : ::nlohmann::json{args...}.dump();
        Logger{"INFO"} << "call" << (
            json.length() <= 40
            ? json
            : std::format(
                "{} /*...*/ {}",
                json.substr(0, 15),
                json.substr(json.size() - 15)
            )
        );

        try {
            _detail::call(
                server, method,
                [callback_](
                    const std::exception *const e, const std::string& json
                ) {
                    (*callback_)(e);
                },
                json
            );
        } catch (const std::exception& e) {
            (*callback_)(&e);
        }
    }
}
