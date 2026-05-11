/*
 * Test suite for /dev/calc
 *
 * Covers:
 *   - Positive, negative, zero operands for ADD and SUB
 *   - Boundary values (INT_MAX / INT_MIN)
 *   - Invalid op code  -> -EINVAL
 *   - Invalid ioctl cmd -> -ENOTTY
 *   - Bad userspace pointer -> -EFAULT
 *   - Repeated ops on a single fd
 *   - Concurrent opens (two fds doing ops back-to-back)
 *
 * Build: see Makefile (`make test`)
 * Run:   sudo ./calc_test   (needs r/w access to /dev/calc)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>
#include "calc_ioctl.h"

#define DEVICE_PATH "/dev/calc"

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond, fmt, ...) do {                                      \
    tests_run++;                                                        \
    if (!(cond)) {                                                      \
        tests_failed++;                                                 \
        fprintf(stderr, "  FAIL [%s:%d]: " fmt "\n",                    \
                __func__, __LINE__, ##__VA_ARGS__);                     \
    } else {                                                            \
        printf("  ok   [%s] " fmt "\n", __func__, ##__VA_ARGS__);       \
    }                                                                   \
} while (0)

static int do_op(int fd, int a, int b, int op, int *result_out)
{
    struct calc_data d = { .num1 = a, .num2 = b, .op = op, .result = 0 };
    int ret = ioctl(fd, CALC_COMPUTE, &d);
    if (ret == 0 && result_out)
        *result_out = d.result;
    return ret;
}

/* ------------------------------------------------------------------ */

static void test_basic_add(int fd)
{
    int r;
    CHECK(do_op(fd, 2, 3, CALC_OP_ADD, &r) == 0 && r == 5,
          "2 + 3 == 5 (got %d)", r);
    CHECK(do_op(fd, 100, 250, CALC_OP_ADD, &r) == 0 && r == 350,
          "100 + 250 == 350 (got %d)", r);
}

static void test_basic_sub(int fd)
{
    int r;
    CHECK(do_op(fd, 10, 4, CALC_OP_SUB, &r) == 0 && r == 6,
          "10 - 4 == 6 (got %d)", r);
    CHECK(do_op(fd, 0, 7, CALC_OP_SUB, &r) == 0 && r == -7,
          "0 - 7 == -7 (got %d)", r);
}

static void test_negatives_and_zero(int fd)
{
    int r;
    CHECK(do_op(fd, -5, -3, CALC_OP_ADD, &r) == 0 && r == -8,
          "-5 + -3 == -8 (got %d)", r);
    CHECK(do_op(fd, -5, -3, CALC_OP_SUB, &r) == 0 && r == -2,
          "-5 - -3 == -2 (got %d)", r);
    CHECK(do_op(fd, 0, 0, CALC_OP_ADD, &r) == 0 && r == 0,
          "0 + 0 == 0 (got %d)", r);
    CHECK(do_op(fd, 0, 0, CALC_OP_SUB, &r) == 0 && r == 0,
          "0 - 0 == 0 (got %d)", r);
}

static void test_boundaries(int fd)
{
    int r;
    /* Driver uses plain int arithmetic — overflow wraps mod 2^32.
     * We just assert the driver returns successfully and matches
     * the same wrap behavior the userspace sees.
     */
    CHECK(do_op(fd, INT_MAX, 1, CALC_OP_ADD, &r) == 0
          && r == (int)((unsigned)INT_MAX + 1u),
          "INT_MAX + 1 wraps consistently (got %d)", r);
    CHECK(do_op(fd, INT_MIN, 1, CALC_OP_SUB, &r) == 0
          && r == (int)((unsigned)INT_MIN - 1u),
          "INT_MIN - 1 wraps consistently (got %d)", r);
}

static void test_invalid_op(int fd)
{
    /* Anything other than CALC_OP_ADD (0) / CALC_OP_SUB (1) -> -EINVAL */
    int ret = do_op(fd, 1, 2, 42, NULL);
    CHECK(ret == -1 && errno == EINVAL,
          "op=42 returns -EINVAL (ret=%d errno=%d)", ret, errno);
}

static void test_invalid_ioctl(int fd)
{
    /* A bogus ioctl number must return -ENOTTY (matches driver default) */
    int bogus_cmd = _IO('z', 99);
    int ret = ioctl(fd, bogus_cmd, 0);
    CHECK(ret == -1 && errno == ENOTTY,
          "bogus ioctl returns -ENOTTY (ret=%d errno=%d)", ret, errno);
}

static void test_bad_pointer(int fd)
{
    /* Passing a deliberately invalid userspace pointer must -> -EFAULT */
    int ret = ioctl(fd, CALC_COMPUTE, (void *)0x1);
    CHECK(ret == -1 && errno == EFAULT,
          "bad user pointer returns -EFAULT (ret=%d errno=%d)", ret, errno);
}

static void test_many_ops_one_fd(int fd)
{
    int r, ok = 1;
    for (int i = 0; i < 1000; i++) {
        if (do_op(fd, i, i, CALC_OP_ADD, &r) != 0 || r != i * 2) {
            ok = 0;
            break;
        }
    }
    CHECK(ok, "1000 sequential ADDs on one fd all correct");
}

static void test_two_fds(void)
{
    int fd1 = open(DEVICE_PATH, O_RDWR);
    int fd2 = open(DEVICE_PATH, O_RDWR);
    CHECK(fd1 >= 0 && fd2 >= 0,
          "open two fds simultaneously (fd1=%d fd2=%d)", fd1, fd2);

    if (fd1 >= 0 && fd2 >= 0) {
        int r1, r2;
        int ok1 = do_op(fd1, 11, 22, CALC_OP_ADD, &r1) == 0 && r1 == 33;
        int ok2 = do_op(fd2, 50, 8,  CALC_OP_SUB, &r2) == 0 && r2 == 42;
        CHECK(ok1 && ok2,
              "interleaved ops on two fds: fd1->%d (want 33), fd2->%d (want 42)",
              r1, r2);
    }

    if (fd1 >= 0) close(fd1);
    if (fd2 >= 0) close(fd2);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    int fd;

    printf("calc_test: opening %s\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", DEVICE_PATH, strerror(errno));
        fprintf(stderr, "Is the module loaded?  sudo insmod calc_driver.ko\n");
        fprintf(stderr, "Is the node accessible? sudo chmod a+rw %s\n",
                DEVICE_PATH);
        return 2;
    }

    test_basic_add(fd);
    test_basic_sub(fd);
    test_negatives_and_zero(fd);
    test_boundaries(fd);
    test_invalid_op(fd);
    test_invalid_ioctl(fd);
    test_bad_pointer(fd);
    test_many_ops_one_fd(fd);

    close(fd);

    test_two_fds();

    printf("\n=========================\n");
    printf(" tests run:    %d\n", tests_run);
    printf(" tests failed: %d\n", tests_failed);
    printf("=========================\n");
    return tests_failed == 0 ? 0 : 1;
}
