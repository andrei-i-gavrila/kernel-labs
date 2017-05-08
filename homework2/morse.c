#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/console_struct.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>


#define QUEUE_SIZE 4
#define BASE_DELAY 250
#define DOT_MULT 4
#define DASH_MULT 8
#define IDLE_MULT 10
#define SPACE_MULT 1
#define STOP_MULT 4


static struct dentry* dir;
static struct {int len; char* msg;} work[QUEUE_SIZE];
static int worker_index = 0;
static int write_index = 0;
static int work_left = 0;


extern int fg_console;
struct tty_driver *console_driver;


struct timer_list timer;


static void lights(void) {
	((console_driver->ops)->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED, 0x07);
}

static void lights_out(void) {
	((console_driver->ops)->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED, 0x00);
}

static ssize_t new_message(struct file* f, const char* buf, size_t len, loff_t *offset) {
    char* msg;

    printk(KERN_NOTICE "Morse: %lu", len);

    if (work_left == QUEUE_SIZE) {
        printk(KERN_NOTICE "Morse: Too busy");
        return len;
    }

	if (len == 0) {
		printk(KERN_NOTICE "Morse: Blank input");
		return len;
	}

    msg = (char*) kmalloc((len)*sizeof(char), GFP_KERNEL);
    memset(msg, 0, (len)*sizeof(char));
    copy_from_user(msg, buf, len);

    work[write_index].len = len;
    work[write_index].msg = msg;

    if(++write_index == QUEUE_SIZE) {
        write_index = 0;
    }
    work_left++;

    return len;
}

static void idle_timer(unsigned long ptr);
static void work_timer(unsigned long ptr);
static void blank_timer(unsigned long ptr);

static void idle_timer(unsigned long ptr) {
	int i;
    if(work_left > 0) {
        timer.function = work_timer;
        work_timer(0);
		return;
    }

	printk(KERN_NOTICE "Morse: sitting idle");
	printk(KERN_NOTICE "Morse: workleft %d workindex %d write_index %d", work_left, worker_index, write_index);

	for (i = 0; i < QUEUE_SIZE; i++) {
		if(work[i].len > 0) {
			printk(KERN_NOTICE "Morse: work[%d]: %s", i, work[i].msg);
		} else {
			printk(KERN_NOTICE "Morse: work[%d] is not present", i);
		}
	}


    timer.expires = jiffies + msecs_to_jiffies(BASE_DELAY*IDLE_MULT);
    add_timer(&timer);
}

static void blank_timer(unsigned long ptr) {
    lights_out();
    timer.expires = jiffies + msecs_to_jiffies(BASE_DELAY);
    timer.function = work_timer;
	printk(KERN_NOTICE "blank");
    add_timer(&timer);
}

static char getBlinkedChar(char c) {
	if (c >= 'A' && c <= 'Z') {
		return c-'A' + 'a';
	}
	if (c == ' ' || (c >= 'a' && c <= 'z')) {
		return c;
	}
	return '.';
}

static void work_timer(unsigned long ptr) {
    static const char* chars[] = {
        ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
        "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
        "..-", "...-", ".--", "-..-", "-.--", "--.."
    };
    static int c_index = 0;
    static int l_index = 0;
	static int phase = 0;
	static char c = '.';

	if (phase == 1) {
		printk(KERN_NOTICE "Phase is 1. Gonna blank");
		timer.function = blank_timer;
		blank_timer(0);
		phase ^= 1;
		return;
	}

	if (l_index == 0) {
		c = getBlinkedChar(work[worker_index].msg[c_index]);
		printk(KERN_NOTICE "l_index is 0. Gonna get a new char to blink.");
	}

    if (c == '.' || c == ' ' || chars[c-'a'][l_index] == 0) {
    	printk(KERN_NOTICE "Reached a %c or ", c);
        l_index = 0;
        c_index++;
    }

    if (c_index == work[worker_index].len) {
		work[worker_index].len = 0;
		kfree(work[worker_index].msg);
		work[worker_index].msg = NULL;

		if (++worker_index == QUEUE_SIZE) {
			printk(KERN_NOTICE "wraparound");
			worker_index = 0;
		}
		work_left--;

        timer.function = idle_timer;
        c_index = 0;
        idle_timer(0);
		return;
    }

	if (c == ' ') {
		lights_out();
		timer.expires = jiffies + msecs_to_jiffies(BASE_DELAY * SPACE_MULT);
		add_timer(&timer);
		return;
	} else if (c == '.') {
		lights_out();
		timer.expires = jiffies + msecs_to_jiffies(BASE_DELAY * STOP_MULT);
		add_timer(&timer);
		return;
	} else {
		lights();
		if (chars[c-'a'][l_index++] == '.') {
			timer.expires = jiffies + msecs_to_jiffies(BASE_DELAY * DOT_MULT);
		} else {
			timer.expires = jiffies + msecs_to_jiffies(BASE_DELAY * DASH_MULT);
		}
		phase ^= 1;
		add_timer(&timer);
		return;
	}
}



static const struct file_operations fops = {
    .write = new_message,
};


int init_module(void) {
    struct dentry* message;
	int i;

    printk(KERN_NOTICE "Morse: in");

    printk(KERN_NOTICE "Morse: Creating debug dir");
    dir = debugfs_create_dir("morse", 0);
    if (!dir) {
        pr_debug("Failed to create debug dir morse");
        return -1;
    }

    printk(KERN_NOTICE "Morse: Creating debug file");
    message = debugfs_create_file("msg", 0666, dir, 0, &fops);
    if (!message) {
        pr_debug("Failed to create debug file morse/msg");
		debugfs_remove_recursive(dir);
        return -1;
    }

	console_driver = vc_cons[fg_console].d->port.tty->driver;


	for(i = 0; i < QUEUE_SIZE; i++) {
		work[i].len = 0;
		work[i].msg = NULL;
	}

    init_timer(&timer);
    timer.function = idle_timer;
    timer.data = 0;
    idle_timer(0);

    printk(KERN_NOTICE "Morse: all systems are operational");
    return 0;
}

void cleanup_module(void) {
	int i;

    printk(KERN_NOTICE "Morse: out");
    del_timer(&timer);
	printk(KERN_NOTICE "1");
    msleep(500);
    lights_out();
	printk(KERN_NOTICE "2");

    debugfs_remove_recursive(dir);
	printk(KERN_NOTICE "3");

	for(i = 0; i < QUEUE_SIZE; i++) {
		if (work[i].len > 0) {
			printk(KERN_NOTICE "4");

			kfree(work[i].msg);
			printk(KERN_NOTICE "5");

		}
	}
	printk(KERN_NOTICE "6");

}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrei Gavrila <andrei@codespace.ro>");
MODULE_DESCRIPTION("Morse code blink keyboard via debugfs");
