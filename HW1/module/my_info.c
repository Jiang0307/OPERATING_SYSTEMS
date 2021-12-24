#include "my_info.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif
#define PROC_NAME "my_info"

static void show_version_info(struct seq_file *m)
{
    seq_printf(m, "\n=============Version=============\n");
    seq_printf(m, "Linux version %s\n", UTS_RELEASE);
    seq_printf(m, "\n=============CPU================\n");
    return;
}

static int show_cpu_info(struct seq_file *m, void *v)
{
    struct cpuinfo_x86 *c = v;
    unsigned int cpu;
    cpu = c->cpu_index;
    if (cpu == 0)
    {
        show_version_info(m);
    }
    seq_printf(m, "processor\t: %u\n", cpu);
    seq_printf(m, "model name\t: %s\n", c->x86_model_id[0] ? c->x86_model_id : "unknown");
    seq_printf(m, "physical id\t: %d\n", c->phys_proc_id);
    seq_printf(m, "core id\t\t: %d\n", c->cpu_core_id);
    seq_printf(m, "cpu cores\t: %d\n", c->booted_cores);
    seq_printf(m, "cache size\t: %d KB\n", c->x86_cache_size);
    seq_printf(m, "clflush size\t: %u\n", c->x86_clflush_size);
    seq_printf(m, "cache_alignment\t: %d\n", c->x86_cache_alignment);
    seq_printf(m, "address sizes\t: %u bits physical, %u bits virtual\n", c->x86_phys_bits, c->x86_virt_bits);
    seq_printf(m, "\n");

    return 0;
}

static void show_memory_info(struct seq_file *m)
{
#define KB(x) ((x) << (PAGE_SHIFT - 10))

#define si_swapinfo(val)                        \
    do                                          \
    {                                           \
        (val)->freeswap = (val)->totalswap = 0; \
    } while (0)

    struct sysinfo i;
    unsigned long pages[NR_LRU_LISTS];
    int lru;

    si_meminfo(&i);
    si_swapinfo(&i);

    for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
        pages[lru] = global_node_page_state(NR_LRU_BASE + lru);

    seq_printf(m, "============MEMORY==============\n");
    seq_printf(m, "MemTotal\t: %lu kB\n", KB(i.totalram));
    seq_printf(m, "MemFree\t\t: %lu kB\n", KB(i.freeram));
    seq_printf(m, "Buffers\t\t: %lu kB\n", KB(i.bufferram));
    seq_printf(m, "Active\t\t: %lu kB\n", KB(pages[LRU_ACTIVE_ANON] + pages[LRU_ACTIVE_FILE]));
    seq_printf(m, "Inactive\t: %lu kB\n", KB(pages[LRU_INACTIVE_ANON] + pages[LRU_INACTIVE_FILE]));
    seq_printf(m, "Shmem\t\t: %lu kB\n", KB(i.sharedram));
    seq_printf(m, "Dirty\t\t: %lu kB\n", KB(global_node_page_state(NR_FILE_DIRTY)));
    seq_printf(m, "Writeback\t: %lu kB\n", KB(global_node_page_state(NR_WRITEBACK)));
    seq_printf(m, "KernelStack\t: %lu kB\n", global_zone_page_state(NR_KERNEL_STACK_KB));
    seq_printf(m, "PageTables\t: %lu kB\n", KB(global_zone_page_state(NR_PAGETABLE)));

    return;
}

static void show_time_info(struct seq_file *m)
{
    struct timespec uptime;
    struct timespec idle;
    u64 nsec;
    u32 rem;
    int j;

    nsec = 0;
    for_each_possible_cpu(j)
        nsec += (__force u64)kcpustat_cpu(j).cpustat[CPUTIME_IDLE];

    get_monotonic_boottime(&uptime);
    idle.tv_sec = div_u64_rem(nsec, NSEC_PER_SEC, &rem);
    idle.tv_nsec = rem;
    seq_printf(m, "\n=============TIME================\n");
    seq_printf(m, "Uptime\t\t: %lu.%02lu (s)\n", (unsigned long)uptime.tv_sec, (uptime.tv_nsec / (NSEC_PER_SEC / 100)));
    seq_printf(m, "Idletime\t: %lu.%02lu (s)\n", (unsigned long)idle.tv_sec, (idle.tv_nsec / (NSEC_PER_SEC / 100)));

    return;
}

static void *seq_start(struct seq_file *m, loff_t *pos)
{
    *pos = cpumask_next(*pos - 1, cpu_online_mask);
    if ((*pos) < nr_cpu_ids)
        return &cpu_data(*pos);

    show_memory_info(m);
    show_time_info(m);
    return NULL;
}

static void *seq_next(struct seq_file *m, void *v, loff_t *pos)
{
    (*pos)++;
    return seq_start(m, pos);
}

static void seq_stop(struct seq_file *s, void *v)
{
}

static struct seq_operations my_seq_ops =
{
        .start = seq_start,
        .next = seq_next,
        .stop = seq_stop,
        .show = show_cpu_info,
};

static int my_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &my_seq_ops);
};

#ifdef HAVE_PROC_OPS
static const struct proc_ops file_ops =
    {
        .proc_open = my_open,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = seq_release,
};
#else
static const struct file_operations file_ops =
    {
        .open = my_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release,
};
#endif

static int __init my_info_init(void)
{
    struct proc_dir_entry *entry;

    entry = proc_create(PROC_NAME, 0, NULL, &file_ops);
    if (entry == NULL)
    {
        remove_proc_entry(PROC_NAME, NULL);
        return -ENOMEM;
    }

    return 0;
}

static void __exit my_info_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);
}

module_init(my_info_init);
module_exit(my_info_exit);
MODULE_LICENSE("GPL");