#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <linux/i2c-dev.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

extern const unsigned char F6x8[][6];
extern const unsigned char F8X16[];

#define OLED_ADDR     0x3C
#define I2C_CTRL_ID   "40015000"
#define MAX_COLUMN    64
#define MAX_ROW       48
#define MAX_SECONDS   30
#define RES_CHIP      "GPIOF"
#define RES_OFFSET    3

static volatile sig_atomic_t running = 1;

static void stop_handler(int sig)
{
	(void)sig;
	running = 0;
}

static void delay_ms(long ms)
{
	usleep((useconds_t)ms * 1000);
}

static int chip_open_by_label(const char *label)
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

static int line_request_output(int chip_fd, unsigned int offset, int value)
{
	struct gpio_v2_line_request req;

	memset(&req, 0, sizeof(req));
	req.offsets[0] = offset;
	req.num_lines = 1;
	req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
	snprintf(req.consumer, sizeof(req.consumer), "ssd1317_demo");

	if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0)
		return -1;

	{
		struct gpio_v2_line_values vals;

		memset(&vals, 0, sizeof(vals));
		vals.mask = 1;
		vals.bits = value ? 1 : 0;
		ioctl(req.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &vals);
	}
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

static int i2c_adapter_find(char *path, size_t len)
{
	DIR *d = opendir("/sys/bus/i2c/devices");
	struct dirent *e;

	if (!d)
		return -1;
	while ((e = readdir(d))) {
		char npath[256];
		char name[128];
		FILE *f;

		if (strncmp(e->d_name, "i2c-", 4))
			continue;
		snprintf(npath, sizeof(npath), "/sys/bus/i2c/devices/%s/name",
			 e->d_name);
		f = fopen(npath, "r");
		if (!f)
			continue;
		if (fgets(name, sizeof(name), f)) {
			fclose(f);
			if (strstr(name, I2C_CTRL_ID)) {
				snprintf(path, len, "/dev/%s", e->d_name);
				closedir(d);
				return 0;
			}
		} else {
			fclose(f);
		}
	}
	closedir(d);
	return -1;
}

static int i2c_fd = -1;

static void oled_cmd(uint8_t c)
{
	uint8_t buf[2] = { 0x00, c };

	if (write(i2c_fd, buf, 2) != 2)
		fprintf(stderr, "i2c cmd 0x%02x failed: %s\n", c,
			strerror(errno));
}

static void oled_set_pos(unsigned char x, unsigned char y)
{
	unsigned char col = (unsigned char)(x + 0x20);

	oled_cmd((uint8_t)(0xb2 + y));
	oled_cmd((uint8_t)(((col >> 4) & 0x0f) | 0x10));
	oled_cmd((uint8_t)(col & 0x0f));
}

static void oled_write_page(unsigned char page, const unsigned char *data)
{
	uint8_t buf[1 + MAX_COLUMN];

	buf[0] = 0x40;
	memcpy(&buf[1], data, MAX_COLUMN);
	oled_set_pos(0, page);
	if (write(i2c_fd, buf, sizeof(buf)) != sizeof(buf))
		fprintf(stderr, "i2c page write failed: %s\n", strerror(errno));
}

static void oled_fill(unsigned char data)
{
	unsigned char row[MAX_COLUMN];

	memset(row, data, sizeof(row));
	for (unsigned char page = 0; page < 6; page++)
		oled_write_page(page, row);
}

static void oled_invert(int on)
{
	oled_cmd(on ? 0xA7 : 0xA6);
}

static void oled_show_char(unsigned char x, unsigned char y, unsigned char chr,
			   unsigned char size)
{
	unsigned char c = (unsigned char)(chr - ' ');

	if (x > MAX_COLUMN - 1) {
		x = 0;
		y = (unsigned char)(y + 2);
	}
	if (size == 16) {
		oled_set_pos(x, y);
		for (int i = 0; i < 8; i++) {
			uint8_t buf[2] = { 0x40, F8X16[c * 16 + i] };

			write(i2c_fd, buf, 2);
		}
		oled_set_pos(x, (unsigned char)(y + 1));
		for (int i = 0; i < 8; i++) {
			uint8_t buf[2] = { 0x40, F8X16[c * 16 + i + 8] };

			write(i2c_fd, buf, 2);
		}
	} else {
		oled_set_pos(x, y);
		for (int i = 0; i < 6; i++) {
			uint8_t buf[2] = { 0x40, F6x8[c][i] };

			write(i2c_fd, buf, 2);
		}
	}
}

