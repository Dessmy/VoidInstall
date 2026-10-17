#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#ifndef PS_DIR
#define PS_DIR "/sys/class/power_supply"
#endif

#define NOTIF_NAME  "org.freedesktop.Notifications"
#define NOTIF_PATH  "/org/freedesktop/Notifications"

#define POP_SIZE    96
#define POP_BYTES   (POP_SIZE * POP_SIZE * 4)
#define POP_Y       44
#define POP_MARGIN  12

#define CLIP_SLOTS  50
#define CLIP_MAX    65536

typedef struct {
	unsigned char b[4096];
	size_t n;
} Buf;

static int bus = -1;
static uint32_t serial = 1;
static uint32_t notif_id = 0;
static unsigned char ibuf[65536];
static size_t ilen;
static char dmsg[384];
static long dexp;

static Window popwin;
static GC popgc;
static XImage *popimg;
static unsigned char popbuf[POP_BYTES], poptmp[POP_BYTES];
static int pop_ready, pop_visible, pop_dirty;
static long pop_exp, fimg_loaded;

static Window clipwin;
static Atom a_clip, a_utf8, a_prop, a_incr;
static int clip_pending, clip_head;
static time_t clip_since, clip_polled;
static char clip_dir[256];
static char clip_last[CLIP_MAX + 1];
static size_t clip_last_len;

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

static long read_msg(char *msg, size_t n, char *img, size_t in)
{
	char path[64], first[32];
	FILE *f;
	long expiry;

	img[0] = '\0';
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
	if (!fgets(img, in, f))
		img[0] = '\0';
	fclose(f);
	msg[strcspn(msg, "\n")] = '\0';
	img[strcspn(img, "\n")] = '\0';
	return msg[0] ? expiry : 0;
}

static void bpad(Buf *o, size_t a)
{
	while (o->n % a)
		o->b[o->n++] = 0;
}

static void pu32(Buf *o, uint32_t v)
{
	bpad(o, 4);
	memcpy(o->b + o->n, &v, 4);
	o->n += 4;
}

static void pstr(Buf *o, const char *s)
{
	uint32_t l = strlen(s);

	pu32(o, l);
	memcpy(o->b + o->n, s, l + 1);
	o->n += l + 1;
}

static void psig(Buf *o, const char *s)
{
	size_t l = strlen(s);

	o->b[o->n++] = (unsigned char)l;
	memcpy(o->b + o->n, s, l + 1);
	o->n += l + 1;
}

static void field_s(Buf *f, int code, const char *sig, const char *val)
{
	bpad(f, 8);
	f->b[f->n++] = (unsigned char)code;
	psig(f, sig);
	pstr(f, val);
}

static void field_u(Buf *f, int code, uint32_t v)
{
	bpad(f, 8);
	f->b[f->n++] = (unsigned char)code;
	psig(f, "u");
	pu32(f, v);
}

static void field_g(Buf *f, int code, const char *sg)
{
	bpad(f, 8);
	f->b[f->n++] = (unsigned char)code;
	psig(f, "g");
	psig(f, sg);
}

static void send_msg(int type, uint32_t reply_serial, const char *dest,
	const char *path, const char *iface, const char *member,
	const char *errname, const char *bsig, const Buf *body)
{
	static Buf f, m;
	uint32_t blen = body ? (uint32_t)body->n : 0, s = serial++;
	size_t off = 0;

	if (bus < 0)
		return;
	f.n = m.n = 0;
	if (path)
		field_s(&f, 1, "o", path);
	if (iface)
		field_s(&f, 2, "s", iface);
	if (member)
		field_s(&f, 3, "s", member);
	if (errname)
		field_s(&f, 4, "s", errname);
	if (reply_serial)
		field_u(&f, 5, reply_serial);
	if (dest)
		field_s(&f, 6, "s", dest);
	if (bsig && *bsig)
		field_g(&f, 8, bsig);

	m.b[m.n++] = 'l';
	m.b[m.n++] = (unsigned char)type;
	m.b[m.n++] = type == 1 ? 0 : 1;
	m.b[m.n++] = 1;
	pu32(&m, blen);
	pu32(&m, s);
	pu32(&m, (uint32_t)f.n);
	memcpy(m.b + m.n, f.b, f.n);
	m.n += f.n;
	bpad(&m, 8);
	if (body) {
		memcpy(m.b + m.n, body->b, body->n);
		m.n += body->n;
	}

	while (off < m.n) {
		ssize_t w = write(bus, m.b + off, m.n - off);

		if (w < 0 && errno == EINTR)
			continue;
		if (w <= 0) {
			close(bus);
			bus = -1;
			return;
		}
		off += (size_t)w;
	}
}

