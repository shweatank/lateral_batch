/*
 * user_app.c - User-space companion for /dev/smart_device
 *
 * Three modes of operation, chosen from the command line:
 *
 *   ./user_app                       interactive menu (default)
 *   ./user_app --auto                run a scripted exercise of every
 *                                    ioctl + read/write, exit non-zero
 *                                    on the first failure
 *   ./user_app --stress N M          spawn N pthreads, each performing
 *                                    M mixed operations against the
 *                                    device, used to validate the
 *                                    mutex serialisation in the driver
 *   ./user_app --cmd <name> [arg]    single-shot invocation, designed
 *                                    so shell scripts (test.sh) can
 *                                    drive each ioctl individually
 *
 * Build: see Makefile (`make user`).
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>

#include "smart_ioctl.h"

#define DEV_PATH "/dev/smart_device"

/* ---------------------------------------------------------------- */
/* Tiny wrappers - the kernel may modify errno; capture it before    */
/* anything else can clobber it.                                     */
/* ---------------------------------------------------------------- */
static int dev_open(int flags)
{
	int fd = open(DEV_PATH, flags);
	if (fd < 0)
		fprintf(stderr, "open(%s): %s\n", DEV_PATH, strerror(errno));
	return fd;
}

static const char *mode_name(uint32_t m)
{
	switch (m) {
	case SMART_MODE_NORMAL: return "NORMAL";
	case SMART_MODE_DEBUG:  return "DEBUG";
	case SMART_MODE_TEST:   return "TEST";
	case SMART_MODE_SILENT: return "SILENT";
	default:                return "?";
	}
}

static void print_stats(const struct smart_stats *s)
{
	printf("  opens         = %llu\n", (unsigned long long)s->opens);
	printf("  closes        = %llu\n", (unsigned long long)s->closes);
	printf("  reads         = %llu\n", (unsigned long long)s->reads);
	printf("  writes        = %llu\n", (unsigned long long)s->writes);
	printf("  ioctls        = %llu\n", (unsigned long long)s->ioctls);
	printf("  bytes_read    = %llu\n", (unsigned long long)s->bytes_read);
	printf("  bytes_written = %llu\n", (unsigned long long)s->bytes_written);
	printf("  errors        = %llu\n", (unsigned long long)s->errors);
}

/* ---------------------------------------------------------------- */
/* Single-shot operations - return 0 on success, -1 on failure.      */
/* ---------------------------------------------------------------- */

static int op_write(int fd, const char *data)
{
	ssize_t n = write(fd, data, strlen(data));
	if (n < 0) {
		perror("write");
		return -1;
	}
	printf("wrote %zd bytes: \"%s\"\n", n, data);
	return 0;
}

static int op_read(int fd, size_t n)
{
	char *buf = calloc(1, n + 1);
	ssize_t r;
	if (!buf) return -1;
	lseek(fd, 0, SEEK_SET);
	r = read(fd, buf, n);
	if (r < 0) {
		perror("read");
		free(buf);
		return -1;
	}
	printf("read %zd bytes: \"%s\"\n", r, buf);
	free(buf);
	return 0;
}

static int op_reset(int fd)
{
	if (ioctl(fd, SMART_RESET_DEVICE) < 0) { perror("RESET"); return -1; }
	puts("device reset");
	return 0;
}

static int op_clear(int fd)
{
	if (ioctl(fd, SMART_CLEAR_BUFFER) < 0) { perror("CLEAR_BUFFER"); return -1; }
	puts("buffer cleared");
	return 0;
}

static int op_stats(int fd)
{
	struct smart_stats s;
	if (ioctl(fd, SMART_GET_DRIVER_STATS, &s) < 0) {
		perror("GET_STATS");
		return -1;
	}
	puts("driver stats:");
	print_stats(&s);
	return 0;
}

static int op_set_mode(int fd, uint32_t mode)
{
	if (ioctl(fd, SMART_SET_DEVICE_MODE, &mode) < 0) {
		perror("SET_MODE");
		return -1;
	}
	printf("mode set -> %s\n", mode_name(mode));
	return 0;
}

static int op_get_mode(int fd)
{
	uint32_t m;
	if (ioctl(fd, SMART_GET_DEVICE_MODE, &m) < 0) {
		perror("GET_MODE");
		return -1;
	}
	printf("current mode = %s\n", mode_name(m));
	return 0;
}

static int op_log(int fd, int on)
{
	unsigned long cmd = on ? SMART_ENABLE_LOGGING : SMART_DISABLE_LOGGING;
	if (ioctl(fd, cmd) < 0) {
		perror("LOG toggle");
		return -1;
	}
	printf("logging %s\n", on ? "ENABLED" : "DISABLED");
	return 0;
}

static int op_last(int fd)
{
	struct smart_cmd_entry e;
	if (ioctl(fd, SMART_GET_LAST_COMMAND, &e) < 0) {
		perror("GET_LAST_COMMAND");
		return -1;
	}
	printf("last cmd: nr=%u  result=%d  ts=%llu ns\n",
	       e.cmd_nr, e.result, (unsigned long long)e.timestamp_ns);
	return 0;
}

