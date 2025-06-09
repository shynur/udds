#define SHYNUR_UDDS_USED_BY_SEER_RBK
#include "udds-broadcast-client.hpp"

int main() {
    rbk::udds::Broadcast_Client c{__FILE__, 1};
}