static int bus_connect(void)
{
	const char *addr = getenv("DBUS_SESSION_BUS_ADDRESS");
	struct sockaddr_un sa;
	socklen_t slen = 0;
	char val[sizeof sa.sun_path], uidstr[16], hex[40], line[256];
	const char *p;
	int fd, abstract = 0, have = 0;
	size_t i, n = 0;
	Buf b;

	if (!addr || strncmp(addr, "unix:", 5))
		return -1;
	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	val[0] = '\0';
	for (p = addr + 5; *p && *p != ';';) {
		const char *e = p + strcspn(p, ",;");
		size_t l = (size_t)(e - p);

		if (!strncmp(p, "path=", 5) || !strncmp(p, "abstract=", 9)) {
			const char *v = strchr(p, '=') + 1;

			abstract = p[0] == 'a';
			l = (size_t)(e - v);
			if (l >= sizeof val - 1)
				return -1;
			memcpy(val, v, l);
			val[l] = '\0';
			have = 1;
		}
		p = *e == ',' ? e + 1 : e;
	}
	if (!have)
		return -1;
	if (abstract) {
		memcpy(sa.sun_path + 1, val, strlen(val));
		slen = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(val);
	} else {
		memcpy(sa.sun_path, val, strlen(val));
		slen = sizeof sa;
	}

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	if (connect(fd, (struct sockaddr *)&sa, slen) < 0) {
		close(fd);
		return -1;
	}

	snprintf(uidstr, sizeof uidstr, "%u", (unsigned)getuid());
	for (i = 0; uidstr[i]; i++)
		snprintf(hex + 2 * i, 3, "%02x", (unsigned char)uidstr[i]);
	n = (size_t)snprintf(line, sizeof line, "%cAUTH EXTERNAL %s\r\n", 0, hex);
	if (write(fd, line, n) != (ssize_t)n) {
		close(fd);
		return -1;
	}
	for (n = 0; n < sizeof line - 1;) {
		if (read(fd, line + n, 1) != 1) {
			close(fd);
			return -1;
		}
		if (line[n++] == '\n')
			break;
	}
	line[n] = '\0';
	if (strncmp(line, "OK", 2) || write(fd, "BEGIN\r\n", 7) != 7) {
		close(fd);
		return -1;
	}

	bus = fd;
	send_msg(1, 0, "org.freedesktop.DBus", "/org/freedesktop/DBus",
		"org.freedesktop.DBus", "Hello", NULL, NULL, NULL);
	b.n = 0;
	pstr(&b, NOTIF_NAME);
	pu32(&b, 4);
	send_msg(1, 0, "org.freedesktop.DBus", "/org/freedesktop/DBus",
		"org.freedesktop.DBus", "RequestName", NULL, "su", &b);
	return bus;
}

static const char *rstr(const unsigned char *b, size_t bl, size_t *q)
{
	uint32_t l;
	const char *s;

	*q = (*q + 3) & ~(size_t)3;
	if (*q + 4 > bl)
		return NULL;
	memcpy(&l, b + *q, 4);
	*q += 4;
	if (*q + (size_t)l + 1 > bl)
		return NULL;
	s = (const char *)b + *q;
	*q += (size_t)l + 1;
	return s;
}

static void clean(char *dst, size_t n, const char *summary, const char *body)
{
	char tmp[1024];
	size_t i, o = 0;
	int tag = 0;

	if (body && *body)
		snprintf(tmp, sizeof tmp, "%s: %s", summary, body);
	else
		snprintf(tmp, sizeof tmp, "%s", summary);
	for (i = 0; tmp[i] && o + 1 < n; i++) {
		if (tmp[i] == '<')
			tag = 1;
		else if (tmp[i] == '>' && tag)
			tag = 0;
		else if (!tag)
			dst[o++] = (tmp[i] == '\n' || tmp[i] == '\r') ? ' ' : tmp[i];
	}
	dst[o] = '\0';
}

