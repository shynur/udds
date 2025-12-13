#ifndef URPC_EXAMPLE_AS_DECL
    #include "urpc.hpp"
#endif

void register_service_d()
#ifdef URPC_EXAMPLE_AS_DECL
;
#else
{
    static auto p = rbk::urpc::serve(
        "S", "d",
        std::function{[](double x) {
            return x * 2;
        }}
    );
}
#endif

void call_d(double x)
#ifdef URPC_EXAMPLE_AS_DECL
;
#else
{
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
#endif