static uint32_t oled_pow(unsigned char m, unsigned char n)
{
	uint32_t result = 1;

	while (n--)
		result *= m;
	return result;
}

static void oled_show_num(unsigned char x, unsigned char y, uint32_t num,
			  unsigned char len, unsigned char size)
{
	unsigned char enshow = 0;

	for (unsigned char t = 0; t < len; t++) {
		unsigned char temp = (unsigned char)((num /
			oled_pow(10, len - t - 1)) % 10);

		if (enshow == 0 && t < (len - 1)) {
			if (temp == 0) {
				oled_show_char((unsigned char)(x +
					(size / 2) * t), y, ' ', size);
				continue;
			}
			enshow = 1;
		}
		oled_show_char((unsigned char)(x + (size / 2) * t), y,
			       (unsigned char)(temp + '0'), size);
	}
}

static void oled_show_string(unsigned char x, unsigned char y,
			     const char *chr, unsigned char size)
{
	unsigned char j = 0;

	while (chr[j]) {
		oled_show_char(x, y, (unsigned char)chr[j], size);
		x = (unsigned char)(x + 8);
		if (x > MAX_COLUMN - 8) {
			x = 0;
			y = (unsigned char)(y + 2);
		}
		j++;
	}
}

static void oled_clear(void)
{
	oled_fill(0x00);
}

static void oled_display_on(void)
{
	oled_cmd(0x8D);
	oled_cmd(0x14);
	oled_cmd(0xAF);
}

static void oled_init(int res_fd)
{
	line_set(res_fd, 1);
	delay_ms(100);
	line_set(res_fd, 0);
	delay_ms(100);
	line_set(res_fd, 1);
	delay_ms(100);

	static const uint8_t seq[] = {
		0xAE, 0x00, 0x12, 0x40, 0x81, 0xCF, 0xA1, 0xC8, 0xA6,
		0xA8, 0x3F, 0xD3, 0x00, 0xD5, 0x80, 0xD9, 0xF1, 0xDA,
		0x12, 0xDB, 0x40, 0x20, 0x02, 0x8D, 0x14, 0xA4, 0xA6,
		0xAF
	};
	for (size_t i = 0; i < sizeof(seq); i++)
		oled_cmd(seq[i]);

	oled_clear();
	oled_set_pos(0, 0);
}

static void oled_demo(void)
{
	unsigned char row[MAX_COLUMN];
	double t0 = 0.0;
	int frames;
	char line[24];

	oled_clear();
	oled_show_string(0, 0, " SSD1317", 16);
	oled_show_string(0, 2, " 64x48", 16);
	oled_show_string(0, 4, " DK1-I2C5", 16);
	delay_ms(1000);

	oled_invert(1);
	delay_ms(1000);
	oled_invert(0);
	delay_ms(500);

	oled_fill(0xFF);
	delay_ms(1000);
	oled_fill(0x00);
	delay_ms(1000);

	for (unsigned char page = 0; page < 6; page++) {
		memset(row, (page & 1) ? 0x00 : 0xFF, sizeof(row));
		oled_write_page(page, row);
	}
	delay_ms(1000);

	for (unsigned char page = 0; page < 6; page++) {
		memset(row, (page & 1) ? 0xF0 : 0x0F, sizeof(row));
		oled_write_page(page, row);
	}
	delay_ms(1000);

	for (unsigned char x = 0; x < MAX_COLUMN; x++)
		row[x] = (x & 1) ? 0xAA : 0x55;
	for (unsigned char page = 0; page < 6; page++)
		oled_write_page(page, row);
	delay_ms(1000);

	for (unsigned char x = 0; x < MAX_COLUMN; x++)
		row[x] = ((x / 8) & 1) ? 0x00 : 0xFF;
	for (unsigned char page = 0; page < 6; page++)
		oled_write_page(page, row);
	delay_ms(1000);

	for (unsigned char page = 0; page < 6; page++) {
		memset(row, 0x00, sizeof(row));
		if (page == 0 || page == 5)
			memset(row, 0xFF, sizeof(row));
		else {
			row[0] = 0xFF;
			row[63] = 0xFF;
		}
		oled_write_page(page, row);
	}
	delay_ms(1000);

	{
		static const unsigned char ramp[6] = { 0x03, 0x0F, 0x3F,
						       0xFF, 0x3F, 0x0F };

		for (unsigned char page = 0; page < 6; page++) {
			memset(row, ramp[page], sizeof(row));
			oled_write_page(page, row);
		}
	}
	delay_ms(1000);

	oled_clear();
	{
		struct timespec ts;

		clock_gettime(CLOCK_MONOTONIC, &ts);
		t0 = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
	}
	for (frames = 0; frames < 40 && running; frames++) {		unsigned char start = (unsigned char)((frames * 4) % 64);

		memset(row, 0x00, sizeof(row));
		memset(&row[start], 0xFF, 8 <= 64 - start ? 8 : 64 - start);
		for (unsigned char page = 0; page < 6; page++)
			oled_write_page(page, row);
	}
	{
		struct timespec ts;
		double dt;

		clock_gettime(CLOCK_MONOTONIC, &ts);
		dt = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9 - t0;
		snprintf(line, sizeof(line), "fps %d",
			 (int)(frames / (dt > 0 ? dt : 1)));
	}
	oled_show_string(0, 2, line, 16);
	printf("oled fps: %s\n", line);
	delay_ms(1000);
}