static void pop_bg(void)
{
	int i;

	for (i = 0; i < POP_SIZE * POP_SIZE; i++) {
		popbuf[i * 4] = 0x2e;
		popbuf[i * 4 + 1] = 0x1e;
		popbuf[i * 4 + 2] = 0x1e;
		popbuf[i * 4 + 3] = 0;
	}
}

static int pop_from_raw(const unsigned char *src, int w, int h, int stride, int ch)
{
	int s = w > h ? w : h, dw = w * POP_SIZE / s, dh = h * POP_SIZE / s;
	int ox, oy, x, y;

	if (dw < 1)
		dw = 1;
	if (dh < 1)
		dh = 1;
	if (dw > POP_SIZE)
		dw = POP_SIZE;
	if (dh > POP_SIZE)
		dh = POP_SIZE;
	ox = (POP_SIZE - dw) / 2;
	oy = (POP_SIZE - dh) / 2;
	pop_bg();
	for (y = 0; y < dh; y++) {
		for (x = 0; x < dw; x++) {
			const unsigned char *p = src + (size_t)(y * h / dh) * stride +
				(size_t)(x * w / dw) * ch;
			unsigned char *o = popbuf + ((size_t)(oy + y) * POP_SIZE + ox + x) * 4;
			int a = ch == 4 ? p[3] : 255;

			o[0] = (p[2] * a + o[0] * (255 - a)) / 255;
			o[1] = (p[1] * a + o[1] * (255 - a)) / 255;
			o[2] = (p[0] * a + o[2] * (255 - a)) / 255;
		}
	}
	return 1;
}

