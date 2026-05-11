/*
 * user_app.c
 * ----------
 * User-space companion for the smart_device kernel driver.
 *
 * Modes:
 *   --menu       (default) interactive menu
 *   --auto       run every IOCTL once, print pass/fail
 *   --stress N   open / close / read / write / ioctl loop, N iterations
 *   --help
 *
 * Compile:
 *   gcc -Wall -Wextra -O2 user_app.c -o smart_app
 *
 * Run:
 *   sudo ./smart_app
 *
 * Why sudo? /dev/smart_device is owned by root by default. You can
 * loosen the perms with udev rules; for development we just run with
 * sudo.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#include "smart_ioctl.h"

#define DEVICE_PATH "/dev/" SMART_DEVICE_NAME

/* ---------- tiny output helpers ---------- */
static const char *C_RED   = "\033[31m";
static const char *C_GRN   = "\033[32m";
static const char *C_YEL   = "\033[33m";
static const char *C_CYN   = "\033[36m";
static const char *C_RST   = "\033[0m";

static void banner(const char *t)
{
	printf("\n%s==== %s ====%s\n", C_CYN, t, C_RST);
}
static void ok(const char *t)
{
	printf("  %s[ OK ]%s %s\n", C_GRN, C_RST, t);
}
static void fail(const char *t)
{
	printf("  %s[FAIL]%s %s (errno=%d %s)\n",
	       C_RED, C_RST, t, errno, strerror(errno));
}
static void info(const char *t)
{
	printf("  %s[ .. ]%s %s\n", C_YEL, C_RST, t);
}

/* ---------- IOCTL wrappers ---------- */
static int do_reset(int fd)            { return ioctl(fd, SMART_IOCTL_RESET_DEVICE); }
static int do_clear(int fd)            { return ioctl(fd, SMART_IOCTL_CLEAR_BUFFER); }
static int do_enable_log(int fd)       { return ioctl(fd, SMART_IOCTL_ENABLE_LOGGING); }
static int do_disable_log(int fd)      { return ioctl(fd, SMART_IOCTL_DISABLE_LOGGING); }

static int do_get_stats(int fd, struct smart_stats *out)
{
	return ioctl(fd, SMART_IOCTL_GET_STATS, out);
}
static int do_set_mode(int fd, int32_t m)
{
	return ioctl(fd, SMART_IOCTL_SET_MODE, &m);
}
static int do_get_mode(int fd, int32_t *m)
{
	return ioctl(fd, SMART_IOCTL_GET_MODE, m);
}
static int do_get_last(int fd, struct smart_cmd_info *out)
{
	return ioctl(fd, SMART_IOCTL_GET_LAST_CMD, out);
}
static int do_set_timeout(int fd, uint32_t t)
{
	return ioctl(fd, SMART_IOCTL_SET_TIMEOUT, &t);
}
static int do_get_timeout(int fd, uint32_t *t)
{
	return ioctl(fd, SMART_IOCTL_GET_TIMEOUT, t);
}

static const char *mode_name(int32_t m)
{
	switch (m) {
	case SMART_MODE_NORMAL: return "NORMAL";
	case SMART_MODE_DEBUG:  return "DEBUG";
	case SMART_MODE_PERF:   return "PERF";
	case SMART_MODE_SAFE:   return "SAFE";
	default:                return "UNKNOWN";
	}
}

static void print_stats(const struct smart_stats *s)
{
	printf("    open_count    = %lu\n", (unsigned long)s->open_count);
	printf("    close_count   = %lu\n", (unsigned long)s->close_count);
	printf("    read_count    = %lu\n", (unsigned long)s->read_count);
	printf("    write_count   = %lu\n", (unsigned long)s->write_count);
	printf("    ioctl_count   = %lu\n", (unsigned long)s->ioctl_count);
	printf("    bytes_read    = %lu\n", (unsigned long)s->bytes_read);
	printf("    bytes_written = %lu\n", (unsigned long)s->bytes_written);
	printf("    error_count   = %lu\n", (unsigned long)s->error_count);
}

/* =====================================================================
 *  AUTOMATED MODE — runs every IOCTL once, reports pass/fail.
 *  Designed to be used by test.sh and CI.
 * =====================================================================
 */
