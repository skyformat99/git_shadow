#!/bin/sh

zInitEnv() {
    zProjName=$1
    zSvnServPath=/home/git/svn_$zProjName  #Subversion repo to receive code from remote developers
    zSyncPath=/home/git/sync_$zProjName  #Sync snv repo to git repo
    zDeployPath=/home/git/$zProjName #Used to deploy code! --CORE--

    rm -rf /home/git/*
    if [[ 0 -ne $? ]]; then exit 1; fi

    mkdir $zSvnServPath
    mkdir $zSyncPath
    mkdir $zDeployPath

    cp -rf ../../../demo/${zProjName}_shadow ${zDeployPath}/.git_shadow

    #Init Subversion Server
    svnadmin create $zSvnServPath
    perl -p -i.bak -e 's/^#\s*anon-access\s*=.*$/anon-access = write/' $zSvnServPath/conf/svnserve.conf
    svnserve --listen-port=$2 -d -r $zSvnServPath

    #Init svn repo
    svn co svn://10.30.2.126:$2/ $zSyncPath
    svn propset svn:ignore '.git' $zSyncPath

    #Init Sync Git Env
    cd $zSyncPath
    git init .
    git config user.name "sync"
    git config user.email "sync@_"
    printf ".svn" > .gitignore
    git add --all .
    git commit --allow-empty -m "__sync_init__"
    git branch -M master sync_git  # 此git的作用是将svn库代码转换为git库代码

    #Init Deploy Git Env
    cd $zDeployPath
    git init .
    git config user.name "deploy"
    git config user.email "deploy@_"
    printf ".svn" > .gitignore
    git add --all .
    git commit --allow-empty -m "__deploy_init__"
    git branch CURRENT
    git branch server  #Act as Git server

    # 初始化 git_shadow 自身的库，不需要建 CURRENT 与 server 分支
    cd $zDeployPath/.git_shadow
    git init .
    git config user.name "git_shadow"
    git config user.email "git_shadow@_"
    git add --all .
    git commit --allow-empty -m "_"

    cp ${zCurDir}/zsvn_post-commit.sh ${zSvnServPath}/hooks/post-commit
    chmod 0755 ${zSvnServPath}/hooks/post-commit

    cp ${zCurDir}/zsync_git_post-update.sh ${zDeployPath}/.git/hooks/post-update
    chmod 0755 ${zDeployPath}/.git/hooks/post-update

    chown -R git:git /home/git
}

zCurDir=$PWD

# 启动服务器
mkdir -p ../../../bin
mkdir -p ../../../log
rm -rf ../../../bin/*

cc -O2 -Wall -Wextra -std=c99 \
    -I../../../inc \
    -lm \
    -lpthread \
    -lpcre2-8 \
    -D_XOPEN_SOURCE=700 \
    -o ../../../bin/git_shadow \
    ../../../src/zmain.c

strip ../../../bin/git_shadow
if [[ 0 -ne $? ]]; then exit 1; fi

cc -O2 -Wall -Wextra -std=c99 \
    -I../../../inc \
    -D_XOPEN_SOURCE=700 \
    -o ../../../bin/git_shadow_client \
    ../../../src/client/zmain_client.c

strip ../../../bin/git_shadow_client
if [[ 0 -ne $? ]]; then exit 1; fi

killall -9 ssh 2>/dev/null
killall -9 git 2>/dev/null
killall -9 git_shadow 2>/dev/null
../../../bin/git_shadow -f /home/fh/zgit_shadow/conf/sample.conf -h 10.30.2.126 -p 20000 2>../../../log/log 1>&2

sleep 2

# 运行环境
killall svnserve
zInitEnv miaopai 50000

# 摸拟一个 svn 客户端
rm -rf /tmp/miaopai
mkdir /tmp/miaopai
cd /tmp/miaopai
svn co svn://10.30.2.126:50000
cp -r /etc/conf.d ./ 2>/dev/null
chmod -R 0777 /tmp/miaopai
chown -R git:git /tmp/miaopai