static int op_set_timeout(int fd, uint32_t ms)
{
	if (ioctl(fd, SMART_SET_TIMEOUT, &ms) < 0) {
		perror("SET_TIMEOUT");
		return -1;
	}
	printf("timeout set -> %u ms\n", ms);
	return 0;
}

static int op_get_timeout(int fd)
{
	uint32_t ms;
	if (ioctl(fd, SMART_GET_TIMEOUT, &ms) < 0) {
		perror("GET_TIMEOUT");
		return -1;
	}
	printf("current timeout = %u ms\n", ms);
	return 0;
}

/* ---------------------------------------------------------------- */
/* Interactive menu                                                  */
/* ---------------------------------------------------------------- */
static void menu_loop(int fd)
{
	char line[64];

	for (;;) {
		printf("\n----- /dev/smart_device menu -----\n"
		       " 1) write             6) get mode\n"
		       " 2) read              7) enable logging\n"
		       " 3) reset device      8) disable logging\n"
		       " 4) clear buffer      9) get last command\n"
		       " 5) set mode         10) set timeout\n"
		       " s) stats            11) get timeout\n"
		       " q) quit\n"
		       "> ");
		if (!fgets(line, sizeof(line), stdin)) break;

		/* Trim trailing newline. */
		size_t L = strlen(line);
		if (L && line[L - 1] == '\n') line[--L] = 0;
		if (L == 0) continue;

		if (line[0] == 'q' || line[0] == 'Q') return;
		if (line[0] == 's' || line[0] == 'S') { op_stats(fd); continue; }

		int choice = atoi(line);
		switch (choice) {
		case 1: {
			char data[256];
			printf("payload> ");
			if (fgets(data, sizeof(data), stdin)) {
				size_t l = strlen(data);
				if (l && data[l - 1] == '\n') data[l - 1] = 0;
				op_write(fd, data);
			}
			break;
		}
		case 2: op_read(fd, 256); break;
		case 3: op_reset(fd); break;
		case 4: op_clear(fd); break;
		case 5: {
			unsigned int m;
			printf("mode (0=NORMAL,1=DEBUG,2=TEST,3=SILENT)> ");
			if (scanf("%u%*c", &m) == 1) op_set_mode(fd, m);
			break;
		}
		case 6:  op_get_mode(fd); break;
		case 7:  op_log(fd, 1); break;
		case 8:  op_log(fd, 0); break;
		case 9:  op_last(fd); break;
		case 10: {
			unsigned int t;
			printf("timeout ms> ");
			if (scanf("%u%*c", &t) == 1) op_set_timeout(fd, t);
			break;
		}
		case 11: op_get_timeout(fd); break;
		default: puts("?");
		}
	}
}

/* ---------------------------------------------------------------- */
/* Automated end-to-end exercise of every operation                  */
/* ---------------------------------------------------------------- */
static int auto_mode(void)
{
	int fd = dev_open(O_RDWR);
	if (fd < 0) return 1;

	int rc = 0;
	rc |= op_reset(fd);
	rc |= op_log(fd, 1);
	rc |= op_set_mode(fd, SMART_MODE_NORMAL);
	rc |= op_get_mode(fd);
	rc |= op_set_timeout(fd, 2500);
	rc |= op_get_timeout(fd);
	rc |= op_write(fd, "hello smart_device");
	rc |= op_read(fd, 256);
	rc |= op_clear(fd);
	rc |= op_read(fd, 256);          /* should be EOF (0 bytes) */
	rc |= op_last(fd);
	rc |= op_stats(fd);
	rc |= op_log(fd, 0);

	close(fd);
	puts(rc == 0 ? "\nAUTO: PASS" : "\nAUTO: FAIL");
	return rc == 0 ? 0 : 1;
}

/* ---------------------------------------------------------------- */
/* Stress mode - many threads, mixed operations                      */
/* ---------------------------------------------------------------- */
struct stress_ctx {
	int      iterations;
	int      tid;
	uint64_t errors;
};

static void *stress_worker(void *arg)
{
	struct stress_ctx *ctx = arg;
	int fd = dev_open(O_RDWR);
	if (fd < 0) { ctx->errors++; return NULL; }

	for (int i = 0; i < ctx->iterations; i++) {
		struct smart_stats s;
		uint32_t v;
		char buf[64];
		int len = snprintf(buf, sizeof(buf),
				   "t%d-i%d", ctx->tid, i);

		switch (i & 7) {
		case 0:
			if (write(fd, buf, len) != len) ctx->errors++;
			break;
		case 1:
			lseek(fd, 0, SEEK_SET);
			if (read(fd, buf, sizeof(buf)) < 0) ctx->errors++;
			break;
		case 2:
			if (ioctl(fd, SMART_GET_DRIVER_STATS, &s) < 0)
				ctx->errors++;
			break;
		case 3:
			if (ioctl(fd, SMART_GET_DEVICE_MODE, &v) < 0)
				ctx->errors++;
			break;
		case 4:
			if (ioctl(fd, SMART_GET_TIMEOUT, &v) < 0)
				ctx->errors++;
			break;
		case 5:
			v = (i % SMART_MODE_MAX);
			if (ioctl(fd, SMART_SET_DEVICE_MODE, &v) < 0)
				ctx->errors++;
			break;
		case 6:
			if (ioctl(fd, SMART_CLEAR_BUFFER) < 0)
				ctx->errors++;
			break;
		case 7:
			v = 100 + (i % 5000);
			if (ioctl(fd, SMART_SET_TIMEOUT, &v) < 0)
				ctx->errors++;
			break;
		}
	}

	close(fd);
	return NULL;
}

