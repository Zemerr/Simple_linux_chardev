#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/spinlock.h>


#define MAX_BUF (size_t)((10) * PAGE_SIZE)
#define MYDEV_NAME "custom_device"
#define DEBUG 1

#define DATABASE_EMPTY "Database is empty\n"
// max Minor devices
#define MAX_DEV 2

struct database {
    char *saved_str;
    int len;
    struct list_head list;
};

struct ref_str {
    char *reference_str;
    int reference_str_len;
};

struct device_data {
    struct cdev cdev;
    struct fasync_struct *async;

    // string database
    struct database *database;

    //reference string
    struct ref_str ref_str;

    struct mutex data_lock;
    int general_data_len;
    int general_str_count;
    int dev_id;
};


struct custom_device_driver {
    // device Major number
    int dev_major;

    // array of device data
    struct device_data dev_data[MAX_DEV];

    // debugfs entry
    struct dentry *dirret;
    struct dentry *files[MAX_DEV];
};



