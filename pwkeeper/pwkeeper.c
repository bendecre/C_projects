/* Benjamin Decre
 * bed1@umbc.edu
 *
 * Password keeper for passwords. Process master and user passwords for linux accounts.
 *
 * sources:
 * https://stackoverflow.com/questions/9207850/why-do-we-need-list-for-each-safe-in-for-deleting-nodes-in-kernel-linked-list
 * https://stackoverflow.com/questions/2559602/spin-lock-irqsave-vs-spin-lock-irq
 * https://stackoverflow.com/questions/43599961/test-for-linux-cap-fowner-capability-in-c
 */

/*
 * This file uses kernel-doc style comments, which is similar to
 * Javadoc and Doxygen-style comments. See
 * ~/linux/Documentation/doc-guide/kernel-doc.rst for details.
 */

/*
 * Getting compilation warnings? The Linux kernel is written against
 * C89, which means:
 *  - No // comments, and
 *  - All variables must be declared at the top of functions.
 * Read ~/linux/Documentation/process/coding-style.rst to ensure your
 * project compiles without warnings.
 */

#define pr_fmt(fmt) "pwkeeper: " fmt

#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uidgid.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <crypto/hash.h>

#include "xt_cs421net.h"

#define MASTERPW_LEN 32
#define ACCOUNTNAME_LEN 16
#define ACCOUNTPW_LEN 16

struct element {
	kuid_t UID;
	char password[32];
	struct list_head list;
};

struct users {
	kuid_t UID;
	char name[16];
	char password[16];
	struct list_head list;
};

LIST_HEAD(linkedlist);
LIST_HEAD(linkedlist1);

int INITIALIZED = 0;

static DEFINE_SPINLOCK(lock1);
static DEFINE_SPINLOCK(lock2);

unsigned long FLAGS;

static irqreturn_t cs421net_bottom(int irq, void *cookie);

/**
 * sha3_digest() - calculate the SHA-3 digest for an arbitrary input buffer
 * @input: input data buffer
 * @input_len: number of bytes in @input
 * @digest: destination pointer to store digest
 * @digest_len: size of digest buffer (in/out parameter)
 *
 * Hash the input buffer pointed to by @input, up to @input_len
 * bytes. Store the resulting digest at @digest. Afterwards, update
 * the value pointed to by @digest_len by the size of the stored
 * digest.
 *
 * <strong>You do not need to modify this function.</strong>
 *
 * Return: 0 on success, negative on error
 */
static int sha3_digest(const void *input, size_t input_len, u8 * digest,
		       size_t * digest_len)
{
	struct crypto_shash *sha3_tfm;
	struct shash_desc *sha3_desc;
	unsigned int digestsize;
	size_t i;
	int retval;

	sha3_tfm = crypto_alloc_shash("sha3-512", 0, 0);
	if (IS_ERR_OR_NULL(sha3_tfm)) {
		pr_err("Could not allocate hash tfm: %ld\n", PTR_ERR(sha3_tfm));
		return PTR_ERR(sha3_tfm);
	}

	digestsize = crypto_shash_digestsize(sha3_tfm);
	if (*digest_len < digestsize) {
		pr_err("Digest buffer too small, need at least %u bytes\n",
		       digestsize);
		retval = -EINVAL;
		goto out;
	}

	sha3_desc =
	    kzalloc(sizeof(*sha3_desc) + crypto_shash_descsize(sha3_tfm),
		    GFP_KERNEL);
	if (!sha3_desc) {
		pr_err("Could not allocate hash desc\n");
		retval = -ENOMEM;
		goto out;
	}
	sha3_desc->tfm = sha3_tfm;
	sha3_desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	retval = crypto_shash_digest(sha3_desc, input, input_len, digest);
	*digest_len = digestsize;
	pr_info("Hashed %zu bytes, digest = ", input_len);
	for (i = 0; i < digestsize; i++)
		pr_cont("%02x", digest[i]);
	pr_info("\n");
	kfree(sha3_desc);
out:
	crypto_free_shash(sha3_tfm);
	return retval;
}

/**
 * pwkeeper_master_write() - callback invoked when a process writes to
 * /dev/pwkeeper_master
 * @filp: process's file object that is writing to this device (ignored)
 * @ubuf: source buffer from user
 * @count: number of bytes in @ubuf
 * @ppos: file offset (in/out parameter)
 *
 * If *@ppos does not point to zero, do nothing and return -EINVAL.
 *
 * Copy the contents of @ubuf to the master password for the user, the
 * lesser of @count and MASTERPW_LEN. Then increment the value pointed
 * to by @ppos by the number of bytes copied.
 *
 * When replacing an existing master password, recalculate all account
 * passwords.
 *
 * <em>Caution: @ubuf is not a string; it is not null-terminated.</em>
 *
 * Return: number of bytes copied from @ubuf, or negative on error
 */
