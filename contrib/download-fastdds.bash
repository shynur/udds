#! /bin/bash -e
# Usage: (cd /tmp; "$0" 3.2.2)
shopt -s globstar

SCRIPT_DIR=`cd \`dirname $0\`; pwd`

if [ -z $1 ]; then
    echo '需要提供 Fast DDS 的版本号, 建议使用 3.2.2'
    exit 1
fi

if [ -f /etc/apt/sources.list ]; then
    sed -i 's/\(archive\|security\)\.ubuntu\.com/mirrors.cloud.aliyuncs.com/g' /etc/apt/sources.list
fi
apt update
apt install -y wget patch emacs-nox

if ! type cmake || [ 3.31 = `cmake --version | head -n 1 | awk '{print $3"\n3.31"}' | sort -V -r | head -n 1` ]; then
    wget -O /tmp/cmake-installer.sh https://github.com/Kitware/CMake/releases/download/v3.31.9/cmake-3.31.9-linux-$HOSTTYPE.sh
    chmod +x /tmp/cmake-installer.sh
    /tmp/cmake-installer.sh --prefix=/usr --exclude-subdir
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
        tar xzf $WHICH_FASTDDS_I_WANNA_DOWNLOAD
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

for f in ./**/*.h ./**/*.c ./**/*.hpp ./**/*.cpp ./**/*.cxx; do
    sed -i s/asio::io_service/asio::io_context/g $f
done

emacs src/fastdds/src/cpp/fastdds/topic/DDSSQLFilter/DDSFilterValue.hpp -batch -Q -eval '
(progn
  (goto-line 25)
  (end-of-line)
  (newline)
  (insert "#include <cstdint>")
  (save-buffer))'
emacs src/fastdds/src/cpp/fastdds/topic/DDSSQLFilter/DDSFilterCompoundCondition.hpp -batch -Q -eval '
(progn
  (goto-line 22)
  (end-of-line)
  (newline)
  (insert "#include <cstdint>")
  (save-buffer))'
emacs src/fastdds/src/cpp/rtps/reader/BaseReader.cpp -batch -Q -eval '
(progn
  (goto-line 520)
  (beginning-of-line)
  (insert "//")
  (save-buffer))'

if [ -f install.sh.bak ]; then
    rm install.sh
    cp install.sh{.bak,}
else
    cp install.sh{,.bak}
fi
patch <$SCRIPT_DIR/fastdds-install.sh.patch

: ${CC:=cc} ${CXX:=c++}
export CC CXX
bash ./install.sh --build-cores `nproc` --no-security --install-prefix /usr/local