static int run_auto(void)
{
	int fd, rc, fails = 0;
	struct smart_stats stats;
	struct smart_cmd_info last;
	int32_t  mode;
	uint32_t t;
	const char *payload = "Hello from user_app auto-test!";
	char rbuf[128];
	ssize_t n;

	banner("AUTOMATED TEST");

	fd = open(DEVICE_PATH, O_RDWR);
	if (fd < 0) { fail("open"); return 1; }
	ok("open");

	/* --- write ---------------------------------------------------- */
	n = write(fd, payload, strlen(payload));
	if (n != (ssize_t)strlen(payload)) { fail("write"); fails++; }
	else ok("write");

	/* --- read back ----------------------------------------------- */
	lseek(fd, 0, SEEK_SET);
	memset(rbuf, 0, sizeof(rbuf));
	n = read(fd, rbuf, sizeof(rbuf) - 1);
	if (n < 0 || strncmp(rbuf, payload, strlen(payload)) != 0) {
		fail("read");
		fails++;
	} else {
		ok("read (data matches what we wrote)");
	}

	/* --- ENABLE / DISABLE LOGGING -------------------------------- */
	if (do_enable_log(fd))  { fail("ENABLE_LOGGING");  fails++; } else ok("ENABLE_LOGGING");
	if (do_disable_log(fd)) { fail("DISABLE_LOGGING"); fails++; } else ok("DISABLE_LOGGING");
	if (do_enable_log(fd))  { fail("re-ENABLE_LOGGING"); fails++; } else ok("re-ENABLE_LOGGING");

	/* --- SET_MODE / GET_MODE ------------------------------------- */
	for (int32_t m = SMART_MODE_NORMAL; m < SMART_MODE_MAX; m++) {
		if (do_set_mode(fd, m))               { fail("SET_MODE");  fails++; continue; }
		if (do_get_mode(fd, &mode) || mode != m) { fail("GET_MODE"); fails++; continue; }
		printf("       mode set/got round-trip: %s\n", mode_name(mode));
	}
	ok("SET_MODE / GET_MODE round-trip");

	/* invalid mode — should be rejected */
	rc = do_set_mode(fd, 999);
	if (rc == 0) { fail("SET_MODE(999) accepted (BAD)"); fails++; }
	else         { ok("SET_MODE(999) correctly rejected (EINVAL)"); }

	/* --- SET_TIMEOUT / GET_TIMEOUT ------------------------------- */
	if (do_set_timeout(fd, 2500)) { fail("SET_TIMEOUT"); fails++; }
	else if (do_get_timeout(fd, &t) || t != 2500) { fail("GET_TIMEOUT"); fails++; }
	else printf("       timeout = %u ms (round-trip OK)\n", t);

	/* invalid timeout — 0 must be rejected */
	rc = do_set_timeout(fd, 0);
	if (rc == 0) { fail("SET_TIMEOUT(0) accepted (BAD)"); fails++; }
	else         { ok("SET_TIMEOUT(0) correctly rejected (EINVAL)"); }

	/* --- GET_STATS ------------------------------------------------ */
	if (do_get_stats(fd, &stats)) { fail("GET_STATS"); fails++; }
	else { ok("GET_STATS"); print_stats(&stats); }

	/* --- GET_LAST_CMD --------------------------------------------- */
	if (do_get_last(fd, &last)) { fail("GET_LAST_CMD"); fails++; }
	else {
		printf("       last_cmd=0x%x desc=\"%s\" t=%u\n",
		       last.last_cmd, last.description, last.timestamp);
		ok("GET_LAST_CMD");
	}

	/* --- CLEAR_BUFFER -------------------------------------------- */
	if (do_clear(fd)) { fail("CLEAR_BUFFER"); fails++; } else ok("CLEAR_BUFFER");

	/* read after clear should now return 0 (EOF) */
	lseek(fd, 0, SEEK_SET);
	n = read(fd, rbuf, sizeof(rbuf));
	if (n != 0) { fail("read after CLEAR_BUFFER should be 0"); fails++; }
	else        ok("read after CLEAR_BUFFER returned EOF");

	/* --- bogus ioctl: must return ENOTTY -------------------------- */
	rc = ioctl(fd, _IO('Z', 99));
	if (rc == 0)        { fail("bogus ioctl accepted!"); fails++; }
	else if (errno != ENOTTY) { fail("bogus ioctl gave wrong errno"); fails++; }
	else                ok("bogus ioctl correctly rejected (ENOTTY)");

	/* --- RESET_DEVICE clears everything --------------------------- */
	if (do_reset(fd)) { fail("RESET_DEVICE"); fails++; } else ok("RESET_DEVICE");

	close(fd);
	ok("close");

	banner(fails ? "AUTO TEST: FAILED" : "AUTO TEST: PASSED");
	printf("Failures: %d\n", fails);
	return fails ? 1 : 0;
}