static ssize_t pwkeeper_master_write(struct file *filp,
				     const char __user * ubuf, size_t count,
				     loff_t * ppos)
{
	/* Variables */

	size_t allowed;
	kuid_t uid = current_uid();
	struct element *ptr = NULL;
	int loopSaver = 0;
	int flag = 0;
	int check = 0;

	struct element master = {
		.UID = {0},
		.password = "MasterBlaster",
		.list = LIST_HEAD_INIT(master.list)
	};

	/* Code */

	spin_lock_irqsave(&lock1, FLAGS);

	if (*ppos != 0)
		return -EPERM;

	if (INITIALIZED == 0) {
		list_add_tail(&master.list, &linkedlist);
		INITIALIZED = 1;
	}

	if (count > 32)
		allowed = 32;
	else
		allowed = count;

	list_for_each_entry(ptr, &linkedlist, list) {
		if (loopSaver > 3) {
			break;
		}

		if (uid_eq(ptr->UID, uid)) {
			memset(ptr->password, 0, 32);
			check += copy_from_user(ptr->password, ubuf, allowed);
			flag = 1;
		}

		loopSaver++;
	}

	if (flag == 0) {
		struct element user = {
			.UID = uid,
			.list = LIST_HEAD_INIT(user.list)
		};

		check += copy_from_user(user.password, ubuf, allowed);

		list_add_tail(&user.list, &linkedlist);
	}

	spin_unlock_irqrestore(&lock1, FLAGS);

	if (check > 0)
		return -1;

	return count;
}

/**
 * pwkeeper_account_read() - callback invoked when a process reads
 * from /dev/pwkeeper_account
 * @filp: process's file object that is reading from this device (ignored)
 * @ubuf: destination to store account password
 * @count: number of bytes in @ubuf
 * @ppos: file offset (in/out parameter)
 *
 * Write to @ubuf the password generated for the most recently written
 * account name for the current UID, offset by @ppos. Copy the lesser
 * of @count and (ACCOUNTPW_LEN - *@ppos). Then increment the value
 * pointed to by @ppos by the number of bytes written. If @ppos is
 * greater than or equal to ACCOUNTPW_LEN, then write
 * nothing.
 *
 * If no account name was set (via previous successful invocation of
 * pwkeeper_account_write()), do nothing and return -ENOKEY.
 *
 * Return: number of bytes written to @ubuf, 0 on end of file, or
 * negative on error
 */
static ssize_t pwkeeper_account_read(struct file *filp, char __user * ubuf,
				     size_t count, loff_t * ppos)
{
	struct users *ptr;
	int loopSaver = 0;
	int check = 0;
	int bytes = 0;

	list_for_each_entry(ptr, &linkedlist1, list) {
		if (loopSaver > 3) {
			break;
		}

		loopSaver++;
		if (ptr != NULL) {
			check += copy_to_user(ubuf, &(ptr->UID.val), 1);
			check += copy_to_user(ubuf, ptr->password, 16);
			bytes += 17;
		}
	}

	if (check > 0)
		return -1;

	return bytes;
}

/**
 * pwkeeper_account_write() - callback invoked when a process writes
 * to /dev/pwkeeper_account
 * @filp: process's file object that is writing to this device (ignored)
 * @ubuf: source buffer from user
 * @count: number of bytes in @ubuf
 * @ppos: file offset (in/out parameter)
 *
 * If *@ppos does not point to zero, do nothing and return -EINVAL.
 *
 * If the current user has not set a master password, do nothing and
 * return -ENOKEY.
 *
 * Otherwise check if @ubuf is already in the accounts list associated
 * with the current user. If it is already there, do nothing and
 * return @count.
 *
 * Otherwise, create a new node in the accounts list associated with
 * the current user. Copy the contents of @ubuf to that node, the
 * lesser of @count and ACCOUNTNAME_LEN. Increment the value pointed
 * to by @ppos by the number of bytes copied. Finally, perform the key
 * derivation function as specified in the project description, to
 * determine the account's password.
 *
 * <em>Caution: @ubuf is not a string; it is not null-terminated.</em>
 *
 * Return: @count, or negative on error
 */
