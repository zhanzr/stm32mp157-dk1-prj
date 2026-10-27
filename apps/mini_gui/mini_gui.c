#include <errno.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define SAMPLE_MS  500
#define NPOINTS    120
#define WIN_W      800
#define WIN_H      480
#define PLOT_H     230
#define MAX_PANELS 8

typedef struct {
	char name[96];
	double vals[NPOINTS];
	int count;
	double last;
} series_t;

typedef struct {
	GtkWidget *widget;
	GstElement *pipe;
} cam_try_t;

static series_t s_temp;
static GPtrArray *volts;
static GtkWidget *plot_area;

static const struct {
	int ch;
	const char *name;
} internal_map[] = {
	{ 12, "vsense(T)*" },
	{ 13, "vrefint INVALID" },
	{ 14, "vddcore"    },
	{ 15, "vbat/4"     },
	{ 16, "dac1"       },
	{ 17, "dac2"       },
};

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

static void series_append(series_t *s, double v)
{
	if (s->count < NPOINTS) {
		s->vals[s->count++] = v;
	} else {
		memmove(s->vals, s->vals + 1, sizeof(double) * (NPOINTS - 1));
		s->vals[NPOINTS - 1] = v;
	}
	s->last = v;
}

static int find_adc2_base(char *base, size_t len)
{
	DIR *d = opendir("/sys/bus/iio/devices");
	struct dirent *e;

	if (!d)
		return -1;
	while ((e = readdir(d))) {
		char p[512];
		char name[128];

		if (strncmp(e->d_name, "iio:device", 10))
			continue;
		snprintf(p, sizeof(p), "/sys/bus/iio/devices/%s/name",
			 e->d_name);
		if (read_str_file(p, name, sizeof(name)))
			continue;
		if (strstr(name, "adc@100")) {
			snprintf(base, len, "/sys/bus/iio/devices/%s",
				 e->d_name);
			closedir(d);
			return 0;
		}
	}
	closedir(d);
	return -1;
}

static void scan_sensors(void)
{
	char base[256];
	char path[512];

	memset(&s_temp, 0, sizeof(s_temp));
	snprintf(s_temp.name, sizeof(s_temp.name), "CPU temp (DTS)");
	{
		double v = 0;

		snprintf(base, sizeof(base),
			 "/sys/class/thermal/thermal_zone0/temp");
		if (read_double_file(base, &v) == 0)
			series_append(&s_temp, v / 1000.0);
	}

	volts = g_ptr_array_new();

	if (find_adc2_base(base, sizeof(base)) < 0) {
		fprintf(stderr, "mini-gui: ADC2 instance not found\n");
		return;
	}
	{
		double gscale = 1.0;

		snprintf(path, sizeof(path), "%s/in_voltage_scale", base);
		read_double_file(path, &gscale);
		for (size_t i = 0;
		     i < sizeof(internal_map) / sizeof(internal_map[0]);
		     i++) {
			char path2[512];
			double mv = 0;
			int have = 0;
			series_t *s;

			snprintf(path2, sizeof(path2), "%s/in_voltage%d_input",
				 base, internal_map[i].ch);
			if (read_double_file(path2, &mv) == 0)
				have = 1;
			else {
				snprintf(path2, sizeof(path2),
					 "%s/in_voltage%d_raw", base,
					 internal_map[i].ch);
				if (read_double_file(path2, &mv) == 0) {
					mv *= gscale;
					have = 1;
				}
			}
			if (!have)
				continue;
			s = g_new0(series_t, 1);
			snprintf(s->name, sizeof(s->name), "%s (mV)",
				 internal_map[i].name);
			series_append(s, mv);
			g_ptr_array_add(volts, s);
		}
	}
}

