/* -*- c-file-style: "ruby" -*- */
/*
 * console IO module
 */
#include "ruby.h"
#include "rubyio.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <sys/ioctl.h>

#ifndef HAVE_RB_IO_T
typedef OpenFile rb_io_t;
#endif
#ifdef GetReadFile
#define GetReadFileNo(fptr) fileno(GetReadFile(fptr))
#define GetWriteFileNo(fptr) fileno(GetWriteFile(fptr))
#else
#define GetReadFileNo(fptr) ((fptr)->fd)
#define GetWriteFileNo(fptr) ((fptr)->fd)
#endif

#if defined HAVE_TERMIOS_H
# include <termios.h>
typedef struct termios conmode;
# define setattr(fd, t) (tcsetattr(fd, TCSAFLUSH, t) == 0)
# define getattr(fd, t) (tcgetattr(fd, t) == 0)
#elif defined HAVE_TERMIO_H
# include <termio.h>
typedef struct termio conmode;
# define setattr(fd, t) (ioctl(fd, TCSETAF, t) == 0)
# define getattr(fd, t) (ioctl(fd, TCGETA, t) == 0)
#elif defined HAVE_SGTTY_H
# include <sgtty.h>
typedef struct sgttyb conmode;
# ifdef HAVE_STTY
# define setattr(fd, t)  (stty(fd, t) == 0)
# else
# define setattr(fd, t)  (ioctl((fd), TIOCSETP, (t)) == 0)
# endif
# ifdef HAVE_GTTY
# define getattr(fd, t)  (gtty(fd, t) == 0)
# else
# define getattr(fd, t)  (ioctl((fd), TIOCGETP, (t)) == 0)
# endif
#elif defined _WIN32
#include <winioctl.h>
typedef DWORD conmode;
#  define setattr(fd, t) SetConsoleMode((HANDLE)rb_w32_get_osfhandle(fd), *t)
#  define getattr(fd, t) GetConsoleMode((HANDLE)rb_w32_get_osfhandle(fd), t)
#endif

static ID id_getc, id_console;

#ifdef HAVE_CFMAKERAW
#define set_rawmode cfmakeraw
#else
static int
set_rawmode(t)
    conmode *t;
{
#if defined HAVE_TERMIOS_H || defined HAVE_TERMIO_H
    t->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
    t->c_oflag &= ~OPOST;
    t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
    t->c_cflag &= ~(CSIZE|PARENB);
    t->c_cflag |= CS8;
#elif defined HAVE_SGTTY_H
    t->sg_flags &= ~ECHO;
    t->sg_flags |= RAW;
#elif defined _WIN32
    *t = 0;
#endif
    return 0;
}
#endif

static void
set_noecho(t)
    conmode *t;
{
#if defined HAVE_TERMIOS_H || defined HAVE_TERMIO_H
    t->c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
#elif defined HAVE_SGTTY_H
    t->sg_flags &= ~ECHO;
#elif defined _WIN32
    *t &= ~ENABLE_ECHO_INPUT;
#endif
}

static void
set_echo(t)
    conmode *t;
{
#if defined HAVE_TERMIOS_H || defined HAVE_TERMIO_H
    t->c_lflag |= (ECHO | ECHOE | ECHOK | ECHONL);
#elif defined HAVE_SGTTY_H
    t->sg_flags |= ECHO;
#elif defined _WIN32
    *t |= ENABLE_ECHO_INPUT;
#endif
}

static int
echo_p(t)
    conmode *t;
{
#if defined HAVE_TERMIOS_H || defined HAVE_TERMIO_H
    return (t->c_lflag & (ECHO | ECHOE | ECHOK | ECHONL)) != 0;
#elif defined HAVE_SGTTY_H
    return (t->sg_flags & ECHO) != 0;
#elif defined _WIN32
    return (*t & ENABLE_ECHO_INPUT) != 0;
#endif
}

static int
set_ttymode(fd, t, setter)
    int fd;
    conmode *t;
    void (*setter) _((conmode *));
{
    conmode r;
    if (!getattr(fd, t)) return -1;
    r = *t;
    setter(&r);
    return setattr(fd, &r);
}

