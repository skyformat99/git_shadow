#include "zRun.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

extern struct zThreadPool__ zThreadPool_;
extern struct zNetUtils__ zNetUtils_;
extern struct zNativeUtils__ zNativeUtils_;
extern struct zNativeOps__ zNativeOps_;
extern struct zDpOps__ zDpOps_;
extern struct zPgSQL__ zPgSQL_;
extern struct zLibGit__ zLibGit_;

static void zstart_server(zNetSrv__ *zpNetSrv_, zPgLogin__ *zpPgLogin_);
static void * zops_route_tcp (void *zp);

static void * zudp_daemon(void *zp __attribute__ ((__unused__)));
static void * zops_route_udp (void *zp);

static _i zhistory_import (cJSON *zpJ __attribute__ ((__unused__)), _i zSd);

/* 不区分项目的全局通用锁 */
static pthread_mutex_t zGlobCommLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t zGlobCommCond = PTHREAD_COND_INITIALIZER;

struct zRun__ zRun_ = {
    .run = zstart_server,

    .route_tcp = zops_route_tcp,
    .route_udp = zops_route_udp,

    .ops_tcp = { NULL },
    .ops_udp = { NULL },

    .p_servPath = NULL,

    .p_commLock = & zGlobCommLock,
    .p_commCond = & zGlobCommCond,
};

