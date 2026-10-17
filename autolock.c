#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>

static int64_t idle_ms(Display *dpy, XSyncCounter c)
{
	XSyncValue v;

	if (!XSyncQueryCounter(dpy, c, &v))
		return -1;
	return ((int64_t)XSyncValueHigh32(v) << 32) | XSyncValueLow32(v);
}

int main(int argc, char **argv)
{
	Display *dpy;
	XSyncSystemCounter *list;
	XSyncCounter counter = None;
	int ev, er, maj, min, n, i;
	long limit;

	if (argc < 3 || (limit = atol(argv[1])) <= 0) {
		fputs("usage: autolock seconds command [args...]\n", stderr);
		return 1;
	}
	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fputs("autolock: cannot open display\n", stderr);
		return 1;
	}
	if (!XSyncQueryExtension(dpy, &ev, &er) || !XSyncInitialize(dpy, &maj, &min)) {
		fputs("autolock: SYNC extension missing\n", stderr);
		return 1;
	}
	list = XSyncListSystemCounters(dpy, &n);
	for (i = 0; list && i < n; i++)
		if (!strcmp(list[i].name, "IDLETIME"))
			counter = list[i].counter;
	if (list)
		XSyncFreeSystemCounterList(list);
	if (counter == None) {
		fputs("autolock: IDLETIME counter missing\n", stderr);
		return 1;
	}

	for (;;) {
		int64_t idle = idle_ms(dpy, counter), wait;

		if (idle < 0)
			return 1;
		if (idle >= (int64_t)limit * 1000) {
			pid_t pid = fork();

			if (pid == 0) {
				execvp(argv[2], argv + 2);
				_exit(1);
			}
			if (pid > 0)
				waitpid(pid, NULL, 0);
			sleep(2);
			continue;
		}
		wait = ((int64_t)limit * 1000 - idle) / 1000 + 1;
		sleep((unsigned)wait);
	}
}