/* =====================================================================
 *  STRESS MODE — N threads each do M iterations of mixed operations.
 *  Exercises the mutex; if locking is wrong, dmesg will scream OOPS.
 * =====================================================================
 */
struct stress_arg {
	int      id;
	int      iters;
	int      failures;
};

static void *stress_worker(void *p)
{
	struct stress_arg *a = p;
	int fd = open(DEVICE_PATH, O_RDWR);
	if (fd < 0) { a->failures++; return NULL; }

	for (int i = 0; i < a->iters; i++) {
		char buf[64];
		int n = snprintf(buf, sizeof(buf), "T%d-i%d", a->id, i);
		if (write(fd, buf, n) != n)               a->failures++;
		lseek(fd, 0, SEEK_SET);
		char rb[64];
		if (read(fd, rb, sizeof(rb)) < 0)         a->failures++;
		struct smart_stats s;
		if (do_get_stats(fd, &s))                 a->failures++;
		if (do_set_mode(fd, i % SMART_MODE_MAX))  a->failures++;
		if (do_clear(fd))                         a->failures++;
	}
	close(fd);
	return NULL;
}

static int run_stress(int iters)
{
	enum { THREADS = 8 };
	pthread_t  th[THREADS];
	struct stress_arg args[THREADS];
	int total_fail = 0;
	struct timespec t0, t1;

	banner("STRESS TEST");
	printf("  threads=%d  iters/thread=%d\n", THREADS, iters);

	clock_gettime(CLOCK_MONOTONIC, &t0);

	for (int i = 0; i < THREADS; i++) {
		args[i].id = i;
		args[i].iters = iters;
		args[i].failures = 0;
		pthread_create(&th[i], NULL, stress_worker, &args[i]);
	}
	for (int i = 0; i < THREADS; i++) {
		pthread_join(th[i], NULL);
		total_fail += args[i].failures;
	}

	clock_gettime(CLOCK_MONOTONIC, &t1);
	double dt = (t1.tv_sec - t0.tv_sec) +
		    (t1.tv_nsec - t0.tv_nsec) / 1e9;

	printf("  Total ops: %d   Failures: %d   Wall-time: %.3fs\n",
	       THREADS * iters * 5, total_fail, dt);
	banner(total_fail ? "STRESS: FAIL" : "STRESS: PASS");
	return total_fail ? 1 : 0;
}

/* =====================================================================
 *  MENU MODE — humans
 * =====================================================================
 */
static void menu_print(void)
{
	printf("\n"
	"  +------ smart_device interactive menu ------+\n"
	"  |  1)  open  /dev/%s                |\n"
	"  |  2)  close                                |\n"
	"  |  3)  write  string                        |\n"
	"  |  4)  read   (up to 128 bytes)             |\n"
	"  |  5)  RESET_DEVICE                         |\n"
	"  |  6)  CLEAR_BUFFER                         |\n"
	"  |  7)  GET_STATS                            |\n"
	"  |  8)  SET_MODE  (0=NORM 1=DBG 2=PERF 3=SAFE) |\n"
	"  |  9)  GET_MODE                             |\n"
	"  | 10)  ENABLE_LOGGING                       |\n"
	"  | 11)  DISABLE_LOGGING                      |\n"
	"  | 12)  GET_LAST_CMD                         |\n"
	"  | 13)  SET_TIMEOUT (ms)                     |\n"
	"  | 14)  GET_TIMEOUT                          |\n"
	"  |  0)  quit                                 |\n"
	"  +-------------------------------------------+\n"
	"  >  ", SMART_DEVICE_NAME);
}

