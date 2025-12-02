/*
 * Author: 谢骐 <shynur@outlook.com>
 * URL: https://github.com/shynur/udds
 */
#pragma once
#include <bits/stdc++.h>
using namespace std::literals;
#include "shynur-polyfill.hpp"

namespace shynur::utils {
    struct [[gnu::weak]] Logger {
        static inline std::atomic_bool enabled = false;
        const std::string_view                                   level;
        const std::chrono::time_point<std::chrono::system_clock> now;

        Logger(const std::string_view level)
        : level{level}, now{std::chrono::system_clock::now()} {}

        ~Logger() {
            const auto time_s = [this] {
                const auto t = std::chrono::system_clock::to_time_t(this->now);
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(this->now.time_since_epoch()) % 1000;
                const auto tm = std::localtime(&t);
                return static_cast<const std::ostringstream&>(
                    std::ostringstream{}
                    << std::put_time(tm, "%Y-%m-%d_%H:%M:%S")
                    << '.' << std::setw(3) << std::setfill('0') << ms.count()
                ).str();
            }();
            const auto level_color = this->level == "DEBUG" ? "\033[34;1m"
                                     : this->level == "INFO" ? "\033[32;1m"
                                     : "\033[31;1m";

            const auto msg = std::format(
                "{} {}[{}] \033[37;1m{}\033[m{}\n",
                time_s,
                level_color,
                this->level,
                this->context,
                this->oss.str()
            );

            if (std::decay_t<decltype(*this)>::enabled)
                (this->level == "ERROR" ? std::cerr : std::clog) << msg;
        }
        #ifndef __cpp_concepts
            template <typename T>
        #endif
        auto& operator<<(const
        #ifdef __cpp_concepts
            auto
        #else
            T
        #endif
            & v
        ) {
            if (this->context.empty())
                this->context = static_cast<const std::ostringstream&>(
                    std::ostringstream{} << v
                ).str();
            else
                this->oss << ' ' << v;
            return *this;
        }
      private:
        std::string        context;
        std::ostringstream oss;
    };
}

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
struct [[gnu::weak]] Application {
    virtual ~Application() = default;
    virtual void run()     = 0;
    virtual void stop()    = 0;

    struct Options {
        std::string   service;  // 服务名
        std::string   entity;  // server|client
        std::size_t   thread_pool_size = 2;  // (可选) server 线程池数量
    };

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
    } config;

    ServerApp(
        const std::string_view service_name, const Config& config
    ): config{config}, server_{[&, this] {
        const auto server = ::create_ShynurUrpcProcessorServer(
            *this->participant_,
            std::string{service_name}.c_str(),
            [] {
                auto qos = ::eprosima::fastdds::dds::ReplierQos{};
                return qos;
            }(),
            this->config.thread_pool_size,
            this->server_impl_
        );
        if (!server)
            throw std::runtime_error{"Server initialization failed"};
        return server;
    }()} {
        ::shynur::utils::Logger{"DEBUG"}
            << "Server Initialized"
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

        ::shynur::utils::Logger{"INFO"}
            << "Server Running"
            << this->participant_->guid().guidPrefix;
        this->server_->run();
    }
    void stop() override {
        this->stopped_ = true;
        this->server_->stop();

        ::shynur::utils::Logger{"INFO"}
            << "Server Stopped"
            << this->participant_->guid().guidPrefix;
    }
  private:
    std::atomic_bool                                   stopped_     = false;
    ::eprosima::fastdds::dds::DomainParticipant *const participant_ = [] {
        const auto factory = ::eprosima::fastdds::dds::DomainParticipantFactory::get_shared_instance();
        if (!factory)
            throw std::runtime_error{"shynur.urpc Failed to get participant factory instance"};

        const auto participant = factory->create_participant_with_default_profile();
        if (!participant)
            throw std::runtime_error{"shynur.urpc Participant initialization failed"};
        return participant;
    }();
    std::shared_ptr<ServerImpl> server_impl_{(ServerImpl *)new UserDefinedServerImpl};
    std::shared_ptr<::ShynurUrpcProcessorServer> server_;
};

