#ifndef URPC_EXAMPLE_AS_DECL
    #include "urpc.hpp"
#endif

void register_service_i()
#ifdef URPC_EXAMPLE_AS_DECL
;
#else
{
    static auto p = rbk::urpc::serve(
        "S", "i",
        std::function{[](int x) {
            return x * 2;
        }}
    );
}
#endif

void call_i(int x)
#ifdef URPC_EXAMPLE_AS_DECL
;
#else
{
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
#endif