static void sample_tick_scan(void)
{
	char path[512];
	double v;
	double gscale = 1.0;
	DIR *d;
	struct dirent *e;

	snprintf(path, sizeof(path),
		 "/sys/class/thermal/thermal_zone0/temp");
	if (read_double_file(path, &v) == 0)
		series_append(&s_temp, v / 1000.0);

	if (find_adc2_base(path, sizeof(path)) < 0)
		return;
	{
		char spath[600];

		snprintf(spath, sizeof(spath), "%s/in_voltage_scale", path);
		read_double_file(spath, &gscale);
	}
	d = opendir(path);
	if (!d)
		return;
	while ((e = readdir(d))) {
		char lpath[512];
		double mv = 0, scale = 1;
		int mapped = -1;

		if (strncmp(e->d_name, "in_voltage", 10))
			continue;
		if (sscanf(e->d_name, "in_voltage%d_", &mapped) != 1)
			continue;
		for (size_t i = 0;
		     i < sizeof(internal_map) / sizeof(internal_map[0]);
		     i++)
			if (internal_map[i].ch == mapped)
				mapped = (int)i;
		if (mapped < 0 ||
		    mapped >= (int)(sizeof(internal_map) /
				    sizeof(internal_map[0])))
			continue;
		snprintf(lpath, sizeof(lpath), "%s/%s", path, e->d_name);
		if (strstr(e->d_name, "_input")) {
			if (read_double_file(lpath, &mv) != 0)
				continue;
		} else if (strstr(e->d_name, "_raw")) {
			if (read_double_file(lpath, &mv) != 0)
				continue;
			mv *= gscale;
		} else {
			continue;
		}
		for (guint i = 0; i < volts->len; i++) {
			series_t *s = g_ptr_array_index(volts, i);

			if (strncmp(s->name, internal_map[mapped].name,
				    strlen(internal_map[mapped].name)) == 0)
				series_append(s, mv);
		}
	}
	closedir(d);
}

static gboolean queue_draw_idle(gpointer user)
{
	char txt[256];
	size_t o = 0;

	gtk_widget_queue_draw(GTK_WIDGET(user));
	o += snprintf(txt + o, sizeof(txt) - o, "gui values: %.1f C",
		      s_temp.last);
	for (guint i = 0; i < volts->len && o < sizeof(txt) - 24; i++) {
		series_t *s = g_ptr_array_index(volts, i);

		o += snprintf(txt + o, sizeof(txt) - o, " | %s: %.1f",
			      s->name, s->last);
	}
	printf("%s\n", txt);
	return G_SOURCE_REMOVE;
}

static void *sample_thread_fn(void *arg)
{
	(void)arg;
	while (1) {
		sample_tick_scan();
		g_main_context_invoke(NULL, queue_draw_idle, plot_area);
		usleep(SAMPLE_MS * 1000);
	}
	return NULL;
}

static gboolean sample_tick_fallback(gpointer user)
{
	(void)user;
	sample_tick_scan();
	gtk_widget_queue_draw(plot_area);
	return G_SOURCE_CONTINUE;
}

static void draw_series(cairo_t *cr, double x, double y, double w, double h,
			series_t *s, double lo, double hi)
{
	cairo_set_source_rgb(cr, 0.2, 0.9, 0.9);
	cairo_set_line_width(cr, 1.6);
	for (int i = 0; i < s->count; i++) {
		double px = x + (s->count > 1 ? (double)i / (NPOINTS - 1) *
				 w : 0);
		double span = hi - lo;

		if (span < 1e-9)
			span = 1;
		double py = y + h - (s->vals[i] - lo) / span * h;

		if (i == 0)
			cairo_move_to(cr, px, py);
		else
			cairo_line_to(cr, px, py);
	}
	cairo_stroke(cr);
}

