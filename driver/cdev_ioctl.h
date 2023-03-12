#include <linux/ioctl.h>

#define BUF_LEN_STR 200

#define MYDBASE 'k'
#define IOCTL_DEL_DTB _IO(MYDBASE, 1)
#define IOCTL_SET_STR _IOR(MYDBASE, 0, char *)