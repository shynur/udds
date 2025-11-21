/*
 * Author: 谢骐 <shynur@outlook.com>
 * URL: https://github.com/shynur/udds/blob/125d701aac1bd4036ebb083e04eff02b05a5034f/src.rpc/urpc.hpp
 */
#pragma once
#include <bits/stdc++.h>
using namespace std::literals;

namespace shynur::utils {
    struct [[gnu::weak]] Logger {
        const std::string_view                                   level;
        const std::chrono::time_point<std::chrono::system_clock> now;

        Logger(const std::string_view level)
        : level{level}, now{std::chrono::system_clock::now()} {}

        ~Logger() {
            const auto time_s = std::format(
                "{}", std::chrono::current_zone()->to_local(this->now)
            );
            const auto level_color = this->level == "DEBUG" ? "\033[34;1m"
                                     : this->level == "INFO" ? "\033[32;1m"
                                     : "\033[31;1m";

            const auto msg = std::format(
                "{}_{} {}[{}] \033[37;1m{}\033[m{}\n",
                time_s.substr(0, time_s.find(' ')),
                time_s.substr(time_s.find(' ') + 1, 12),
                level_color,
                this->level,
                this->context,
                this->oss.str()
            );

            (this->level == "ERROR" ? std::cerr : std::clog) << msg;
        }
        auto& operator<<(const auto& v) {
            if (this->context.empty())
                this->context = (std::ostringstream{} << v).str();
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
        std::size_t   thread_pool_size = 0;  // (可选) server 线程池数量
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
    template <std::derived_from<ServerImpl> UserDefinedServerImpl>
    static auto make_app(const Options& options) -> std::shared_ptr<Application>;
};

template <std::derived_from<Application::ServerImpl> UserDefinedServerImpl>
class ServerApp: public Application {
    std::atomic_flag                                   stopped_     = ATOMIC_FLAG_INIT;
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
  public:
    const struct Config {
        const std::size_t thread_pool_size = 0;
    } config;

    ServerApp(
        const std::string_view service_name, const Config& config
    ): config{config}, server_{[&, this] {
          const auto server = ::create_ShynurUrpcProcessorServer(
              *this->participant_,
              std::string{service_name}.c_str(),
              ::eprosima::fastdds::dds::ReplierQos{},
              this->config.thread_pool_size,
              this->server_impl_
          );
          if (!server)
              throw std::runtime_error{"Server initialization failed"};
          return server;
      }()} {
        ::shynur::utils::Logger{"INFO"}
            << "ServerApp"
            << "Server initialized with ID: "
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
        if (this->stopped_.test())
            return;

        this->server_->run();
        ::shynur::utils::Logger{"INFO"}
            << "ServerApp"
            << "Server running";
    }
    void stop() override {
        this->stopped_.test_and_set();
        this->server_->stop();

        ::shynur::utils::Logger{"INFO"}
            << "ServerApp"
            << "Server execution stopping...";
    }
};

struct [[gnu::weak]] ClientApp: Application {
    struct Operation {
        enum class OperationStatus {SUCCESS, TIMEOUT, ERROR};
        using enum OperationStatus;
        virtual auto execute() -> OperationStatus = 0;
        virtual ~Operation()              = default;
      protected:
        auto call_rpc(
            const auto rpc, auto&& result, auto&&... args
        ) /* final */ {
            if (auto client = this->client_.lock()) {
                auto future = std::mem_fn(rpc)(
                    client, std::forward<decltype(args)>(args)...
                );
                if (future.wait_for(1s) != std::future_status::ready) {
                    ::shynur::utils::Logger{"INFO"}
                        << "ClientApp"
                        << "Timed out";
                    return TIMEOUT;
                }
                try {
                    result = future.get();
                    ::shynur::utils::Logger{"INFO"}
                        << "ClientApp"
                        << "operation successful";
                    return SUCCESS;
                } catch (const ::eprosima::fastdds::dds::rpc::RpcBrokenPipeException&) {
                    ::shynur::utils::Logger{"INFO"}
                        << "ClientApp"
                        << "Server not reachable";
                    return ERROR;
                } catch (const ::eprosima::fastdds::dds::rpc::RpcException& e) {
                    ::shynur::utils::Logger{"ERROR"}
                        << "ClientApp"
                        << "RPC exception occurred: " << e.what();
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
                ::eprosima::fastdds::dds::RequesterQos{}
            );
            if (!client)
                throw std::runtime_error{"Failed to create client"};
            return client;
        }()
    } {
        ::shynur::utils::Logger{"INFO"}
            << "ClientApp"
            << "Client initialized with ID: "
            << this->participant_->guid().guidPrefix;
    }
    ~ClientApp() override {
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
        this->stopped_.test_and_set();
        ::shynur::utils::Logger{"INFO"}
            << "ClientApp"
            << "Client execution stopped";
    }
    auto call(std::derived_from<Operation> auto op) {
        this->set_operation(std::move(op));
        this->run();
    }
  protected:
    void run() override {
        if (this->stopped_.test())
            return;

        if (!this->ping_server()) {
            if (!this->stopped_.test()) {
                 ::shynur::utils::Logger{"INFO"}
                    << "ClientApp"
                    << "Server not reachable. Stopping client execution...";
                throw std::runtime_error{"Server not reachable"};
            }
        }

        if (!this->stopped_.test())
            try {
                if (this->operation_->execute() != Operation::SUCCESS)
                    throw std::runtime_error{
                        "shynur.urpc Operation failed or interrupted"
                    };
            } catch (const std::runtime_error& e) {
                ::shynur::utils::Logger{"ERROR"}
                    << "ClientApp"
                    << e.what() + ". Stopping client execution..."s;
                throw std::runtime_error{"Error occurred during RPC"};
            }
    }
    void set_operation(std::derived_from<Operation> auto op) {
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
                ::shynur::utils::Logger{"INFO"} << "Ping";
                return op_status;
            }
        };

