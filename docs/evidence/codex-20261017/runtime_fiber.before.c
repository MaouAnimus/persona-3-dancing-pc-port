/* Fiber management (libSceFiber) for cooperative user-space context switching. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#define ORBIS_OK 0
#define ORBIS_FIBER_ERROR_NULL         ((int32_t)0x80590001)
#define ORBIS_FIBER_ERROR_ALIGNMENT    ((int32_t)0x80590002)
#define ORBIS_FIBER_ERROR_RANGE        ((int32_t)0x80590003)
#define ORBIS_FIBER_ERROR_INVALID      ((int32_t)0x80590004)
#define ORBIS_FIBER_ERROR_PERMISSION   ((int32_t)0x80590005)
#define ORBIS_FIBER_ERROR_STATE        ((int32_t)0x80590006)

#define ORBIS_FIBER_MAX_NAME_LENGTH 31
#define ORBIS_FIBER_CONTEXT_MINIMUM_SIZE 512

static const uint32_t kFiberSignature0 = 0xdef1649c;
static const uint32_t kFiberSignature1 = 0xb37592a0;
static const uint32_t kFiberOptSignature = 0xbb40e64d;
static const uint64_t kFiberStackSignature = 0x7149f2ca7149f2caULL;
static const uint64_t kFiberStackSizeCheck = 0xdeadbeefdeadbeefULL;

typedef void (ABI *OrbisFiberEntry)(uint64_t arg_on_initialize, uint64_t arg_on_run);

typedef enum {
    FiberState_Run = 1,
    FiberState_Idle = 2,
    FiberState_Terminated = 3
} FiberState;

typedef enum {
    FiberFlags_None = 0,
    FiberFlags_NoUlobjmgr = 1,
    FiberFlags_ContextSizeCheck = 0x10,
    FiberFlags_SetFpuRegs = 0x100
} FiberFlags;

struct OrbisFiber;

typedef struct OrbisFiberContext {
    uint64_t rax, rcx, rdx, rbx, rsp, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
    uint16_t fpucw;
    uint32_t mxcsr;
    struct OrbisFiber *current_fiber;
    struct OrbisFiber *prev_fiber;
    uint64_t arg_on_run_to;
    uint64_t arg_on_return;
    uint64_t return_val;
} OrbisFiberContext;

typedef struct OrbisFiberData {
    OrbisFiberEntry entry;
    uint64_t arg_on_initialize;
    uint64_t arg_on_run_to;
    void *stack_addr;
    _Atomic uint32_t *state;
    uint16_t fpucw;
    int8_t pad[2];
    uint32_t mxcsr;
} OrbisFiberData;

typedef struct OrbisFiber {
    uint32_t magic_start;
    _Atomic uint32_t state;
    OrbisFiberEntry entry;
    uint64_t arg_on_initialize;
    void *addr_context;
    uint64_t size_context;
    char name[ORBIS_FIBER_MAX_NAME_LENGTH + 1];
    OrbisFiberContext *context;
    uint32_t flags;
    void *context_start;
    void *context_end;
    uint32_t magic_end;
} OrbisFiber;

typedef struct OrbisFiberInfo {
    uint64_t size;
    OrbisFiberEntry entry;
    uint64_t arg_on_initialize;
    void *addr_context;
    uint64_t size_context;
    char name[ORBIS_FIBER_MAX_NAME_LENGTH + 1];
    uint64_t size_context_margin;
    uint8_t pad[48];
} OrbisFiberInfo;

typedef struct OrbisFiberOptParam {
    uint32_t magic;
} OrbisFiberOptParam;

/* Thread-local fiber execution context */
static _Thread_local OrbisFiberContext *g_thread_fiber_context = NULL;

static OrbisFiberContext *GetFiberContext(void) {
    return g_thread_fiber_context;
}

extern int ABI _sceFiberSetJmp(OrbisFiberContext *ctx);
extern int ABI _sceFiberLongJmp(OrbisFiberContext *ctx);
extern void ABI _sceFiberSwitchEntry(OrbisFiberData *data, int set_fpu);
extern void ABI _sceFiberForceQuit(uint64_t ret);

