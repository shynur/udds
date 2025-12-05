#pragma once
#include <bits/stdc++.h>
using namespace std::literals;

namespace shynur::polyfill {
    template <typename... Args>
    std::string format(const std::string& fmt, Args&&... args) {
        auto argss = std::vector<std::string>{};
        ((argss.push_back(
            static_cast<const std::ostringstream&>(
                std::ostringstream{} << std::forward<decltype(args)>(args)
            ).str()
         )),
         ...);
        auto it = argss.cbegin();

        auto s = ""s;
        for (auto i = 0u; i != fmt.length(); ) {
            char c = fmt[i++];
            if (c == '{') {
                char d = fmt[i++];
                if (d == '{') {
                    s += '{';
                    continue;
                }
                while (d != '}')
                    d = fmt[i++];
                s += *it++;
                continue;
            }
            if (c == '}') {
                i++;
                s += '}';
                continue;
            }
            s += c;
        }
        return s;
    }

    template <typename... Args>
    void print(std::ostream& os, const std::string& fmt, Args&&... args) {
        os << format(fmt, args...);
    }
    template <typename... Args>
    void print(const std::string& fmt, Args&&... args) {
        print(std::cout, fmt, args...);
    }

    template <typename... Args>
    void println(std::ostream& os, const std::string& fmt, Args&&... args) {
        print(os, fmt + '\n', args...);
    }
    template <typename... Args>
    void println(const std::string& fmt, Args&&... args) {
        println(std::cout, fmt, args...);
    }
    inline void println(std::ostream& os) {
        println(os, "");
    }
    inline void println() {
        println(std::cout);
    }
}

namespace std {
#ifndef __cpp_lib_format
    using ::shynur::polyfill::format;
#endif
#ifndef __cpp_lib_print
    using ::shynur::polyfill::print;
    using ::shynur::polyfill::println;
#endif
}