static gboolean draw_cb(GtkWidget *widget, cairo_t *cr, gpointer user)
{
	(void)user;
	GtkAllocation a;
	int npanels = 1 + (int)volts->len;
	double gap = 10;
	double pw;
	double ph;
	double py;

	gtk_widget_get_allocation(widget, &a);

	cairo_set_source_rgb(cr, 0.06, 0.09, 0.12);
	cairo_paint(cr);

	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 10);

	if (npanels > MAX_PANELS)
		npanels = MAX_PANELS;
	pw = (a.width - 20 - gap * (npanels - 1)) / npanels;
	ph = a.height - 46;
	py = 24;

	for (int p = 0; p < npanels; p++) {
		double px = 10 + p * (pw + gap);
		series_t *s = NULL;
		char title[96];
		char txt[64];
		double lo = 1e18, hi = -1e18;
		double col[3] = { 0.2, 0.9, 0.9 };

		if (p == 0) {
			s = &s_temp;
			snprintf(title, sizeof(title), "%s", s->name);
			col[0] = 1.0;
			col[1] = 0.6;
			col[2] = 0.1;
		} else if ((int)volts->len >= p) {
			static const double pal[7][3] = {
				{ 0.2, 0.9, 0.9 }, { 0.9, 0.3, 0.9 },
				{ 0.9, 0.9, 0.2 }, { 0.3, 0.9, 0.3 },
				{ 0.9, 0.4, 0.4 }, { 0.6, 0.6, 1.0 },
				{ 1.0, 1.0, 1.0 },
			};
			int pi = (p - 1) % 7;

			s = g_ptr_array_index(volts, p - 1);
			snprintf(title, sizeof(title), "%s", s->name);
			col[0] = pal[pi][0];
			col[1] = pal[pi][1];
			col[2] = pal[pi][2];
		} else {
			snprintf(title, sizeof(title), "n/a");
		}

		cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
		cairo_move_to(cr, px, py - 6);
		cairo_show_text(cr, title);

		cairo_set_source_rgb(cr, 0.25, 0.28, 0.32);
		cairo_rectangle(cr, px, py, pw, ph);
		cairo_stroke(cr);
		for (int g = 1; g < 4; g++) {
			cairo_move_to(cr, px, py + ph * g / 4);
			cairo_line_to(cr, px + pw, py + ph * g / 4);
			cairo_stroke(cr);
		}

		if (!s || s->count == 0) {
			cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
			cairo_move_to(cr, px + 8, py + ph / 2);
			cairo_show_text(cr, "no data");
			continue;
		}
		for (int i = 0; i < s->count; i++) {
			if (s->vals[i] < lo)
				lo = s->vals[i];
			if (s->vals[i] > hi)
				hi = s->vals[i];
		}
		{
			double pad = (hi - lo) * 0.12;
			double span;

			if (pad < 1e-6)
				pad = hi * 0.05 + 0.5;
			lo -= pad;
			hi += pad;
			span = hi - lo;
			cairo_set_source_rgb(cr, col[0], col[1], col[2]);
			cairo_set_line_width(cr, 1.8);
			for (int i = 0; i < s->count; i++) {
				double vpx = px +
					(s->count > 1 ?
					 (double)i / (NPOINTS - 1) * pw : 0);
				double vpy = py + ph -
					(s->vals[i] - lo) / span * ph;

				if (i == 0)
					cairo_move_to(cr, vpx, vpy);
				else
					cairo_line_to(cr, vpx, vpy);
			}
			cairo_stroke(cr);
		}
		cairo_set_source_rgb(cr, col[0], col[1], col[2]);
		cairo_move_to(cr, px + pw / 2 - 20, py + ph - 8);
		snprintf(txt, sizeof(txt), "%.2f", s->last);
		cairo_show_text(cr, txt);
	}

	return FALSE;
}

static int capture_device(char *dev, size_t len)
{
	for (int i = 0; i < 16; i++) {
		char path[64];
		struct v4l2_capability cap;
		int fd;

		snprintf(path, sizeof(path), "/dev/video%d", i);
		fd = open(path, O_RDWR);
		if (fd < 0)
			continue;
		memset(&cap, 0, sizeof(cap));
		if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0 &&
		    (cap.device_caps & V4L2_CAP_VIDEO_CAPTURE)) {
			close(fd);
			snprintf(dev, len, "/dev/video%d", i);
			return 0;
		}
		close(fd);
	}
	return -1;
}

static void cam_try_destroy(cam_try_t *t)
{
	if (t->widget)
		gtk_widget_destroy(t->widget);
	if (t->pipe) {
		gst_element_set_state(t->pipe, GST_STATE_NULL);
		gst_object_unref(t->pipe);
	}
	t->widget = NULL;
	t->pipe = NULL;
}

static int cam_try_build(cam_try_t *t, const char *dev, const char *mode,
			 const char *sinkname)
{
	gchar *desc;

	t->widget = NULL;
	t->pipe = NULL;
	desc = g_strdup_printf(
		"v4l2src device=%s ! %s ! videoconvert ! %s name=s",
		dev, mode, sinkname);
	printf("mini-gui: trying %s\n", desc);
	t->pipe = gst_parse_launch(desc, NULL);
	g_free(desc);
	if (!t->pipe)
		return -1;
	{
		GstElement *s = gst_bin_get_by_name(GST_BIN(t->pipe), "s");

		if (!s) {
			gst_object_unref(t->pipe);
			t->pipe = NULL;
			return -1;
		}
		g_object_set(s, "force-aspect-ratio", TRUE, NULL);
		g_object_get(s, "widget", &t->widget, NULL);
		gst_object_unref(s);
	}
	if (!t->widget) {
		cam_try_destroy(t);
		return -1;
	}
	return 0;
}


