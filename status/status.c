#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>

#ifndef PS_DIR
#define PS_DIR "/sys/class/power_supply"
#endif

static int read_line(const char *path, char *buf, size_t n)
{
	FILE *f = fopen(path, "r");

	if (!f)
		return 0;
	if (!fgets(buf, n, f))
		buf[0] = '\0';
	fclose(f);
	buf[strcspn(buf, "\n")] = '\0';
	return buf[0] != '\0';
}

int main(void)
{
	static const char *bats[] = { "BAT0", "BAT1" };
	Display *dpy = XOpenDisplay(NULL);
	Window root;

	if (!dpy) {
		fputs("status: cannot open display\n", stderr);
		return 1;
	}
	root = DefaultRootWindow(dpy);

	for (;;) {
		char cap[16] = "", raw[32] = "", path[96], bar[160], tbuf[64];
		const char *st;
		size_t i;
		time_t now = time(NULL);
		struct tm tm;
		struct timespec ts = { 0, 0 };

		localtime_r(&now, &tm);
		strftime(tbuf, sizeof tbuf, "%a, %b %e   %I:%M %p", &tm);

		for (i = 0; i < sizeof bats / sizeof *bats; i++) {
			snprintf(path, sizeof path, PS_DIR "/%s/capacity", bats[i]);
			if (read_line(path, cap, sizeof cap)) {
				snprintf(path, sizeof path, PS_DIR "/%s/status", bats[i]);
				read_line(path, raw, sizeof raw);
				break;
			}
		}

		if (!strcmp(raw, "Charging"))          st = "Chg";
		else if (!strcmp(raw, "Full"))         st = "Full";
		else if (!strcmp(raw, "Discharging"))  st = "Dis";
		else if (!strcmp(raw, "Not charging")) st = "Idle";
		else                                   st = raw[0] ? raw : "n/a";

		if (cap[0])
			snprintf(bar, sizeof bar, "BAT %s%% (%s)   %s", cap, st, tbuf);
		else /* no battery (VM, desktop) */
			snprintf(bar, sizeof bar, "%s", tbuf);

		XStoreName(dpy, root, bar);
		XFlush(dpy);

		/* sleep until the next full minute */
		ts.tv_sec = tm.tm_sec > 59 ? 1 : 60 - tm.tm_sec;
		nanosleep(&ts, NULL);
	}
}
