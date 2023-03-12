#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <linux/ioctl.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <libgen.h>
#include <stdlib.h>
#include "cdev_ioctl.h"


#define OPTSTR "tf:s:dih"
#define BUF_LEN_STR 200
#define DEFAULT_DEVICE_NAME "/dev/custom_device0"
#define DEFAULT_REFERENCE_STRING "123456"

const static char *usage = 
              "This is simple tool for test and control custom device driver\n\n"
              "Usage: ./device_app -d -t -f [file_name] -i -s [reference_str] -h\n\n"
              "Flags:\n"
              "-d : Use this flag to delete all strings in device\n"
              "-f : Set file name, default is </dev/custom_device0\n"
              "-s : Set reference string, default is <123456>\n"
              "-t : Run test mode. Wait 10 sec for signal. Used in fasync test\n"
              "-i : Interactive mode. Wait for signal until user input\n";


static int flag = 0;


void sigc(int sig)
{
  printf("!!!!!!!!!!!!!  NEW STRING MATCH REFERENCE !!!!!!!!!!!!!\n\n\n");
  flag = 2;
}


int ioctl_set_msg(int file_desc, char *message)
{
  int ret_val;

  ret_val = ioctl(file_desc, IOCTL_SET_STR, message);

  if (ret_val < 0) {
      printf ("ioctl_set_msg failed:%d\n", ret_val);
      return 1;
  }

  return 0;
}


int ioctl_del_str(int file_desc)
{
  int ret_val;

  ret_val = ioctl(file_desc, IOCTL_DEL_DTB);

  if (ret_val < 0) {
      printf ("ioctl_set_msg failed:%d\n", ret_val);
      return 1;
  }

  return 0;
}


int main(int argc, char **argv) {
  int file_desc = 0;
  char msg[BUF_LEN_STR] = {0};
  char file_name[BUF_LEN_STR] = {0};
  int c = 0;
  int i = 0;
  int delete = 0;
  char ch = 0;
  int oflags = 0;
  int test = 0;
  int sec = 0;

  memcpy(msg, DEFAULT_REFERENCE_STRING, strlen(DEFAULT_REFERENCE_STRING));
  memcpy(file_name, DEFAULT_DEVICE_NAME, strlen(DEFAULT_DEVICE_NAME));


  opterr = 0;


  while ((c = getopt (argc, argv, OPTSTR)) != -1)
  switch (c)
    {
    case 's':
      strncpy(msg, optarg, BUF_LEN_STR);
      break;
    case 'f':
      strncpy(file_name, optarg, BUF_LEN_STR);
      break;
    case 'i':
      i = 1;
      break;
    case 'd':
      delete = 1;
      break;
    case 't':
      test = 1;
      break;
    case 'h':
      printf("%s", usage);
      exit(0);
      break;
    default:
      break;
    }

  printf("%s:> Set Reference str = %s\n", basename(argv[0]), msg);
  printf("%s:> Set file_name = %s\n",  basename(argv[0]), file_name);

  file_desc = open(file_name, 0);
  if (file_desc < 0) {
      printf ("Can't open device file: %s\n", 
              file_name);
      return 1;
  }

  ioctl_set_msg(file_desc, msg);

  signal(SIGIO, sigc);
  fcntl(file_desc, F_SETOWN, getpid());
  oflags = fcntl(file_desc, F_GETFL);
  fcntl(file_desc, F_SETFL, oflags | FASYNC);


  if (i) {
    printf("%s:> --- RUN INTERACTIVE MODE ---\n",  basename(argv[0]));
    printf("%s:> Procces wait for signal ...\n",  basename(argv[0]));
    printf("%s:> Print 'q and press \"ENTER\" to exit\n",  basename(argv[0]));
    while (((ch != 'q') && (ch != EOF))) {
      ch = getchar();
    }
  }

  if (test) {
      printf("%s:> --- RUN TEST MODE ---\n", basename(argv[0]));
      while ((sec < 10) && (flag != 2)) {
        sec++;
        printf("%s:> Wait for signal...\n", basename(argv[0]));
        sleep(1);
      }
  }

  if (delete) {
    printf("%s:> Delete database\n", basename(argv[0]));
    ioctl_del_str(file_desc);
  }

  close(file_desc);
  printf("-------- Exit from %s -------\n\n", basename(argv[0]));
  return flag;
}