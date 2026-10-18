/* Regression tests for the PS4 event flag ABI and blocking/consumption rules.
 * MSYS2 CLANG64: clang -std=c11 -O2 -g -Wall -Wextra -Werror -pthread
 *   -I. -Isrc tests/test_eventflag.c -o out/eventflag-test.exe
 */
#undef NDEBUG
#include "../src/runtime_kernel.c"
#include <assert.h>

static _Thread_local int32_t test_errno;
int32_t *runtime_errno(void) { return &test_errno; }
void runtime_report(void) {}
uintptr_t runtime_lookup(const RuntimeExport *table,size_t count,const char *name) {
    for (size_t i=0;i<count;++i) if (!strcmp(table[i].name,name)) return (uintptr_t)table[i].function;
    return 0;
}
#ifdef _WIN32
void compat_sleep_ns(uint64_t ns) { (void)ns; }
long compat_utc_offset(time_t t) { (void)t; return 0; }
int compat_random(void *buffer,size_t size) { memset(buffer,0,size); return 0; }
int compat_cpu_times(int who,int64_t *user,int64_t *kernel) { (void)who; *user=*kernel=0; return 0; }
#endif

typedef struct {
    uintptr_t id;
    uint64_t bits,pattern;
    uint32_t mode;
    int32_t result;
} WaitJob;
static void *wait_job(void *argument) {
    WaitJob *job=argument;
    job->result=kernel_wait_event_flag(job->id,job->bits,job->mode,&job->pattern,NULL);
    return NULL;
}
static void wait_for_waiters(uintptr_t id,unsigned expected) {
    uint64_t deadline=event_flag_now_ns()+UINT64_C(2000000000);
    for (;;) {
        unsigned count=0;
        pthread_mutex_lock(&ef_lock);
        KernelEventFlag *f=event_flag_find(id);
        if (f) for (EventFlagWaiter *w=f->first;w;w=w->next) ++count;
        pthread_mutex_unlock(&ef_lock);
        if (count==expected) return;
        assert(event_flag_now_ns()<deadline);
        sched_yield();
    }
}
static int32_t poll_wait(uintptr_t id,uint64_t bits,uint32_t mode,uint64_t *pattern) {
    uint32_t timeout=0;
    return kernel_wait_event_flag(id,bits,mode,pattern,&timeout);
}

int main(void) {
    struct { uintptr_t id; uint64_t canary; } handle={UINTPTR_MAX,UINT64_C(0x123456789abcdef0)};
    uint64_t pattern=0;
    assert(kernel_create_event_flag(&handle.id,"flags",0x21,6,NULL)==0);
    assert(handle.id && handle.id!=UINTPTR_MAX && handle.canary==UINT64_C(0x123456789abcdef0));
    uintptr_t id=handle.id;
    assert(poll_wait(id,3,2,&pattern)==0 && pattern==6); /* OR does not clear */
    assert(poll_wait(id,2,1,&pattern)==0 && pattern==6);
    assert(poll_wait(id,3,1,&pattern)==ERR(60) && pattern==6); /* AND requires every bit */
    assert(kernel_set_event_flag(id,9)==0);
    assert(poll_wait(id,3,0x21,&pattern)==0 && pattern==15); /* CLEAR_PAT only removes 3 */
    assert(poll_wait(id,12,1,&pattern)==0 && pattern==12);
    assert(poll_wait(id,4,0x11,&pattern)==0 && pattern==12); /* CLEAR_ALL removes all */
    assert(poll_wait(id,8,1,&pattern)==ERR(60) && pattern==0);

    struct { uint32_t us,canary; } timed={20000,0xfedcba98};
    uint64_t before=event_flag_now_ns();
    assert(kernel_wait_event_flag(id,1,1,&pattern,&timed.us)==ERR(60));
    assert(event_flag_now_ns()-before>=UINT64_C(10000000));
    assert(timed.us==0 && timed.canary==0xfedcba98 && pattern==0);
    assert(poll_wait(id,0,1,NULL)==ERR(22));
    assert(poll_wait(id,1,0,NULL)==ERR(22));
    assert(poll_wait(id,1,3,NULL)==ERR(22));
    assert(poll_wait(id,1,0x31,NULL)==ERR(22));

    /* Two consumers: the first registered waiter consumes a signal exactly once. */
    WaitJob a={.id=id,.bits=1,.mode=0x21},b=a;
    pthread_t ta,tb;
    assert(pthread_create(&ta,NULL,wait_job,&a)==0);
    wait_for_waiters(id,1);
    assert(pthread_create(&tb,NULL,wait_job,&b)==0);
    wait_for_waiters(id,2);
    assert(kernel_set_event_flag(id,1)==0);
    assert(pthread_join(ta,NULL)==0 && a.result==0 && a.pattern==1);
    wait_for_waiters(id,1);
    assert(kernel_set_event_flag(id,1)==0);
    assert(pthread_join(tb,NULL)==0 && b.result==0 && b.pattern==1);

    /* Deletion wakes blocked calls before freeing their synchronization state. */
    a=(WaitJob){.id=id,.bits=4,.mode=1}; b=a;
    assert(pthread_create(&ta,NULL,wait_job,&a)==0);
    assert(pthread_create(&tb,NULL,wait_job,&b)==0);
    wait_for_waiters(id,2);
    assert(kernel_delete_event_flag(id)==0);
    assert(pthread_join(ta,NULL)==0 && a.result==ERR(13));
    assert(pthread_join(tb,NULL)==0 && b.result==ERR(13));
    assert(kernel_set_event_flag(id,1)==ERR(3));
    assert(kernel_delete_event_flag(id)==ERR(3));

    /* SINGLE/default permits one blocked waiter and rejects a second. */
    uintptr_t single=0;
    assert(kernel_create_event_flag(&single,"single",0,0,NULL)==0 && single!=id);
    a=(WaitJob){.id=single,.bits=1,.mode=1};
    assert(pthread_create(&ta,NULL,wait_job,&a)==0);
    wait_for_waiters(single,1);
    assert(poll_wait(single,1,1,NULL)==ERR(1));
    assert(kernel_set_event_flag(single,1)==0);
    assert(pthread_join(ta,NULL)==0 && a.result==0);
    assert(kernel_delete_event_flag(single)==0);

    assert(kernel_create_event_flag(NULL,"bad",0,0,NULL)==ERR(22));
    assert(kernel_create_event_flag(&single,NULL,0,0,NULL)==ERR(22));
    assert(kernel_create_event_flag(&single,"bad",3,0,NULL)==ERR(22));
    assert(kernel_create_event_flag(&single,"bad",0x30,0,NULL)==ERR(22));
    assert(kernel_create_event_flag(&single,"name longer than the 31 byte limit",0,0,NULL)==ERR(63));
    puts("event flag tests: ok (ABI, AND/OR, clearing, timeout, FIFO, deletion, SINGLE/MULTI)");
    return 0;
}
