#ifndef URPC_EXAMPLE_AS_DECL
    #include "urpc.hpp"
#endif

void register_service_v()
#ifdef URPC_EXAMPLE_AS_DECL
;
#else
{
    static auto p = rbk::urpc::serve(
        "S", "v",
        std::function{[]() {
            return;
        }}
    );
}
#endif

void call_v()
#ifdef URPC_EXAMPLE_AS_DECL
;
#else
{
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
#endif
