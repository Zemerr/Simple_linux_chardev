#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/debugfs.h>

#include "custom_device.h"
#include "cdev_ioctl.h"

static int open_device(struct inode *inode, struct file *file);
static int release_device(struct inode *inode, struct file *file);
static ssize_t write_device(struct file *file, const char __user *buf, size_t size, loff_t *offset);
static ssize_t read_device(struct file *file, char __user *buf, size_t size, loff_t *offset);
static ssize_t read_device_debug(struct file *file, char __user *buf, size_t size, loff_t *offset);
static long device_ioctl (struct file *file, unsigned int cmd, unsigned long arg);
static int device_fasync(int fd, struct file *file, int mode);

/* Array of devices entry */
static struct custom_device_driver dev_driver = {0};

/* initialize device file_operations */
static const struct file_operations custom_device_fops = {
    .owner          = THIS_MODULE,
    .open           = open_device,
    .release        = release_device,
    .unlocked_ioctl = device_ioctl,
    .read           = read_device,
    .write          = write_device,
    .fasync         = device_fasync
};

/* initialize debugfs file_operations */
static const struct file_operations debug_device_fops = {
    .owner      = THIS_MODULE,
    .read       = read_device_debug
};

/*************** Device File Operation ****************/

static int open_device(struct inode *inode, struct file *file) {
    struct device_data *dev_data = container_of(inode->i_cdev, struct device_data, cdev);
    file->private_data = dev_data;

    return 0;
}

static int release_device(struct inode *inode, struct file *file) {
    file->private_data = NULL;

    return 0;
}

static ssize_t read_device(struct file *file, char __user *buf, size_t size, loff_t *offset) {
    struct device_data *dev_data = NULL;
    struct database *database = NULL;
    char *message = NULL;
    int len_to_return = 0;
    int nbytes = 0;

    dev_data = (struct device_data *)file->private_data;

    if (mutex_lock_interruptible(&dev_data->data_lock)) {
        return 0;
    }

    /* Take last written string */
    database = list_first_entry_or_null(&dev_data->database->list, struct database, list);

    /* Copy string in temp buffer or notify about empty database */
    if (database) {
        message = kmalloc(database->len, GFP_KERNEL);
        strncpy(message, database->saved_str, database->len);
        len_to_return = database->len - *offset;
    } else {
        message = kmalloc(strlen(DATABASE_EMPTY), GFP_KERNEL);
        strncpy(message, DATABASE_EMPTY, strlen(DATABASE_EMPTY));
        len_to_return = strlen(DATABASE_EMPTY) - *offset;
    }
    mutex_unlock(&dev_data->data_lock);

    /* Copy string to user */
    len_to_return = min(len_to_return, (int)size);
    if (len_to_return > 0) {
        nbytes = len_to_return - copy_to_user(buf, message + *offset, len_to_return);
        *offset += nbytes;
    }
    kfree(message);

    return nbytes;
}


