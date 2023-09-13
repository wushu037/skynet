#include "skynet.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct logger {
    FILE *handle;
    char *filename;
    uint32_t starttime;
    int close;
};

struct logger *
logger_create(void) {
    struct logger *inst = skynet_malloc(sizeof(*inst));
    inst->handle = NULL;
    inst->close = 0;
    inst->filename = NULL;

    return inst;
}

void logger_release(struct logger *inst) {
    if (inst->close) {
        fclose(inst->handle);
    }
    skynet_free(inst->filename);
    skynet_free(inst);
}

#define SIZETIMEFMT 250

static int
timestring(struct logger *inst, char tmp[SIZETIMEFMT]) {
    uint64_t now = skynet_now();
    time_t ti = now / 100 + inst->starttime;
    struct tm info;
    (void)localtime_r(&ti, &info);
    strftime(tmp, SIZETIMEFMT, "%d/%m/%y %H:%M:%S", &info);
    return now % 100;
}

// 用来处理队列中的消息的函数 （cb=callback）
static int
logger_cb(struct skynet_context *context, void *ud, int type, int session, uint32_t source, const void *msg, size_t sz) {
    struct logger *inst = ud;
    switch (type) {
        case PTYPE_SYSTEM:
            if (inst->filename) { // 这里表示打开一个新的日志文件
                inst->handle = freopen(inst->filename, "a", inst->handle);
            }
            break;
        case PTYPE_TEXT:
            if (inst->filename) { // 如果指定了日志文件，则加点时间信息
                char tmp[SIZETIMEFMT];
                int csec = timestring(ud, tmp);
                fprintf(inst->handle, "%s.%02d ", tmp, csec);
            }
            fprintf(inst->handle, "[:%08x] ", source); // 默认写日志都以服务的id开头
            fwrite(msg, sz, 1, inst->handle);
            fprintf(inst->handle, "\n");
            fflush(inst->handle);
            break;
    }

    return 0;
}

int logger_init(struct logger *inst, struct skynet_context *ctx, const char *parm) {
    const char *r = skynet_command(ctx, "STARTTIME", NULL);
    inst->starttime = strtoul(r, NULL, 10);
    if (parm) {
        inst->handle = fopen(parm, "a"); // 这里的handle是打开的系统文件描述符 不要和skynet服务的handle搞混了
        if (inst->handle == NULL) {
            return 1;
        }
        inst->filename = skynet_malloc(strlen(parm) + 1);
        strcpy(inst->filename, parm);
        inst->close = 1;       // 表示在释放的时候 需要关闭文件描述符
    } else {                   // 默认情况下是走这里
        inst->handle = stdout; // 没有配置日志写入哪个文件 就写入标准输出
    }
    if (inst->handle) {
        skynet_callback(ctx, inst, logger_cb); // 设置模块实例 和 回调函数
        return 0;
    }
    return 1; // 走到这里表示已经出现错误了
}
