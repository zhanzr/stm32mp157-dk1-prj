#include <ctype.h>
#include <dirent.h>
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
#define SENSOR_PERIOD_CYCLES 8

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

static int read_str_file(const char *path, char *buf, size_t len)
{
	FILE *f = fopen(path, "r");

	if (!f)
		return -1;
	if (!fgets(buf, (int)len, f)) {
		fclose(f);
		return -1;
	}
	fclose(f);
	buf[strcspn(buf, "\r\n")] = '\0';
	return 0;
}

static int read_int_file(const char *path, int *out)
{
	FILE *f = fopen(path, "r");

	if (!f)
		return -1;
	if (fscanf(f, "%d", out) != 1) {
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

static int read_double_file(const char *path, double *out)
{
	FILE *f = fopen(path, "r");

	if (!f)
		return -1;
	if (fscanf(f, "%lf", out) != 1) {
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

static void report_sensors(void)
{
	DIR *d;
	struct dirent *e;
	char path[512];
	char buf[128];

	d = opendir("/sys/class/thermal");
	if (d) {
		while ((e = readdir(d))) {
			int mt;

			if (strncmp(e->d_name, "thermal_zone", 12))
				continue;
			snprintf(path, sizeof(path), "/sys/class/thermal/%s/type",
				 e->d_name);
			if (read_str_file(path, buf, sizeof(buf)))
				continue;
			snprintf(path, sizeof(path), "/sys/class/thermal/%s/temp",
				 e->d_name);
			if (read_int_file(path, &mt))
				continue;
			printf("thermal %s [%s]: %.3f C\n", e->d_name, buf,
			       mt / 1000.0);
		}
		closedir(d);
	}

	d = opendir("/sys/bus/iio/devices");
	if (d) {
		while ((e = readdir(d))) {
			double scale = 1.0;
			int have_scale;

			if (strncmp(e->d_name, "iio:device", 10))
				continue;
			snprintf(path, sizeof(path), "/sys/bus/iio/devices/%s/name",
				 e->d_name);
			if (read_str_file(path, buf, sizeof(buf)))
				continue;
			printf("adc %s [%s]:\n", e->d_name, buf);
			snprintf(path, sizeof(path), "/sys/bus/iio/devices/%s/in_voltage_scale",
				 e->d_name);
			have_scale = (read_double_file(path, &scale) == 0);

			for (int ch = 0; ch <= 19; ch++) {
				char label[64] = "";
				int have_label;
				double mv;
				int raw;

				snprintf(path, sizeof(path),
					 "/sys/bus/iio/devices/%s/in_voltage%d_label",
					 e->d_name, ch);
				have_label = (read_str_file(path, label,
							    sizeof(label)) == 0);
				snprintf(path, sizeof(path),
					 "/sys/bus/iio/devices/%s/in_voltage%d_input",
					 e->d_name, ch);
				if (read_double_file(path, &mv) == 0) {
					printf("  ch%02d%s%s%s: %.0f mV (processed)\n",
					       ch, have_label ? " [" : "",
					       label, have_label ? "]" : "", mv);
					continue;
				}
				snprintf(path, sizeof(path),
					 "/sys/bus/iio/devices/%s/in_voltage%d_raw",
					 e->d_name, ch);
				if (read_int_file(path, &raw) == 0) {
					printf("  ch%02d%s%s%s: %.2f mV\n",
					       ch, have_label ? " [" : "",
					       label, have_label ? "]" : "",
					       have_scale ? raw * scale : (double)raw);
				}
			}
		}
		closedir(d);
	}
	fflush(stdout);
}

int main(void)
{
	int chip_fds[8] = { 0 };
	int nchips = 0;
	int line_fds[NLEDS] = { 0 };
	int active[NLEDS] = { 0 };
	int nactive = 0;
	unsigned long cycles = 0;

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
	printf("blinking %d LED(s), sensors every %d ms, Ctrl-C to stop\n",
	       nactive, SENSOR_PERIOD_CYCLES * 2 * BLINK_PERIOD_MS);

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
		if (++cycles % SENSOR_PERIOD_CYCLES == 0)
			report_sensors();
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
