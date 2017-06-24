#define _XOPEN_SOURCE 700
#define _BSD_SOURCE

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/inotify.h>

#ifndef zPTHREAD_H
#define zPTHREAD_H
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#include <libgen.h>

#include "zpcre2.h"

#define zCommonBufSiz 4096 

#define zHashSiz 8192
#define zThreadPollSiz 64

#define zBaseWatchBit \
	IN_MODIFY | IN_CREATE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF | IN_MOVE_SELF

#define zNewDirMark "2"
#define zNewFileMark "1"
#define zModMark "0"
#define zDelFileMark "-1"
#define zDelDirMark "-2"
#define zDelFileSelfMark "-3"
#define zDelDirSelfMark "-4"
#define zUnknownMark "-5"

/**********************
 * DATA STRUCT DEFINE *
 **********************/
typedef struct zObjInfo {
	struct zObjInfo *p_next;
	_i RecursiveMark;  // Mark recursive monitor.
	char path[];  // The directory to be monitored.
}zObjInfo;

typedef struct zSubObjInfo {
	_i UpperWid;
	_i RecursiveMark;
	char *ActionMark;
	char path[];
}zSubObjInfo;

/********************
 * FUNCTION DECLARE *
 ********************/
extern void zdaemonize(const char *);
extern void zfork_do_exec(const char *, char **);
extern char * zget_one_line_from_FILE(FILE *);

static void * zthread_func(void *);

static void zthread_poll_init(void);
//static void zthread_poll_destroy(void);

/**************
 * GLOBAL VAR *
 **************/
static _i zInotifyFD;

static pthread_mutex_t zGitLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t zGitCond = PTHREAD_COND_INITIALIZER;

static zSubObjInfo *zpPathHash[zHashSiz];
static char zBitHash[zHashSiz];

static char *zpShellCommand;  // What to do when get events, two extra VAR available: $zEventType and $zEventPath
static FILE *zpShellRetHandler;

/***************
 * THREAD POOL *
 ***************/
#define zAdd_To_Thread_Pool(zFunc, zParam) do {\
		pthread_mutex_lock(&(zLock[0]));\
		while (-1 == zJobQueue) {\
			pthread_cond_wait(&(zCond[0]), &(zLock[0]));\
		}\
\
		zThreadPoll[zJobQueue].OpsFunc = zFunc;\
		zThreadPoll[zJobQueue].p_param = zParam;\
		zThreadPoll[zJobQueue].MarkStart = 1;\
\
		pthread_mutex_lock(&(zLock[1]));\
		zJobQueue = -1;\
		pthread_cond_signal(&(zCond[1]));\
		pthread_mutex_unlock(&(zLock[1]));\
\
		pthread_mutex_unlock(&(zLock[0]));\
\
		pthread_mutex_lock(&(zLock[2]));\
		pthread_mutex_unlock(&(zLock[2]));\
		pthread_cond_signal(&(zCond[2]));\
}while(0)

typedef void * (* zThreadOps) (void *);

typedef struct zThreadJobInfo {
	pthread_t Tid;
	_i MarkStart;
	zThreadOps OpsFunc;
	void *p_param;
}zThreadJobInfo;

static zThreadJobInfo zThreadPoll[zThreadPollSiz];
static _i zIndex[zThreadPollSiz];
static _i zJobQueue = -1;

static pthread_mutex_t zLock[3] = {PTHREAD_MUTEX_INITIALIZER};
static pthread_cond_t zCond[3] = {PTHREAD_COND_INITIALIZER};

static void
zthread_poll_init(void) {
//TEST: PASS
	for (_i i = 0; i < zThreadPollSiz; i++) {
		zIndex[i] = i;
		zThreadPoll[i].MarkStart= 0;
		zCheck_Pthread_Func_Exit(
				pthread_create(&(zThreadPoll[i].Tid), NULL, zthread_func, &(zIndex[i]))
				);
	}
}

//static void
//zthread_poll_destroy(void) {
//// DO NOT USE!!!
//// Will kill new created threads!
//
//	for (_i i = 0; i < zThreadPollSiz; i++) {
//		pthread_cancel(zThreadPoll[i].Tid);
//	}
//}