static ssize_t pwkeeper_account_write(struct file *filp,
				      const char __user * ubuf, size_t count,
				      loff_t * ppos)
{
	/* Variables */

	struct users *ptr;
	int loopSaver = 0;
	kuid_t uid = current_uid();
	int flag = 0;
	char buf[48];
	int i;
	char DIGEST[64];
	char extraction;
	size_t bufSize = 48;
	int check = 0;
	size_t digestSize = 64;

	/* Code */

	spin_lock_irqsave(&lock2, FLAGS);

	list_for_each_entry(ptr, &linkedlist1, list) {
		if (loopSaver > 3) {
			break;
		}

		if (uid_eq(ptr->UID, uid)) {
			flag = 1;
			check += copy_from_user(buf, ptr->password, 32);
		}

		loopSaver++;
	}

	if (flag != 0) {
		struct users t = {
			.UID = uid,
			.list = LIST_HEAD_INIT(t.list)
		};

		check += copy_from_user(t.name, ubuf, 16);

		/* Code for KDF function to generate password */

		for (i = 0; i < 16; i++) {
			buf[i + 32] = t.name[i];
		}

		sha3_digest(&buf, bufSize, &DIGEST[0], &digestSize);

		/* Come up with passwords and store it on the account */

		for (i = 0; i < 16; i++) {
			extraction = DIGEST[i];
			extraction = extraction & 0x3F;
			t.password[i] = extraction;

		}
	}

	if (check > 0)
		return -1;

	spin_unlock_irqrestore(&lock2, FLAGS);
	return count;
}

static const struct file_operations pwkeeper_master_fops = {
	.write = pwkeeper_master_write,
};

static struct miscdevice pwkeeper_master_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "pwkeeper_master",
	.fops = &pwkeeper_master_fops,
	.mode = 0666
};

static const struct file_operations pwkeeper_account_fops = {
	.read = pwkeeper_account_read,
	.write = pwkeeper_account_write,
};

static struct miscdevice pwkeeper_account_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "pwkeeper_account",
	.fops = &pwkeeper_account_fops,
	.mode = 0666
};

/**
 * pwkeeper_accounts_show() - callback invoked when a process reads from
 * /sys/devices/platform/pwkeeper/accounts
 *
 * @dev: device driver data for sysfs entry (ignored)
 * @attr: sysfs entry context (ignored)
 * @buf: destination to store current user's accounts
 *
 * Write to @buf, up to PAGE_SIZE characters, a human-readable message
 * that lists all accounts registered for the current UID, and the
 * associated account passwords. Note that @buf is a normal character
 * buffer, not a __user buffer. Use scnprintf() in this function.
 *
 * @return Number of bytes written to @buf, or negative on error.
 */
static ssize_t pwkeeper_accounts_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct users *ptr;
	int loopSaver = 0;
	int check = 0;
	kuid_t uid = current_uid();
	int bytes = 0;

	list_for_each_entry(ptr, &linkedlist1, list) {
		if (loopSaver > 3) {
			break;
		}

		loopSaver++;
		if (ptr != NULL && (uid_eq(ptr->UID, uid))) {
			check += copy_to_user(buf, ptr->name, 16);
			check += copy_to_user(buf, ptr->password, 16);
			bytes += 32;
		}
	}

	if (check > 0)
		return -1;

	return bytes;
}

/**
 * pwkeeper_master_show() - callback invoked when a process reads from
 * /sys/devices/platform/pwkeeper/masters
 *
 * @dev: device driver data for sysfs entry (ignored)
 * @attr: sysfs entry context (ignored)
 * @buf: destination to store login statistics
 *
 * Check if the calling process has CAP_SYS_ADMIN. If not, return
 * -EPERM.
 *
 * Otherwise, write to @buf, up to PAGE_SIZE characters, a
 * human-readable message that lists all users IDs that have
 * registered master passwords. Note that @buf is a normal character
 * buffer, not a __user buffer. Use scnprintf() in this function.
 *
 * @return Number of bytes written to @buf, or negative on error.
 */
static ssize_t pwkeeper_masters_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	/* Variables */

	struct users *ptr;
	int loopSaver = 0;
	int check = 0;
	int bytes = 0;

	/* Code */

	if (!capable(CAP_SYS_ADMIN)) {
		return -EPERM;
	}

	list_for_each_entry(ptr, &linkedlist, list) {
		if (loopSaver > 3) {
			break;
		}

		loopSaver++;
		if (ptr != NULL) {
			check += copy_to_user(buf, &(ptr->UID.val), 1);
			bytes += 1;
		}
	}

	if (check > 0)
		return -1;

	return bytes;
}

static DEVICE_ATTR(accounts, S_IRUGO, pwkeeper_accounts_show, NULL);
static DEVICE_ATTR(masters, S_IRUGO, pwkeeper_masters_show, NULL);

/**
 * cs421net_top() - top-half of CS421Net ISR
 * @irq: IRQ that was invoked (ignored)
 * @cookie: Pointer to data that was passed into
 * request_threaded_irq() (ignored)
 *
 * If @irq is CS421NET_IRQ, then wake up the bottom-half. Otherwise,
 * return IRQ_NONE.
 */
static irqreturn_t cs421net_top(int irq, void *cookie)
{
	if (irq == CS421NET_IRQ)
		return cs421net_bottom(irq, cookie);

	return IRQ_NONE;
}

