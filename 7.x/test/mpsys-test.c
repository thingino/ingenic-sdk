// SPDX-License-Identifier: GPL-2.0
/*
 * mpsys-test: the mpsys_driver.ko ioctls without libimp.
 *
 *   mpsys-test [rounds]
 *
 * /dev/mpsys_data: the parent marks itself the main process, a child
 * waits on key 7 (MPSYS_GET_DATA), the parent pushes a numbered message
 * (MPSYS_PUSH_DATA), the child checks it and hands it back
 * (MPSYS_RELEASE_DATA), the parent takes it back (MPSYS_WAIT_RELEASE) and
 * checks it again. /dev/nna_lock (T40/T41 with nmem=): two blocks from the
 * nmem region, the global lock taken and dropped, the blocks freed.
 *
 * Prints "mpsys-test: PASS" or the first failure.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

struct mpsys_msg {
	int32_t key;
	void *buf;
	int32_t size;
	int32_t tgid;
};

#define MPSYS_SET_MAIN		0xc0044401
#define MPSYS_GET_DATA		0xc0044403
#define MPSYS_PUSH_DATA		0xc0044404
#define MPSYS_RELEASE_DATA	0xc0044405
#define MPSYS_WAIT_RELEASE	0xc0044406
#define MPSYS_NMEM_ALLOC	0xc0046307
#define MPSYS_SEM_LOCK		0xc0046308
#define MPSYS_SEM_UNLOCK	0xc0046309
#define MPSYS_NMEM_FREE		0xc004630a

#define KEY	7
#define LEN	64

static int fail(const char *what)
{
	printf("mpsys-test: FAIL %s (%s)\n", what, strerror(errno));
	return 1;
}

static void fill(char *b, int round)
{
	memset(b, 0, LEN);
	snprintf(b, LEN, "mpsys message %d", round);
}

static int consumer(int rounds, int main_tgid)
{
	int fd = open("/dev/mpsys_data", O_RDWR);
	char buf[LEN], want[LEN];

	if (fd < 0)
		return fail("child open");
	for (int i = 0; i < rounds; i++) {
		struct mpsys_msg m = { KEY, buf, LEN, 0 };

		memset(buf, 0, LEN);
		if (ioctl(fd, MPSYS_GET_DATA, &m))
			return fail("GET_DATA");
		fill(want, i);
		if (memcmp(buf, want, LEN) || m.tgid != main_tgid) {
			printf("mpsys-test: FAIL got \"%.*s\" tgid %d\n", LEN, buf, m.tgid);
			return 1;
		}
		m.size = LEN;
		if (ioctl(fd, MPSYS_RELEASE_DATA, &m))
			return fail("RELEASE_DATA");
	}
	close(fd);
	return 0;
}

static int data_test(int rounds)
{
	int fd = open("/dev/mpsys_data", O_RDWR);
	char buf[LEN], back[LEN];
	int status, pushed;
	pid_t pid;

	if (fd < 0)
		return fail("open /dev/mpsys_data");
	if (ioctl(fd, MPSYS_SET_MAIN, 0))
		return fail("SET_MAIN");
	pid = fork();
	if (pid == 0)
		_exit(consumer(rounds, getppid()));

	for (int i = 0; i < rounds; i++) {
		struct mpsys_msg m = { KEY, buf, LEN, 0 };

		fill(buf, i);
		/* the push reaches only a consumer already waiting: retry until it is */
		for (int t = 0; t < 500; t++) {
			pushed = ioctl(fd, MPSYS_PUSH_DATA, &m);
			if (pushed)
				break;
			usleep(10000);
		}
		if (pushed != 1) {
			printf("mpsys-test: FAIL round %d pushed to %d consumers\n", i, pushed);
			return 1;
		}
		memset(back, 0, LEN);
		m.buf = back;
		if (ioctl(fd, MPSYS_WAIT_RELEASE, &m))
			return fail("WAIT_RELEASE");
		if (memcmp(back, buf, LEN)) {
			printf("mpsys-test: FAIL released \"%.*s\"\n", LEN, back);
			return 1;
		}
	}
	waitpid(pid, &status, 0);
	close(fd);
	if (!WIFEXITED(status) || WEXITSTATUS(status))
		return printf("mpsys-test: FAIL consumer status 0x%x\n", status), 1;
	printf("mpsys-test: data %d rounds ok\n", rounds);
	return 0;
}

static int utils_test(void)
{
	int fd = open("/dev/nna_lock", O_RDWR);
	int32_t a[2] = { 0x10000, 0 }, b[2] = { 0x20000, 0 };

	if (fd < 0) {
		printf("mpsys-test: no /dev/nna_lock (no nmem= or not a T40/T41), skipped\n");
		return 0;
	}
	if (ioctl(fd, MPSYS_NMEM_ALLOC, a) < 0 || ioctl(fd, MPSYS_NMEM_ALLOC, b) < 0)
		return fail("NMEM_ALLOC");
	printf("mpsys-test: nmem blocks 0x%x 0x%x\n", a[1], b[1]);
	if (!a[1] || b[1] != a[1] + a[0])
		return printf("mpsys-test: FAIL nmem addresses\n"), 1;
	if (ioctl(fd, MPSYS_SEM_LOCK, 0) || ioctl(fd, MPSYS_SEM_UNLOCK, 0))
		return fail("SEM_LOCK/UNLOCK");
	if (ioctl(fd, MPSYS_NMEM_FREE, 0) || ioctl(fd, MPSYS_NMEM_FREE, 0))
		return fail("NMEM_FREE");
	close(fd);
	printf("mpsys-test: nna_lock ok\n");
	return 0;
}

int main(int argc, char **argv)
{
	int rounds = argc > 1 ? atoi(argv[1]) : 20;

	setvbuf(stdout, NULL, _IOLBF, 0);
	if (data_test(rounds) || utils_test())
		return 1;
	printf("mpsys-test: PASS\n");
	return 0;
}
