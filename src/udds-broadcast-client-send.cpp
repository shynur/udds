#define SHYNUR_UDDS_USED_BY_SEER_RBK
#include "udds-broadcast-client.hpp"

int main(int, const char *const argv[]) {
    rbk::udds::Broadcast_Client c{argv[1], 1};

    auto msg = rbk::udds::UddsJsonStruct{};

    while (std::cin >> msg.json)
        if (msg.json == "q")
            break;
        else
            c.send(msg);
}
