#ifndef CALC_IOCTL_H
#define CALC_IOCTL_H

#include <linux/ioctl.h>

struct calc_data {
	int a;
	int b;
	int op;       /* 0 = add, 1 = sub */
	int result;
};

#define CALC_MAGIC 'C'
#define CALC_OP_ADD 0
#define CALC_OP_SUB 1

#define CALC_COMPUTE _IOWR(CALC_MAGIC, 1, struct calc_data)

#endif
