#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/console_struct.h>
#include <linux/keyboard.h>
#include <linux/input.h>
#include <linux/string.h>


#define BUF_LEN (PAGE_SIZE << 2)
#define CHUNK_LEN 12


extern int fg_console;
struct tty_driver *console_driver;

static unsigned char led_state;

int keysniffer_cb(struct notifier_block *nblock, unsigned long code, void *_param);
void start_caps_cb(void);
void stop_caps_cb(void);
void start_num_cb(void);
void stop_num_cb(void);
void lights_in_cb(void);
void lights_out_cb(void);

static struct {
	const char* keys;
	int pos;
	void (*callback)(void);
} commands[] = {
	{"start caps", 0, start_caps_cb},
	{"stop caps", 0, stop_caps_cb},
	{"start num", 0, start_num_cb},
	{"stop num", 0, stop_num_cb},
	{"let there be light", 0, lights_in_cb},
	{"lights out", 0, lights_out_cb},
	{NULL}
};

static void light_leds(void) {
	((console_driver->ops)->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED, led_state);
}

static void start_state(unsigned char state_param) {
	led_state |= state_param;
	light_leds();
}

void stop_state(unsigned char state_param) {
	led_state &= (~state_param);
	light_leds();
}

void lights_in_cb(void) {
	pr_debug("Let there be light\n");
	start_state(0x07);
}

void lights_out_cb(void) {
	pr_debug("The bulb is gone\n");
	stop_state(0x07);
}

void start_num_cb(void) {
	pr_debug("started num\n");
	start_state(0x02);
}

void stop_num_cb(void) {
	pr_debug("stopped num\n");
	stop_state(0x02);
}

void start_caps_cb(void) {
	pr_debug("started caps\n");
	start_state(0x04);
}

void stop_caps_cb(void) {
	pr_debug("stopped caps\n");
	stop_state(0x04);
}