void
zerr_vec_init(void) {
    zRun_.p_errVec[1] = "无法识别或未定义的操作请求";
    zRun_.p_errVec[2] = "项目不存在或正在创建过程中";
    zRun_.p_errVec[3] = "指定的版本号不存在";
    zRun_.p_errVec[4] = "指定的文件 ID 不存在";
    zRun_.p_errVec[5] = "";
    zRun_.p_errVec[6] = "项目被锁定，请解锁后重试";
    zRun_.p_errVec[7] = "服务端接收到的数据无法解析";
    zRun_.p_errVec[8] = "已产生新的布署记录，请刷新页面";
    zRun_.p_errVec[9] = "服务端错误：缓冲区容量不足，无法解析网络数据";
    zRun_.p_errVec[10] = "请求的数据类型错误：非提交记录或布署记录";
    zRun_.p_errVec[11] = "系统忙，请两秒后重试...";
    zRun_.p_errVec[12] = "布署失败";
    zRun_.p_errVec[13] = "正在布署过程中，或上一次布署失败，查看最近一次布署动作的实时进度";
    zRun_.p_errVec[14] = "用户指定的布署后命令执行失败";
    zRun_.p_errVec[15] = "服务端布署前动作出错";
    zRun_.p_errVec[16] = "系统当前负载太高，请稍稍后重试";
    zRun_.p_errVec[17] = "IPnum ====> IPstr 失败";
    zRun_.p_errVec[18] = "IPstr ====> IPnum 失败";
    zRun_.p_errVec[19] = "指定的目标机列表中存在重复 IP";
    zRun_.p_errVec[20] = "";
    zRun_.p_errVec[21] = "";
    zRun_.p_errVec[22] = "";
    zRun_.p_errVec[23] = "部分或全部目标机初始化失败";
    zRun_.p_errVec[24] = "前端没有指明目标机总数";
    zRun_.p_errVec[25] = "";
    zRun_.p_errVec[26] = "";
    zRun_.p_errVec[27] = "";
    zRun_.p_errVec[28] = "指定的目标机总数与实际解析出的数量不一致";
    zRun_.p_errVec[29] = "指定的项目路径不合法";
    zRun_.p_errVec[30] = "指定项目路径不是目录，存在非目录文件与之同名";
    zRun_.p_errVec[31] = "SSHUserName 字段太长(>255 char)";
    zRun_.p_errVec[32] = "指定的项目 ID 超限(0 - 1024)";
    zRun_.p_errVec[33] = "服务端无法创建指定的项目路径";
    zRun_.p_errVec[34] = "项目信息格式错误：信息不足或存在不合法字段";
    zRun_.p_errVec[35] = "项目 ID 已存在";
    zRun_.p_errVec[36] = "服务端项目路径已存在";
    zRun_.p_errVec[37] = "未指定远程代码库的版本控制系统类型：git";
    zRun_.p_errVec[38] = "";
    zRun_.p_errVec[39] = "SSHPort 字段太长(>5 char)";
    zRun_.p_errVec[40] = "服务端项目路径操作错误";
    zRun_.p_errVec[41] = "服务端 git 库异常";
    zRun_.p_errVec[42] = "git clone 错误";
    zRun_.p_errVec[43] = "git config 错误";
    zRun_.p_errVec[44] = "git branch 错误";
    zRun_.p_errVec[45] = "git add and commit 错误";
    zRun_.p_errVec[46] = "libgit2 初始化错误";
    zRun_.p_errVec[47] = "";
    zRun_.p_errVec[48] = "";
    zRun_.p_errVec[49] = "指定的源库分支无效/同步失败";
    zRun_.p_errVec[50] = "";
    zRun_.p_errVec[51] = "";
    zRun_.p_errVec[52] = "";
    zRun_.p_errVec[53] = "";
    zRun_.p_errVec[54] = "";
    zRun_.p_errVec[55] = "";
    zRun_.p_errVec[56] = "";
    zRun_.p_errVec[57] = "";
    zRun_.p_errVec[58] = "";
    zRun_.p_errVec[59] = "";
    zRun_.p_errVec[60] = "";
    zRun_.p_errVec[61] = "";
    zRun_.p_errVec[62] = "";
    zRun_.p_errVec[63] = "";
    zRun_.p_errVec[64] = "";
    zRun_.p_errVec[65] = "";
    zRun_.p_errVec[66] = "";
    zRun_.p_errVec[67] = "";
    zRun_.p_errVec[68] = "";
    zRun_.p_errVec[69] = "";
    zRun_.p_errVec[70] = "无内容 或 服务端版本号列表缓存错误";
    zRun_.p_errVec[71] = "无内容 或 服务端差异文件列表缓存错误";
    zRun_.p_errVec[72] = "无内容 或 服务端单个文件的差异内容缓存错误";
    zRun_.p_errVec[73] = "";
    zRun_.p_errVec[74] = "";
    zRun_.p_errVec[75] = "";
    zRun_.p_errVec[76] = "";
    zRun_.p_errVec[77] = "";
    zRun_.p_errVec[78] = "";
    zRun_.p_errVec[79] = "";
    zRun_.p_errVec[80] = "目标机请求下载的文件路径不存在或无权访问";
    zRun_.p_errVec[81] = "同一目标机的同一次布署动作，收到重复的状态确认";
    zRun_.p_errVec[82] = "无法创建 <PATH>_SHADOW/____post-deploy.sh 文件";
    zRun_.p_errVec[83] = "";
    zRun_.p_errVec[84] = "";
    zRun_.p_errVec[85] = "";
    zRun_.p_errVec[86] = "";
    zRun_.p_errVec[87] = "";
    zRun_.p_errVec[88] = "";
    zRun_.p_errVec[89] = "";
    zRun_.p_errVec[90] = "数据库连接失败";
    zRun_.p_errVec[91] = "SQL 命令执行失败";
    zRun_.p_errVec[92] = "SQL 执行结果错误";  /* 发生通常代表存在 BUG */
    zRun_.p_errVec[93] = "";
    zRun_.p_errVec[94] = "";
    zRun_.p_errVec[95] = "";
    zRun_.p_errVec[96] = "";
    zRun_.p_errVec[97] = "";
    zRun_.p_errVec[98] = "";
    zRun_.p_errVec[99] = "";
    zRun_.p_errVec[100] = "";
    zRun_.p_errVec[101] = "目标机返回的版本号与正在布署的不一致";
    zRun_.p_errVec[102] = "目标机 post-update 出错返回";
    zRun_.p_errVec[103] = "";
    zRun_.p_errVec[104] = "";
    zRun_.p_errVec[105] = "";
    zRun_.p_errVec[106] = "";
    zRun_.p_errVec[107] = "";
    zRun_.p_errVec[108] = "";
    zRun_.p_errVec[109] = "";
    zRun_.p_errVec[110] = "";
    zRun_.p_errVec[111] = "";
    zRun_.p_errVec[112] = "";
    zRun_.p_errVec[113] = "";
    zRun_.p_errVec[114] = "";
    zRun_.p_errVec[115] = "";
    zRun_.p_errVec[116] = "";
    zRun_.p_errVec[117] = "";
    zRun_.p_errVec[118] = "";
    zRun_.p_errVec[119] = "";
    zRun_.p_errVec[120] = "";
    zRun_.p_errVec[121] = "";
    zRun_.p_errVec[122] = "";
    zRun_.p_errVec[123] = "";
    zRun_.p_errVec[124] = "";
    zRun_.p_errVec[125] = "";
    zRun_.p_errVec[126] = "服务端操作系统错误";
    zRun_.p_errVec[127] = "被新的布署请求打断";
}