int main(void)
{
	char path[64];
	int res_chip, res_fd;
	int leds_chip_a, leds_chip_h;
	int led_fds[3] = { -1, -1, -1 };
	int nleds = 0;

	signal(SIGINT, stop_handler);
	signal(SIGTERM, stop_handler);

	if (i2c_adapter_find(path, sizeof(path)) < 0) {
		fprintf(stderr, "no I2C adapter for controller " I2C_CTRL_ID
			" (I2C5) found - DTB patch missing?\n");
		return 1;
	}
	i2c_fd = open(path, O_RDWR);
	if (i2c_fd < 0) {
		fprintf(stderr, "open %s: %s\n", path, strerror(errno));
		return 1;
	}
	if (ioctl(i2c_fd, I2C_SLAVE, OLED_ADDR) < 0) {
		fprintf(stderr, "I2C_SLAVE: %s\n", strerror(errno));
		return 1;
	}

	res_chip = chip_open_by_label(RES_CHIP);
	if (res_chip < 0) {
		fprintf(stderr, "GPIO chip " RES_CHIP " not found\n");
		return 1;
	}
	res_fd = line_request_output(res_chip, RES_OFFSET, 1);
	if (res_fd < 0) {
		fprintf(stderr, "RES line " RES_CHIP "%d busy: %s\n",
			RES_OFFSET, strerror(errno));
		return 1;
	}
	printf("RES = " RES_CHIP "%d ok\n", RES_OFFSET);

	leds_chip_a = chip_open_by_label("GPIOA");
	leds_chip_h = chip_open_by_label("GPIOH");
	if (leds_chip_a >= 0) {
		led_fds[0] = line_request_output(leds_chip_a, 14, 1);
		led_fds[1] = line_request_output(leds_chip_a, 13, 1);
	}
	if (leds_chip_h >= 0)
		led_fds[2] = line_request_output(leds_chip_h, 7, 1);
	for (int i = 0; i < 3; i++)
		if (led_fds[i] >= 0)
			nleds++;
	printf("pacing LEDs available: %d\n", nleds);

	{
		uint8_t probe[2] = { 0x00, 0xAE };

		if (write(i2c_fd, probe, 2) != 2) {
			fprintf(stderr, "OLED not detected at 0x%02x on %s "
				"(%s) - check wiring (SCL=CN2-5, SDA=CN2-3), "
				"VDD=CN2-1, GND=CN2-39 and module pull-ups\n",
				OLED_ADDR, path, strerror(errno));
			return 1;
		}
	}

	printf("SSD1317 MD066 64x48 I2C demo start (%s @ 0x%02x)\n", path,
	       OLED_ADDR);
	oled_init(res_fd);

	int led_state[3] = { 1, 1, 1 };

	while (running) {
		oled_demo();
		if (!running)
			break;

		led_state[0] ^= 1;
		if (led_fds[0] >= 0)
			line_set(led_fds[0], led_state[0]);
		delay_ms(200);

		led_state[1] ^= 1;
		if (led_fds[1] >= 0)
			line_set(led_fds[1], led_state[1]);
		delay_ms(400);

		led_state[2] ^= 1;
		if (led_fds[2] >= 0)
			line_set(led_fds[2], led_state[2]);
		delay_ms(800);
	}

	for (int i = 0; i < 3; i++) {
		if (led_fds[i] >= 0) {
			line_set(led_fds[i], 1);
			close(led_fds[i]);
		}
	}
	line_set(res_fd, 1);
	close(res_fd);
	close(res_chip);
	close(leds_chip_a);
	close(leds_chip_h);
	close(i2c_fd);
	puts("bye");
	return 0;
}