__asm__(
    ".text\n"
    ".globl _sceFiberSetJmp\n"
    ".def _sceFiberSetJmp; .scl 2; .type 32; .endef\n"
    "_sceFiberSetJmp:\n"
    "    movq %rax, 0x0(%rdi)\n"
    "    movq (%rsp), %rdx\n"
    "    movq %rdx, 0x10(%rdi)\n"
    "    movq %rcx, 0x08(%rdi)\n"
    "    movq %rbx, 0x18(%rdi)\n"
    "    movq %rsp, 0x20(%rdi)\n"
    "    movq %rbp, 0x28(%rdi)\n"
    "    movq %r8,  0x30(%rdi)\n"
    "    movq %r9,  0x38(%rdi)\n"
    "    movq %r10, 0x40(%rdi)\n"
    "    movq %r11, 0x48(%rdi)\n"
    "    movq %r12, 0x50(%rdi)\n"
    "    movq %r13, 0x58(%rdi)\n"
    "    movq %r14, 0x60(%rdi)\n"
    "    movq %r15, 0x68(%rdi)\n"
    "    fnstcw  0x70(%rdi)\n"
    "    stmxcsr 0x72(%rdi)\n"
    "    xor %eax, %eax\n"
    "    ret\n"

    ".globl _sceFiberLongJmp\n"
    ".def _sceFiberLongJmp; .scl 2; .type 32; .endef\n"
    "_sceFiberLongJmp:\n"
    "    stmxcsr -0x4(%rsp)\n"
    "    movl 0x72(%rdi), %eax\n"
    "    andl $0xffffffc0, %eax\n"
    "    movl -0x4(%rsp), %ecx\n"
    "    andl $0x3f, %ecx\n"
    "    xorl %eax, %ecx\n"
    "    movl %ecx, -0x4(%rsp)\n"
    "    ldmxcsr -0x4(%rsp)\n"
    "    movq 0x00(%rdi), %rax\n"
    "    movq 0x08(%rdi), %rcx\n"
    "    movq 0x10(%rdi), %rdx\n"
    "    movq 0x18(%rdi), %rbx\n"
    "    movq 0x20(%rdi), %rsp\n"
    "    movq 0x28(%rdi), %rbp\n"
    "    movq 0x30(%rdi), %r8\n"
    "    movq 0x38(%rdi), %r9\n"
    "    movq 0x40(%rdi), %r10\n"
    "    movq 0x48(%rdi), %r11\n"
    "    movq 0x50(%rdi), %r12\n"
    "    movq 0x58(%rdi), %r13\n"
    "    movq 0x60(%rdi), %r14\n"
    "    movq 0x68(%rdi), %r15\n"
    "    fldcw 0x70(%rdi)\n"
    "    movq %rdx, 0x00(%rsp)\n"
    "    movl $0x1, %eax\n"
    "    ret\n"

    ".globl _sceFiberSwitchEntry\n"
    ".def _sceFiberSwitchEntry; .scl 2; .type 32; .endef\n"
    "_sceFiberSwitchEntry:\n"
    "    mov %rdi, %r11\n"
    "    movq 0x18(%r11), %rsp\n"
    "    xorl %ebp, %ebp\n"
    "    movq 0x20(%r11), %r10\n"
    "    test %r10, %r10\n"
    "    jz 1f\n"
    "    movl $2, (%r10)\n"
    "1:\n"
    "    test %esi, %esi\n"
    "    jz 2f\n"
    "    ldmxcsr 0x2c(%r11)\n"
    "    fldcw 0x28(%r11)\n"
    "2:\n"
    "    movq 0x08(%r11), %rdi\n"
    "    movq 0x10(%r11), %rsi\n"
    "    movq 0x00(%r11), %r11\n"
    "    xorl %eax, %eax\n"
    "    xorl %ebx, %ebx\n"
    "    xorl %ecx, %ecx\n"
    "    xorl %edx, %edx\n"
    "    xorq %r8, %r8\n"
    "    xorq %r9, %r9\n"
    "    xorq %r10, %r10\n"
    "    xorq %r12, %r12\n"
    "    xorq %r13, %r13\n"
    "    xorq %r14, %r14\n"
    "    xorq %r15, %r15\n"
    "    call *%r11\n"
    "    movl $1, %edi\n"
    "    call _sceFiberForceQuit\n"
    "    ret\n"
);

