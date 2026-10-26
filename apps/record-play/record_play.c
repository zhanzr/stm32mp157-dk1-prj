#include <alsa/asoundlib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CAPTURE_DEV  "hw:0,1"
#define PLAYBACK_DEV "hw:0,0"
#define SAMPLE_RATE  48000
#define CHANNELS     2
#define SAMPLE_SIZE  2
#define FRAME_BYTES  (CHANNELS * SAMPLE_SIZE)
#define MAX_SECONDS  30
#define CHUNK_FRAMES 1024

static volatile sig_atomic_t running = 1;

static struct termios saved_tio;
static int raw_mode_on;

static void set_raw_mode(void)
{
	struct termios t;

	if (!isatty(0))
		return;
	tcgetattr(0, &saved_tio);
	t = saved_tio;
	t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &t);
	raw_mode_on = 1;
}

static void restore_mode(void)
{
	if (raw_mode_on)
		tcsetattr(0, TCSANOW, &saved_tio);
}

static void stop_handler(int sig)
{
	(void)sig;
	running = 0;
}

static void tstp_handler(int sig)
{
	sigset_t mask;

	(void)sig;
	restore_mode();
	sigemptyset(&mask);
	sigaddset(&mask, SIGTSTP);
	signal(SIGTSTP, SIG_DFL);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
	raise(SIGTSTP);
	signal(SIGTSTP, tstp_handler);
}

static void cont_handler(int sig)
{
	(void)sig;
	set_raw_mode();
}

static void install_handlers(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = stop_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sa.sa_handler = tstp_handler;
	sigaction(SIGTSTP, &sa, NULL);
	sa.sa_handler = cont_handler;
	sigaction(SIGCONT, &sa, NULL);
}

static int wait_key(void)
{
	unsigned char c;

	if (read(0, &c, 1) == 1)
		return c;
	return -1;
}

static int key_pressed(void)
{
	fd_set fds;
	struct timeval tv = { 0, 0 };

	FD_ZERO(&fds);
	FD_SET(0, &fds);
	return select(1, &fds, NULL, NULL, &tv) > 0;
}

