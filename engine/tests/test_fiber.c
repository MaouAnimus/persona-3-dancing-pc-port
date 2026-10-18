/* Standalone optimized regression for libSceFiber context switching. */
#undef NDEBUG
#include "../src/runtime_fiber.c"
#include <assert.h>

uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) {
    for (size_t i = 0; i < count; ++i) {
        if (!strcmp(table[i].name, nid)) return (uintptr_t)table[i].function;
    }
    return 0;
}

static uint32_t read_mxcsr(void) {
    uint32_t value;
    __asm__ volatile("stmxcsr %0" : "=m"(value));
    return value;
}

static void write_mxcsr(uint32_t value) {
    __asm__ volatile("ldmxcsr %0" : : "m"(value));
}

static uint16_t read_fpucw(void) {
    uint16_t value;
    __asm__ volatile("fnstcw %0" : "=m"(value));
    return value;
}

static void write_fpucw(uint16_t value) {
    __asm__ volatile("fldcw %0" : : "m"(value));
}

static OrbisFiber resume_fiber, switch_a, switch_b, restart_fiber;
static _Alignas(16) unsigned char resume_stack[65536], a_stack[65536], b_stack[65536];
static unsigned resume_count, switch_steps, restart_count;

static void assert_self(OrbisFiber *expected) {
    OrbisFiber *self = NULL;
    assert(sceFiberGetSelf(&self) == ORBIS_OK && self == expected);
}

static void ABI resume_entry(uint64_t init, uint64_t run) {
    volatile uint64_t stack_cookie = 0xa55a0123456789abULL;
    assert(init == 11 && run == 22);
    assert_self(&resume_fiber);
    assert(read_fpucw() == 0x037f && (read_mxcsr() & ~0x3fu) == 0x9fc0);
    write_mxcsr(0x5fc0); /* A different rounding mode must survive suspension. */
    write_fpucw(0x077f);
    for (unsigned i = 0; ; ++i) {
        uint64_t incoming = 0;
        resume_count = i + 1;
        assert(sceFiberReturnToThread(100 + i, &incoming) == ORBIS_OK);
        assert(incoming == 23 + i);
        assert(stack_cookie == 0xa55a0123456789abULL);
        assert(read_fpucw() == 0x077f && (read_mxcsr() & ~0x3fu) == 0x5fc0);
        assert_self(&resume_fiber);
    }
}

static void ABI entry_a(uint64_t init, uint64_t run) {
    volatile uint64_t cookie = 0xabcdef0123456789ULL;
    uint64_t incoming = 0;
    assert(init == 1 && run == 10);
    assert_self(&switch_a);
    switch_steps |= 1;
    assert(sceFiberSwitch(&switch_b, 20, &incoming) == ORBIS_OK && incoming == 30);
    assert(cookie == 0xabcdef0123456789ULL);
    assert_self(&switch_a);
    switch_steps |= 4;
    assert(sceFiberReturnToThread(40, &incoming) == ORBIS_OK && incoming == 70);
    assert(cookie == 0xabcdef0123456789ULL);
    switch_steps |= 16;
    for (;;) assert(sceFiberReturnToThread(80, NULL) == ORBIS_OK);
}

static void ABI entry_b(uint64_t init, uint64_t run) {
    uint64_t incoming = 0;
    assert(init == 2 && run == 20);
    assert_self(&switch_b);
    switch_steps |= 2;
    assert(sceFiberSwitch(&switch_a, 30, &incoming) == ORBIS_OK && incoming == 50);
    assert_self(&switch_b);
    switch_steps |= 8;
    for (;;) assert(sceFiberReturnToThread(60, NULL) == ORBIS_OK);
}

static void ABI restart_entry(uint64_t init, uint64_t run) {
    assert(init == 9 && run == 90 + restart_count);
    assert_self(&restart_fiber);
    ++restart_count;
    (void)sceFiberReturnToThread(900 + restart_count, NULL);
    abort(); /* A fiber without a context restarts on the next Run. */
}

int main(void) {
    const uint32_t original_mxcsr = read_mxcsr();
    const uint16_t original_fpucw = read_fpucw();
    const uint32_t thread_mxcsr = (original_mxcsr & ~0x6000u) | 0x2000u;
    const uint16_t thread_fpucw = (original_fpucw & ~0x0c00u) | 0x0800u;
    uint64_t returned = 0;
    OrbisFiber *self = NULL;
    OrbisFiberContext saved = {0};

    write_mxcsr(thread_mxcsr);
    write_fpucw(thread_fpucw);
    assert(_sceFiberSetJmp(&saved) == 0);
    assert(saved.mxcsr == read_mxcsr() && saved.fpucw == read_fpucw());
    assert(sceFiberGetSelf(&self) == ORBIS_FIBER_ERROR_PERMISSION);
    assert(sceFiberReturnToThread(0, NULL) == ORBIS_FIBER_ERROR_PERMISSION);
    assert(runtime_fiber_resolve("a0LLrZWac0M#libSceFiber") == (uintptr_t)sceFiberRun);

    assert(sceFiberInitialize(&resume_fiber, "resume", resume_entry, 11,
                             resume_stack, sizeof(resume_stack), NULL, 0x03500000) == ORBIS_OK);
    assert(resume_fiber.flags & FiberFlags_SetFpuRegs);
    for (unsigned i = 0; i < 32; ++i) {
        assert(sceFiberRun(&resume_fiber, 22 + i, &returned) == ORBIS_OK);
        assert(returned == 100 + i && resume_count == i + 1);
        assert(atomic_load(&resume_fiber.state) == FiberState_Idle);
        assert(g_thread_fiber_context == NULL);
        assert((read_mxcsr() & ~0x3fu) == (thread_mxcsr & ~0x3fu));
        assert(read_fpucw() == thread_fpucw);
    }
    assert(sceFiberFinalize(&resume_fiber) == ORBIS_OK);

    assert(sceFiberInitialize(&switch_a, "a", entry_a, 1,
                             a_stack, sizeof(a_stack), NULL, 0x03400000) == ORBIS_OK);
    assert(sceFiberInitialize(&switch_b, "b", entry_b, 2,
                             b_stack, sizeof(b_stack), NULL, 0x03400000) == ORBIS_OK);
    assert(!(switch_a.flags & FiberFlags_SetFpuRegs));
    assert(sceFiberRun(&switch_a, 10, &returned) == ORBIS_OK && returned == 40 && switch_steps == 7);
    assert(sceFiberRun(&switch_b, 50, &returned) == ORBIS_OK && returned == 60 && switch_steps == 15);
    assert(sceFiberRun(&switch_a, 70, &returned) == ORBIS_OK && returned == 80 && switch_steps == 31);
    assert(sceFiberFinalize(&switch_a) == ORBIS_OK && sceFiberFinalize(&switch_b) == ORBIS_OK);

    assert(sceFiberInitialize(&restart_fiber, "restart", restart_entry, 9,
                             NULL, 0, NULL, 0x03500000) == ORBIS_OK);
    for (unsigned i = 0; i < 8; ++i) {
        assert(sceFiberRun(&restart_fiber, 90 + i, &returned) == ORBIS_OK);
        assert(returned == 901 + i && restart_count == i + 1);
    }
    assert(sceFiberFinalize(&restart_fiber) == ORBIS_OK);
    assert(sceFiberGetSelf(&self) == ORBIS_FIBER_ERROR_PERMISSION);
    write_mxcsr(original_mxcsr);
    write_fpucw(original_fpucw);
    puts("fiber tests passed (resume, switch, restart, FPU controls)");
    return 0;
}