void
zserv_vec_init(void) {
    /*
     * TCP serv vec
     * 索引范围：0 至 zTCP_SERV_HASH_SIZ - 1
     */
    zRun_.ops_tcp[0] = zDpOps_.tcp_pang;  /* 目标机使用此接口测试与服务端的连通性 */
    zRun_.ops_tcp[1] = zDpOps_.creat;  /* 创建新项目 */
    zRun_.ops_tcp[2] = zDpOps_.sys_update;  /* 系统文件升级接口：下一次布署时需要重新初始化所有目标机 */
    zRun_.ops_tcp[3] = zDpOps_.SI_update;  /* 源库URL或分支更改 */
    zRun_.ops_tcp[4] = NULL;  /* 删除项目接口预留 */
    zRun_.ops_tcp[5] = zhistory_import;  /* 临时接口，用于导入旧版系统已产生的数据 */
    zRun_.ops_tcp[6] = NULL;
    zRun_.ops_tcp[7] = zDpOps_.glob_res_confirm;  /* 目标机自身布署成功之后，向服务端核对全局结果，若全局结果是失败，则执行回退 */
    zRun_.ops_tcp[8] = zDpOps_.state_confirm_wraper;  /* 远程主机初始经状态、布署结果状态、错误信息 */
    zRun_.ops_tcp[9] = zDpOps_.print_revs;  /* 显示提交记录或布署记录 */
    zRun_.ops_tcp[10] = zDpOps_.print_diff_files;  /* 显示差异文件路径列表 */
    zRun_.ops_tcp[11] = zDpOps_.print_diff_contents;  /* 显示差异文件内容 */
    zRun_.ops_tcp[12] = zDpOps_.dp;  /* 批量布署或撤销 */
    zRun_.ops_tcp[13] = zDpOps_.req_dp;  /* 目标机主协要求同步或布署到正式环境外(如测试用途)的目标机上 */
    zRun_.ops_tcp[14] = zDpOps_.req_file;  /* 请求服务器发送指定的文件 */
    zRun_.ops_tcp[15] = zDpOps_.show_dp_process;  /* 查询指定项目的详细信息及最近一次的布署进度 */

    /* UDP serv vec */
    zRun_.ops_udp[0] = zDpOps_.udp_pang;
    zRun_.ops_udp[1] = zDpOps_.state_confirm;
    zRun_.ops_udp[2] = NULL;
    zRun_.ops_udp[3] = NULL;
}

/************
 * 网络服务 *
 ************/
static void
zexit_clean(void) {
    /*
     * 进程退出时
     * 清理同一进程组的所有进程
     */
    kill(0, SIGUSR1);
}


/*
 * 服务启动入口
 */
static void
zstart_server() {
    /*
     * 必须指定服务端的根路径
     */
    if (NULL == zRun_.p_servPath) {
        zPRINT_ERR(0, NULL, "==== !!! FATAL !!! ====");
        exit(1);
    }

    /*
     * 检查 pgSQL 运行环境是否是线程安全的
     */
    if (zFalse == zPgSQL_.thread_safe_check()) {
        zPRINT_ERR(0, NULL, "==== !!! FATAL !!! ====");
        exit(1);
    }

    /*
     * 主进程退出时，清理所有项目进程
     */
    atexit(zexit_clean);

    /*
     * 转换为后台守护进程
     */
    zNativeUtils_.daemonize(zRun_.p_servPath);

    /*
     * 只需在主进程执行一次，项目进程会继承之
     * 提取必要的基础信息
     */
    if (NULL == zRun_.p_loginName) {
        zRun_.p_loginName = "git";
    }

    struct passwd *zpPWD = getpwnam(zRun_.p_loginName);
    zCHECK_NULL_EXIT(zRun_.p_homePath = zpPWD->pw_dir);

    zRun_.homePathLen = strlen(zRun_.p_homePath);

    zMEM_ALLOC(zRun_.p_SSHPubKeyPath, char, zRun_.homePathLen + sizeof("/.ssh/id_rsa.pub"));
    sprintf(zRun_.p_SSHPubKeyPath, "%s/.ssh/id_rsa.pub", zRun_.p_homePath);

    zMEM_ALLOC(zRun_.p_SSHPrvKeyPath, char, zRun_.homePathLen + sizeof("/.ssh/id_rsa"));
    sprintf(zRun_.p_SSHPrvKeyPath, "%s/.ssh/id_rsa", zRun_.p_homePath);

    /*
     * 只需在主进程执行一次，项目进程会继承之
     * 初始化错误信息影射表
     */
    zserv_vec_init();
    zerr_vec_init();

    /*
     * 主进程线程池初始化
     */
    zThreadPool_.init(64);

    /*
     * 只运行于主进程
     * 用于收集所有目标机监控数据的 UDP 服务器
     */
    zThreadPool_.add(zudp_daemon, NULL);

    /*
     * 只运行于主进程
     * 定时扩展新的 pgSQL 日志分区表及清理已过期的旧分区表
     */
    zThreadPool_.add(zNativeOps_.extend_pg_partition, NULL);

    /*
     * 只运行于项目进程
     * 服务器内部使用的基于 AF_UNIX 的 UDP 服务器
     */
    zThreadPool_.add(zudp_daemon, ".s.git_shadow");

    /* 扫描所有项目库并初始化之 */
    zNativeOps_.proj_init_all(& (zRun_.pgLogin_));

    /*
     * 返回的 socket 已经做完 bind 和 listen
     * 若出错，其内部会 exit
     */
    _i zMajorSd = zNetUtils_.gen_serv_sd(
            zRun_.netSrv_.p_ipAddr,
            zRun_.netSrv_.p_port,
            NULL,
            zProtoTCP);

    /*
     * 会传向新线程，使用静态变量
     * 使用数组防止负载高时造成线程参数混乱
     */
    static _i zSd[256] = {0};
    _uc zMsgId = 0;
    for (_ui i = 0;; i++) {
        zMsgId = i % 256;
        if (-1 == (zSd[zMsgId] = accept(zMajorSd, NULL, 0))) {
            zPRINT_ERR_EASY_SYS();
        } else {
            zThreadPool_.add(zops_route_tcp, & (zSd[zMsgId]));
        }
    }
}