void ABI _sceFiberForceQuit(uint64_t ret) {
    OrbisFiberContext *g_ctx = GetFiberContext();
    if (g_ctx) {
        g_ctx->return_val = ret;
        _sceFiberLongJmp(g_ctx);
    }
    abort();
}

static void _sceFiberCheckStackOverflow(OrbisFiberContext *ctx) {
    if (!ctx || !ctx->current_fiber) return;
    uint64_t *stack_base = (uint64_t *)ctx->current_fiber->addr_context;
    if (stack_base && *stack_base != kFiberStackSignature) {
        fputs("Runtime: Stack overflow detected in fiber!\n", stderr);
        abort();
    }
}

static int32_t _sceFiberAttachContext(OrbisFiber *fiber, void *addr_context, uint64_t size_context) {
    if (size_context && size_context < ORBIS_FIBER_CONTEXT_MINIMUM_SIZE) return ORBIS_FIBER_ERROR_RANGE;
    if (size_context & 15) return ORBIS_FIBER_ERROR_INVALID;
    if (!addr_context || !size_context) return ORBIS_FIBER_ERROR_INVALID;
    if (fiber->addr_context) return ORBIS_FIBER_ERROR_INVALID;

    fiber->addr_context = addr_context;
    fiber->size_context = size_context;
    fiber->context_start = addr_context;
    fiber->context_end = (unsigned char *)addr_context + size_context;
    *(uint64_t *)addr_context = kFiberStackSignature;

    if (fiber->flags & FiberFlags_ContextSizeCheck) {
        uint64_t *stack_start = (uint64_t *)fiber->context_start;
        uint64_t *stack_end = (uint64_t *)fiber->context_end;
        uint64_t *p = stack_start + 1;
        while (p < stack_end) *p++ = kFiberStackSizeCheck;
    }
    return ORBIS_OK;
}

static void _sceFiberSwitchToFiber(OrbisFiber *fiber, uint64_t arg_on_run_to, OrbisFiberContext *ctx) {
    OrbisFiberContext *fiber_ctx = fiber->context;
    if (fiber_ctx) {
        ctx->arg_on_run_to = arg_on_run_to;
        _sceFiberLongJmp(fiber_ctx);
        __builtin_trap();
    }

    OrbisFiberData data;
    memset(&data, 0, sizeof(data));
    if (ctx->prev_fiber) {
        OrbisFiber *prev_fiber = ctx->prev_fiber;
        ctx->prev_fiber = NULL;
        data.state = &prev_fiber->state;
    } else {
        data.state = NULL;
    }

    data.entry = fiber->entry;
    data.arg_on_initialize = fiber->arg_on_initialize;
    data.arg_on_run_to = arg_on_run_to;
    data.stack_addr = (unsigned char *)fiber->addr_context + fiber->size_context;
    if (fiber->flags & FiberFlags_SetFpuRegs) {
        data.fpucw = 0x037f;
        data.mxcsr = 0x9fc0;
        _sceFiberSwitchEntry(&data, 1);
    } else {
        _sceFiberSwitchEntry(&data, 0);
    }
    __builtin_trap();
}

static void _sceFiberSwitch(OrbisFiber *cur_fiber, OrbisFiber *fiber, uint64_t arg_on_run_to, OrbisFiberContext *ctx) {
    ctx->prev_fiber = cur_fiber;
    ctx->current_fiber = fiber;

    if (!fiber->addr_context) {
        ctx->prev_fiber = NULL;
        OrbisFiberData data;
        memset(&data, 0, sizeof(data));
        data.entry = fiber->entry;
        data.arg_on_initialize = fiber->arg_on_initialize;
        data.arg_on_run_to = arg_on_run_to;
        data.stack_addr = (void *)(ctx->rsp & ~15ULL);
        data.state = &cur_fiber->state;
        if (fiber->flags & FiberFlags_SetFpuRegs) {
            data.fpucw = 0x037f;
            data.mxcsr = 0x9fc0;
            _sceFiberSwitchEntry(&data, 1);
        } else {
            _sceFiberSwitchEntry(&data, 0);
        }
        __builtin_trap();
    }

    _sceFiberSwitchToFiber(fiber, arg_on_run_to, ctx);
    __builtin_trap();
}

