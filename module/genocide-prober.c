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

static atomic_t killcount = ATOMIC_INIT(0);

static ssize_t 
device_read(struct file * file, char * buffer, size_t length, loff_t *offset)
{
    int total;

    if ((length == 0) || (length < sizeof(total)))
        return 0;
   
    total = atomic_read(&killcount);
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
};

static struct miscdevice killcounter_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &fops,
    .mode = S_IRUGO,
};

asmlinkage long handle_sys_kill(int pid, int sig)
{
    if (sig == SIGKILL) {
        atomic_inc(&killcount);
    }
    
    jprobe_return();

    // should not happen
    return 0;
}

static struct jprobe killprobe;
static int genocide_prober_init(void)
{
    int ret;
    unsigned long addr; 

    printk("Registering genocide prober\n");

    ret = misc_register(&killcounter_dev);
    if (ret) {
        pr_err("misc device register failed\n");
        return ret;
    }

    addr = kallsyms_lookup_name("sys_kill");

    killprobe.kp.addr = (kprobe_opcode_t *) addr;
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
    printk("Deregistering genocide prober\n");
    misc_deregister(&killcounter_dev);
    unregister_jprobe(&killprobe); 
}
 
module_init(genocide_prober_init);
module_exit(genocide_prober_exit);

MODULE_AUTHOR("Juan Pablo Darago");
MODULE_DESCRIPTION("Prober of SIGKILL calls");
MODULE_LICENSE("GPL");