/*
 * tcp 路由函数，主要用于面向用户的服务
 */
static void *
zops_route_tcp(void *zp) {
    _i zSd = * ((_i *) zp);

    char zDataBuf[zGLOB_COMMON_BUF_SIZ] = {'\0'};
    char *zpDataBuf = zDataBuf;

    _i zErrNo = 0,
       zOpsId = -1,
       zDataLen = -1,
       zDataBufSiz = zGLOB_COMMON_BUF_SIZ;

    /*
     * 若收到的数据量很大，
     * 直接一次性扩展为 1024 倍的缓冲区
     */
    if (zDataBufSiz == (zDataLen = recv(zSd, zpDataBuf, zDataBufSiz, MSG_NOSIGNAL))) {
        zDataBufSiz *= 1024;
        zMEM_ALLOC(zpDataBuf, char, zDataBufSiz);
        strcpy(zpDataBuf, zDataBuf);
        zDataLen += recv(zSd, zpDataBuf + zDataLen, zDataBufSiz - zDataLen, MSG_NOSIGNAL);
    }

    /*
     * 最短的 json 字符串：{"a":}
     * 长度合计 6 字节
     */
    if (zBYTES(6) > zDataLen) {
        zPRINT_ERR(errno, NULL, "recvd data too short(< 6bytes)");
        goto zMarkEnd;
    }

    /* 提取 value[OpsId] */
    cJSON *zpJRoot = cJSON_Parse(zpDataBuf);
    cJSON *zpOpsId = cJSON_GetObjectItemCaseSensitive(zpJRoot, "opsId");
    if (cJSON_IsNumber(zpOpsId)) {
        zOpsId = zpOpsId->valueint;

        /* 检验 value[OpsId] 合法性 */
        if (0 > zOpsId || zTCP_SERV_HASH_SIZ <= zOpsId || NULL == zRun_.ops_tcp[zOpsId]) {
            zErrNo = -1;
        } else {
            zErrNo = zRun_.ops_tcp[zOpsId](zpJRoot, zSd);
        }
    } else {
        zErrNo = -1;
    }
    cJSON_Delete(zpJRoot);

    /*
     * 成功状态及特殊的错误信息在执行函数中直接回复
     * 通用的错误状态返回至此处统一处理
     */
    if (0 > zErrNo) {
        /* 无法解析的数据，打印出其原始信息 */
        if (-1 == zErrNo) {
            // fprintf(stderr, "\342\224\224\342\224\200\342\224\200\033[31;01m[OrigMsg]:\033[00m %s\n", zpDataBuf);
            fprintf(stderr, "\n\033[31;01m[OrigMsg]:\033[00m %s\n", zpDataBuf);
        }

        if (14 != zOpsId) {
            zDataLen = snprintf(zpDataBuf, zDataBufSiz, "{\"errNo\":%d,\"content\":\"[opsId: %d] %s\"}", zErrNo, zOpsId, zRun_.p_errVec[-1 * zErrNo]);
            zNetUtils_.send(zSd, zpDataBuf, zDataLen);
        }
    }

zMarkEnd:
    close(zSd);
    if (zpDataBuf != &(zDataBuf[0])) {
        free(zpDataBuf);
    }

    return NULL;
}