static int run_menu(void)
{
	int fd = -1, choice;
	char line[256];

	for (;;) {
		menu_print();
		if (!fgets(line, sizeof(line), stdin)) break;
		if (sscanf(line, "%d", &choice) != 1)  continue;

		switch (choice) {
		case 0:
			if (fd >= 0) close(fd);
			return 0;
		case 1:
			if (fd >= 0) { info("already open"); break; }
			fd = open(DEVICE_PATH, O_RDWR);
			if (fd < 0) fail("open"); else ok("opened");
			break;
		case 2:
			if (fd < 0) { info("not open"); break; }
			close(fd); fd = -1; ok("closed");
			break;
		case 3: {
			if (fd < 0) { info("open first"); break; }
			printf("    string > "); fflush(stdout);
			if (!fgets(line, sizeof(line), stdin)) break;
			line[strcspn(line, "\n")] = 0;
			ssize_t n = write(fd, line, strlen(line));
			if (n < 0) fail("write");
			else printf("    wrote %zd bytes\n", n);
			break;
		}
		case 4: {
			if (fd < 0) { info("open first"); break; }
			char rb[129] = {0};
			lseek(fd, 0, SEEK_SET);
			ssize_t n = read(fd, rb, 128);
			if (n < 0) fail("read");
			else printf("    read %zd bytes: \"%s\"\n", n, rb);
			break;
		}
		case 5: if (fd >= 0 && !do_reset(fd))   ok("RESET");   else fail("RESET");        break;
		case 6: if (fd >= 0 && !do_clear(fd))   ok("CLEAR");   else fail("CLEAR");        break;
		case 7: {
			struct smart_stats s;
			if (fd >= 0 && !do_get_stats(fd, &s)) { ok("GET_STATS"); print_stats(&s); }
			else fail("GET_STATS");
			break;
		}
		case 8: {
			int m;
			printf("    mode > "); fflush(stdout);
			if (scanf("%d", &m) == 1) { getchar();
				if (fd >= 0 && !do_set_mode(fd, m)) ok("SET_MODE");
				else fail("SET_MODE");
			}
			break;
		}
		case 9: {
			int32_t m;
			if (fd >= 0 && !do_get_mode(fd, &m))
				printf("    mode = %d (%s)\n", m, mode_name(m));
			else fail("GET_MODE");
			break;
		}
		case 10: if (fd>=0 && !do_enable_log(fd))  ok("logging ON");  else fail("ENABLE");  break;
		case 11: if (fd>=0 && !do_disable_log(fd)) ok("logging OFF"); else fail("DISABLE"); break;
		case 12: {
			struct smart_cmd_info l;
			if (fd >= 0 && !do_get_last(fd, &l))
				printf("    last: cmd=0x%x \"%s\" t=%u\n",
				       l.last_cmd, l.description, l.timestamp);
			else fail("GET_LAST_CMD");
			break;
		}
		case 13: {
			unsigned t;
			printf("    timeout ms > "); fflush(stdout);
			if (scanf("%u", &t) == 1) { getchar();
				if (fd >= 0 && !do_set_timeout(fd, t)) ok("SET_TIMEOUT");
				else fail("SET_TIMEOUT");
			}
			break;
		}
		case 14: {
			uint32_t t;
			if (fd >= 0 && !do_get_timeout(fd, &t))
				printf("    timeout = %u ms\n", t);
			else fail("GET_TIMEOUT");
			break;
		}
		default:
			info("unknown choice");
		}
	}
	if (fd >= 0) close(fd);
	return 0;
}

/* ---------- main ---------- */
int main(int argc, char **argv)
{
	if (argc == 1)
		return run_menu();

	if (!strcmp(argv[1], "--menu"))
		return run_menu();

	if (!strcmp(argv[1], "--auto"))
		return run_auto();

	if (!strcmp(argv[1], "--stress")) {
		int n = (argc > 2) ? atoi(argv[2]) : 100;
		if (n <= 0) n = 100;
		return run_stress(n);
	}

	if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
		printf("Usage: %s [--menu | --auto | --stress N | --help]\n",
		       argv[0]);
		return 0;
	}

	fprintf(stderr, "Unknown option: %s (try --help)\n", argv[1]);
	return 2;
}
