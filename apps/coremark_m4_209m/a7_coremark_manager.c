#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TRACE_PATH "/sys/kernel/debug/remoteproc/remoteproc0/trace0"
#define EXPECTED_RUNS 3
#define TIMEOUT_SECS 240
#define POLL_MS 300

static double now_sec(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int read_trace(char *buf, size_t len)
{
	FILE *f = fopen(TRACE_PATH, "r");
	size_t n;

	if (!f)
		return -1;
	n = fread(buf, 1, len - 1, f);
	fclose(f);
	buf[n] = '\0';
	return (int)n;
}

int main(void)
{
	static char buf[4096];
	double scores[EXPECTED_RUNS];
	double start = now_sec();
	int reported = 0;

	printf("a7_coremark_manager: polling %s every %d ms for %d "
	       "result(s), timeout %d s\n", TRACE_PATH, POLL_MS,
	       EXPECTED_RUNS, TIMEOUT_SECS);
	fflush(stdout);

	while (now_sec() - start < TIMEOUT_SECS &&
	       reported < EXPECTED_RUNS) {
		int total = 0;

		if (read_trace(buf, sizeof(buf)) < 0) {
			fprintf(stderr, "cannot read %s (%s)\n", TRACE_PATH,
				strerror(errno));
			return 1;
		}
		for (const char *p = buf;
		     (p = strstr(p, "CoreMark 1.0 :")) != NULL; p++)
			total++;
		while (reported < total && reported < EXPECTED_RUNS) {
			const char *p = buf;
			int k = reported;
			char outline[160];
			const char *eol;
			size_t len;

			while (k-- > 0)
				p = strstr(p, "CoreMark 1.0 :") +
				    strlen("CoreMark 1.0 :");
			p = strstr(p, "CoreMark 1.0 :");
			if (!p)
				break;
			eol = strchr(p, '\n');
			len = eol ? (size_t)(eol - p) : strlen(p);
			if (len >= sizeof(outline))
				len = sizeof(outline) - 1;
			memcpy(outline, p, len);
			outline[len] = '\0';
			scores[reported] = strtod(
				p + strlen("CoreMark 1.0 :"), NULL);
			printf("  M4: %s\n", outline);
			fflush(stdout);
			reported++;
		}
		if (reported < EXPECTED_RUNS)
			usleep(POLL_MS * 1000);
	}

	if (reported == 0) {
		fprintf(stderr, "no CoreMark results collected\n");
		return 1;
	}
	{
		double sum = 0, best = 0, min = 1e9;

		for (int i = 0; i < reported; i++) {
			sum += scores[i];
			if (scores[i] > best)
				best = scores[i];
			if (scores[i] < min)
				min = scores[i];
		}
		printf("==== CoreMark M4 (Cortex-M4 @ 208.878 MHz) ====\n");
		for (int i = 0; i < reported; i++)
			printf("  run %d: %.3f CoreMark/s\n", i + 1,
			       scores[i]);
		printf("  runs=%d  best=%.3f  avg=%.3f  spread=%.2f%%\n",
		       reported, best, sum / reported,
		       reported > 1 ? (best - min) / min * 100.0 : 0.0);
	}
	return 0;
}
