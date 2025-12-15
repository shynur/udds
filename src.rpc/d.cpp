#include "urpc.hpp"
#include "h.hpp"

void register_service_d() {
    static auto p = rbk::urpc::serve(
        server_name, "d",
        std::function{[](double x) {
            return x * 2;
        }}
    );
}

void call_d(double x) {
    rbk::urpc::call(
        server_name, "d",
        std::function{[](const std::exception *err, double r) noexcept {
	    if (err)
                std::println(std::cerr, "==== Error ===> {}", err->what());
            else
                std::println("==== Result ===> {}", r);
        }},
        x
    );
}
