#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define BLINK_MS 400

typedef struct {
	const char *chip_label;
	unsigned int offset;
	const char *name;
	int chip_fd;
	int line_fd;
} out_t;

static out_t outs[] = {
	{ "GPIOA", 11, "PA11 / CN2-5  (SCL wire)" },
	{ "GPIOA", 12, "PA12 / CN2-3  (SDA wire)" },
	{ "GPIOF",  3, "PF3  / CN2-26 (RES wire)" },
};

static volatile sig_atomic_t running = 1;

static void stop_handler(int sig)
{
	(void)sig;
	running = 0;
}

static int chip_open_by_label(const char *label)
{
	char path[64];

	for (int i = 0; i < 16; i++) {
		struct gpiochip_info info;
		int fd;

		snprintf(path, sizeof(path), "/dev/gpiochip%d", i);
		fd = open(path, O_RDWR);
		if (fd < 0)
			continue;
		if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) == 0 &&
		    strcmp(info.label, label) == 0)
			return fd;
		close(fd);
	}
	return -1;
}

static int line_req(int chip_fd, unsigned int offset)
{
	struct gpio_v2_line_request req;

	memset(&req, 0, sizeof(req));
	req.offsets[0] = offset;
	req.num_lines = 1;
	req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
	snprintf(req.consumer, sizeof(req.consumer), "blink_check");
	if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0)
		return -1;
	return req.fd;
}

static void line_set(int fd, int on)
{
	struct gpio_v2_line_values vals;

	memset(&vals, 0, sizeof(vals));
	vals.mask = 1;
	vals.bits = on ? 1 : 0;
	ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &vals);
}

static void i2c5_unbind(void)
{
	int f = open("/sys/bus/platform/drivers/stm32f7-i2c/unbind",
		     O_WRONLY);

	if (f < 0)
		return;
	if (write(f, "40015000.i2c", 12) == 12)
		printf("i2c5 driver unbound (pins released for GPIO)\n");
	else
		printf("i2c5 unbind skipped (%s)\n", strerror(errno));
	close(f);
}

static void i2c5_bind(void)
{
	int f = open("/sys/bus/platform/drivers/stm32f7-i2c/bind", O_WRONLY);

	if (f < 0)
		return;
	if (write(f, "40015000.i2c", 12) == 12)
		printf("i2c5 driver re-bound\n");
	else
		printf("i2c5 re-bind failed: %s\n", strerror(errno));
	close(f);
}

int main(void)
{
	signal(SIGINT, stop_handler);
	signal(SIGTERM, stop_handler);

	i2c5_unbind();

	for (size_t i = 0; i < sizeof(outs) / sizeof(outs[0]); i++) {
		outs[i].chip_fd = chip_open_by_label(outs[i].chip_label);
		outs[i].line_fd = -1;
		if (outs[i].chip_fd >= 0)
			outs[i].line_fd = line_req(outs[i].chip_fd,
						   outs[i].offset);
		if (outs[i].line_fd >= 0) {
			line_set(outs[i].line_fd, 0);
			printf("%s: OK\n", outs[i].name);
		} else {
			printf("%s: line unavailable (%s)\n", outs[i].name,
			       strerror(errno));
		}
	}

	printf("blinking each wire in rotation, Ctrl-C to stop\n");
	while (running) {
		for (size_t i = 0; i < sizeof(outs) / sizeof(outs[0]) &&
				   running; i++) {
			if (outs[i].line_fd < 0)
				continue;
			printf("\r  ON : %s ", outs[i].name);
			fflush(stdout);
			line_set(outs[i].line_fd, 1);
			for (int ms = 0; ms < BLINK_MS && running; ms += 20)
				usleep(20000);
			line_set(outs[i].line_fd, 0);
			printf("\r  off: %s ", outs[i].name);
			fflush(stdout);
			for (int ms = 0; ms < BLINK_MS && running; ms += 20)
				usleep(20000);
		}
	}
	printf("\n");

	for (size_t i = 0; i < sizeof(outs) / sizeof(outs[0]); i++) {
		if (outs[i].line_fd >= 0) {
			line_set(outs[i].line_fd, 0);
			close(outs[i].line_fd);
		}
		if (outs[i].chip_fd >= 0)
			close(outs[i].chip_fd);
	}
	i2c5_bind();
	puts("bye");
	return 0;
}