static GtkWidget *build_camera(GtkWidget *parent_box)
{
	static const char *sinks[] = { "gtkglsink", "gtksink" };
	static const char *modes[] = {
		"image/jpeg ! jpegdec",
		"video/x-raw,format=YUY2",
		"video/x-raw",
	};
	char dev[64];
	GtkWidget *cam = NULL;

	if (capture_device(dev, sizeof(dev)) < 0) {
		printf("mini-gui: no capture-capable /dev/video node\n");
		return NULL;
	}

	for (size_t si = 0; si < sizeof(sinks) / sizeof(sinks[0]) && !cam;
	     si++) {
		for (size_t m = 0;
		     m < sizeof(modes) / sizeof(modes[0]) && !cam; m++) {
			cam_try_t t;
			GstStateChangeReturn ret;

			if (cam_try_build(&t, dev, modes[m], sinks[si]) != 0)
				continue;
			gtk_box_pack_start(GTK_BOX(parent_box), t.widget,
					   TRUE, TRUE, 0);
			gtk_widget_set_hexpand(t.widget, TRUE);
			gtk_widget_set_vexpand(t.widget, TRUE);
			gtk_widget_show(t.widget);
			gtk_widget_realize(t.widget);
			while (gtk_events_pending())
				gtk_main_iteration_do(FALSE);
			gst_element_set_state(t.pipe, GST_STATE_PLAYING);
			ret = gst_element_get_state(t.pipe, NULL, NULL,
						    4 * GST_SECOND);
			if (ret == GST_STATE_CHANGE_SUCCESS) {
				cam = t.widget;
				printf("mini-gui: camera live on %s "
				       "(%s, %s)\n", dev, sinks[si], modes[m]);
			} else {
				GstBus *bus = gst_element_get_bus(t.pipe);
				GstMessage *msg = gst_bus_pop_filtered(
					bus, GST_MESSAGE_ERROR);

				printf("mini-gui: %s + %s failed\n",
				       sinks[si], modes[m]);
				if (msg) {
					GError *e = NULL;
					gchar *dbg = NULL;

					gst_message_parse_error(msg, &e,
								&dbg);
					printf("  bus: %s (%s)\n", e->message,
					       dbg ? dbg : "");
					g_clear_error(&e);
					g_free(dbg);
					gst_message_unref(msg);
				}
				if (bus)
					gst_object_unref(bus);
				cam_try_destroy(&t);
			}
		}
	}
	return cam;
}

int main(int argc, char **argv)
{
	GtkWidget *win, *vbox, *cam, *plot;

	setvbuf(stdout, NULL, _IOLBF, 0);
	gtk_init(&argc, &argv);
	gst_init(&argc, &argv);

	scan_sensors();
	printf("mini-gui: temp series + %d internal ADC series\n",
	       volts->len);

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(win), "mini-gui - STM32MP157 DK1");
	gtk_window_set_default_size(GTK_WINDOW(win), WIN_W, WIN_H);
	g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	{
		GtkCssProvider *css = gtk_css_provider_new();

		gtk_css_provider_load_from_data(css,
			"window { background-color: black; }", -1, NULL);
		gtk_style_context_add_provider(
			gtk_widget_get_style_context(win),
			GTK_STYLE_PROVIDER(css),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_unref(css);
	}

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_add(GTK_CONTAINER(win), vbox);

	cam = build_camera(vbox);
	if (cam) {
		/* already packed into vbox by build_camera() */
	} else {
		GtkWidget *lbl = gtk_label_new(
			"No UVC camera available\n"
			"Check wiring/other capture apps and restart");

		gtk_widget_set_size_request(lbl, WIN_W, WIN_H - PLOT_H);
		gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
		gtk_box_pack_start(GTK_BOX(vbox), lbl, TRUE, TRUE, 0);
		printf("mini-gui: no working camera pipeline - showing "
		       "strings\n");
	}

	plot_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(plot_area, WIN_W, PLOT_H);
	gtk_box_pack_start(GTK_BOX(vbox), plot_area, TRUE, TRUE, 0);
	g_signal_connect(plot_area, "draw", G_CALLBACK(draw_cb), NULL);

	{ pthread_t spt; if (pthread_create(&spt, NULL, sample_thread_fn, NULL) != 0) g_timeout_add(SAMPLE_MS, sample_tick_fallback, NULL); }

	gtk_widget_show_all(win);
	gtk_main();
	return 0;
}
