#include <bits/stdc++.h>
using namespace std::literals;
#define URPC_EXAMPLE_AS_DECL
#include "i.cpp"
#include "d.cpp"
#include "v.cpp"

// struct I__ {
//     I__() {
//         ::eprosima::fastdds::dds::Log::SetVerbosity(eprosima::fastdds::dds::Log::Info);
//         ::eprosima::fastdds::dds::Log::ReportFilenames(true);
//         ::eprosima::fastdds::dds::Log::ReportFunctions(true);
//     }
// } i__;

int main(const int argc, const char *const argv[]) {
    auto is_server = false;
    auto calls = std::vector<char>{};
    for (auto i = 1; i != argc; i++) {
        if (const auto arg = std::string{argv[i]}; arg == "-s")
            is_server = true;
        else if (arg.rfind("-c", 0) == 0)
	    for (auto c : arg.substr(2))
                calls.push_back(c);
	else {
	    std::cerr << "Unknown option: "s + arg + '\n';
	    std::exit(1);
	}
    }

    if (is_server) {
        register_service_i();
	register_service_d();
	register_service_v();
	static auto exit_flag = std::atomic_bool{false};
	static auto cv = std::condition_variable{};
	signal(SIGINT, [](auto) {
	    exit_flag = true;
	    cv.notify_one();
	});
	cv.wait(
	    [] -> auto& {
	        static auto m = std::mutex{};
		static auto l = std::unique_lock{m};
		return l;
	    }(),
	    [] {return !!exit_flag;}
	);
    } else
        for (auto c : calls)
            switch (c) {
	        case 'i': call_i(1); break;
		case 'd': call_d(2.3); break;
		case 'v': call_v(); break;
            }
}
