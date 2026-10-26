/*
 * a7_manager - A7-side skeleton of a7_m4_hello.
 *
 * Waits for the rpmsg TTY that appears when the M4 firmware
 * (OpenAMP_TTY_echo) is running, exchanges messages with the M4, and prints
 * everything on the console. This is the manager entry point: replace the
 * demo loop with real commands when needed.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#define TTY_PATH  "/dev/ttyRPMSG0"
#define WAIT_SECS 30

static int wait_for_tty(int secs)
{
	for (int i = 0; i < secs * 2; i++) {
		int fd = open(TTY_PATH, O_RDWR);

		if (fd >= 0)
			return fd;
		usleep(500000);
	}
	return -1;
}

static int rpmsg_send(int fd, const char *msg)
{
	size_t len = strlen(msg);

	return write(fd, msg, len) == (ssize_t)len ? 0 : -1;
}

static int rpmsg_recv(int fd, char *buf, size_t len, int timeout_ms)
{
	fd_set fds;
	struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
	ssize_t n;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0)
		return -1;
	n = read(fd, buf, len - 1);
	if (n <= 0)
		return -1;
	buf[n] = '\0';
	return (int)n;
}

int main(void)
{
	char buf[128];
	int fd;
	int round;

	fd = wait_for_tty(WAIT_SECS);
	if (fd < 0) {
		fprintf(stderr, "%s did not appear within %d s "
			"(M4 firmware started? remoteproc state?)\n",
			TTY_PATH, WAIT_SECS);
		return 1;
	}
	printf("a7_manager: %s open - M4 is up\n", TTY_PATH);

	for (round = 1; round <= 3 && 1; round++) {
		char msg[64];

		snprintf(msg, sizeof(msg), "hello from A7 (%d)", round);
		if (rpmsg_send(fd, msg) < 0) {
			fprintf(stderr, "send failed: %s\n", strerror(errno));
			break;
		}
		printf("A7 sent : \"%s\"\n", msg);

		if (rpmsg_recv(fd, buf, sizeof(buf), 3000) < 0) {
			fprintf(stderr, "no answer from M4\n");
			break;
		}
		printf("A7 got  : \"%s\"\n", buf);
	}

	close(fd);
	printf("a7_manager: done\n");
	return 0;
}
