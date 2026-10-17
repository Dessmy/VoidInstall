#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

int main(int argc, char **argv)
{
	Display *dpy = XOpenDisplay(NULL);
	int i;

	if (!dpy) {
		fputs("screenctl: cannot open display\n", stderr);
		return 1;
	}
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "nobell")) {
			XKeyboardControl kc;

			kc.bell_percent = 0;
			XChangeKeyboardControl(dpy, KBBellPercent, &kc);
		} else if (!strcmp(argv[i], "noblank")) {
			int timeout, interval, blanking, exposures;

			XGetScreenSaver(dpy, &timeout, &interval, &blanking, &exposures);
			XSetScreenSaver(dpy, 0, interval, blanking, exposures);
		} else if (!strcmp(argv[i], "dpms") && i + 3 < argc) {
			int ev, er;

			if (DPMSQueryExtension(dpy, &ev, &er) && DPMSCapable(dpy)) {
				DPMSSetTimeouts(dpy, atoi(argv[i + 1]), atoi(argv[i + 2]),
					atoi(argv[i + 3]));
				DPMSEnable(dpy);
			}
			i += 3;
		} else if (!strcmp(argv[i], "dpmsoff")) {
			int ev, er;

			if (DPMSQueryExtension(dpy, &ev, &er) && DPMSCapable(dpy))
				DPMSDisable(dpy);
		} else {
			fprintf(stderr, "screenctl: unknown argument: %s\n", argv[i]);
			XCloseDisplay(dpy);
			return 1;
		}
	}
	XCloseDisplay(dpy);
	return 0;
}
