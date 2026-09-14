/*
 * Scheduler queue residency, keyed by TID across CPU migration.
 * This optional CO-RE program requires kernel BTF; the server never loads BPF.
 */
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

struct {
    __uint(type, BPF_MAP_TYPE_LRU_HASH);
    __uint(max_entries, 16384);
    __type(key, __u32);
    __type(value, __u64);
} enqueued SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 64);
    __type(key, __u32);
    __type(value, __u64);
} histogram SEC(".maps");

/* 0: samples, 1: switch-ins without an enqueue timestamp, 2: map update failures. */
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 3);
    __type(key, __u32);
    __type(value, __u64);
} counters SEC(".maps");

static __always_inline void count(__u32 key) {
    __u64* value = bpf_map_lookup_elem(&counters, &key);
    if (value)
        __sync_fetch_and_add(value, 1);
}

static __always_inline void enqueue(struct task_struct* task) {
    __u32 tid = BPF_CORE_READ(task, pid);
    if (!tid)
        return;
    __u64 now = bpf_ktime_get_ns();
    if (bpf_map_update_elem(&enqueued, &tid, &now, BPF_ANY))
        count(2);
}

SEC("raw_tp/sched_wakeup")
int on_wakeup(struct bpf_raw_tracepoint_args* ctx) {
    enqueue((struct task_struct*)ctx->args[0]);
    return 0;
}

SEC("raw_tp/sched_wakeup_new")
int on_new_task(struct bpf_raw_tracepoint_args* ctx) {
    enqueue((struct task_struct*)ctx->args[0]);
    return 0;
}

SEC("raw_tp/sched_switch")
int on_switch(struct bpf_raw_tracepoint_args* ctx) {
    bool preempt = ctx->args[0];
    struct task_struct* prev = (struct task_struct*)ctx->args[1];
    struct task_struct* next = (struct task_struct*)ctx->args[2];
    if (preempt || BPF_CORE_READ(prev, __state) == 0)
        enqueue(prev);
    __u32 tid = BPF_CORE_READ(next, pid);
    if (!tid)
        return 0;
    __u64* start = bpf_map_lookup_elem(&enqueued, &tid);
    if (!start) {
        count(1);
        return 0;
    }
    __u64 begin = *start;
    bpf_map_delete_elem(&enqueued, &tid);
    __u64 now = bpf_ktime_get_ns();
    if (now < begin) {
        count(1);
        return 0;
    }
    __u64 us = (now - begin) / 1000;
    __u32 bucket = 0;
#pragma unroll
    for (int bit = 1; bit < 64; ++bit) {
        if (us >> bit)
            bucket = bit;
    }
    __u64* value = bpf_map_lookup_elem(&histogram, &bucket);
    if (value)
        __sync_fetch_and_add(value, 1);
    count(0);
    return 0;
}

SEC("raw_tp/sched_process_exit")
int on_exit(struct bpf_raw_tracepoint_args* ctx) {
    struct task_struct* task = (struct task_struct*)ctx->args[0];
    __u32 tid = BPF_CORE_READ(task, pid);
    bpf_map_delete_elem(&enqueued, &tid);
    return 0;
}
char LICENSE[] SEC("license") = "GPL";