static ssize_t write_device(struct file *file, const char __user *buf, size_t size, loff_t *offset) {
    struct device_data *dev_data = NULL;
    struct database *new_node = NULL;
    int nbytes = 0;
    int len = 0;

    dev_data = (struct device_data *) file->private_data;

    if (size >= 0 && size <= MAX_BUF) {
        /* Create new node and allocate memory for new string */
        new_node = kmalloc(sizeof(struct database), GFP_KERNEL);
        if (!new_node) {
            return 0;
        }
        new_node->saved_str = kmalloc(size, GFP_KERNEL);
        if (!new_node->saved_str) {
            kfree(new_node);
            return 0;
        }
        new_node->len = size;

        /* Copy string from user */
        memset(new_node->saved_str, 0, size);
        nbytes = size - copy_from_user(new_node->saved_str, buf, size);
        *offset += nbytes;

        /* Compare new string with reference */
        if (!mutex_lock_interruptible(&dev_data->data_lock)) {
            if (dev_data->ref_str.reference_str) {
                len = (dev_data->ref_str.reference_str_len > size) ? dev_data->ref_str.reference_str_len : size;

                if (!strncmp(dev_data->ref_str.reference_str, new_node->saved_str, len - 1)) {
                    kill_fasync(&dev_data->async, SIGIO, POLL_IN);
                }
            }

            /* Add node with new string to dev database */
            list_add(&new_node->list, &dev_data->database->list);
            dev_data->general_data_len += nbytes;
            dev_data->general_str_count++;
            mutex_unlock(&dev_data->data_lock);
        } else {
            kfree(new_node->saved_str);
            kfree(new_node);
            return 0;
        }

    }

    return nbytes;
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct device_data *dev_data = container_of(file->f_inode->i_cdev, struct device_data, cdev);
    struct database *database = NULL;
    struct database *tmp= NULL;
    int i = 0;
    char ch = 0;
    char *tmp_arg = NULL;
    char *tmp_str = NULL;

    if (_IOC_TYPE(cmd) != MYDBASE) {
        return -EINVAL;
    }

    switch (cmd) {
        case IOCTL_DEL_DTB:
        /* Delete device database */
            if (mutex_lock_interruptible(&dev_data->data_lock)) {
                return 0;
            }
            list_for_each_entry_safe(database, tmp, &(dev_data->database->list), list) {
                list_del(&database->list);
                kfree(database->saved_str);
                database->saved_str = NULL;
                database->len = 0;
                kfree(database);
                database = NULL;
            }
            dev_data->general_data_len = 0;
            dev_data->general_str_count = 0;
            mutex_unlock(&dev_data->data_lock);

            return 0;

        case IOCTL_SET_STR:
            /* Set reference string */

            /* Get the parameter given to ioctl by the process */
            tmp_arg = (char *) arg;
   
            /* Find the length of the string */
            get_user(ch, tmp_arg);
            for (i=0; ch && i < BUF_LEN_STR; i++, tmp_arg++) {
                get_user(ch, tmp_arg);
            }

            tmp_str = kmalloc(i, GFP_KERNEL);
            if (!tmp_str) {
                return -EFAULT;
            }
            memset(tmp_str, 0, i);

            /* Copy string from user */
            if (copy_from_user(tmp_str, (char *)arg, i)) {
                kfree(tmp_str);
                return -EFAULT;
            }

            /* Save reference string in data struct */
            if (!mutex_lock_interruptible(&dev_data->data_lock)) {
                kfree(dev_data->ref_str.reference_str);
                dev_data->ref_str.reference_str_len = i;
                dev_data->ref_str.reference_str = tmp_str;
                mutex_unlock(&dev_data->data_lock);
            }
            else {
                kfree(tmp_str);
            }

            return 0;
        default:
            return -EINVAL;
    }
}

static int device_fasync(int fd, struct file *file, int mode) {
    struct device_data *dev_data = (struct device_data *) file->private_data;

    return fasync_helper(fd, file, mode, &dev_data->async);
}

/*************************************************************/


/*************** Debugfs File Operation ****************/

