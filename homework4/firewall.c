#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/debugfs.h>

#include <linux/ip.h>
#include <uapi/linux/netfilter_ipv4.h>

/* function signature: http://lxr.free-electrons.com/source/include/linux/netfilter.h?v=4.10#L60
 * definition for socket buffer: http://lxr.free-electrons.com/source/include/linux/skbuff.h
 * sk_buff explanation: http://vger.kernel.org/~davem/skb.html
 * sk_buff data handling: http://vger.kernel.org/~davem/skb_data.html
 * for hook return codes see: http://lxr.free-electrons.com/source/include/uapi/linux/netfilter.h?v=4.10#L10
 */

static struct dentry * base_dir;


#define IP_MAX_LEN 17

static struct {
	int dropped;
	int total;
} stats;

static struct {
	ssize_t len;
	char buffer[IP_MAX_LEN];
} ip;



unsigned int firewall_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state) {
	struct iphdr *ip_head = ip_hdr(skb);
	char saddr[IP_MAX_LEN], daddr[IP_MAX_LEN];
	int random;

	snprintf(saddr, IP_MAX_LEN-1, "%pI4", &ip_head->saddr); 
	snprintf(daddr, IP_MAX_LEN-1, "%pI4", &ip_head->daddr); 
	

	pr_info("PACKET: data len: %d, src: %pI4, dest: %pI4\n", skb->len, &ip_head->saddr, &ip_head->daddr);

	if (skb->len == 200 && (memcmp(saddr, ip.buffer, ip.len) == 0 || memcmp(daddr, ip.buffer, ip.len) == 0)) {
		stats.total++;
		
		get_random_bytes(&random, sizeof random);

		if(random & 1) {
			stats.dropped++;

			pr_info("Packet dropped");
			pr_info("Current drop rate: %d%%", 100*stats.dropped/stats.total);
			return NF_DROP;
		} else {
			pr_info("Packet accepted");
		}

	} else {
		pr_info("Packet not significant");
	}

	return NF_ACCEPT;
}	


static ssize_t new_ip(struct file* f, const char* buf, size_t len, loff_t *offset) {
	if (len == 0) {
		return len;
	}

	memset(ip.buffer, 0, IP_MAX_LEN);

	ip.len = len-copy_from_user(ip.buffer, buf, len);

	if (ip.buffer[ip.len-1] == '\n') {
		ip.buffer[--ip.len] = 0;
	}

	pr_info("New ip given: %s", ip.buffer);
	return len;
}

static ssize_t get_ip(struct file *f, char __user *buf, size_t len, loff_t *offset) {
	return simple_read_from_buffer(buf, len, offset, ip.buffer, ip.len);;
}

static const struct file_operations ip_fops = {
	.write = new_ip,
	.read = get_ip,
};

static struct nf_hook_ops firewall_ops = {
	.hook = firewall_hook,
	.hooknum = 0,
	.pf = PF_INET,
	.priority = NF_IP_PRI_FIRST,
};

int init_module(void) {
	struct dentry * ip_file;

	memcpy(ip.buffer, "192.168.0.0\n", IP_MAX_LEN);
	ip.len = 12;

	pr_info("Registering the firewall hooks");
	nf_register_hook(&firewall_ops);


	base_dir = debugfs_create_dir("firewall", 0);

	if(!base_dir) {
		pr_debug("Base directory firewall could not be initalized");
		return -1;
	}

	ip_file = debugfs_create_file("ip", 0666, base_dir, 0, &ip_fops);

	if(!ip_file) {
		debugfs_remove_recursive(base_dir);
		pr_debug("Cannot initalize ip debugfs file");
		return -1;
	}

	return 0;
}

void cleanup_module(void)
{
	nf_unregister_hook(&firewall_ops);
	pr_info("Unregistering the firewall hooks");
	debugfs_remove_recursive(base_dir);
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrei Gavrila <andreigavrila1401@gmail.com>");
MODULE_DESCRIPTION("Hello network");
