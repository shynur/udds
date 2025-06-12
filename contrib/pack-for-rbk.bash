#! /bin/bash

if ! [ $USER = shynur ]; then
    exit 1
fi

(
    cd /tmp
    rm -rf udds
    git clone git@github.com:shynur/udds.git

    cd udds
    rm -rf .git
    git clone https://cnb.cool/seer-robotics/robokit/rbk3/sdk/utils/udds.git
    mv udds/.git .
    rm -r udds

    (
        cd include/udds
        mv udds-broadcast-client.hpp{,.bak}
        echo '#define SHYNUR_UDDS_USED_BY_SEER_RBK 30408UL' > udds-broadcast-client.hpp
        cat udds-broadcast-client.hpp.bak >> udds-broadcast-client.hpp
        rm udds-broadcast-client.hpp.bak
    )
    git add . && git commit -m ';' && git push
)

cd `dirname $0`/../bin
if [ -f udds-broadcast-cli.d/udds-broadcast-cli ]; then
    rm -rf /mnt/shared-thru-vbox/udds-broadcast-cli.d
    cp -r udds-broadcast-cli.d /mnt/shared-thru-vbox/
else
    echo '没有编译 udds-broadcast-cli' 1>&2
    exit 1
fi