static void _sceFiberTerminate(OrbisFiber *fiber, uint64_t arg_on_return, OrbisFiberContext *ctx) {
    (void)fiber;
    ctx->arg_on_return = arg_on_return;
    _sceFiberLongJmp(ctx);
    __builtin_trap();
}

static int32_t ABI sceFiberInitializeImpl(OrbisFiber *fiber, const char *name, OrbisFiberEntry entry,
                                         uint64_t arg_on_initialize, void *addr_context, uint64_t size_context,
                                         const OrbisFiberOptParam *opt_param, uint32_t flags, uint32_t build_ver) {
    (void)build_ver;
    if (!fiber || !name || !entry) return ORBIS_FIBER_ERROR_NULL;
    if (((uintptr_t)fiber & 7) || ((uintptr_t)addr_context & 15)) return ORBIS_FIBER_ERROR_ALIGNMENT;
    if (opt_param && ((uintptr_t)opt_param & 7)) return ORBIS_FIBER_ERROR_ALIGNMENT;
    if (size_context && size_context < ORBIS_FIBER_CONTEXT_MINIMUM_SIZE) return ORBIS_FIBER_ERROR_RANGE;
    if (size_context & 15) return ORBIS_FIBER_ERROR_INVALID;
    if (!addr_context && size_context) return ORBIS_FIBER_ERROR_INVALID;
    if (addr_context && !size_context) return ORBIS_FIBER_ERROR_INVALID;
    if (opt_param && opt_param->magic != kFiberOptSignature) return ORBIS_FIBER_ERROR_INVALID;

    memset(fiber, 0, sizeof(*fiber));
    fiber->entry = entry;
    fiber->arg_on_initialize = arg_on_initialize;
    fiber->addr_context = addr_context;
    fiber->size_context = size_context;
    strncpy(fiber->name, name, ORBIS_FIBER_MAX_NAME_LENGTH);
    fiber->flags = flags;
    fiber->magic_start = kFiberSignature0;
    fiber->magic_end = kFiberSignature1;

    if (addr_context) {
        fiber->context_start = addr_context;
        fiber->context_end = (unsigned char *)addr_context + size_context;
        *(uint64_t *)addr_context = kFiberStackSignature;
        if (flags & FiberFlags_ContextSizeCheck) {
            uint64_t *start = (uint64_t *)fiber->context_start;
            uint64_t *end = (uint64_t *)fiber->context_end;
            uint64_t *p = start + 1;
            while (p < end) *p++ = kFiberStackSizeCheck;
        }
    }
    atomic_store(&fiber->state, FiberState_Idle);
    return ORBIS_OK;
}

static int32_t ABI sceFiberOptParamInitialize(OrbisFiberOptParam *opt_param) {
    if (!opt_param) return ORBIS_FIBER_ERROR_NULL;
    if ((uintptr_t)opt_param & 7) return ORBIS_FIBER_ERROR_ALIGNMENT;
    opt_param->magic = kFiberOptSignature;
    return ORBIS_OK;
}

static int32_t ABI sceFiberFinalize(OrbisFiber *fiber) {
    if (!fiber) return ORBIS_FIBER_ERROR_NULL;
    if ((uintptr_t)fiber & 7) return ORBIS_FIBER_ERROR_ALIGNMENT;
    if (fiber->magic_start != kFiberSignature0 || fiber->magic_end != kFiberSignature1) return ORBIS_FIBER_ERROR_INVALID;
    uint32_t expected = FiberState_Idle;
    if (!atomic_compare_exchange_strong(&fiber->state, &expected, FiberState_Terminated)) return ORBIS_FIBER_ERROR_STATE;
    return ORBIS_OK;
}

