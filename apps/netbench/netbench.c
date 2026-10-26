#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_PORT 5201
#define PATTERN_SIZE (64 * 1024)

static volatile sig_atomic_t keep_running = 1;

static double now_sec(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void report(const char *what, double bytes, double dt)
{
	if (dt <= 0.001)
		dt = 0.001;
	printf("server %s: %.0f bytes in %.2f s = %.2f Mbps (%.2f MiB/s)\n",
	       what, bytes, dt, bytes * 8.0 / dt / 1e6,
	       bytes / dt / 1048576.0);
	fflush(stdout);
}

static void handle_client(int fd)
{
	char cmd = 0;
	char *pattern;
	size_t sent = 0;

	if (read(fd, &cmd, 1) != 1)
		return;

	if (cmd == 'W') {
		char buf[256 * 1024];
		double start = now_sec();
		ssize_t n;
		double total = 0;

		while ((n = read(fd, buf, sizeof(buf))) > 0)
			total += n;
		report("received", total, now_sec() - start);
	} else if (cmd == 'R') {
		char lenbuf[32] = { 0 };
		size_t i;
		double start;
		ssize_t n;
		size_t want;

		if (read(fd, lenbuf, sizeof(lenbuf) - 1) <= 0)
			return;
		want = (size_t)strtoull(lenbuf, NULL, 10);
		pattern = malloc(PATTERN_SIZE);
		if (!pattern)
			return;
		for (i = 0; i < PATTERN_SIZE; i++)
			pattern[i] = (char)(i * 31 + i / 251);
		start = now_sec();
		while (keep_running && sent < want) {
			size_t chunk = want - sent;

			if (chunk > PATTERN_SIZE)
				chunk = PATTERN_SIZE;
			n = send(fd, pattern, chunk, MSG_NOSIGNAL);
			if (n <= 0)
				break;
			sent += (size_t)n;
		}
		free(pattern);
		report("sent", (double)sent, now_sec() - start);
	}
}

static void on_int(int sig)
{
	(void)sig;
	keep_running = 0;
}

int main(int argc, char **argv)
{
	int port = DEFAULT_PORT;
	int srv;
	struct sockaddr_in addr;
	int yes = 1;

	if (argc > 1)
		port = atoi(argv[1]);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, on_int);
	signal(SIGTERM, on_int);

	srv = socket(AF_INET, SOCK_STREAM, 0);
	if (srv < 0) {
		perror("socket");
		return 1;
	}
	setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((uint16_t)port);
	if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
	    listen(srv, 1) < 0) {
		perror("bind/listen");
		return 1;
	}
	printf("netbench server listening on port %d\n", port);
	fflush(stdout);

	while (keep_running) {
		int fd = accept(srv, NULL, NULL);

		if (fd < 0)
			break;
		handle_client(fd);
		close(fd);
	}
	close(srv);
	return 0;
}