static VALUE
ttymode(io, func, setter)
    VALUE io;
    VALUE (*func) _((VALUE));
    void (*setter) _((conmode *));
{
    rb_io_t *fptr;
    int fd1 = -1, status = 0;
#ifdef GetReadFile
    int fd2 = -1, error = 0;
#endif
    conmode t[2];
    VALUE result = Qnil;

    GetOpenFile(io, fptr);
#ifdef GetReadFile
    if (fptr->f) {
	if (set_ttymode(fileno(fptr->f), t+0, setter)) {
	    fd1 = fileno(fptr->f);
	}
	else {
	    error = errno;
	    status = -1;
	}
    }
    if (fptr->f2 && status == 0) {
	if (set_ttymode(fileno(fptr->f2), t+1, setter)) {
	    fd2 = fileno(fptr->f2);
	}
	else {
	    error = errno;
	    status = -1;
	}
    }
    if (status == 0) {
	result = rb_protect(func, io, &status);
    }
    if (fptr->f && fd1 == fileno(fptr->f)) {
	if (!setattr(fd1, t+0)) {
	    error = errno;
	    status = -1;
	}
    }
    if (fptr->f2 && fd2 == fileno(fptr->f2)) {
	if (!setattr(fd2, t+1)) {
	    error = errno;
	    status = -1;
	}
    }
    if (status) {
	if (status == -1) {
	    errno = error;
	    rb_sys_fail(0);
	}
	rb_jump_tag(status);
    }
#else
    if (fptr->fd != -1) {
	if (!set_ttymode(fptr->fd, t+0, setter)) {
	    rb_sys_fail(0);
	}
	fd1 = fptr->fd;
    }
    result = rb_protect(func, io, &status);
    if (fd1 != -1 && fd1 == fptr->fd) {
	if (!setattr(fd1, t+0)) {
	    rb_sys_fail(0);
	}
    }
    if (status) {
	rb_jump_tag(status);
    }
#endif
    return result;
}

static VALUE
console_raw(io)
    VALUE io;
{
    return ttymode(io, rb_yield, set_rawmode);
}

static VALUE
getch(io)
    VALUE io;
{
    return rb_funcall2(io, id_getc, 0, 0);
}

static VALUE
console_getch(io)
    VALUE io;
{
    return ttymode(io, getch, set_rawmode);
}

static VALUE
console_noecho(io)
    VALUE io;
{
    return ttymode(io, rb_yield, set_noecho);
}

static VALUE
console_set_echo(io, f)
    VALUE io, f;
{
    conmode t;
    rb_io_t *fptr;
    int fd;

    GetOpenFile(io, fptr);
    fd = GetReadFileNo(fptr);
    if (!getattr(fd, &t)) rb_sys_fail(0);
    if (RTEST(f))
	set_echo(&t);
    else
	set_noecho(&t);
    if (!setattr(fd, &t)) rb_sys_fail(0);
    return io;
}

static VALUE
console_echo_p(io)
    VALUE io;
{
    conmode t;
    rb_io_t *fptr;
    int fd;

    GetOpenFile(io, fptr);
    fd = GetReadFileNo(fptr);
    if (!getattr(fd, &t)) rb_sys_fail(0);
    return echo_p(&t) ? Qtrue : Qfalse;
}

static VALUE
console_iflush(io)
    VALUE io;
{
    rb_io_t *fptr;
    int fd;

    GetOpenFile(io, fptr);
    fd = GetReadFileNo(fptr);
#if defined HAVE_TERMIOS_H || defined HAVE_TERMIO_H
    if (tcflush(fd, TCIFLUSH)) rb_sys_fail(0);
#endif
    return io;
}

static VALUE
console_oflush(io)
    VALUE io;
{
    rb_io_t *fptr;
    int fd;

    GetOpenFile(io, fptr);
#ifdef GetWriteFile
    fd = fileno(GetWriteFile(fptr));
#else
    fd = fptr->fd;
#endif
#if defined HAVE_TERMIOS_H || defined HAVE_TERMIO_H
    if (tcflush(fd, TCOFLUSH)) rb_sys_fail(0);
#endif
    return io;
}

static VALUE
console_ioflush(io)
    VALUE io;
{
    rb_io_t *fptr;

    GetOpenFile(io, fptr);
#if defined HAVE_TERMIOS_H || defined HAVE_TERMIO_H
#ifdef GetWriteFile
    if (fptr->f2) {
	if (tcflush(GetReadFileNo(fptr), TCIFLUSH)) rb_sys_fail(0);
	if (tcflush(GetWriteFileNo(fptr), TCOFLUSH)) rb_sys_fail(0);
    }
    else {
	if (tcflush(GetReadFileNo(fptr), TCIOFLUSH)) rb_sys_fail(0);
    }
#else
    if (tcflush(GetReadFileNo(fptr), TCIOFLUSH)) rb_sys_fail(0);
#endif
#endif
    return io;
}

