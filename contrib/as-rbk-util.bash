#! /bin/bash

# Usage: [假设 udds 克隆在 RBK 的 utils 目录下] $0

set -e

cd `dirname $0`/..

make clean
make

(
    cd include/udds
    for f in *.hpp; do
        echo '#define SHYNUR_UDDS_USED_BY_SEER_RBK 30408UL' > ../../$f
        cat $f >> ../../$f
    done
)

for f in protos/*; do
    mv $f .
done

cat <<'EOF' > CMakeLists.txt
set(PROJECT_NAME utils_udds)
include(SetOutputDirectory)
EOF