static void *
zthread_func(void *zpIndex) {
//TEST: PASS
	zCheck_Pthread_Func_Warning(
			pthread_detach(pthread_self())
			);
	_i i = *((_i *)zpIndex);

zMark:;
	pthread_mutex_lock(&(zLock[1]));
	while (-1 != zJobQueue) {  // -1: no other thread is ahead of me.
		pthread_cond_wait(&zCond[1], &(zLock[1]));
	}

	pthread_mutex_lock(&(zLock[0]));
	zJobQueue = i;
	pthread_cond_signal(&zCond[0]);
	pthread_mutex_unlock(&(zLock[0]));

	pthread_mutex_unlock(&(zLock[1]));

	// 0: param is not ready, can not start; 1: param ready, can start.
	pthread_mutex_lock(&(zLock[2]));
	while (1 != zThreadPoll[i].MarkStart) {
		pthread_cond_wait(&zCond[2], &(zLock[2]));
	}

	zThreadPoll[i].MarkStart = 0;
	pthread_mutex_unlock(&(zLock[2]));

	zThreadPoll[i].OpsFunc(zThreadPoll[i].p_param);
	
	goto zMark;
	return NULL;
}


/*************
 * ADD WATCH *
 *************/
static void *
zinotify_add_sub_watch(void *zpIf) {
//TEST: PASS
	zSubObjInfo *zpCurIf = (zSubObjInfo *) zpIf;

	if (-1 == chdir(zpCurIf->path)) { return NULL; }  // Robustness
	_i zWid = inotify_add_watch(zInotifyFD, zpCurIf->path, zBaseWatchBit | IN_DONT_FOLLOW);
	zCheck_Negative_Exit(zWid);

	if (NULL != zpPathHash[zWid]) { free(zpPathHash[zWid]); }  // Free old memory before using the same index again.
	zpPathHash[zWid] = zpCurIf;

	if (-1 == chdir(zpCurIf->path)) { return NULL; }  // Robustness
	DIR *zpDir = opendir(zpCurIf->path);
	zCheck_Null_Exit(zpDir);

	size_t zLen = strlen(zpCurIf->path);
	struct dirent *zpEntry;

	while (NULL != (zpEntry = readdir(zpDir))) {
		if (DT_DIR == zpEntry->d_type
				&& strcmp(".", zpEntry->d_name) 
				&& strcmp("..", zpEntry->d_name) 
				&& strcmp(".git", zpEntry->d_name)) {
			// Must do "malloc" here.
			zSubObjInfo *zpSubIf = malloc(sizeof(zSubObjInfo) 
					+ 2 + zLen + strlen(zpEntry->d_name));
			zCheck_Null_Exit(zpSubIf);

			zpSubIf->RecursiveMark = 1;
			zpSubIf->UpperWid = zpCurIf->UpperWid;

			strcpy(zpSubIf->path, zpCurIf->path);
			strcat(zpSubIf->path, "/");
			strcat(zpSubIf->path, zpEntry->d_name);

			zAdd_To_Thread_Pool(zinotify_add_sub_watch, zpSubIf);
		}
	}

//	free(zpCurIf);  // Can safely free, but NOT free it!
	closedir(zpDir);
	return NULL;
}

static void *
zinotify_add_top_watch(void *zpIf) {
//TEST: PASS
	zObjInfo *zpObjIf = (zObjInfo *) zpIf;
	char *zpPath = zpObjIf->path;

	zSubObjInfo *zpTopIf = malloc(
			sizeof(zSubObjInfo) + 1 + strlen(zpPath)
			);

	zpTopIf->RecursiveMark = zpObjIf->RecursiveMark;
	zpTopIf->UpperWid = inotify_add_watch(zInotifyFD, zpPath, zBaseWatchBit | IN_DONT_FOLLOW);
	zCheck_Negative_Exit(zpTopIf->UpperWid);

	strcpy(zpTopIf->path, zpPath);
	zpPathHash[zpTopIf->UpperWid] = zpTopIf;

	if (((zObjInfo *) zpObjIf)->RecursiveMark) {
		size_t zLen = strlen(zpPath);
		struct dirent *zpEntry;

		DIR *zpDir = opendir(zpPath);
		zCheck_Null_Exit(zpDir);

		while (NULL != (zpEntry = readdir(zpDir))) {
			if (DT_DIR == zpEntry->d_type
					&& strcmp(".", zpEntry->d_name) 
					&& strcmp("..", zpEntry->d_name) 
					&& strcmp(".git", zpEntry->d_name)) {

				// Must do "malloc" here.
				zSubObjInfo *zpSubIf = malloc(sizeof(zSubObjInfo) 
						+ 2 + zLen + strlen(zpEntry->d_name));
				zCheck_Null_Exit(zpSubIf);
	
				zpSubIf->RecursiveMark = 1;
				zpSubIf->UpperWid = zpTopIf->UpperWid;
	
				strcpy(zpSubIf->path, zpPath);
				strcat(zpSubIf->path, "/");
				strcat(zpSubIf->path, zpEntry->d_name);

				zAdd_To_Thread_Pool(zinotify_add_sub_watch, zpSubIf);
			}
		}
		closedir(zpDir);
	}
	return NULL;
}


