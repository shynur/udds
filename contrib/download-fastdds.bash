#! /bin/bash

# Usage: (cd /tmp; "$0" 3.2.2)
set -e

if [ -z $1 ]; then
    echo '需要提供 Fast DDS 的版本号, 建议使用 3.2.2'
    exit 1
fi
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
if [ "$REPLY" != y ]; then
    echo '你可稍后以相同命令再次执行此脚本以继续安装'
    exit 0
fi

cd fast-dds.installer.d
if [ -f install.sh.bak ]; then
    rm install.sh
    cp install.sh{.bak,}
else
    cp install.sh{,.bak}
fi
patch <`dirname $0`/fastdds-install.sh.patch

if [ -z $CXX ]; then
    CXX=c++
fi
if $CXX --version | grep 'Free Software Foundation' >/dev/null; then
    echo "使用了 G++"
    for v in {1..14}; do
        if $CXX --version | grep $CXX | grep ") $v\\." >/dev/null; then
            echo "使用的 G++ 版本是 $v"
            SHYNUR_GCC_VERSION_LE_14=1
            break
        fi
    done
    if ! (($SHYNUR_GCC_VERSION_LE_14)); then
        echo "G++ 版本太高了, '<cstdint>' 不是默认包含的"
        exit 1
    fi
fi
sudo -E ./install.sh --build-cores `nproc` --no-security
