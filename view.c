#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#define BG "0x1e1e2e"

static Display *dpy;
static Window win;
static GC gc;
static XImage *img;
static unsigned char *buf;
static int ww, wh, depth, failed;
static char **files;
static int nfiles, cur;
static Atom wm_delete;
static double zoom = 1.0;
static int actual, panx, pany;

static int decode(const char *path, int w, int h)
{
	char graph[768], scalef[320];
	struct stat st;
	int fd[2], nul;
	pid_t pid;
	size_t want = (size_t)w * h * 4, got = 0;
	time_t deadline = time(NULL) + 15;

	if (stat(path, &st) || !S_ISREG(st.st_mode) || pipe(fd))
		return 0;
	if (actual)
		snprintf(scalef, sizeof scalef, "[0:v]scale=iw:ih[z];");
	else
		snprintf(scalef, sizeof scalef,
			"[0:v]scale=w='max(1,trunc(iw*min(%d/iw,%d/ih)*%.4f))':"
			"h='max(1,trunc(ih*min(%d/iw,%d/ih)*%.4f))'[z];",
			w, h, zoom, w, h, zoom);
	snprintf(graph, sizeof graph,
		"color=c=" BG ":s=%dx%d[bg];%s"
		"[bg][z]overlay="
		"x='clip((W-w)/2+%d,min(0,W-w),max(0,W-w))':"
		"y='clip((H-h)/2+%d,min(0,H-h),max(0,H-h))':format=auto",
		w, h, scalef, panx, pany);
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
			"-filter_complex", graph, "-frames:v", "1",
			"-f", "rawvideo", "-pix_fmt", "bgr0", "-", (char *)NULL);
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

static void settitle(void)
{
	char t[1024], z[32] = "";

	if (actual)
		snprintf(z, sizeof z, " [1:1]");
	else if (zoom != 1.0)
		snprintf(z, sizeof z, " [x%.2f]", zoom);
	snprintf(t, sizeof t, "%s (%d/%d)%s%s", files[cur], cur + 1, nfiles, z,
		failed ? " [error]" : "");
	XStoreName(dpy, win, t);
}

static void clamp_pan(void)
{
	double bx = actual ? 2.0 * ww : (ww * zoom - ww) / 2;
	double by = actual ? 2.0 * wh : (wh * zoom - wh) / 2;

	if (bx < 0)
		bx = 0;
	if (by < 0)
		by = 0;
	if (panx > bx)
		panx = (int)bx;
	if (panx < -bx)
		panx = (int)-bx;
	if (pany > by)
		pany = (int)by;
	if (pany < -by)
		pany = (int)-by;
}

static void resetview(void)
{
	zoom = 1.0;
	actual = 0;
	panx = pany = 0;
}

static void reload(void)
{
	size_t i;

	clamp_pan();
	if (img) {
		img->data = NULL;
		XDestroyImage(img);
		img = NULL;
	}
	free(buf);
	buf = malloc((size_t)ww * wh * 4);
	if (!buf)
		exit(1);
	failed = !decode(files[cur], ww, wh);
	if (failed)
		for (i = 0; i < (size_t)ww * wh; i++) {
			buf[i * 4] = 0x2e;
			buf[i * 4 + 1] = 0x1e;
			buf[i * 4 + 2] = 0x1e;
			buf[i * 4 + 3] = 0;
		}
	img = XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), depth,
		ZPixmap, 0, (char *)buf, ww, wh, 32, 0);
	settitle();
}

static void draw(void)
{
	if (img)
		XPutImage(dpy, win, gc, img, 0, 0, 0, 0, ww, wh);
}

int main(int argc, char **argv)
{
	int scr, dirty = 1;
	XClassHint ch = { "view", "view" };
	XEvent ev;

	if (argc < 2) {
		fputs("usage: view file...\n", stderr);
		return 1;
	}
	files = argv + 1;
	nfiles = argc - 1;
	signal(SIGPIPE, SIG_IGN);

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fputs("view: cannot open display\n", stderr);
		return 1;
	}
	scr = DefaultScreen(dpy);
	depth = DefaultDepth(dpy, scr);
	if (depth != 24 && depth != 32) {
		fputs("view: unsupported display depth\n", stderr);
		return 1;
	}
	ww = DisplayWidth(dpy, scr) * 3 / 4;
	wh = DisplayHeight(dpy, scr) * 3 / 4;
	win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, ww, wh, 0,
		BlackPixel(dpy, scr), BlackPixel(dpy, scr));
	XSetClassHint(dpy, win, &ch);
	XSelectInput(dpy, win, KeyPressMask | ExposureMask | StructureNotifyMask);
	wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wm_delete, 1);
	gc = XCreateGC(dpy, win, 0, NULL);
	XMapWindow(dpy, win);

	for (;;) {
		do {
			XNextEvent(dpy, &ev);
			switch (ev.type) {
			case Expose:
				if (ev.xexpose.count == 0)
					draw();
				break;
			case ConfigureNotify:
				if (ev.xconfigure.width > 0 && ev.xconfigure.height > 0 &&
				    (ev.xconfigure.width != ww || ev.xconfigure.height != wh)) {
					ww = ev.xconfigure.width;
					wh = ev.xconfigure.height;
					dirty = 1;
				}
				break;
			case ClientMessage:
				if ((Atom)ev.xclient.data.l[0] == wm_delete)
					return 0;
				break;
			case KeyPress:
				switch (XLookupKeysym(&ev.xkey, 0)) {
				case XK_q:
				case XK_Escape:
					return 0;
				case XK_space:
				case XK_n:
				case XK_Right:
					cur = (cur + 1) % nfiles;
					resetview();
					dirty = 1;
					break;
				case XK_p:
				case XK_BackSpace:
				case XK_Left:
					cur = (cur + nfiles - 1) % nfiles;
					resetview();
					dirty = 1;
					break;
				case XK_Up:
				case XK_plus:
				case XK_equal:
				case XK_KP_Add:
					actual = 0;
					zoom = zoom * 1.25 > 32 ? 32 : zoom * 1.25;
					dirty = 1;
					break;
				case XK_Down:
				case XK_minus:
				case XK_KP_Subtract:
					actual = 0;
					zoom = zoom / 1.25 < 0.1 ? 0.1 : zoom / 1.25;
					dirty = 1;
					break;
				case XK_0:
					resetview();
					dirty = 1;
					break;
				case XK_1:
					resetview();
					actual = 1;
					dirty = 1;
					break;
				case XK_h:
					panx += ww / 10;
					dirty = 1;
					break;
				case XK_l:
					panx -= ww / 10;
					dirty = 1;
					break;
				case XK_k:
					pany += wh / 10;
					dirty = 1;
					break;
				case XK_j:
					pany -= wh / 10;
					dirty = 1;
					break;
				}
				break;
			}
		} while (XPending(dpy));
		if (dirty) {
			reload();
			draw();
			dirty = 0;
		}
	}
}