static const char *us_keymap[][2] = {
	{"\0", "\0"}, {"_ESC_", "_ESC_"}, {"1", "!"}, {"2", "@"},         //0-3
	{"3", "#"}, {"4", "$"}, {"5", "%"}, {"6", "^"},                   //4-7
	{"7", "&"}, {"8", "*"}, {"9", "("}, {"0", ")"},                  //8-11
	{"-", "_"}, {"=", "+"}, {"_BACKSPACE_", "_BACKSPACE_"},         //12-14
	{"_TAB_", "_TAB_"}, {"q", "Q"}, {"w", "W"}, {"e", "E"}, {"r", "R"},
	{"t", "T"}, {"y", "Y"}, {"u", "U"}, {"i", "I"},                 //20-23
	{"o", "O"}, {"p", "P"}, {"[", "{"}, {"]", "}"},                 //24-27
	{"_ENTER_", "_ENTER_"}, {"_CTRL_", "_CTRL_"}, {"a", "A"}, {"s", "S"},
	{"d", "D"}, {"f", "F"}, {"g", "G"}, {"h", "H"},                 //32-35
	{"j", "J"}, {"k", "K"}, {"l", "L"}, {";", ":"},                 //36-39
	{"'", "\""}, {"`", "~"}, {"_SHIFT_", "_SHIFT_"}, {"\\", "|"},   //40-43
	{"z", "Z"}, {"x", "X"}, {"c", "C"}, {"v", "V"},                 //44-47
	{"b", "B"}, {"n", "N"}, {"m", "M"}, {",", "<"},                 //48-51
	{".", ">"}, {"/", "?"}, {"_SHIFT_", "_SHIFT_"}, {"_PRTSCR_", "_KPD*_"},
	{"_ALT_", "_ALT_"}, {" ", " "}, {"_CAPS_", "_CAPS_"}, {"F1", "F1"},
	{"F2", "F2"}, {"F3", "F3"}, {"F4", "F4"}, {"F5", "F5"},         //60-63
	{"F6", "F6"}, {"F7", "F7"}, {"F8", "F8"}, {"F9", "F9"},         //64-67
	{"F10", "F10"}, {"_NUM_", "_NUM_"}, {"_SCROLL_", "_SCROLL_"},   //68-70
	{"_KPD7_", "_HOME_"}, {"_KPD8_", "_UP_"}, {"_KPD9_", "_PGUP_"}, //71-73
	{"-", "-"}, {"_KPD4_", "_LEFT_"}, {"_KPD5_", "_KPD5_"},         //74-76
	{"_KPD6_", "_RIGHT_"}, {"+", "+"}, {"_KPD1_", "_END_"},         //77-79
	{"_KPD2_", "_DOWN_"}, {"_KPD3_", "_PGDN"}, {"_KPD0_", "_INS_"}, //80-82
	{"_KPD._", "_DEL_"}, {"_SYSRQ_", "_SYSRQ_"}, {"\0", "\0"},      //83-85
	{"\0", "\0"}, {"F11", "F11"}, {"F12", "F12"}, {"\0", "\0"},     //86-89
	{"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"},
	{"\0", "\0"}, {"_ENTER_", "_ENTER_"}, {"_CTRL_", "_CTRL_"}, {"/", "/"},
	{"_PRTSCR_", "_PRTSCR_"}, {"_ALT_", "_ALT_"}, {"\0", "\0"},    //99-101
	{"_HOME_", "_HOME_"}, {"_UP_", "_UP_"}, {"_PGUP_", "_PGUP_"}, //102-104
	{"_LEFT_", "_LEFT_"}, {"_RIGHT_", "_RIGHT_"}, {"_END_", "_END_"},
	{"_DOWN_", "_DOWN_"}, {"_PGDN", "_PGDN"}, {"_INS_", "_INS_"}, //108-110
	{"_DEL_", "_DEL_"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, //111-114
	{"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"}, {"\0", "\0"},       //115-118
	{"_PAUSE_", "_PAUSE_"},                                           //119
};

static struct notifier_block keysniffer_blk = {
	.notifier_call = keysniffer_cb,
};

void keycode_to_string(int keycode, int shift_mask, char *buf) {
	if (keycode > KEY_RESERVED && keycode <= KEY_PAUSE) {
		const char *us_key = (shift_mask == 1)? us_keymap[keycode][1] : us_keymap[keycode][0];
		snprintf(buf, CHUNK_LEN, "%s", us_key);
	}
}

int keysniffer_cb(struct notifier_block *nblock, unsigned long code, void *_param) {
	size_t len;
	char keybuf[CHUNK_LEN] = {0};
	struct keyboard_notifier_param *param = _param;
	int i, j;

	// pr_debug("code: 0x%lx, down: 0x%x, shift: 0x%x, value: 0x%x\n",
	// 	 code, param->down, param->shift, param->value);

	if (!(param->down))
		return NOTIFY_OK;

	keycode_to_string(param->value, param->shift, keybuf);
	len = strlen(keybuf);

	if (len < 1)
		return NOTIFY_OK;

	for(i = 0; commands[i].keys != NULL; i++) {
		for(j = 0; j < len; j++) {
			if(commands[i].keys[commands[i].pos] != keybuf[j]) {
				commands[i].pos = commands[i].keys[0] == keybuf[j]? 1: 0;
				break;
			}
			commands[i].pos++;
		}
		if(commands[i].pos == strlen(commands[i].keys)) { 
			commands[i].callback();
		}
	}

	return NOTIFY_OK;
}

int init_module(void) {
	register_keyboard_notifier(&keysniffer_blk);
	console_driver = vc_cons[fg_console].d->port.tty->driver;

	pr_debug("Started listening");
	return 0;
}

void cleanup_module(void) {
	unregister_keyboard_notifier(&keysniffer_blk);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Andrei Gavrila");
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Listens to certain key combinations and lights up accordingly.");