/********************
 * DEAL WITH EVENTS *
 ********************/
static void *
zcallback_common_action(void *zpCurIf) {
//TEST: PASS
	zSubObjInfo *zpSubIf = (zSubObjInfo *)zpCurIf;

	pthread_mutex_lock(&zGitLock);
	while (0 != zBitHash[zpSubIf->UpperWid]) {
		pthread_cond_wait(&zGitCond, &zGitLock);
	}
	zBitHash[zpSubIf->UpperWid] = 1;
	pthread_mutex_unlock(&zGitLock);

	char zShellBuf[1 + strlen("zEventPath=;zEventType=;") + strlen(zpSubIf->path) + strlen(zpShellCommand)];
	strcpy(zShellBuf, "zEventPath=");
	strcat(zShellBuf, zpSubIf->path);
	strcat(zShellBuf, ";");
	strcat(zShellBuf, "zEventMark=");  //
	strcat(zShellBuf, zpSubIf->ActionMark);
	strcat(zShellBuf, ";");
	strcat(zShellBuf, zpShellCommand);

	system(zShellBuf);

	pthread_mutex_lock(&zGitLock);
	zBitHash[zpSubIf->UpperWid] = 0;
	pthread_cond_signal(&zGitCond);
	pthread_mutex_unlock(&zGitLock);

	return NULL;
}

static void *
zinotify_wait(void *_) {
//TEST: PASS
	char zBuf[zCommonBufSiz]
		__attribute__ ((aligned(__alignof__(struct inotify_event))));
	ssize_t zLen;

	const struct inotify_event *zpEv;
	char *zpOffset;

	for (;;) {
		zLen = read(zInotifyFD, zBuf, sizeof(zBuf));
		zCheck_Negative_Exit(zLen);

		for (zpOffset = zBuf; zpOffset < zBuf + zLen; zpOffset += sizeof(struct inotify_event) + zpEv->len) {
			zpEv = (const struct inotify_event *)zpOffset;

			if (1 == zpPathHash[zpEv->wd]->RecursiveMark 
					&& (zpEv->mask & IN_ISDIR) 
					&& ( (zpEv->mask & IN_CREATE) || (zpEv->mask & IN_MOVED_TO) )
					) {
		  		// If a new subdir is created or moved in, add it to the watch list.
				// Must do "malloc" here.
				zSubObjInfo *zpSubIf = malloc(sizeof(zSubObjInfo) 
						+ 2 + strlen(zpPathHash[zpEv->wd]->path) + zpEv->len);
				zCheck_Null_Exit(zpSubIf);
	
				zpSubIf->RecursiveMark = 1;
				zpSubIf->UpperWid = zpPathHash[zpEv->wd]->UpperWid;
	
				strcpy(zpSubIf->path, zpPathHash[zpEv->wd]->path);
				strcat(zpSubIf->path, "/");
				strcat(zpSubIf->path, zpEv->name);

				zAdd_To_Thread_Pool(zinotify_add_sub_watch, zpSubIf);
			}

			if (zpEv->mask & (IN_CREATE | IN_MOVED_TO)) { 
				if (zpEv->mask & IN_ISDIR) { zpPathHash[zpEv->wd]->ActionMark = zNewDirMark; }
				else { zpPathHash[zpEv->wd]->ActionMark = zNewFileMark; }
			}
			else if (zpEv->mask & (IN_DELETE | IN_MOVED_FROM)) { 
				if (zpEv->mask & IN_ISDIR) { zpPathHash[zpEv->wd]->ActionMark = zDelDirMark; }
				else { zpPathHash[zpEv->wd]->ActionMark = zDelFileMark; }
			}
			else if (zpEv->mask & (IN_MOVE_SELF | IN_DELETE_SELF)) { 
				if (zpEv->mask & IN_ISDIR) { zpPathHash[zpEv->wd]->ActionMark = zDelDirSelfMark; }
				else { zpPathHash[zpEv->wd]->ActionMark = zDelFileSelfMark; }
			}
			else if (zpEv->mask & IN_MODIFY) {
				zpPathHash[zpEv->wd]->ActionMark = zModMark;
			}
			else if (zpEv->mask & IN_Q_OVERFLOW) {  // Robustness
				zpPathHash[zpEv->wd]->ActionMark = zUnknownMark;
				zPrint_Err(0, NULL, "\033[31;01mQueue overflow, some events may be lost!!\033[00m\n");
				goto zMark;
			}
			else if (zpEv->mask & IN_IGNORED){ goto zMark; }
			else {
				zpPathHash[zpEv->wd]->ActionMark = zUnknownMark;
				zPrint_Err(0, NULL, "\033[31;01mUnknown event occur!!\033[00m\n");
				goto zMark;
			}
			zAdd_To_Thread_Pool(zcallback_common_action, zpPathHash[zpEv->wd]);
zMark:;
		}
	}
}


