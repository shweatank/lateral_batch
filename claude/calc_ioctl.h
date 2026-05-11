#ifndef CALC_IOCTL_H
#define CALC_IOCTL_H

#include <linux/ioctl.h>

#define CALC_OP_ADD 0
#define CALC_OP_SUB 1

struct calc_data {
    int num1;
    int num2;
    int op;
    int result;
};

#define CALC_MAGIC 'k'
#define CALC_COMPUTE _IOWR(CALC_MAGIC, 1, struct calc_data)

#endif
