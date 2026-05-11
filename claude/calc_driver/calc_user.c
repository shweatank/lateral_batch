#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "calc_ioctl.h"

#define DEV_PATH "/dev/calc_dev"

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s <num1> <num2> <add|sub>\n", prog);
}

int main(int argc, char **argv)
{
	int fd;
	struct calc_data data;

	if (argc != 4) {
		usage(argv[0]);
		return 1;
	}

	data.a = atoi(argv[1]);
	data.b = atoi(argv[2]);

	if (!strcmp(argv[3], "add"))
		data.op = CALC_OP_ADD;
	else if (!strcmp(argv[3], "sub"))
		data.op = CALC_OP_SUB;
	else {
		usage(argv[0]);
		return 1;
	}

	fd = open(DEV_PATH, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", DEV_PATH, strerror(errno));
		return 1;
	}

	if (ioctl(fd, CALC_COMPUTE, &data) < 0) {
		fprintf(stderr, "ioctl failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("%d %s %d = %d\n", data.a,
	       data.op == CALC_OP_ADD ? "+" : "-",
	       data.b, data.result);

	close(fd);
	return 0;
}