/***************
 * CONFIG FILE *
 ***************/
static zObjInfo *
zread_conf_file(const char *zpConfPath) {
//TEST: PASS
	zObjInfo *zpObjIf[3] = {NULL};

	zPCREInitInfo *zpInitIf[4] = {NULL};
	zPCRERetInfo *zpRetIf[4] = {NULL};

	_i zCnt = 0;
	char *zpRes = NULL;
	FILE *zpFile = fopen(zpConfPath, "r");

	struct stat zStatBuf;

	zpInitIf[0] = zpcre_init("^\\s*\\d\\s*/[/\\w]+");
	zpInitIf[1] = zpcre_init("^\\d(?=\\s+)");
	zpInitIf[2] = zpcre_init("[/\\w]+(?=\\s*$)");
	zpInitIf[3] = zpcre_init("^\\s*($|#)");

	for (_i i = 1; NULL != (zpRes = zget_one_line_from_FILE(zpFile)); i++) {
		zpRes[strlen(zpRes) - 1] = '\0';

		zpRetIf[3] = zpcre_match(zpInitIf[3], zpRes, 0);
		if (0 < zpRetIf[3]->cnt) {
			zpcre_free_tmpsource(zpRetIf[3]);
			continue;
		}
		zpcre_free_tmpsource(zpRetIf[3]);

		zpRetIf[0] = zpcre_match(zpInitIf[0], zpRes, 0);
		if (0 == zpRetIf[0]->cnt) {
			zpcre_free_tmpsource(zpRetIf[0]);
			zPrint_Time();
			fprintf(stderr, "\033[31m[Line %d] \"%s\": Invalid entry.\033[00m\n", i ,zpRes);
			continue;
		} else {
			zpRetIf[1] = zpcre_match(zpInitIf[1], zpRetIf[0]->p_rets[0], 0);
			zpRetIf[2] = zpcre_match(zpInitIf[2], zpRetIf[0]->p_rets[0], 0);
			if (-1 == lstat(zpRetIf[2]->p_rets[0], &zStatBuf) 
					|| !S_ISDIR(zStatBuf.st_mode)) {
				zpcre_free_tmpsource(zpRetIf[2]);
				zpcre_free_tmpsource(zpRetIf[1]);
				zpcre_free_tmpsource(zpRetIf[0]);
				zPrint_Time();
				fprintf(stderr, "\033[31m[Line %d] \"%s\": NO such directory or NOT a directory.\033[00m\n", i, zpRes);
				continue;
			}

			zpObjIf[0] = malloc(sizeof(zObjInfo) + 1 + strlen(zpRetIf[2]->p_rets[0]));
			if (0 == zCnt) {
				zCnt++;
				zpObjIf[2] = zpObjIf[1] = zpObjIf[0];
			}
			zpObjIf[1]->p_next = zpObjIf[0];
			zpObjIf[1] = zpObjIf[0];
			zpObjIf[0]->p_next = NULL;  // Must here!!!

			zpObjIf[0]->RecursiveMark = atoi(zpRetIf[1]->p_rets[0]);
			strcpy(zpObjIf[0]->path, zpRetIf[2]->p_rets[0]);

			zpcre_free_tmpsource(zpRetIf[2]);
			zpcre_free_tmpsource(zpRetIf[1]);
			zpcre_free_tmpsource(zpRetIf[0]);

			zpObjIf[0] = zpObjIf[0]->p_next;
		}
	}

	zpcre_free_metasource(zpInitIf[3]);
	zpcre_free_metasource(zpInitIf[2]);
	zpcre_free_metasource(zpInitIf[1]);
	zpcre_free_metasource(zpInitIf[0]);

	fclose(zpFile);
	return zpObjIf[2];
}