static int pop_from_file(const char *path)
{
	struct stat st;
	int fd[2], nul;
	pid_t pid;
	size_t got = 0;
	time_t deadline = time(NULL) + 3;

	if (path[0] != '/' || stat(path, &st) || !S_ISREG(st.st_mode))
		return 0;
	if (pipe(fd))
		return 0;
	pid = fork();
	if (pid < 0) {
		close(fd[0]);
		close(fd[1]);
		return 0;
	}
	if (pid == 0) {
		sigset_t none;

		sigemptyset(&none);
		sigprocmask(SIG_SETMASK, &none, NULL);
		dup2(fd[1], 1);
		close(fd[0]);
		close(fd[1]);
		nul = open("/dev/null", O_RDWR);
		if (nul >= 0) {
			dup2(nul, 0);
			dup2(nul, 2);
		}
		execlp("ffmpeg", "ffmpeg", "-v", "quiet", "-nostdin", "-i", path,
			"-frames:v", "1", "-vf",
			"scale=" "96:96:force_original_aspect_ratio=decrease,"
			"pad=96:96:(ow-iw)/2:(oh-ih)/2:color=0x1e1e2e",
			"-f", "rawvideo", "-pix_fmt", "bgr0", "-", (char *)NULL);
		_exit(1);
	}
	close(fd[1]);
	while (got < POP_BYTES && time(NULL) < deadline) {
		fd_set r;
		struct timeval tv = { 1, 0 };
		ssize_t n;

		FD_ZERO(&r);
		FD_SET(fd[0], &r);
		if (select(fd[0] + 1, &r, NULL, NULL, &tv) <= 0)
			continue;
		n = read(fd[0], poptmp + got, POP_BYTES - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	close(fd[0]);
	if (got < POP_BYTES)
		kill(pid, SIGKILL);
	waitpid(pid, NULL, 0);
	if (got < POP_BYTES)
		return 0;
	memcpy(popbuf, poptmp, POP_BYTES);
	return 1;
}

static int pop_from_path(const char *path)
{
	if (!strncmp(path, "file://", 7))
		path += 7;
	return pop_from_file(path);
}

static int pop_from_hints(const unsigned char *h, size_t hl)
{
	size_t p = 0;
	char imgpath[512] = "";

	while (p < hl) {
		const char *key, *sig;
		int sl;

		p = (p + 7) & ~(size_t)7;
		if (p >= hl)
			break;
		key = rstr(h, hl, &p);
		if (!key || p + 2 > hl)
			break;
		sl = h[p++];
		sig = (const char *)h + p;
		p += (size_t)sl + 1;
		if (p > hl)
			break;

		if (!strcmp(sig, "(iiibiiay)")) {
			int32_t v[6];
			uint32_t len;

			p = (p + 7) & ~(size_t)7;
			if (p + 28 > hl)
				break;
			memcpy(v, h + p, 24);
			memcpy(&len, h + p + 24, 4);
			p += 28;
			if (p + (size_t)len > hl)
				break;
			if ((!strcmp(key, "image-data") || !strcmp(key, "image_data") ||
			     !strcmp(key, "icon_data")) &&
			    v[0] > 0 && v[0] <= 2048 && v[1] > 0 && v[1] <= 2048 &&
			    v[4] == 8 && (v[5] == 3 || v[5] == 4) && v[2] >= v[0] * v[5] &&
			    (size_t)len >= (size_t)v[2] * (size_t)(v[1] - 1) + (size_t)v[0] * v[5])
				return pop_from_raw(h + p, v[0], v[1], v[2], v[5]);
			p += len;
		} else if (!strcmp(sig, "s") || !strcmp(sig, "o")) {
			const char *val = rstr(h, hl, &p);

			if (!val)
				break;
			if (!strcmp(key, "image-path") || !strcmp(key, "image_path"))
				snprintf(imgpath, sizeof imgpath, "%s", val);
		} else if (!strcmp(sig, "y")) {
			p += 1;
		} else if (!strcmp(sig, "b") || !strcmp(sig, "i") || !strcmp(sig, "u")) {
			p = ((p + 3) & ~(size_t)3) + 4;
		} else if (!strcmp(sig, "n") || !strcmp(sig, "q")) {
			p = ((p + 1) & ~(size_t)1) + 2;
		} else if (!strcmp(sig, "x") || !strcmp(sig, "t") || !strcmp(sig, "d")) {
			p = ((p + 7) & ~(size_t)7) + 8;
		} else {
			break;
		}
	}
	return imgpath[0] ? pop_from_path(imgpath) : 0;
}

static void pop_init(Display *dpy, Window root)
{
	int scr = DefaultScreen(dpy), depth = DefaultDepth(dpy, scr);
	XSetWindowAttributes at;

	if (depth != 24 && depth != 32)
		return;
	memset(&at, 0, sizeof at);
	at.override_redirect = True;
	at.background_pixel = BlackPixel(dpy, scr);
	popwin = XCreateWindow(dpy, root,
		DisplayWidth(dpy, scr) - POP_SIZE - POP_MARGIN, POP_Y, POP_SIZE, POP_SIZE,
		0, CopyFromParent, InputOutput, CopyFromParent,
		CWOverrideRedirect | CWBackPixel, &at);
	XSelectInput(dpy, popwin, ExposureMask);
	popgc = XCreateGC(dpy, popwin, 0, NULL);
	pop_bg();
	popimg = XCreateImage(dpy, DefaultVisual(dpy, scr), depth, ZPixmap, 0,
		(char *)popbuf, POP_SIZE, POP_SIZE, 32, 0);
}

static void pop_draw(Display *dpy)
{
	if (popimg)
		XPutImage(dpy, popwin, popgc, popimg, 0, 0, 0, 0, POP_SIZE, POP_SIZE);
}

static void pop_update(Display *dpy, long now)
{
	int show = popimg && pop_ready && pop_exp > now;

	if (show && !pop_visible) {
		XMapRaised(dpy, popwin);
		pop_visible = 1;
		pop_draw(dpy);
	} else if (show && pop_dirty) {
		pop_draw(dpy);
	} else if (!show && pop_visible) {
		XUnmapWindow(dpy, popwin);
		pop_visible = 0;
	}
	pop_dirty = 0;
}

static void reply_invalid(const char *sender, uint32_t ser)
{
	Buf r;

	r.n = 0;
	pstr(&r, "Invalid Notify arguments");
	send_msg(3, ser, sender, NULL, NULL, NULL,
		"org.freedesktop.DBus.Error.InvalidArgs", "s", &r);
}

static void do_notify(const unsigned char *b, size_t bl, const char *sender,
	uint32_t ser)
{
	size_t q = 0;
	uint32_t replaces, alen, hlen;
	int32_t expire = -1;
	const char *app, *icon, *summary, *body;
	long secs = 5;
	size_t hstart = 0, hsize = 0;
	int got = 0;
	Buf r;

	app = rstr(b, bl, &q);
	q = (q + 3) & ~(size_t)3;
	if (!app || q + 4 > bl) {
		reply_invalid(sender, ser);
		return;
	}
	memcpy(&replaces, b + q, 4);
	q += 4;
	icon = rstr(b, bl, &q);
	summary = icon ? rstr(b, bl, &q) : NULL;
	body = summary ? rstr(b, bl, &q) : NULL;
	if (!body) {
		reply_invalid(sender, ser);
		return;
	}

	q = (q + 3) & ~(size_t)3;
	if (q + 4 <= bl) {
		memcpy(&alen, b + q, 4);
		q += 4 + alen;
		q = (q + 3) & ~(size_t)3;
		if (q + 4 <= bl) {
			memcpy(&hlen, b + q, 4);
			q += 4;
			q = (q + 7) & ~(size_t)7;
			hstart = q;
			hsize = hlen;
			q += hlen;
			q = (q + 3) & ~(size_t)3;
			if (q + 4 <= bl)
				memcpy(&expire, b + q, 4);
		}
	}
	if (expire > 0) {
		secs = (expire + 999) / 1000;
		if (secs > 15)
			secs = 15;
	} else if (expire == 0) {
		secs = 10;
	}

	clean(dmsg, sizeof dmsg, summary, body);
	dexp = (long)time(NULL) + secs;
	if (!dmsg[0])
		dexp = 0;

	if (hsize && hstart + hsize <= bl)
		got = pop_from_hints(b + hstart, hsize);
	if (!got && *icon)
		got = pop_from_path(icon);
	pop_ready = got;
	pop_exp = dexp;
	pop_dirty = 1;

	if (!replaces)
		replaces = ++notif_id;
	r.n = 0;
	pu32(&r, replaces);
	send_msg(2, ser, sender, NULL, NULL, NULL, NULL, "u", &r);
}

static void handle(const unsigned char *m, size_t total)
{
	uint32_t blen, ser, fl;
	const char *path = NULL, *iface = NULL, *member = NULL, *sender = NULL;
	size_t p = 16, end, bp;
	Buf r;

	if (m[0] != 'l' || m[1] != 1)
		return;
	memcpy(&blen, m + 4, 4);
	memcpy(&ser, m + 8, 4);
	memcpy(&fl, m + 12, 4);
	end = 16 + (size_t)fl;
	if (end > total)
		return;

	while (p < end) {
		int code, sl;
		const char *sig;

		p = (p + 7) & ~(size_t)7;
		if (p + 2 >= end)
			break;
		code = m[p++];
		sl = m[p++];
		sig = (const char *)m + p;
		p += (size_t)sl + 1;
		if (p >= end)
			break;
		if (sig[0] == 's' || sig[0] == 'o') {
			uint32_t l;

			p = (p + 3) & ~(size_t)3;
			if (p + 4 > end)
				break;
			memcpy(&l, m + p, 4);
			p += 4;
			if (p + (size_t)l + 1 > end)
				break;
			if (code == 1) path = (const char *)m + p;
			else if (code == 2) iface = (const char *)m + p;
			else if (code == 3) member = (const char *)m + p;
			else if (code == 7) sender = (const char *)m + p;
			p += (size_t)l + 1;
		} else if (sig[0] == 'g') {
			p += (size_t)m[p] + 2;
		} else if (sig[0] == 'u') {
			p = ((p + 3) & ~(size_t)3) + 4;
		} else {
			break;
		}
	}
	if (!member || !sender)
		return;
	(void)path;
	bp = (end + 7) & ~(size_t)7;
	if (bp + blen > total)
		return;
	if (!iface)
		iface = "";

	if (!strcmp(iface, NOTIF_NAME)) {
		if (!strcmp(member, "Notify")) {
			do_notify(m + bp, blen, sender, ser);
			return;
		}
		if (!strcmp(member, "GetCapabilities")) {
			size_t start;

			r.n = 0;
			start = r.n;
			pu32(&r, 0);
			pstr(&r, "body");
			{
				uint32_t l = (uint32_t)(r.n - start - 4);

				memcpy(r.b + start, &l, 4);
			}
			send_msg(2, ser, sender, NULL, NULL, NULL, NULL, "as", &r);
			return;
		}
		if (!strcmp(member, "GetServerInformation")) {
			r.n = 0;
			pstr(&r, "status");
			pstr(&r, "local");
			pstr(&r, "1.0");
			pstr(&r, "1.2");
			send_msg(2, ser, sender, NULL, NULL, NULL, NULL, "ssss", &r);
			return;
		}
		if (!strcmp(member, "CloseNotification")) {
			send_msg(2, ser, sender, NULL, NULL, NULL, NULL, NULL, NULL);
			return;
		}
	}
	if (!strcmp(iface, "org.freedesktop.DBus.Peer") && !strcmp(member, "Ping")) {
		send_msg(2, ser, sender, NULL, NULL, NULL, NULL, NULL, NULL);
		return;
	}
	r.n = 0;
	pstr(&r, "Unknown method");
	send_msg(3, ser, sender, NULL, NULL, NULL,
		"org.freedesktop.DBus.Error.UnknownMethod", "s", &r);
}

static void bus_read(void)
{
	ssize_t n = read(bus, ibuf + ilen, sizeof ibuf - ilen);

	if (n <= 0) {
		if (n < 0 && errno == EINTR)
			return;
		close(bus);
		bus = -1;
		return;
	}
	ilen += (size_t)n;

	while (ilen >= 16) {
		uint32_t blen, fl;
		size_t total;

		memcpy(&blen, ibuf + 4, 4);
		memcpy(&fl, ibuf + 12, 4);
		total = 16 + (((size_t)fl + 7) & ~(size_t)7) + (size_t)blen;
		if (total > sizeof ibuf) {
			close(bus);
			bus = -1;
			ilen = 0;
			return;
		}
		if (ilen < total)
			break;
		handle(ibuf, total);
		if (bus < 0) {
			ilen = 0;
			return;
		}
		memmove(ibuf, ibuf + total, ilen - total);
		ilen -= total;
	}
}

static void clip_slot_path(char *out, size_t n, int slot)
{
	snprintf(out, n, "%s/%d", clip_dir, slot);
}

static void clip_init(Display *dpy, Window root)
{
	const char *home = getenv("HOME");
	char path[300];
	FILE *f;
	int last;

	if (!home)
		return;
	clipwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	a_clip = XInternAtom(dpy, "CLIPBOARD", False);
	a_utf8 = XInternAtom(dpy, "UTF8_STRING", False);
	a_prop = XInternAtom(dpy, "STATUS_CLIP", False);
	a_incr = XInternAtom(dpy, "INCR", False);

	snprintf(path, sizeof path, "%s/.cache", home);
	mkdir(path, 0700);
	snprintf(clip_dir, sizeof clip_dir, "%s/.cache/clipboard", home);
	mkdir(clip_dir, 0700);

	snprintf(path, sizeof path, "%s/head", clip_dir);
	f = fopen(path, "r");
	if (f) {
		if (fscanf(f, "%d", &clip_head) != 1)
			clip_head = 0;
		fclose(f);
	}
	if (clip_head < 0 || clip_head >= CLIP_SLOTS)
		clip_head = 0;

	last = (clip_head + CLIP_SLOTS - 1) % CLIP_SLOTS;
	clip_slot_path(path, sizeof path, last);
	f = fopen(path, "r");
	if (f) {
		clip_last_len = fread(clip_last, 1, CLIP_MAX, f);
		fclose(f);
	}
}

static void clip_poll(Display *dpy)
{
	time_t now = time(NULL);

	if (!clip_dir[0])
		return;
	if (now == clip_polled)
		return;
	if (clip_pending && now - clip_since < 5)
		return;
	if (XGetSelectionOwner(dpy, a_clip) == None)
		return;
	XConvertSelection(dpy, a_clip, a_utf8, a_prop, clipwin, CurrentTime);
	clip_pending = 1;
	clip_since = clip_polled = now;
}

static void clip_store(const unsigned char *data, size_t n)
{
	char path[300];
	FILE *f;
	size_t i;
	int blank = 1;

	for (i = 0; i < n; i++)
		if (data[i] != ' ' && data[i] != '\t' && data[i] != '\n' && data[i] != '\r') {
			blank = 0;
			break;
		}
	if (blank || n > CLIP_MAX)
		return;
	if (n == clip_last_len && !memcmp(data, clip_last, n))
		return;

	clip_slot_path(path, sizeof path, clip_head);
	f = fopen(path, "w");
	if (!f)
		return;
	fchmod(fileno(f), 0600);
	fwrite(data, 1, n, f);
	fclose(f);

	memcpy(clip_last, data, n);
	clip_last_len = n;
	clip_head = (clip_head + 1) % CLIP_SLOTS;

	snprintf(path, sizeof path, "%s/head", clip_dir);
	f = fopen(path, "w");
	if (f) {
		fchmod(fileno(f), 0600);
		fprintf(f, "%d\n", clip_head);
		fclose(f);
	}
}

static void clip_event(Display *dpy, const XSelectionEvent *e)
{
	Atom type;
	int fmt;
	unsigned long nitems, after;
	unsigned char *data = NULL;

	if (e->selection != a_clip)
		return;
	clip_pending = 0;
	if (e->property == None)
		return;
	if (XGetWindowProperty(dpy, clipwin, a_prop, 0, CLIP_MAX / 4 + 1, True,
	    AnyPropertyType, &type, &fmt, &nitems, &after, &data) != Success)
		return;
	if (data && type != a_incr && fmt == 8 && after == 0 && nitems > 0)
		clip_store(data, nitems);
	if (data)
		XFree(data);
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
	signal(SIGPIPE, SIG_IGN);

	bus_connect();
	clip_init(dpy, root);
	pop_init(dpy, root);

	for (;;) {
		char cap[16] = "", raw[32] = "", path[96], bar[512], tbuf[64], fmsg[384], fimg[512];
		static char lastbar[512];
		const char *st, *text = NULL;
		long fexp;
		size_t i;
		time_t now = time(NULL);
		struct tm tm;
		struct timespec ts = { 0, 0 };
		fd_set rfds;
		int maxfd = -1;

		while (XPending(dpy)) {
			XEvent ev;

			XNextEvent(dpy, &ev);
			if (ev.type == SelectionNotify)
				clip_event(dpy, &ev.xselection);
			else if (ev.type == Expose && ev.xexpose.window == popwin && pop_visible)
				pop_draw(dpy);
		}
		clip_poll(dpy);

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

		fexp = read_msg(fmsg, sizeof fmsg, fimg, sizeof fimg);
		if (fexp <= (long)now)
			fexp = 0;
		if (dexp <= (long)now)
			dexp = 0;
		if (dexp && dexp >= fexp) {
			text = dmsg;
		} else if (fexp) {
			text = fmsg;
		}
		if (text)
			snprintf(bar, sizeof bar, "%s", text);
		if (text == fmsg && fexp != fimg_loaded) {
			fimg_loaded = fexp;
			pop_ready = fimg[0] ? pop_from_path(fimg) : 0;
			pop_exp = fexp;
			pop_dirty = 1;
		}
		pop_update(dpy, (long)now);

		if (strcmp(bar, lastbar)) {
			XStoreName(dpy, root, bar);
			snprintf(lastbar, sizeof lastbar, "%s", bar);
		}
		XFlush(dpy);

		ts.tv_sec = 1;

		FD_ZERO(&rfds);
		FD_SET(ConnectionNumber(dpy), &rfds);
		maxfd = ConnectionNumber(dpy);
		if (bus >= 0) {
			FD_SET(bus, &rfds);
			if (bus > maxfd)
				maxfd = bus;
		}
		if (pselect(maxfd + 1, &rfds, NULL, NULL, &ts, &old) > 0 &&
		    bus >= 0 && FD_ISSET(bus, &rfds))
			bus_read();
	}
}
