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
            const auto time_s      = std::format("{}", std::chrono::current_zone()->to_local(this->now));
            const auto level_color = this->level == "DEBUG" ? "\e[34;1m" : this->level == "INFO" ? "\e[32;1m"
                                                                                                 : "\e[31;1m";
            const auto msg         = std::format(
                "{}_{} {}[{}] \e[37;1m{}\e[m{}\n",
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

struct [[gnu::weak]] Application {
    virtual ~Application() = default;
    virtual void run()     = 0;
    virtual void stop()    = 0;

    struct Options {
        std::string   entity;  // server|client
        std::size_t   thread_pool_size = 0;  // (可选) server 线程池数量
        std::string   operation;
        std::int32_t  x;
        std::int32_t  y;
    };

    struct ServerImpl: ShynurUrpcProcessorServerImplementation,
                       std::enable_shared_from_this<ServerImpl> {
        auto ptr() -> std::shared_ptr<ServerImpl>{
            return shared_from_this();
        }
      private:
        auto ping_(
            const ShynurUrpcProcessorServer_ClientContext&,
            const std::string&
        ) -> std::string override {
            return "";
        }
    };
    template <std::derived_from<ServerImpl> UserDefinedServerImpl>
    static auto make_app(const Options& options) -> std::shared_ptr<Application>;
};

template <std::derived_from<::Application::ServerImpl> UserDefinedServerImpl>
class ServerApp: public ::Application {
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
    std::shared_ptr<ShynurUrpcProcessorServer> server_;
  public:
    struct Config {
        const std::size_t thread_pool_size = 0;
    } config;
    ServerApp(const std::string_view service_name, const Config& config)
    : config{config}, server_{[&, this] {
          const auto server = create_ShynurUrpcProcessorServer(
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
            << "Server initialized with ID: " << this->participant_->guid().guidPrefix;
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

enum class OperationStatus {SUCCESS, TIMEOUT, ERROR};
struct Operation {
    virtual OperationStatus execute() = 0;
    virtual ~Operation()              = default;
};
struct Ping: ::Operation {
    Ping(const std::shared_ptr<ShynurUrpcProcessor> client): client_{client} {}
    auto execute() -> OperationStatus override {
        if (auto client = this->client_.lock()) {
            auto future = client->ping_("");
            if (future.wait_for(1s) != std::future_status::ready) {
                ::shynur::utils::Logger{"INFO"}
                    << "ClientApp"
                    << "Ping operation timed out";
                return OperationStatus::TIMEOUT;
            }
            try {
                future.get();
                ::shynur::utils::Logger{"INFO"}
                    << "ClientApp"
                    << "Ping operation successful";
                return OperationStatus::SUCCESS;
            } catch (const ::eprosima::fastdds::dds::rpc::RpcBrokenPipeException&) {
                ::shynur::utils::Logger{"INFO"}
                    << "ClientApp"
                    << "Server not reachable";
                return OperationStatus::ERROR;
            } catch (const ::eprosima::fastdds::dds::rpc::RpcException& e) {
                ::shynur::utils::Logger{"ERROR"}
                    << "ClientApp"
                    << "RPC exception occurred during ping: " << e.what();
                return OperationStatus::ERROR;
            }
        }
        throw std::runtime_error{"Client reference expired"};
    }
  protected:
    std::weak_ptr<ShynurUrpcProcessor> client_;
};
struct Addition: ::Operation {
    Addition(
        const std::shared_ptr<ShynurUrpcProcessor> client,
        const std::int32_t x, const std::int32_t y
    ): x_{x}, y_{y}, client_{client} {}

    OperationStatus execute() override {
        if (auto client = this->client_.lock()) {
            auto future = client->addition(this->x_, this->y_);
            try {
                this->result_ = future.get();
                ::shynur::utils::Logger{"INFO"}
                    << "ClientApp"
                    << "Addition result: "
                    << this->x_ << " + " << this->y_
                    << " = " << this->result_;
                return OperationStatus::SUCCESS;
            } catch (const ::eprosima::fastdds::dds::rpc::RpcException& e) {
                ::shynur::utils::Logger{"ERROR"}
                    << "ClientApp"
                    << "RPC exception occurred: " << e.what();
                return OperationStatus::ERROR;
            }
        } else
            throw std::runtime_error{"Client reference expired"};
    }
  protected:
    const std::int32_t             x_, y_;
    std::int32_t                  result_;
    const std::weak_ptr<ShynurUrpcProcessor> client_;
};
struct Substraction: ::Operation {
    Substraction(
        const std::shared_ptr<ShynurUrpcProcessor> client,
        const std::int32_t x, const std::int32_t y
    ): x_{x}, y_{y}, client_{client} {}

    OperationStatus execute() override {
        if (auto client = this->client_.lock()) {
            auto future = client->subtraction(this->x_, this->y_);

            if (future.wait_for(1000ms) != std::future_status::ready) {
                ::shynur::utils::Logger{"ERROR"}
                    << "ClientApp"
                    << "Operation timed out";
                return OperationStatus::TIMEOUT;
            }

            try {
                this->result_ = future.get();
                ::shynur::utils::Logger{"INFO"}
                    << "ClientApp"
                    << "Addition result: "
                    << this->x_ << " - " << this->y_
                    << " = " << this->result_;
                return OperationStatus::SUCCESS;
            } catch (const ::eprosima::fastdds::dds::rpc::RpcException& e) {
                ::shynur::utils::Logger{"ERROR"}
                    << "ClientApp"
                    << "RPC exception occurred: " << e.what();
                return OperationStatus::ERROR;
            }
        }
        throw std::runtime_error{"Client reference expired"};
    }
  protected:
    const std::int32_t             x_, y_;
    std::int32_t                  result_;
    const std::weak_ptr<ShynurUrpcProcessor> client_;
};

struct ClientApp: ::Application {
    struct Config {
        const std::size_t connection_attempts = 10;
        const std::string  operation;
        const std::int32_t x, y;
    } config;

    ClientApp(const std::string_view service_name, const Config& config)
    : config{config}, client_{[&, this] {
          const auto client = create_ShynurUrpcProcessorClient(
              *this->participant_,
              std::string{service_name}.c_str(),
              ::eprosima::fastdds::dds::RequesterQos{}
          );
          if (!client)
              throw std::runtime_error("Failed to create client");
          return client;
      }()} {
        ::shynur::utils::Logger{"INFO"}
            << "ClientApp"
            << "Client initialized with ID: " << this->participant_->guid().guidPrefix;
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
                set_operation();
                if (this->operation_->execute() != OperationStatus::SUCCESS)
                    throw std::runtime_error{"shynur.urpc Operation failed or interrupted"};
            } catch (const std::runtime_error& e) {
                ::shynur::utils::Logger{"ERROR"}
                    << "ClientApp"
                    << e.what() + ". Stopping client execution..."s;
                throw std::runtime_error{"Error occurred during RPC"};
            }
    }
    void stop() override {
        this->stopped_.test_and_set();
        ::shynur::utils::Logger{"INFO"}
            << "ClientApp"
            << "Client execution stopped";
    }
  protected:
    void set_operation() {
        if (this->config.operation == "+")
            this->operation_ = std::unique_ptr<::Operation>{
                new ::Addition{
                    this->client_,
                    this->config.x, this->config.y
                }
            };
        else if (this->config.operation == "-")
            this->operation_ = std::unique_ptr<::Operation>{
                new ::Substraction{
                    this->client_,
                    this->config.x, this->config.y
                }
            };
        else
            throw std::runtime_error{"shynur.urpc Invalid operation"};
    }
    bool ping_server() {
        this->operation_ = std::unique_ptr<::Operation>{new ::Ping{this->client_}};

        for (auto i = 0u; i < this->config.connection_attempts; i++)
            if (!this->stopped_.test()) {
                ::shynur::utils::Logger{"DEBUG"}
                   << "ClientApp"
                   << "Trying to reach server, attempt "
                   << (i + 1) << "/" << this->config.connection_attempts;

                 std::this_thread::sleep_for(1s);

               if (this->operation_->execute() == OperationStatus::SUCCESS)
                     return true;

                 ::shynur::utils::Logger{"DEBUG"}
                     << "ClientApp"
                     << "Server not reachable, attempt "
                     << (i + 1) << "/" << this->config.connection_attempts << " failed.";

                if (i == this->config.connection_attempts - 1)
                     ::shynur::utils::Logger{"ERROR"}
                         << "ClientApp"
                        << "Failed to connect to server";
            }

         return false;
     }

    std::atomic_flag                             stopped_     = ATOMIC_FLAG_INIT;
    ::eprosima::fastdds::dds::DomainParticipant *participant_ = [] {
        const auto factory = ::eprosima::fastdds::dds::DomainParticipantFactory::get_shared_instance();
        if (!factory)
            throw std::runtime_error{"shynur.urpc Failed to get participant factory instance"};

        const auto participant = factory->create_participant_with_default_profile();
        if (!participant)
            throw std::runtime_error{"shynur.urpc Participant initialization failed"};
        return participant;
    }();
    std::shared_ptr<ShynurUrpcProcessor> client_;
    std::unique_ptr<::Operation>    operation_;
};

template <std::derived_from<::Application::ServerImpl> UserDefinedServerImpl>
auto ::Application::make_app(const Options& options) -> std::shared_ptr<::Application> {
    constexpr auto service_name = "ShynurUrpcProcessor_Service"sv;
    ::Application *app;
    if (options.entity == "server"s)
        app = new ::ServerApp<UserDefinedServerImpl>{
            service_name,
            {
                .thread_pool_size = options.thread_pool_size,
            }
        };
    else
        app = new ::ClientApp{
            service_name,
            {
                .operation = options.operation,
                .x = options.x, .y = options.y,
            }
        };
    return std::shared_ptr<::Application>{app};
}
