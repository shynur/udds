#include "urpc.hpp"

void register_service_i() {
    static auto p = rbk::urpc::serve(
        "S", "i",
        std::function{[](int x) {
            return x * 2;
        }}
    );
}

void call_i(int x) {
    rbk::urpc::call(
        "S", "i",
        std::function{[](const std::exception *err, int r) noexcept {
	    if (err)
                std::println(std::cerr, "==== Error ===> {}", err->what());
            else
                std::println("==== Result ===> {}", r);
        }},
        x
    );
}
