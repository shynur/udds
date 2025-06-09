#define SHYNUR_UDDS_USED_BY_SEER_RBK
#include "udds-broadcast-client.hpp"

int main(int, const char *const argv[]) {
    rbk::udds::Broadcast_Client c{argv[1], 1};

    while (true) {
        int i;
        std::cin >> i;
        std::cout << "\n\n------------------------------------------\n\n";

        for (auto [robot_id, message] : c.received_from()) {
            std::cout << "小车 ID: " << robot_id << '\n'
                      << "发送时间: " << message.send_timestamp_ns << '\n'
                      << "接收时间: " << message.received_timestamp_ns << '\n'
                      << "JSON: " << message.json << "\n\n";
        }
    }
}