static double now_sec(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int open_pcm(snd_pcm_t **pcm, snd_pcm_stream_t stream, const char *dev)
{
	int err;
	snd_pcm_sw_params_t *sw = NULL;
	static const int rates[] = { SAMPLE_RATE, 44100, 32000, 16000 };
	size_t i;

	for (i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
		err = snd_pcm_open(pcm, dev, stream, 0);
		if (err < 0)
			return err;
		err = snd_pcm_set_params(*pcm, SND_PCM_FORMAT_S16_LE,
					 SND_PCM_ACCESS_RW_INTERLEAVED,
					 CHANNELS, rates[i], 1, 500000);
		if (err >= 0)
			break;
		snd_pcm_close(*pcm);
		*pcm = NULL;
	}
	if (err < 0)
		return err;

	err = snd_pcm_sw_params_malloc(&sw);
	if (err < 0)
		return err;
	snd_pcm_sw_params_current(*pcm, sw);
	snd_pcm_sw_params_set_start_threshold(*pcm, sw, 1);
	snd_pcm_sw_params_set_avail_min(*pcm, sw, CHUNK_FRAMES);
	snd_pcm_sw_params(*pcm, sw);
	snd_pcm_sw_params_free(sw);

	printf("  device %s: %d Hz, %d ch, S16_LE\n", dev, rates[i],
	       CHANNELS);
	return 0;
}

static size_t do_record(snd_pcm_t *cap, char *buf, size_t cap_bytes)
{
	size_t recorded = 0;
	double start = now_sec();
	double last_print = -1;

	snd_pcm_prepare(cap);

	while (running) {
		double elapsed = now_sec() - start;
		snd_pcm_sframes_t frames;

		if (elapsed >= MAX_SECONDS) {
			printf("\n  30 s limit reached\n");
			break;
		}
		if (key_pressed()) {
			int c = wait_key();

			if (c == 'S' || c == 's') {
				printf("\n  'S' pressed, stopping\n");
				break;
			}
		}
		if ((size_t)recorded >= cap_bytes) {
			printf("\n  buffer full\n");
			break;
		}
		if (elapsed - last_print >= 1.0) {
			printf("\r  recording: %5.1f s (S = stop)", elapsed);
			fflush(stdout);
			last_print = elapsed;
		}
		frames = snd_pcm_readi(cap, buf + recorded, CHUNK_FRAMES);
		if (frames == -EPIPE) {
			snd_pcm_prepare(cap);
			continue;
		}
		if (frames < 0) {
			fprintf(stderr, "\n  capture error: %s\n",
				snd_strerror((int)frames));
			break;
		}
		recorded += (size_t)frames * FRAME_BYTES;
	}
	snd_pcm_drop(cap);
	return recorded;
}

static int do_play(snd_pcm_t *pb, const char *buf, size_t bytes)
{
	size_t off = 0;
	double start, last_print = -1;

	snd_pcm_prepare(pb);
	start = now_sec();

	while (running && off < bytes) {
		size_t remaining = bytes - off;
		snd_pcm_uframes_t want = CHUNK_FRAMES;
		snd_pcm_sframes_t frames;
		double elapsed = now_sec() - start;

		if (elapsed - last_print >= 1.0) {
			printf("\r  playing: %5.1f s", elapsed);
			fflush(stdout);
			last_print = elapsed;
		}
		if ((size_t)want * FRAME_BYTES > remaining)
			want = remaining / FRAME_BYTES;
		if (want == 0)
			break;
		frames = snd_pcm_writei(pb, buf + off, want);
		if (frames == -EPIPE) {
			snd_pcm_prepare(pb);
			continue;
		}
		if (frames < 0) {
			fprintf(stderr, "\n  playback error: %s\n",
				snd_strerror((int)frames));
			return -1;
		}
		off += (size_t)frames * FRAME_BYTES;
	}
	if (running)
		snd_pcm_drain(pb);
	else
		snd_pcm_drop(pb);
	printf("\n");
	return 0;
}

int main(void)
{
	snd_pcm_t *cap = NULL, *pb = NULL;
	char *buf = NULL;
	size_t cap_bytes;
	int err;

	install_handlers();

	err = open_pcm(&cap, SND_PCM_STREAM_CAPTURE, CAPTURE_DEV);
	if (err < 0) {
		fprintf(stderr, "capture open failed (%s): %s\n", CAPTURE_DEV,
			snd_strerror(err));
		return 1;
	}

	cap_bytes = (size_t)SAMPLE_RATE * CHANNELS * SAMPLE_SIZE * MAX_SECONDS;
	buf = malloc(cap_bytes);
	if (!buf) {
		fprintf(stderr, "cannot allocate %zu bytes\n", cap_bytes);
		return 1;
	}

	set_raw_mode();
	printf("record-play: %d Hz, %d ch, S16_LE, up to %d s per take\n",
	       SAMPLE_RATE, CHANNELS, MAX_SECONDS);

	while (running) {
		size_t recorded;

		printf("\n[1] Press any key to START recording "
		       "(%d s max, 'S' to stop early)\n", MAX_SECONDS);
		if (wait_key() < 0)
			break;
		if (!running)
			break;

		recorded = do_record(cap, buf, cap_bytes);
		printf("  recorded %.2f s (%zu bytes)\n",
		       (double)recorded / (SAMPLE_RATE * FRAME_BYTES), recorded);
		if (!running)
			break;

		printf("\n[2] Press any key to PLAY the recorded buffer\n");
		if (wait_key() < 0)
			break;
		if (!running)
			break;

		if (!pb && open_pcm(&pb, SND_PCM_STREAM_PLAYBACK,
				    PLAYBACK_DEV) < 0) {
			fprintf(stderr, "playback open failed\n");
			break;
		}
		if (do_play(pb, buf, recorded) < 0)
			break;
		printf("  playback done\n");
	}

	if (cap)
		snd_pcm_close(cap);
	if (pb)
		snd_pcm_close(pb);
	free(buf);
	restore_mode();
	puts("bye");
	return 0;
}