static int32_t ABI sceFiberRunImpl(OrbisFiber *fiber, void *addr_context, uint64_t size_context,
                                  uint64_t arg_on_run_to, uint64_t *arg_on_return) {
    if (!fiber) return ORBIS_FIBER_ERROR_NULL;
    if (((uintptr_t)fiber & 7) || ((uintptr_t)addr_context & 15)) return ORBIS_FIBER_ERROR_ALIGNMENT;
    if (fiber->magic_start != kFiberSignature0 || fiber->magic_end != kFiberSignature1) return ORBIS_FIBER_ERROR_INVALID;
    if (g_thread_fiber_context != NULL) return ORBIS_FIBER_ERROR_PERMISSION;

    if (addr_context != NULL || size_context != 0) {
        int32_t res = _sceFiberAttachContext(fiber, addr_context, size_context);
        if (res < 0) return res;
    }

    uint32_t expected = FiberState_Idle;
    if (!atomic_compare_exchange_strong(&fiber->state, &expected, FiberState_Run)) return ORBIS_FIBER_ERROR_STATE;

    OrbisFiberContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.current_fiber = fiber;
    ctx.prev_fiber = NULL;
    ctx.return_val = 0;

    g_thread_fiber_context = &ctx;

    int32_t jmp = _sceFiberSetJmp(&ctx);
    if (!jmp) {
        if (fiber->addr_context) {
            _sceFiberSwitchToFiber(fiber, arg_on_run_to, &ctx);
            __builtin_trap();
        }
        OrbisFiberData data;
        memset(&data, 0, sizeof(data));
        data.entry = fiber->entry;
        data.arg_on_initialize = fiber->arg_on_initialize;
        data.arg_on_run_to = arg_on_run_to;
        data.stack_addr = (void *)(ctx.rsp & ~15ULL);
        data.state = NULL;
        if (fiber->flags & FiberFlags_SetFpuRegs) {
            data.fpucw = 0x037f;
            data.mxcsr = 0x9fc0;
            _sceFiberSwitchEntry(&data, 1);
        } else {
            _sceFiberSwitchEntry(&data, 0);
        }
    }

    OrbisFiber *cur_fiber = ctx.current_fiber;
    ctx.current_fiber = NULL;
    atomic_store(&cur_fiber->state, FiberState_Idle);

    if (ctx.return_val != 0) {
        fputs("Runtime: Fiber entry function unexpectedly returned\n", stderr);
        abort();
    }
    if (arg_on_return) *arg_on_return = ctx.arg_on_return;
    g_thread_fiber_context = NULL;
    return ORBIS_OK;
}

static int32_t ABI sceFiberSwitchImpl(OrbisFiber *fiber, void *addr_context, uint64_t size_context,
                                     uint64_t arg_on_run_to, uint64_t *arg_on_run) {
    if (!fiber) return ORBIS_FIBER_ERROR_NULL;
    if (((uintptr_t)fiber & 7) || ((uintptr_t)addr_context & 15)) return ORBIS_FIBER_ERROR_ALIGNMENT;
    if (fiber->magic_start != kFiberSignature0 || fiber->magic_end != kFiberSignature1) return ORBIS_FIBER_ERROR_INVALID;

    OrbisFiberContext *g_ctx = GetFiberContext();
    if (!g_ctx) return ORBIS_FIBER_ERROR_PERMISSION;

    if (addr_context != NULL || size_context != 0) {
        int32_t res = _sceFiberAttachContext(fiber, addr_context, size_context);
        if (res < 0) return res;
    }

    uint32_t expected = FiberState_Idle;
    if (!atomic_compare_exchange_strong(&fiber->state, &expected, FiberState_Run)) return ORBIS_FIBER_ERROR_STATE;

    OrbisFiber *cur_fiber = g_ctx->current_fiber;
    if (!cur_fiber->addr_context) {
        _sceFiberSwitch(cur_fiber, fiber, arg_on_run_to, g_ctx);
        __builtin_trap();
    }

    OrbisFiberContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    int32_t jmp = _sceFiberSetJmp(&ctx);
    if (!jmp) {
        cur_fiber->context = &ctx;
        _sceFiberCheckStackOverflow(g_ctx);
        _sceFiberSwitch(cur_fiber, fiber, arg_on_run_to, g_ctx);
        __builtin_trap();
    }

    g_ctx = GetFiberContext();
    if (g_ctx->prev_fiber) {
        atomic_store(&g_ctx->prev_fiber->state, FiberState_Idle);
        g_ctx->prev_fiber = NULL;
    }
    if (arg_on_run) *arg_on_run = g_ctx->arg_on_run_to;
    return ORBIS_OK;
}

