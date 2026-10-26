#include <linux/gpio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int chip_open(const char *label)
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

static int req_input(int chip_fd, unsigned int offset, unsigned long long pull)
{
	struct gpio_v2_line_request req;

	memset(&req, 0, sizeof(req));
	req.offsets[0] = offset;
	req.num_lines = 1;
	req.config.flags = GPIO_V2_LINE_FLAG_INPUT | pull;
	snprintf(req.consumer, sizeof(req.consumer), "i2cpin_probe");
	if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0)
		return -1;
	return req.fd;
}

static int get_val(int fd)
{
	struct gpio_v2_line_values vals;

	memset(&vals, 0, sizeof(vals));
	vals.mask = 1;
	if (ioctl(fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &vals) < 0)
		return -1;
	return (int)vals.bits;
}

static void probe(const char *what, int chip, unsigned int off)
{
	int f_none = req_input(chip, off, 0);
	int f_pu = -1;
	int a[5] = { 0 }, b[5] = { 0 };

	if (f_none < 0) {
		printf("%s (pin %u): GPIO request failed: %s "
		       "(line muxed to another function - I2C5 active)\n",
		       what, off, strerror(errno));
		return;
	}
	f_pu = req_input(chip, off, GPIO_V2_LINE_FLAG_BIAS_PULL_UP);
	if (f_pu < 0) {
		printf("%s (pin %u): pull-up variant failed: %s\n",
		       what, off, strerror(errno));
		close(f_none);
		return;
	}
	for (int i = 0; i < 5; i++) {
		usleep(20000);
		a[i] = get_val(f_none);
		b[i] = get_val(f_pu);
	}
	printf("%s (pin %u): no-bias reads:", what, off);
	for (int i = 0; i < 5; i++)
		printf(" %d", a[i]);
	printf("   with-internal-pullup reads:");
	for (int i = 0; i < 5; i++)
		printf(" %d", b[i]);
	printf("\n");
	close(f_none);
	close(f_pu);
}

int main(void)
{
	int chip = chip_open("GPIOA");

	if (chip < 0) {
		fprintf(stderr, "GPIOA not found\n");
		return 1;
	}
	probe("PA11 (CN2-5  SCL)", chip, 11);
	probe("PA12 (CN2-3  SDA)", chip, 12);
	close(chip);
	puts("interpretation: all-1 with and without pull-up = module present with own pull-ups;");
	puts("0 without / 1 with pull-up = line held low (short or unpowered module);");
	puts("random = floating (nothing connected)");
	return 0;
}
