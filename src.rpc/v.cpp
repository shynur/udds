#include "urpc.hpp"

void register_service_v() {
    static auto p = rbk::urpc::serve(
        "S", "v",
        std::function{[]() {
            return;
        }}
    );
}

void call_v() {
    rbk::urpc::call(
        "S", "v",
        std::function{[](const std::exception *err) noexcept {
	    if (err)
                std::println(std::cerr, "==== Error ===> {}", err->what());
            else
                std::println("==== Result ===> {}", "void");
        }}
    );
}
