#include "urpc.hpp"

void register_service_d() {
    static auto p = rbk::urpc::serve(
        "S", "d",
        std::function{[](double x) {
            return x * 2;
        }}
    );
}

void call_d(double x) {
    rbk::urpc::call(
        "S", "d",
        std::function{[](const std::exception *err, double r) noexcept {
	    if (err)
                std::println(std::cerr, "==== Error ===> {}", err->what());
            else
                std::println("==== Result ===> {}", r);
        }},
        x
    );
}
