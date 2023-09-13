#include "skynet.h"

#include "skynet_module.h"
#include "spinlock.h"

#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MODULE_TYPE 32

struct modules {
    int count;
    struct spinlock lock;
    const char *path;
    struct skynet_module m[MAX_MODULE_TYPE];
};

static struct modules *M = NULL;

static void *
_try_open(struct modules *m, const char *name) {
    const char *l;
    const char *path = m->path;
    size_t path_size = strlen(path);
    size_t name_size = strlen(name);

    int sz = path_size + name_size;
    //search path
    void *dl = NULL;
    char tmp[sz];
    do {
        memset(tmp, 0, sz);
        while (*path == ';') path++;
        if (*path == '\0') break;
        l = strchr(path, ';');
        if (l == NULL) l = path + strlen(path);
        int len = l - path;
        int i;
        for (i = 0; path[i] != '?' && i < len; i++) {
            tmp[i] = path[i];
        }
        memcpy(tmp + i, name, name_size);
        if (path[i] == '?') {
            strncpy(tmp + i + name_size, path + i + 1, len - i - 1);
        } else {
            fprintf(stderr, "Invalid C service path\n");
            exit(1);
        }
        dl = dlopen(tmp, RTLD_NOW | RTLD_GLOBAL);
        path = l;
    } while (dl == NULL);

    if (dl == NULL) {
        fprintf(stderr, "try open %s failed : %s\n", name, dlerror());
    }

    return dl;
}

static struct skynet_module *
_query(const char *name) {
    int i;
    for (i = 0; i < M->count; i++) {
        if (strcmp(M->m[i].name, name) == 0) {
            return &M->m[i];
        }
    }
    return NULL;
}

static void *
get_api(struct skynet_module *mod, const char *api_name) {
    size_t name_size = strlen(mod->name);
    size_t api_size = strlen(api_name);
    char tmp[name_size + api_size + 1];
    memcpy(tmp, mod->name, name_size);
    memcpy(tmp + name_size, api_name, api_size + 1);
    char *ptr = strrchr(tmp, '.');
    if (ptr == NULL) {
        ptr = tmp;
    } else {
        ptr = ptr + 1;
    }
    return dlsym(mod->module, ptr);
}

static int
open_sym(struct skynet_module *mod) {      // 用动态库里面的函数填充模块
    mod->create = get_api(mod, "_create"); // 创建模块实例的函数
    mod->init = get_api(mod, "_init");     // 对实例进行初始化的函数
    mod->release = get_api(mod, "_release");
    mod->signal = get_api(mod, "_signal");

    return mod->init == NULL;
}

struct skynet_module *
skynet_module_query(const char *name) {
    struct skynet_module *result = _query(name); // 查询模块
    if (result)                                  // 说明不是第一次查询 因为已经缓存了 则直接返回
        return result;

    SPIN_LOCK(M)

    result = _query(name); // double check

    // 如果是第一次查询
    if (result == NULL && M->count < MAX_MODULE_TYPE) { // 这个模块是第一次被查询
        int index = M->count;                           // M可以认为是一个模块集合, 里面保存了所有模块 index是为新添加的模块分配一个位置
        void *dl = _try_open(M, name);                  // 打开这个模块对应的so文件
        if (dl) {
            M->m[index].name = name; // 设置这个模块的名字
            M->m[index].module = dl; // 设置这个模块对应的动态库

            if (open_sym(&M->m[index]) == 0) { // 用动态库里面的函数填充模块
                M->m[index].name = skynet_strdup(name);
                M->count++;
                result = &M->m[index]; // result是返回结果 即模块
            }
        }
    }

    SPIN_UNLOCK(M)

    return result;
}

void *
skynet_module_instance_create(struct skynet_module *m) {
    if (m->create) {
        return m->create();
    } else {
        return (void *)(intptr_t)(~0);
    }
}

int skynet_module_instance_init(struct skynet_module *m, void *inst, struct skynet_context *ctx, const char *parm) {
    return m->init(inst, ctx, parm);
}

void skynet_module_instance_release(struct skynet_module *m, void *inst) {
    if (m->release) {
        m->release(inst);
    }
}

void skynet_module_instance_signal(struct skynet_module *m, void *inst, int signal) {
    if (m->signal) {
        m->signal(inst, signal);
    }
}

void skynet_module_init(const char *path) {
    struct modules *m = skynet_malloc(sizeof(*m));
    m->count = 0;
    m->path = skynet_strdup(path);

    SPIN_INIT(m)

    M = m;
}