struct [[gnu::weak]] ClientApp: Application {
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
                if (future.wait_for(1s) != std::future_status::ready) {
                    ::shynur::utils::Logger{"ERROR"}
                        << "Client RPC"
                        << "Timed Out";
                    return TIMEOUT;
                }
                try {
                    result = future.get();
                    ::shynur::utils::Logger{"INFO"}
                        << "Client RPC"
                        << "Success";
                    return SUCCESS;
                } catch (const ::eprosima::fastdds::dds::rpc::RpcBrokenPipeException&) {
                    ::shynur::utils::Logger{"ERROR"}
                        << "Client RPC"
                        << "Server not reachable";
                    return ERROR;
                } catch (const ::eprosima::fastdds::dds::rpc::RpcException& e) {
                    ::shynur::utils::Logger{"ERROR"}
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
        ::shynur::utils::Logger{"DEBUG"}
            << "Client Initialized"
            << this->participant_->guid().guidPrefix;
    }
    ~ClientApp() override {
        ::shynur::utils::Logger{"DEBUG"}
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
        ::shynur::utils::Logger{"INFO"}
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
                 ::shynur::utils::Logger{"ERROR"}
                    << "Client RPC"
                    << "Server not reachable.  Aborting this rpc...";
                throw std::runtime_error{"Server not reachable"};
            }
        }

        if (!this->stopped_)
            try {
                if (this->operation_->execute() != Operation::SUCCESS)
                    throw std::runtime_error{
                        "shynur.urpc Operation failed or interrupted"
                    };
            } catch (const std::runtime_error& e) {
                ::shynur::utils::Logger{"ERROR"}
                    << "Client RPC"
                    << "Exception:" << e.what();
                throw std::runtime_error{"Error occurred during RPC"};
            }
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
                ::shynur::utils::Logger{"DEBUG"} << "Client" << "Tried ping server";
                return op_status;
            }
        };

        auto original_operation = std::move(this->operation_);
        this->set_operation<Ping>({});

        auto reachable = false;
        for (auto i = 0u; i < this->config.connection_attempts; i++)
            if (!this->stopped_) {
                ::shynur::utils::Logger{"DEBUG"}
                   << "Client"
                   << "Trying to ping server"
                   << '(' << (i + 1) << "/" << this->config.connection_attempts << ')';

                std::this_thread::sleep_for(1s);

                reachable = this->operation_->execute() == Operation::SUCCESS;
                if (reachable)
                    break;

                if (i == this->config.connection_attempts - 1)
                    ::shynur::utils::Logger{"ERROR"}
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
            throw std::runtime_error{
                "shynur.urpc Failed to get participant factory instance"
            };

        const auto participant = factory->create_participant_with_default_profile();
        if (!participant)
            throw std::runtime_error{
                "shynur.urpc Participant initialization failed"
            };
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
    const auto service_name = options.service + "_Service"s;
    Application *app;
    if (options.entity == "server"s)
        app = new ServerApp<UserDefinedServerImpl>{
            service_name,
            {
                .thread_pool_size = options.thread_pool_size,
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

#include "nlohmann/json.hpp"

namespace rbk::urpc {
    namespace _detail {
        struct [[gnu::weak]] Server: ::shynur::udds_rpc::Application::ServerImpl {
            inline static std::unordered_map<std::string, std::function<std::string(std::string)>> methods{};

            auto f(const ::ShynurUrpcProcessorServer_ClientContext&,
                const std::string& m, const std::string& x
            ) -> std::string override {
                return this->methods.at(m)(x);
            }
        };
        struct [[gnu::weak]] Client: ::shynur::udds_rpc::ClientApp::Operation {
            const std::string m;
            const std::string x;
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
                    switch (op_status) {
                        case OperationStatus::TIMEOUT:
                            throw std::runtime_error{"TIMEOUT"};
                        case OperationStatus::ERROR:
                            throw std::runtime_error{"ERROR"};
                    }
                    callback(nullptr, result);
                } catch (const std::exception& e) {
                    callback(&e, "");
                    op_status = OperationStatus::ERROR;
                }

                ::shynur::utils::Logger{"INFO"}
                    << "Client RPC Result"
                    << result;

                return op_status;
            }
        };

        inline auto serve(
            const std::string& service_name, std::function<std::string(std::string)> handler
        ) {
            Server::methods[service_name] = std::move(handler);

            auto app = ::shynur::udds_rpc::Application::make_app<Server>({
                .service = service_name,
                .entity  = "server",
            });
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
            return std::make_shared<AppRunner>(app);
        }

        inline auto call(
            const std::string& service_name,
            std::function<void(const std::exception *, std::string)> callback,
            const std::string& json
        ) {
            auto app = ::shynur::udds_rpc::Application::make_app<Server>({
                .service = service_name,
                .entity  = "client",
            });

            std::dynamic_pointer_cast<::shynur::udds_rpc::ClientApp>(app)->call(Client{service_name, json, callback});
        }
    } // namespace _detail

    template <typename R, typename... Args>
    auto serve(
        const std::string& service_name, std::function<R(Args...)> handler
    ) {
        return _detail::serve(
            service_name,
            [handler=std::move(handler)](const std::string& json) -> std::string {
                const auto args = ::nlohmann::json::parse(json)
                                  .get<std::tuple<std::decay_t<Args>...>>();
                if constexpr (std::is_same_v<R, void>) {
                    std::apply(handler, args);
                    ::shynur::utils::Logger{"INFO"} << "serve" << "==> void";
                    return "";
                } else {
                    const auto result = std::apply(handler, args);
                    ::shynur::utils::Logger{"INFO"} << "serve" << "==>" << ::nlohmann::json(result).dump();
                    return ::nlohmann::json(result).dump();
                }
            }
        );
    }

    template <typename R, typename... Args>
    auto call(
        const std::string& service_name,
        std::function<void(const std::exception *, R)> callback,
        Args... args
    ) {
        const auto json = sizeof...(args) == 0 ? "[]" : ::nlohmann::json{args...}.dump();
        ::shynur::utils::Logger{"INFO"} << "call" << '(' << json << ')';

        return _detail::call(
            service_name,
            [callback=std::move(callback)](
                const std::exception *const e, const std::string& json
            ) {
                callback(e, !e ? ::nlohmann::json::parse(json).get<R>() : R{});
            },
            json
        );
    }
    template <typename... Args>
    auto call(
        const std::string& service_name,
        std::function<void(const std::exception *)> callback,
        Args... args
    ) {
        const auto json = sizeof...(args) == 0 ? "[]" : ::nlohmann::json{args...}.dump();
        ::shynur::utils::Logger{"INFO"} << "call" << '(' << json << ')';

        return _detail::call(
            service_name,
            [callback=std::move(callback)](
                const std::exception *const e, const std::string& json
            ) {
                callback(e);
            },
            json
        );
    }
}
