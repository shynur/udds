#pragma once
#include <sstream>
#include <string>
#include <vector>
using namespace std::literals;

namespace shynur::polyfill {
    template <typename... Args>
    std::string format(const std::string& f, Args&&... args) {
        auto argss = std::vector<std::string>{};
        ((argss.push_back((std::ostringstream{} << std::forward<decltype(args)>(args)).str())), ...);
        auto it = argss.cbegin();

        auto s = ""s;
        auto i = 0u;
        while (i != f.length()) {
            char c = f[i++];
            if (c == '{') {
                char d = f[i++];
                if (d == '{') {
                    s += '{';
                    continue;
                }
                while (d != '}')
                    d = f[i++];
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
}

namespace std {
#ifndef __cpp_lib_format
    using shynur::polyfill::format;
#endif
}