static void
zconfig_file_monitor(const char *zpConfPath) {
//TEST: PASS
	_i zConfFD = inotify_init();
	zCheck_Negative_Exit(
			inotify_add_watch(
				zConfFD,
				zpConfPath, 
				IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF
				)
			); 

	char zBuf[zCommonBufSiz] 
		__attribute__ ((aligned(__alignof__(struct inotify_event))));
	ssize_t zLen;

	const struct inotify_event *zpEv;
	char *zpOffset;

	for (;;) {
		zLen = read(zConfFD, zBuf, sizeof(zBuf));
		zCheck_Negative_Exit(zLen);

		for (zpOffset = zBuf; zpOffset < zBuf + zLen; 
				zpOffset += sizeof(struct inotify_event) + zpEv->len) {
			zpEv = (const struct inotify_event *)zpOffset;

			if (zpEv->mask & (IN_MODIFY | IN_MOVE_SELF | IN_DELETE_SELF | IN_IGNORED)) { return; }
		}
	}
}


/********
 * MAIN *
 ********/
_i
main(_i zArgc, char **zppArgv) {
//TEST: PASS
//	extern char *optarg;
//	extern int optind, opterr, optopt;
	struct stat zStat;

	opterr = 0;  // prevent getopt to print err info
	for (_i zOpt = 0; -1 != (zOpt = getopt(zArgc, zppArgv, "f:x:"));) {
		switch (zOpt) {
		case 'f':
			if (-1 == stat(optarg, &zStat) || !S_ISREG(zStat.st_mode)) {
				zPrint_Time();
				fprintf(stderr, "\033[31;01mConfig file not exists or is not a regular file!\n"
						"Usage: %s -f <Config File Path>\033[00m\n", zppArgv[0]);
				exit(1);
			}
			break;
		case 'x':
			zpShellCommand = optarg;
			break;
		default: // zOpt == '?'
			zPrint_Time();
		 	fprintf(stderr, "\033[31;01mInvalid option: %c\nUsage: %s -f <Config File Absolute Path>\033[00m\n", optopt, zppArgv[0]);
			exit(1);
		}
	}
//	for (; optind < zArgc; optind++) { printf("Argument = %s\n", zppArgv[optind]); }

	zdaemonize("/");

zReLoad:;
	zInotifyFD = inotify_init();
	zCheck_Negative_Exit(zInotifyFD);

	zthread_poll_init();

	zObjInfo *zpObjIf = NULL;
	if (NULL == (zpObjIf = zread_conf_file(zppArgv[2]))) {
		zPrint_Time();
		fprintf(stderr, "\033[31;01mNo valid entry found in config file!!!\n\033[00m\n");
	}
	else {
		do {
			zAdd_To_Thread_Pool(zinotify_add_top_watch, zpObjIf);
			zpObjIf = zpObjIf->p_next;
		} while (NULL != zpObjIf);
	}

	zAdd_To_Thread_Pool(zinotify_wait, NULL);
	zconfig_file_monitor(zppArgv[2]);  // Robustness

	close(zInotifyFD);
	pid_t zPid = fork();
	zCheck_Negative_Exit(zPid);
	if (0 == zPid) { goto zReLoad; }
	else { exit(0); }
}