/**
 * cs421net_bottom() - bottom-half to CS421Net ISR
 * @irq: IRQ that was invoked (ignore)
 * @cookie: Pointer that was passed into request_threaded_irq()
 * (ignored)
 *
 * Fetch the incoming packet, via cs421net_get_data(). Treat the input
 * as a 32-BIT LITTLE ENDIAN BINARY VALUE representing a UID. Search
 * through the master list and accounts list, deleting all nodes with
 * that UID. If the UID is exactly zero, then delete ALL nodes in the
 * master and accounts lists.
 *
 * If the packet length is not exactly 4 bytes, or if the provided
 * value does not match a registered UID in the master list, then do
 * nothing.
 *
 * Remember to add appropriate spin lock calls in this function.
 *
 * <em>Caution: The incoming payload is not a string; it is not null-terminated.</em>
 * You can NOT use strcpy() or strlen() on it.
 *
 * Return: always IRQ_HANDLED
 */
static irqreturn_t cs421net_bottom(int irq, void *cookie)
{
	size_t data = 32;
	struct users *ptr;
	int loopSaver = 0;

	cs421net_get_data(&data);

	list_for_each_entry(ptr, &linkedlist, list) {
		if (loopSaver > 3) {
			break;
		}

		loopSaver++;
		if (ptr->UID.val == data) {
			list_del(&(ptr->list));
		}
	}
	return IRQ_HANDLED;
}

/**
 * pwkeeper_probe() - callback invoked when this driver is probed
 * @pdev platform device driver data (ignored)
 *
 * Return: 0 on successful probing, negative on error
 */
static int pwkeeper_probe(struct platform_device *pdev)
{
	int retval;

	retval = misc_register(&pwkeeper_master_dev);
	if (retval) {
		pr_err("Could not register master device\n");
		goto err;
	}

	retval = misc_register(&pwkeeper_account_dev);
	if (retval) {
		pr_err("Could not register account device\n");
		goto err_deregister_master;
	}

	retval = device_create_file(&pdev->dev, &dev_attr_accounts);
	if (retval) {
		pr_err("Could not create sysfs entry\n");
		goto err_deregister_account;
	}

	retval = device_create_file(&pdev->dev, &dev_attr_masters);
	if (retval) {
		pr_err("Could not create sysfs entry\n");
		goto err_remove_sysfs_accounts;
	}

	retval = device_create_file(&pdev->dev, &dev_attr_masters);
	if (retval) {
		pr_err("Could not create sysfs entry\n");
		goto err_remove_sysfs_masters;
	}

	pr_info("Probe successful\n");
	return 0;

err_remove_sysfs_masters:
	device_remove_file(&pdev->dev, &dev_attr_masters);
err_remove_sysfs_accounts:
	device_remove_file(&pdev->dev, &dev_attr_accounts);
err_deregister_account:
	misc_deregister(&pwkeeper_account_dev);
err_deregister_master:
	misc_deregister(&pwkeeper_master_dev);
err:
	pr_err("Probe failed, error %d\n", retval);
	return retval;

}

/**
 * pwkeeper_remove() - callback when this driver is removed
 * @pdev platform device driver data (ignored)
 *
 * Return: Always 0
 */
static int pwkeeper_remove(struct platform_device *pdev)
{
	pr_info("Removing\n");

	cs421net_top(0, 0);
	cs421net_bottom(0, 0);
	device_remove_file(&pdev->dev, &dev_attr_masters);
	device_remove_file(&pdev->dev, &dev_attr_accounts);
	misc_deregister(&pwkeeper_account_dev);
	misc_deregister(&pwkeeper_master_dev);
	return 0;
}

static struct platform_driver cs421_driver = {
	.driver = {
		   .name = "pwkeeper",
		   },
	.probe = pwkeeper_probe,
	.remove = pwkeeper_remove,
};

static struct platform_device *pdev;

/**
 * cs421_init() -  create the platform driver
 * This is needed so that the device gains a sysfs group.
 *
 * <strong>You do not need to modify this function.</strong>
 */
static int __init cs421_init(void)
{
	pdev = platform_device_register_simple("pwkeeper", -1, NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);
	return platform_driver_register(&cs421_driver);
}

/**
 * cs421_exit() - remove the platform driver
 * Unregister the driver from the platform bus.
 *
 * <strong>You do not need to modify this function.</strong>
 */
static void __exit cs421_exit(void)
{
	platform_driver_unregister(&cs421_driver);
	platform_device_unregister(pdev);
}

module_init(cs421_init);
module_exit(cs421_exit);

MODULE_DESCRIPTION("CS421 Password Keeper - project 2");
MODULE_LICENSE("GPL");