#ifdef HAVE_TERMIOS_H
static VALUE
console_get_winsize(io)
    VALUE io;
{
    rb_io_t *fptr;
    struct winsize w;

    GetOpenFile(io, fptr);
    if (ioctl(GetReadFileNo(fptr), TIOCGWINSZ, &w) < 0) rb_sys_fail(0);
    return rb_ary_new3(4,
		       INT2NUM(w.ws_row), INT2NUM(w.ws_col),
		       INT2NUM(w.ws_xpixel), INT2NUM(w.ws_ypixel));
}

static VALUE
console_set_winsize(io, size)
    VALUE io, size;
{
    rb_io_t *fptr;
    struct winsize w;
    VALUE row, col, xpixel, ypixel;

    GetOpenFile(io, fptr);
    size = rb_Array(size);
    rb_scan_args(RARRAY_LEN(size), RARRAY_PTR(size), "22",
		 &row, &col, &xpixel, &ypixel);
    w.ws_row = w.ws_col = w.ws_xpixel = w.ws_ypixel = 0;
#define SET(m) w.ws_##m = NIL_P(m) ? 0 : (unsigned short)NUM2UINT(m)
    SET(row);
    SET(col);
    SET(xpixel);
    SET(ypixel);
#undef SET
    if (ioctl(GetReadFileNo(fptr), TIOCSWINSZ, &w) < 0) rb_sys_fail(0);
    return io;
}
#endif

static VALUE
console_dev(klass)
    VALUE klass;
{
    VALUE con = 0;
    rb_io_t *fptr;

    if (rb_const_defined(klass, id_console)) {
	con = rb_const_get(klass, id_console);
	if (TYPE(con) == T_FILE) {
	    if ((fptr = RFILE(con)->fptr) &&
#ifdef GetReadFile
		fptr->f
#else
		fptr->fd >= 0
#endif
		)
		return con;
	}
	rb_mod_remove_const(klass, ID2SYM(id_console));
    }
    {
	VALUE args[2];
#if defined HAVE_TERMIOS_H || defined HAVE_TERMIO_H || defined HAVE_SGTTY_H
#define CONSOLE_DEVISE "/dev/tty"
#elif defined _WIN32
#define CONSOLE_DEVISE "CON$"
#endif

#if defined GetReadFile && defined _WIN32
	VALUE out;
	rb_io_t *ofptr;

	args[0] = rb_str_new2("CONOUT$");
	args[1] = INT2FIX(O_WRONLY);
	out = rb_class_new_instance(2, args, klass);
	args[0] = rb_str_new2("CONIN$");
	args[1] = INT2FIX(O_RDONLY);
#else
	args[0] = rb_str_new2(CONSOLE_DEVISE);
	args[1] = INT2FIX(O_RDWR);
#endif
	con = rb_class_new_instance(2, args, klass);
#if defined GetReadFile && defined _WIN32
	GetOpenFile(con, fptr);
	GetOpenFile(out, ofptr);
	fptr->f2 = ofptr->f;
	ofptr->f = 0;
	fptr->mode |= FMODE_WRITABLE;
#endif
	rb_const_set(klass, id_console, con);
    }
    return con;
}

void
Init_console()
{
    id_getc = rb_intern("getc");
    id_console = rb_intern("console");
    rb_define_method(rb_cIO, "raw", console_raw, 0);
    rb_define_method(rb_cIO, "getch", console_getch, 0);
    rb_define_method(rb_cIO, "echo=", console_set_echo, 1);
    rb_define_method(rb_cIO, "echo?", console_echo_p, 0);
    rb_define_method(rb_cIO, "noecho", console_noecho, 0);
    rb_define_method(rb_cIO, "iflush", console_iflush, 0);
    rb_define_method(rb_cIO, "oflush", console_oflush, 0);
    rb_define_method(rb_cIO, "ioflush", console_ioflush, 0);
#ifdef HAVE_TERMIOS_H
    rb_define_method(rb_cIO, "winsize", console_get_winsize, 0);
    rb_define_method(rb_cIO, "winsize=", console_set_winsize, 1);
#endif
    rb_define_singleton_method(rb_cFile, "console", console_dev, 0);
}
