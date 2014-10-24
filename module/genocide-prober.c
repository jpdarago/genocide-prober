#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kprobes.h> 
#include <linux/kallsyms.h>
#include <linux/syscalls.h>
#include <linux/atomic.h>
#include <linux/signal.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

#define DEVICE_NAME "killcounter"

static atomic_long_t killcount;

static int device_open(struct inode * inode, struct file *file)
{
    long * countptr = kzalloc(sizeof(long), GFP_KERNEL);

    if(!countptr) 
        return -ENOMEM;

    *countptr = atomic_long_read(&killcount);
    file->private_data = countptr;

    return 0;
}

static int device_release(struct inode * inode, struct file *file)
{
    kfree(file->private_data);
    return 0;
}

static ssize_t 
device_read(struct file * file, char * buffer, size_t length, loff_t *offset)
{
    long started, currval, total;

    started = *((long *)file->private_data);
    currval = atomic_long_read(&killcount);
    total = currval - started;

    if ((length == 0) || (length < sizeof(total))
        return 0;

    copy_to_user(buffer, &total, sizeof(total));
    return sizeof(total);
}

static ssize_t 
device_write(struct file *filep, const char * buf, size_t len, loff_t * off)
{
    return -EINVAL;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static struct miscdevice killcounter_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &fops,
    .mode = S_IRUGO,
};

long handle_sys_kill(int pid, int sig)
{
    if (sig == SIGKILL) 
        atomic_long_inc(&killcount);

    jprobe_return();

    // should not happen
    return 0;
}

static struct jprobe killprobe;
static int genocide_prober_init(void)
{
    int ret;
    atomic_long_set(&killcount, 0); 
    ret = misc_register(&killcounter_dev);
    if (ret) {
        pr_err("misc device register failed\n");
        return ret;
    }
    killprobe.kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name("sys_kill");
    killprobe.entry = (kprobe_opcode_t *)handle_sys_kill;
    ret = register_jprobe(&killprobe);

    if(ret) {
        misc_deregister(&killcounter_dev);
        pr_err("jprobe loading failed");
        return ret;
    }
    return 0;
}
 
static void genocide_prober_exit(void)
{
    misc_deregister(&killcounter_dev);
    unregister_jprobe(&killprobe); 
}
 
module_init(genocide_prober_init);
module_exit(genocide_prober_exit);

MODULE_AUTHOR("Juan Pablo Darago");
MODULE_DESCRIPTION("Prober of SIGKILL calls");
MODULE_LICENSE("GPL");