static ssize_t read_device_debug(struct file *file, char __user *buf, size_t size, loff_t *offset) {
    int database_id = 0;
    struct database *database = NULL;
    int res = kstrtoint(file->f_path.dentry->d_iname, 10, &database_id);
    int len_to_return = 0;
    char *str_to_user = NULL;
    char *tmp = NULL;
    int nbytes = 0;
    int size_to_user = 0;

    if (res) {
        return 0;
    }

    if (mutex_lock_interruptible(&dev_driver.dev_data[database_id].data_lock)) {
        return 0;
    }

    /* Calculate size of all data should be written for user */
    size_to_user = dev_driver.dev_data[database_id].general_data_len
                     + dev_driver.dev_data[database_id].general_str_count - *offset;
    len_to_return = min(size_to_user, (int)size);

    if (len_to_return <= 0) {
        mutex_unlock(&dev_driver.dev_data[database_id].data_lock);
        return 0;
    }

    /* Allocate memory for this size */
    str_to_user = kmalloc(size_to_user, GFP_KERNEL);
    if (!str_to_user) {
        mutex_unlock(&dev_driver.dev_data[database_id].data_lock);
        return 0;
    }
    memset(str_to_user, 0, size_to_user);
    tmp = str_to_user;

    /* Fill the buffer with all strings */
    list_for_each_entry(database, &(dev_driver.dev_data[database_id].database->list), list) {
        snprintf(tmp, database->len + 1, "%s\n", database->saved_str);
        tmp = tmp + database->len + 1;
    }
    mutex_unlock(&dev_driver.dev_data[database_id].data_lock);

    /* Copy buffer to user */
    nbytes = len_to_return - copy_to_user(buf, str_to_user + *offset, len_to_return);
    *offset += nbytes;
    kfree(str_to_user);

    return nbytes;
}

/*************************************************************/


static int __init init_dev(void) {
    dev_t dev;
    int i = 0;
    int err = 0;
    char file_name[10] = {0};

    /* allocate chardev region and assign Major number */
    err = alloc_chrdev_region(&dev, 0, MAX_DEV, MYDEV_NAME);

    if (err < 0) {
        printk(KERN_INFO "Major number allocation is failed\n");
        return err;
    }

    dev_driver.dev_major = MAJOR(dev);
    dev_driver.dirret = debugfs_create_dir(THIS_MODULE->name, NULL);

    /* Create necessary number of the devices */
    for (i = 0; i < MAX_DEV; i++) {
        mutex_init(&dev_driver.dev_data[i].data_lock);

        /* init new device */
        cdev_init(&dev_driver.dev_data[i].cdev, &custom_device_fops);
        dev_driver.dev_data[i].cdev.owner = THIS_MODULE;

        /* init dev database */
        dev_driver.dev_data[i].database = kmalloc(sizeof(struct database), GFP_KERNEL);
        INIT_LIST_HEAD(&(dev_driver.dev_data[i].database->list));

        /* add device to the system where "i" is a Minor number of the new device */
        err = cdev_add(&dev_driver.dev_data[i].cdev, MKDEV(dev_driver.dev_major, i), 1);
        if(err < 0 ) {
            printk(KERN_INFO "Unable to add cdev\n");
            return err;
        }

        dev_driver.dev_data[i].dev_id = i;

        sprintf(file_name, "%i", i);
        dev_driver.files[i] = debugfs_create_file(file_name, 0644, dev_driver.dirret, NULL, &debug_device_fops);
    }
    printk(KERN_INFO "Init module %s\n", THIS_MODULE->name);


    return 0;
}

static void __exit clean_dev(void) {
    struct database *database;
    struct database *tmp;
    int i = 0;

    for (i = 0; i < MAX_DEV; i++) {
        mutex_lock(&dev_driver.dev_data[i].data_lock);
        list_for_each_entry_safe(database, tmp, &(dev_driver.dev_data[i].database->list), list) {
            list_del(&database->list);
            kfree(database->saved_str);
            database->saved_str = NULL;
            kfree(database);
            database = NULL;
        }
        kfree(dev_driver.dev_data[i].ref_str.reference_str);
        dev_driver.dev_data[i].ref_str.reference_str = NULL;
        mutex_unlock(&dev_driver.dev_data[i].data_lock);
    }

    unregister_chrdev_region(MKDEV(dev_driver.dev_major, 0), MAX_DEV);
    debugfs_remove_recursive(dev_driver.dirret); 
    printk(KERN_INFO "Cleanup module %s\n", THIS_MODULE->name);

}

module_init (init_dev);
module_exit (clean_dev);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OLEKSANDR STANISLAVSKYI");