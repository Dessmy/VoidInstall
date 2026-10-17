#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

static int ignore_error(Display *d, XErrorEvent *e)
{
	(void)d;
	(void)e;
	return 0;
}

static int decode(const char *path, unsigned char *buf, int w, int h)
{
	char vf[128];
	struct stat st;
	int fd[2], nul;
	pid_t pid;
	size_t want = (size_t)w * h * 4, got = 0;
	time_t deadline = time(NULL) + 30;

	if (stat(path, &st) || !S_ISREG(st.st_mode) || pipe(fd))
		return 0;
	snprintf(vf, sizeof vf,
		"scale=%d:%d:force_original_aspect_ratio=increase,crop=%d:%d", w, h, w, h);
	pid = fork();
	if (pid < 0) {
		close(fd[0]);
		close(fd[1]);
		return 0;
	}
	if (pid == 0) {
		dup2(fd[1], 1);
		close(fd[0]);
		close(fd[1]);
		nul = open("/dev/null", O_RDWR);
		if (nul >= 0) {
			dup2(nul, 0);
			dup2(nul, 2);
		}
		execlp("ffmpeg", "ffmpeg", "-v", "quiet", "-nostdin", "-i", path,
			"-frames:v", "1", "-vf", vf, "-f", "rawvideo",
			"-pix_fmt", "bgr0", "-", (char *)NULL);
		_exit(1);
	}
	close(fd[1]);
	while (got < want && time(NULL) < deadline) {
		fd_set r;
		struct timeval tv = { 1, 0 };
		ssize_t n;

		FD_ZERO(&r);
		FD_SET(fd[0], &r);
		if (select(fd[0] + 1, &r, NULL, NULL, &tv) <= 0)
			continue;
		n = read(fd[0], buf + got, want - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	close(fd[0]);
	if (got < want)
		kill(pid, SIGKILL);
	waitpid(pid, NULL, 0);
	return got == want;
}

static void kill_old(Display *dpy, Window root, Atom prop)
{
	Atom type;
	int fmt;
	unsigned long n, after;
	unsigned char *data = NULL;

	if (XGetWindowProperty(dpy, root, prop, 0, 1, False, XA_PIXMAP, &type,
	    &fmt, &n, &after, &data) == Success && data) {
		if (type == XA_PIXMAP && fmt == 32 && n == 1)
			XKillClient(dpy, *(Pixmap *)data);
		XFree(data);
	}
}

int main(int argc, char **argv)
{
	Display *dpy;
	Window root;
	Pixmap pm;
	GC gc;
	XImage *img;
	Atom a1, a2;
	unsigned char *buf;
	int scr, w, h, depth;

	if (argc != 2) {
		fputs("usage: setbg image\n", stderr);
		return 1;
	}
	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fputs("setbg: cannot open display\n", stderr);
		return 1;
	}
	scr = DefaultScreen(dpy);
	root = RootWindow(dpy, scr);
	w = DisplayWidth(dpy, scr);
	h = DisplayHeight(dpy, scr);
	depth = DefaultDepth(dpy, scr);
	if (depth != 24 && depth != 32) {
		fputs("setbg: unsupported display depth\n", stderr);
		return 1;
	}
	buf = malloc((size_t)w * h * 4);
	if (!buf || !decode(argv[1], buf, w, h)) {
		fputs("setbg: cannot load image\n", stderr);
		return 1;
	}

	XSetErrorHandler(ignore_error);
	a1 = XInternAtom(dpy, "_XROOTPMAP_ID", False);
	a2 = XInternAtom(dpy, "ESETROOT_PMAP_ID", False);
	XSetCloseDownMode(dpy, RetainPermanent);
	kill_old(dpy, root, a1);
	kill_old(dpy, root, a2);

	pm = XCreatePixmap(dpy, root, w, h, depth);
	gc = XCreateGC(dpy, pm, 0, NULL);
	img = XCreateImage(dpy, DefaultVisual(dpy, scr), depth, ZPixmap, 0,
		(char *)buf, w, h, 32, 0);
	XPutImage(dpy, pm, gc, img, 0, 0, 0, 0, w, h);

	XChangeProperty(dpy, root, a1, XA_PIXMAP, 32, PropModeReplace,
		(unsigned char *)&pm, 1);
	XChangeProperty(dpy, root, a2, XA_PIXMAP, 32, PropModeReplace,
		(unsigned char *)&pm, 1);
	XSetWindowBackgroundPixmap(dpy, root, pm);
	XClearWindow(dpy, root);
	XSync(dpy, False);
	XCloseDisplay(dpy);
	return 0;
}
