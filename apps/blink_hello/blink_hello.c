#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/gpio.h>

typedef struct {
	const char *chip_label;
	unsigned int offset;
	int active_low;
	const char *name;
} led_t;

static const led_t leds[] = {
	{ "GPIOA", 14, 1, "LD5-green"  },
	{ "GPIOA", 13, 1, "LD6-red"    },
	{ "GPIOH",  7, 0, "LD7-orange" },
};

#define NLEDS (sizeof(leds) / sizeof(leds[0]))
#define BLINK_PERIOD_MS 250

static volatile sig_atomic_t running = 1;

static void stop_handler(int sig)
{
	(void)sig;
	running = 0;
}

static int open_chip(const char *label)
{
	int fd;

	for (int i = 0; i < 16; i++) {
		char path[64];
		struct gpiochip_info info;

		snprintf(path, sizeof(path), "/dev/gpiochip%d", i);
		fd = open(path, O_RDWR | O_CLOEXEC);
		if (fd < 0) {
			if (errno == ENOENT)
				break;
			continue;
		}
		if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) == 0 &&
		    strcmp(info.label, label) == 0)
			return fd;
		close(fd);
	}
	return -1;
}

static int request_led(int chip_fd, const led_t *led)
{
	struct gpio_v2_line_request req;

	memset(&req, 0, sizeof(req));
	req.offsets[0] = led->offset;
	req.num_lines = 1;
	req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
	if (led->active_low)
		req.config.flags |= GPIO_V2_LINE_FLAG_ACTIVE_LOW;
	snprintf(req.consumer, sizeof(req.consumer), "blink_hello");

	if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0)
		return -1;
	return req.fd;
}

static void set_led(int line_fd, int on)
{
	struct gpio_v2_line_values vals;

	memset(&vals, 0, sizeof(vals));
	vals.mask = 1;
	vals.bits = on ? 1 : 0;
	ioctl(line_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &vals);
}

int main(void)
{
	int chip_fds[8] = { 0 };
	int nchips = 0;
	int line_fds[NLEDS] = { 0 };
	int active[NLEDS] = { 0 };
	int nactive = 0;

	signal(SIGINT, stop_handler);
	signal(SIGTERM, stop_handler);

	for (size_t i = 0; i < NLEDS; i++) {
		int fd;

		if (nchips < 8) {
			int found = -1;

			for (int c = 0; c < nchips; c++) {
				struct gpiochip_info info;

				if (ioctl(chip_fds[c], GPIO_GET_CHIPINFO_IOCTL, &info) == 0 &&
				    strcmp(info.label, leds[i].chip_label) == 0) {
					found = c;
					break;
				}
			}
			if (found < 0) {
				fd = open_chip(leds[i].chip_label);
				if (fd >= 0) {
					chip_fds[nchips++] = fd;
					found = nchips - 1;
				}
			}
			fd = (found >= 0) ? chip_fds[found] : -1;
		} else {
			fd = -1;
		}

		if (fd < 0) {
			fprintf(stderr, "%s: chip %s not found, skipped\n",
				leds[i].name, leds[i].chip_label);
			line_fds[i] = -1;
			continue;
		}

		line_fds[i] = request_led(fd, &leds[i]);
		if (line_fds[i] < 0) {
			fprintf(stderr, "%s: not available (%s)%s\n",
				leds[i].name, strerror(errno),
				errno == EBUSY ? " - held by another driver (gpio-keys?)" : "");
			line_fds[i] = -1;
			continue;
		}

		active[i] = 1;
		nactive++;
		printf("%s: ready (GPIO%c%d, %s)\n", leds[i].name,
		       leds[i].chip_label[4], leds[i].offset,
		       leds[i].active_low ? "active-low" : "active-high");
	}

	if (nactive == 0) {
		fprintf(stderr, "no LED available, exiting\n");
		return 1;
	}
	printf("blinking %d LED(s), Ctrl-C to stop\n", nactive);

	while (running) {
		for (size_t i = 0; i < NLEDS; i++)
			if (active[i])
				set_led(line_fds[i], 1);
		for (int ms = 0; ms < BLINK_PERIOD_MS && running; ms += 20)
			usleep(20000);
		for (size_t i = 0; i < NLEDS; i++)
			if (active[i])
				set_led(line_fds[i], 0);
		for (int ms = 0; ms < BLINK_PERIOD_MS && running; ms += 20)
			usleep(20000);
	}

	for (size_t i = 0; i < NLEDS; i++) {
		if (active[i]) {
			set_led(line_fds[i], 0);
			close(line_fds[i]);
		}
	}
	for (int c = 0; c < nchips; c++)
		close(chip_fds[c]);

	puts("stopped, LEDs off");
	return 0;
}
