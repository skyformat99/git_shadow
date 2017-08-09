#!/bin/sh
zServAddr=$1
zServPort=$2
zShadowPath="${HOME}/zgit_shadow"

git stash
git pull
eval sed -i 's%__MASTER_ADDR%${zServAddr}%g' ./zhost_init_repo.sh
eval sed -i 's%__MASTER_PORT%${zServPort}%g' ./zhost_init_repo.sh
eval sed -i 's%__MASTER_ADDR%${zServAddr}%g' ./zhost_init_repo_slave.sh
eval sed -i 's%__MASTER_PORT%${zServPort}%g' ./zhost_init_repo_slave.sh

killall -9 git 2>/dev/null
killall -9 git_shadow 2>/dev/null

mkdir -p ${zShadowPath}/bin
mkdir -p ${zShadowPath}/log
rm -rf ${zShadowPath}/bin/*

# 编译正则库
cd ${zShadowPath}/lib/
rm -rf pcre2 pcre2-10.23
mkdir pcre2
tar -xf pcre2-10.23.tar.gz
cd pcre2-10.23
./configure --prefix=$HOME/zgit_shadow/lib/pcre2
make && make install

# 编译主程序，静态库文件路径一定要放在源文件之后
cc -Wall -Wextra -std=c99 -O2 -lm -lpthread \
    -D_XOPEN_SOURCE=700 \
    -I${zShadowPath}/inc \
    -o ${zShadowPath}/bin/git_shadow \
    ${zShadowPath}/src/zmain.c \
    ${zShadowPath}/lib/pcre2/lib/libpcre2-8.a

strip ${zShadowPath}/bin/git_shadow

# 编译客户端
cc -O2 -Wall -Wextra -std=c99 \
    -I ${zShadowPath}/inc \
    -D_XOPEN_SOURCE=700 \
    -o ${zShadowPath}/bin/git_shadow_client \
    ${zShadowPath}/src/client/zmain_client.c

strip ${zShadowPath}/bin/git_shadow_client

${zShadowPath}/bin/git_shadow -f ${zShadowPath}/conf/master.conf -h $zServAddr -p $zServPort 2>${zShadowPath}/log/log 1>&2
