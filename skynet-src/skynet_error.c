#include "skynet.h"
#include "skynet_handle.h"
#include "skynet_mq.h"
#include "skynet_server.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_MESSAGE_SIZE 256

// context主要用来设置 skynet_message 的source属性
void skynet_error(struct skynet_context *context, const char *msg, ...) {
    static uint32_t logger = 0;
    if (logger == 0) {
        // 找到默认的日志服务
        // 这个名字是在skynet_start中用skynet_handle_namehandle绑定的名字
        logger = skynet_handle_findname("logger");
    }
    if (logger == 0) {
        return;
    }

    char tmp[LOG_MESSAGE_SIZE];
    char *data = NULL;

    va_list ap;

    va_start(ap, msg);
    int len = vsnprintf(tmp, LOG_MESSAGE_SIZE, msg, ap);
    va_end(ap);
    if (len >= 0 && len < LOG_MESSAGE_SIZE) {
        data = skynet_strdup(tmp); // 分配内存
    } else {
        int max_size = LOG_MESSAGE_SIZE;
        for (;;) {
            max_size *= 2;
            data = skynet_malloc(max_size);
            va_start(ap, msg);
            len = vsnprintf(data, max_size, msg, ap);
            va_end(ap);
            if (len < max_size) {
                break;
            }
            skynet_free(data);
        }
    }
    if (len < 0) {
        skynet_free(data);
        perror("vsnprintf error :");
        return;
    }


    // 构建一条消息
    struct skynet_message smsg;
    if (context == NULL) {
        smsg.source = 0;
    } else {
        smsg.source = skynet_context_handle(context); // 消息来源(发送消息的服务)
    }
    smsg.session = 0;
    smsg.data = data;                                           // 消息内容
    smsg.sz = len | ((size_t)PTYPE_TEXT << MESSAGE_TYPE_SHIFT); // 消息长度，高八位表示skynet消息类型是 PTYPE_TEXT
    // 把消息push到logger服务
    skynet_context_push(logger, &smsg);
}
