#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#define BG "0x1e1e2e"

static Display *dpy;
static Window win;
static GC gc;
static XImage *img;
static unsigned char *fb, *shown;
static size_t fbsize, fill;
static int ww, wh, vw, vh, depth;
static char **files;
static int nfiles, cur;
static pid_t ff = -1;
static int pfd = -1;
static double base, tstart, pausepos, started;
static int paused, noaudio, got_frame, nframes;
static Atom wm_delete;

static double now_s(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static double position(void)
{
	return paused ? pausepos : base + (now_s() - tstart);
}

static void stop_ff(void)
{
	if (ff > 0) {
		kill(ff, SIGCONT);
		kill(ff, SIGKILL);
		waitpid(ff, NULL, 0);
		ff = -1;
	}
	if (pfd >= 0) {
		close(pfd);
		pfd = -1;
	}
}

static void start_ff(double pos)
{
	char ss[32], vf[256], *argv[32];
	int fd[2], nul, n = 0;

	stop_ff();
	vw = ww & ~1;
	vh = wh & ~1;
	if (vw < 2 || vh < 2)
		return;
	free(fb);
	free(shown);
	fbsize = (size_t)vw * vh * 4;
	fb = calloc(1, fbsize);
	shown = calloc(1, fbsize);
	if (!fb || !shown)
		exit(1);
	fill = 0;
	if (img) {
		img->data = NULL;
		XDestroyImage(img);
	}
	img = XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), depth,
		ZPixmap, 0, (char *)shown, vw, vh, 32, 0);

	snprintf(ss, sizeof ss, "%.3f", pos);
	snprintf(vf, sizeof vf,
		"scale=%d:%d:force_original_aspect_ratio=decrease,"
		"pad=%d:%d:(ow-iw)/2:(oh-ih)/2:color=" BG, vw, vh, vw, vh);
	argv[n++] = "ffmpeg";
	argv[n++] = "-v";
	argv[n++] = "quiet";
	argv[n++] = "-nostdin";
	argv[n++] = "-ss";
	argv[n++] = ss;
	argv[n++] = "-re";
	argv[n++] = "-i";
	argv[n++] = files[cur];
	argv[n++] = "-map";
	argv[n++] = "0:v:0";
	argv[n++] = "-vf";
	argv[n++] = vf;
	argv[n++] = "-f";
	argv[n++] = "rawvideo";
	argv[n++] = "-pix_fmt";
	argv[n++] = "bgr0";
	argv[n++] = "pipe:1";
	if (!noaudio) {
		argv[n++] = "-map";
		argv[n++] = "0:a:0?";
		argv[n++] = "-f";
		argv[n++] = "pulse";
		argv[n++] = "default";
	}
	argv[n] = NULL;

	if (pipe(fd))
		return;
	ff = fork();
	if (ff < 0) {
		close(fd[0]);
		close(fd[1]);
		return;
	}
	if (ff == 0) {
		dup2(fd[1], 1);
		close(fd[0]);
		close(fd[1]);
		nul = open("/dev/null", O_RDWR);
		if (nul >= 0) {
			dup2(nul, 0);
			dup2(nul, 2);
		}
		execvp("ffmpeg", argv);
		_exit(1);
	}
	close(fd[1]);
	pfd = fd[0];
	base = pos;
	tstart = started = now_s();
	paused = 0;
	got_frame = 0;
	nframes = 0;
}

static void settitle(void)
{
	char t[1024], l[32] = "";
	int s = (int)position();

	if (nfiles > 1)
		snprintf(l, sizeof l, " (%d/%d)", cur + 1, nfiles);
	snprintf(t, sizeof t, "%s%s  %d:%02d%s", files[cur], l, s / 60, s % 60,
		paused ? " [paused]" : "");
	XStoreName(dpy, win, t);
}

static void draw(void)
{
	if (img && got_frame)
		XPutImage(dpy, win, gc, img, 0, 0, 0, 0, vw, vh);
}

static void next_file(int step)
{
	cur = (cur + nfiles + step) % nfiles;
	noaudio = 0;
	start_ff(0);
}

static void seek(double delta)
{
	double p = position() + delta;

	start_ff(p < 0 ? 0 : p);
}

static void toggle_pause(void)
{
	if (ff <= 0)
		return;
	if (paused) {
		start_ff(pausepos);
	} else {
		pausepos = position();
		kill(ff, SIGSTOP);
		paused = 1;
	}
}

static void read_frames(void)
{
	ssize_t n = read(pfd, fb + fill, fbsize - fill);

	if (n > 0) {
		fill += (size_t)n;
		if (fill == fbsize) {
			unsigned char *t = fb;

			fb = shown;
			shown = t;
			img->data = (char *)shown;
			got_frame = 1;
			nframes++;
			fill = 0;
			draw();
		}
		return;
	}
	if (nframes <= 2 && !noaudio && now_s() - started < 3) {
		noaudio = 1;
		start_ff(base);
		return;
	}
	stop_ff();
	if (cur + 1 < nfiles)
		next_file(1);
	else
		exit(0);
}

int main(int argc, char **argv)
{
	int scr, dirty = 1;
	double lasttitle = 0;
	XClassHint ch = { "play", "play" };
	XEvent ev;

	if (argc < 2) {
		fputs("usage: play file...\n", stderr);
		return 1;
	}
	files = argv + 1;
	nfiles = argc - 1;
	signal(SIGPIPE, SIG_IGN);

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fputs("play: cannot open display\n", stderr);
		return 1;
	}
	scr = DefaultScreen(dpy);
	depth = DefaultDepth(dpy, scr);
	if (depth != 24 && depth != 32) {
		fputs("play: unsupported display depth\n", stderr);
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
		fd_set r;
		struct timeval tv = { 1, 0 };
		int xfd = ConnectionNumber(dpy), maxfd = xfd;

		while (XPending(dpy)) {
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
				if ((Atom)ev.xclient.data.l[0] == wm_delete) {
					stop_ff();
					return 0;
				}
				break;
			case KeyPress:
				switch (XLookupKeysym(&ev.xkey, 0)) {
				case XK_q:
				case XK_Escape:
					stop_ff();
					return 0;
				case XK_space:
					toggle_pause();
					break;
				case XK_Right:
					seek(10);
					break;
				case XK_Left:
					seek(-10);
					break;
				case XK_Up:
					seek(60);
					break;
				case XK_Down:
					seek(-60);
					break;
				case XK_n:
					next_file(1);
					break;
				case XK_p:
					next_file(-1);
					break;
				}
				break;
			}
		}
		if (dirty) {
			start_ff(ff > 0 || got_frame ? position() : 0);
			dirty = 0;
		}

		FD_ZERO(&r);
		FD_SET(xfd, &r);
		if (pfd >= 0) {
			FD_SET(pfd, &r);
			if (pfd > maxfd)
				maxfd = pfd;
		}
		if (select(maxfd + 1, &r, NULL, NULL, &tv) > 0 && pfd >= 0 &&
		    FD_ISSET(pfd, &r))
			read_frames();
		if (now_s() - lasttitle >= 1) {
			settitle();
			lasttitle = now_s();
		}
	}
}
