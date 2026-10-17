#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
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

static void on_usr1(int sig)
{
	(void)sig;
}

static long read_msg(char *msg, size_t n)
{
	char path[64], first[32];
	FILE *f;
	long expiry;

	snprintf(path, sizeof path, "/tmp/bar-msg-%u", (unsigned)getuid());
	f = fopen(path, "r");
	if (!f)
		return 0;
	msg[0] = '\0';
	if (!fgets(first, sizeof first, f)) {
		fclose(f);
		return 0;
	}
	expiry = atol(first);
	if (!fgets(msg, n, f))
		msg[0] = '\0';
	fclose(f);
	msg[strcspn(msg, "\n")] = '\0';
	return msg[0] ? expiry : 0;
}

int main(void)
{
	static const char *bats[] = { "BAT0", "BAT1" };
	Display *dpy = XOpenDisplay(NULL);
	Window root;
	struct sigaction sa;
	sigset_t block, old;

	if (!dpy) {
		fputs("status: cannot open display\n", stderr);
		return 1;
	}
	root = DefaultRootWindow(dpy);

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_usr1;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGUSR1, &sa, NULL);
	sigemptyset(&block);
	sigaddset(&block, SIGUSR1);
	sigprocmask(SIG_BLOCK, &block, &old);

	for (;;) {
		char cap[16] = "", raw[32] = "", path[96], bar[512], tbuf[64], msg[384];
		long expiry;
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

		expiry = read_msg(msg, sizeof msg);
		if (expiry > (long)now)
			snprintf(bar, sizeof bar, "%s", msg);
		else
			expiry = 0;

		XStoreName(dpy, root, bar);
		XFlush(dpy);

		if (expiry)
			ts.tv_sec = expiry - (long)now;
		else
			ts.tv_sec = tm.tm_sec > 59 ? 1 : 60 - tm.tm_sec;
		pselect(0, NULL, NULL, NULL, &ts, &old);
	}
}