static int stress_mode(int threads, int iters)
{
	if (threads <= 0) threads = 4;
	if (iters   <= 0) iters   = 1000;

	pthread_t          *th  = calloc(threads, sizeof(*th));
	struct stress_ctx  *ctx = calloc(threads, sizeof(*ctx));
	if (!th || !ctx) { perror("calloc"); return 1; }

	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);

	for (int i = 0; i < threads; i++) {
		ctx[i].tid        = i;
		ctx[i].iterations = iters;
		pthread_create(&th[i], NULL, stress_worker, &ctx[i]);
	}

	uint64_t total_errors = 0;
	for (int i = 0; i < threads; i++) {
		pthread_join(th[i], NULL);
		total_errors += ctx[i].errors;
	}

	clock_gettime(CLOCK_MONOTONIC, &t1);
	double sec = (t1.tv_sec - t0.tv_sec) +
		     (t1.tv_nsec - t0.tv_nsec) / 1e9;

	printf("\nSTRESS: %d threads x %d iters = %d ops in %.2fs (%.0f ops/s), errors=%llu\n",
	       threads, iters, threads * iters, sec,
	       (threads * iters) / sec, (unsigned long long)total_errors);

	free(th); free(ctx);
	puts(total_errors == 0 ? "STRESS: PASS" : "STRESS: FAIL");
	return total_errors == 0 ? 0 : 1;
}

/* ---------------------------------------------------------------- */
/* Single-shot mode (for shell scripting / test.sh)                  */
/* ---------------------------------------------------------------- */
static int cmd_mode(int argc, char **argv)
{
	if (argc < 1) { fprintf(stderr, "--cmd needs a name\n"); return 1; }
	const char *name = argv[0];

	int fd = dev_open(O_RDWR);
	if (fd < 0) return 1;
	int rc = 0;

	if      (!strcmp(name, "reset"))        rc = op_reset(fd);
	else if (!strcmp(name, "clear"))        rc = op_clear(fd);
	else if (!strcmp(name, "stats"))        rc = op_stats(fd);
	else if (!strcmp(name, "get-mode"))     rc = op_get_mode(fd);
	else if (!strcmp(name, "get-timeout"))  rc = op_get_timeout(fd);
	else if (!strcmp(name, "last"))         rc = op_last(fd);
	else if (!strcmp(name, "log-on"))       rc = op_log(fd, 1);
	else if (!strcmp(name, "log-off"))      rc = op_log(fd, 0);
	else if (!strcmp(name, "set-mode") && argc >= 2)
		rc = op_set_mode(fd, (uint32_t)atoi(argv[1]));
	else if (!strcmp(name, "set-timeout") && argc >= 2)
		rc = op_set_timeout(fd, (uint32_t)atoi(argv[1]));
	else if (!strcmp(name, "write") && argc >= 2)
		rc = op_write(fd, argv[1]);
	else if (!strcmp(name, "read"))
		rc = op_read(fd, argc >= 2 ? (size_t)atoi(argv[1]) : 256);
	else {
		fprintf(stderr, "unknown cmd \"%s\"\n", name);
		rc = 2;
	}

	close(fd);
	return rc == 0 ? 0 : 1;
}

/* ---------------------------------------------------------------- */
/* Main                                                              */
/* ---------------------------------------------------------------- */
int main(int argc, char **argv)
{
	if (argc >= 2 && !strcmp(argv[1], "--auto"))
		return auto_mode();

	if (argc >= 2 && !strcmp(argv[1], "--stress"))
		return stress_mode(argc >= 3 ? atoi(argv[2]) : 4,
				   argc >= 4 ? atoi(argv[3]) : 1000);

	if (argc >= 2 && !strcmp(argv[1], "--cmd"))
		return cmd_mode(argc - 2, argv + 2);

	if (argc >= 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
		printf("usage:\n"
		       "  %s                   interactive menu\n"
		       "  %s --auto            scripted exercise of all ops\n"
		       "  %s --stress N M      N threads, M ops each\n"
		       "  %s --cmd <name> ...  single ioctl (for scripts)\n",
		       argv[0], argv[0], argv[0], argv[0]);
		return 0;
	}

	int fd = dev_open(O_RDWR);
	if (fd < 0) return 1;
	menu_loop(fd);
	close(fd);
	return 0;
}