/*
 * 返回的 udp socket 已经做完 bind，若出错，其内部会 exit
 * 收到的内容会传向新线程，使用静态变量数组防止负载高时造成线程参数混乱
 * 单个消息长度不能超过 510
 */
static void *
zudp_daemon(void *zpUNPath) {
    _i zServSd = -1;

    if (NULL == zpUNPath) {
        zServSd = zNetUtils_.gen_serv_sd(
                zRun_.netSrv_.p_ipAddr,
                zRun_.netSrv_.p_port,
                NULL,
                zProtoUDP);
        zRun_.zUdpServSd[1] = zServSd;
    } else {
        zServSd = zNetUtils_.gen_serv_sd(
                NULL,
                NULL,
                zpUNPath,
                zProtoUDP);
        zRun_.zUdpServSd[0] = zServSd;
    }

    static zUdpInfo__ zUdpInfo_[256];
    _uc zMsgId = 0;
    for (_ui i = 0;; i++) {
        zMsgId = i % 256;
        recvfrom(zServSd, zUdpInfo_[zMsgId].data, zBYTES(510), MSG_NOSIGNAL,
                & zUdpInfo_[zMsgId].peerAddr,
                & zUdpInfo_[zMsgId].peerAddrLen);
        zThreadPool_.add(zops_route_udp, & zUdpInfo_[zMsgId]);
    }
}


/*
 * udp 路由函数，主要用于服务器内部
 */
static void *
zops_route_udp (void *zp) {
    zUdpInfo__ zUdpInfo_;

    /* 必须第一时间复制出来 */
    memcpy(&zUdpInfo_, zp, sizeof(zUdpInfo__));

    if (0 == zRun_.ops_udp[zUdpInfo_.opsId](
                & zUdpInfo_.data,
                & zUdpInfo_.peerAddr,
                zUdpInfo_.peerAddrLen)) {
        return NULL;
    } else {
        return (void *) -1;
    }
}


/**************************************************************
 * 临时接口，用于导入旧版布署系统的项目信息及已产生的布署日志 *
 **************************************************************/
extern struct zPosixReg__ zPosixReg_;
static _i
zhistory_import (cJSON *zpJ __attribute__ ((__unused__)), _i zSd) {
    char *zpConfPath="/home/git/zgit_shadow2/conf/master.conf";
    char zLogPathBuf[4096];

    char zDataBuf[4096];
    char zSQLBuf[1024];

    FILE *zpH0 = fopen(zpConfPath, "r");
    FILE *zpH1 = NULL;

    zPgResTuple__ zRepoMeta_ = {
        .p_taskCnt = NULL
    };

    zRegRes__ zR_ = {
        .alloc_fn = NULL
    };

    while (NULL != zNativeUtils_.read_line(zDataBuf, 4096, zpH0)) {
        zPosixReg_.str_split(&zR_, zDataBuf, " ");

        zRepoMeta_.pp_fields = zR_.pp_rets;
        zNativeOps_.proj_init(&zRepoMeta_, -1);

        sprintf(zLogPathBuf,
                "/home/git/home/git/.____DpSystem/%s_SHADOW/log/deploy/meta",
                zR_.pp_rets[1] + sizeof("/home/git/") -1);

        zpH1 = fopen(zLogPathBuf, "r");
        while (NULL != zNativeUtils_.read_line(zDataBuf, 4096, zpH1)) {
            zDataBuf[40] = '\0';
            sprintf(zSQLBuf,
                    "INSERT INTO dp_log (proj_id,time_stamp,rev_sig,host_ip) "
                    "VALUES (%ld,%s,'%s','%s')",
                    strtol(zR_.pp_rets[0], NULL, 10), zDataBuf + 41,
                    zDataBuf,
                    "::1");
            zPgSQL_.exec_once(zRun_.pgConnInfo, zSQLBuf, NULL);
        }

        zPosixReg_.free_res(&zR_);
    }

    zNetUtils_.send(zSd, "==== Import Success ====" ,sizeof("==== Import Success ====") - 1);
    return 0;
}