static int32_t ABI sceFiberGetSelf(OrbisFiber **fiber) {
    if (!fiber) return ORBIS_FIBER_ERROR_NULL;
    OrbisFiberContext *g_ctx = GetFiberContext();
    if (!g_ctx) return ORBIS_FIBER_ERROR_PERMISSION;
    *fiber = g_ctx->current_fiber;
    return ORBIS_OK;
}

static int32_t ABI sceFiberReturnToThread(uint64_t arg_on_return, uint64_t *arg_on_run) {
    OrbisFiberContext *g_ctx = GetFiberContext();
    if (!g_ctx) return ORBIS_FIBER_ERROR_PERMISSION;

    OrbisFiber *cur_fiber = g_ctx->current_fiber;
    if (cur_fiber->addr_context) {
        OrbisFiberContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        int32_t jmp = _sceFiberSetJmp(&ctx);
        if (jmp) {
            g_ctx = GetFiberContext();
            if (g_ctx->prev_fiber) {
                atomic_store(&g_ctx->prev_fiber->state, FiberState_Idle);
                g_ctx->prev_fiber = NULL;
            }
            if (arg_on_run) *arg_on_run = g_ctx->arg_on_run_to;
            return ORBIS_OK;
        }
        cur_fiber->context = &ctx;
        _sceFiberCheckStackOverflow(g_ctx);
    }

    _sceFiberTerminate(cur_fiber, arg_on_return, g_ctx);
    __builtin_trap();
}

static int32_t ABI sceFiberInitialize(OrbisFiber *fiber, const char *name, OrbisFiberEntry entry,
                                     uint64_t arg_on_initialize, void *addr_context, uint64_t size_context,
                                     const OrbisFiberOptParam *opt_param, uint32_t build_ver) {
    return sceFiberInitializeImpl(fiber, name, entry, arg_on_initialize, addr_context, size_context, opt_param, 0, build_ver);
}

static int32_t ABI sceFiberRun(OrbisFiber *fiber, uint64_t arg_on_run_to, uint64_t *arg_on_return) {
    return sceFiberRunImpl(fiber, NULL, 0, arg_on_run_to, arg_on_return);
}

static int32_t ABI sceFiberSwitch(OrbisFiber *fiber, uint64_t arg_on_run_to, uint64_t *arg_on_run) {
    return sceFiberSwitchImpl(fiber, NULL, 0, arg_on_run_to, arg_on_run);
}

static int32_t ABI sceFiberGetInfo(OrbisFiber *fiber, OrbisFiberInfo *fiber_info) {
    if (!fiber || !fiber_info) return ORBIS_FIBER_ERROR_NULL;
    if (((uintptr_t)fiber & 7) || ((uintptr_t)fiber_info & 7)) return ORBIS_FIBER_ERROR_ALIGNMENT;
    if (fiber_info->size != sizeof(OrbisFiberInfo)) return ORBIS_FIBER_ERROR_INVALID;
    if (fiber->magic_start != kFiberSignature0 || fiber->magic_end != kFiberSignature1) return ORBIS_FIBER_ERROR_INVALID;

    fiber_info->entry = fiber->entry;
    fiber_info->arg_on_initialize = fiber->arg_on_initialize;
    fiber_info->addr_context = fiber->addr_context;
    fiber_info->size_context = fiber->size_context;
    strncpy(fiber_info->name, fiber->name, ORBIS_FIBER_MAX_NAME_LENGTH);
    fiber_info->size_context_margin = (uint64_t)-1;
    return ORBIS_OK;
}

