#pragma once
#include <bits/stdc++.h>
using namespace std::literals;
#include "shynur-polyfill.hpp"

namespace shynur::utils {
    template <class Config>
    struct Logger {
        static inline std::atomic_bool enabled = [](const char *const var) -> bool {
            const auto val = std::string{std::getenv(var) ? std::getenv(var) : ""};
            std::println(
                std::clog,
                "export {}={}",
                var, val
            );
            return val.length();
        }(Config::env_switch);
        const std::string_view                                   level;
        const std::chrono::time_point<std::chrono::system_clock> now;

        Logger(const std::string_view level)
        : level{level}, now{std::chrono::system_clock::now()} {}

        ~Logger() {
            if (!std::decay_t<decltype(*this)>::enabled)
                return;

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

            std::println(
                this->level == "ERROR" ? std::cerr : std::clog,
                "{} {}[{}] \033[37;1m{}\033[m{}",
                time_s,
                level_color,
                this->level,
                this->context,
                this->oss.str()
            );
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
            if (!std::decay_t<decltype(*this)>::enabled)
                return *this;

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
