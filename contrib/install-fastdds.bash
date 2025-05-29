#! /bin/bash

# Usage: (cd /tmp; "$0" 3.2.2)

WHICH_FASTDDS_I_WANNA_DOWNLOAD=eProsima_Fast-DDS-v$1-Linux.tgz

if [ -f fast-dds.installer.d/install.sh ]; then
    echo '已经下载过了'
else
    rm -rf fast-dds.installer.d
    mkdir fast-dds.installer.d
    echo 'Installer 将会下载到 ./fast-dds.installer.d/'
    (
        cd fast-dds.installer.d
        wget https://www.eprosima.com/component/ars/item/$WHICH_FASTDDS_I_WANNA_DOWNLOAD
        tar -xzf *
        rm $WHICH_FASTDDS_I_WANNA_DOWNLOAD
    )
fi

echo
echo -n '是否要立即执行 installer?  (y/n) '
read
if [ "$REPLY" = y ]; then
    sudo ./fast-dds.installer.d/install.sh
fi