static int32_t ABI sceFiberStartContextSizeCheck(uint32_t flags) { (void)flags; return ORBIS_OK; }
static int32_t ABI sceFiberStopContextSizeCheck(void) { return ORBIS_OK; }
static int32_t ABI sceFiberRename(OrbisFiber *fiber, const char *name) {
    if (!fiber || !name) return ORBIS_FIBER_ERROR_NULL;
    if ((uintptr_t)fiber & 7) return ORBIS_FIBER_ERROR_ALIGNMENT;
    if (fiber->magic_start != kFiberSignature0 || fiber->magic_end != kFiberSignature1) return ORBIS_FIBER_ERROR_INVALID;
    strncpy(fiber->name, name, ORBIS_FIBER_MAX_NAME_LENGTH);
    return ORBIS_OK;
}
static int32_t ABI sceFiberGetThreadFramePointerAddress(uint64_t *addr_frame_pointer) {
    if (!addr_frame_pointer) return ORBIS_FIBER_ERROR_NULL;
    OrbisFiberContext *g_ctx = GetFiberContext();
    if (!g_ctx) return ORBIS_FIBER_ERROR_PERMISSION;
    *addr_frame_pointer = g_ctx->rbp;
    return ORBIS_OK;
}

static const RuntimeExport fiber_exports[] = {
    {"sceFiberInitialize", sceFiberInitialize},
    {"hVYD7Ou2pCQ#libSceFiber", sceFiberInitialize},
    {"sceFiberInitializeImpl", sceFiberInitializeImpl},
    {"7+OJIpko9RY#libSceFiber", sceFiberInitializeImpl},
    {"sceFiberOptParamInitialize", sceFiberOptParamInitialize},
    {"asjUJJ+aa8s#libSceFiber", sceFiberOptParamInitialize},
    {"sceFiberFinalize", sceFiberFinalize},
    {"JeNX5F-NzQU#libSceFiber", sceFiberFinalize},
    {"sceFiberRun", sceFiberRun},
    {"a0LLrZWac0M#libSceFiber", sceFiberRun},
    {"sceFiberSwitch", sceFiberSwitch},
    {"PFT2S-tJ7Uk#libSceFiber", sceFiberSwitch},
    {"sceFiberGetSelf", sceFiberGetSelf},
    {"p+zLIOg27zU#libSceFiber", sceFiberGetSelf},
    {"sceFiberReturnToThread", sceFiberReturnToThread},
    {"B0ZX2hx9DMw#libSceFiber", sceFiberReturnToThread},
    {"sceFiberRunImpl", sceFiberRunImpl},
    {"avfGJ94g36Q#libSceFiber", sceFiberRunImpl},
    {"sceFiberSwitchImpl", sceFiberSwitchImpl},
    {"ZqhZFuzKT6U#libSceFiber", sceFiberSwitchImpl},
    {"sceFiberGetInfo", sceFiberGetInfo},
    {"uq2Y5BFz0PE#libSceFiber", sceFiberGetInfo},
    {"sceFiberStartContextSizeCheck", sceFiberStartContextSizeCheck},
    {"Lcqty+QNWFc#libSceFiber", sceFiberStartContextSizeCheck},
    {"sceFiberStopContextSizeCheck", sceFiberStopContextSizeCheck},
    {"Kj4nXMpnM8Y#libSceFiber", sceFiberStopContextSizeCheck},
    {"sceFiberRename", sceFiberRename},
    {"JzyT91ucGDc#libSceFiber", sceFiberRename},
    {"sceFiberGetThreadFramePointerAddress", sceFiberGetThreadFramePointerAddress},
    {"0dy4JtMUcMQ#libSceFiber", sceFiberGetThreadFramePointerAddress},
};

uintptr_t runtime_fiber_resolve(const char *name) {
    return RUNTIME_LOOKUP(fiber_exports, name);
}