        auto original_operation = std::move(this->operation_);
        this->set_operation<Ping>({});

        auto reachable = false;
        for (auto i = 0u; i < this->config.connection_attempts; i++)
            if (!this->stopped_.test()) {
                ::shynur::utils::Logger{"DEBUG"}
                   << "ClientApp"
                   << "Trying to reach server, attempt "
                   << (i + 1) << "/" << this->config.connection_attempts;

                std::this_thread::sleep_for(1s);

                reachable = this->operation_->execute() == Operation::SUCCESS;
                if (reachable)
                    break;

                ::shynur::utils::Logger{"DEBUG"}
                    << "ClientApp"
                    << "Server not reachable, attempt "
                    << (i + 1) << "/" << this->config.connection_attempts << " failed.";

                if (i == this->config.connection_attempts - 1)
                    ::shynur::utils::Logger{"ERROR"}
                        << "ClientApp"
                        << "Failed to connect to server";
            }

        this->operation_ = std::move(original_operation);
        return reachable;
     }

    std::atomic_flag                             stopped_     = ATOMIC_FLAG_INIT;
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

template <std::derived_from<Application::ServerImpl> UserDefinedServerImpl>
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

namespace seer::urpc {
    namespace _detail {
        struct Server: ::shynur::udds_rpc::Application::ServerImpl {
            inline static std::unordered_map<std::string, std::function<std::string(std::string)>> methods{};

            auto f(const ::ShynurUrpcProcessorServer_ClientContext&,
                const std::string& m, const std::string& x
            ) -> std::string override {
                return this->methods.at(m)(x);
            }
        };
        struct Client: ::shynur::udds_rpc::ClientApp::Operation {
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
                    << "ClientApp"
                    << "Addition result == " << result << "!!!";

                return op_status;
            }
        };
    } // namespace _detail

    auto serve(
        const std::string& service_name, std::function<std::string(std::string)> handler
    ) {
        using namespace _detail;
        Server::methods[service_name] = std::move(handler);

        auto app = ::shynur::udds_rpc::Application::make_app<Server>({
            .service = service_name,
            .entity  = "server",
        });
        struct AppRunner {
            const std::shared_ptr<::shynur::udds_rpc::Application> app;
            const std::thread                                    thread;

            AppRunner(
                const std::shared_ptr<::shynur::udds_rpc::Application> app
            ): app{app}, thread{&::shynur::udds_rpc::Application::run, this->app} {}

            ~AppRunner() {this->app->stop();}
        };
        return std::make_shared<AppRunner>(app);
    }

    auto call(
        const std::string& service_name,
        const std::string& json, std::function<void(const std::exception *, std::string)> callback
    ) {
        using namespace _detail;
        auto app = ::shynur::udds_rpc::Application::make_app<Server>({
            .service = service_name,
            .entity  = "client",
        });

        std::dynamic_pointer_cast<::shynur::udds_rpc::ClientApp>(app)->call(Client{service_name, json, callback});
    }
}
