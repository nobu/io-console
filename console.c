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
#elif defined _WIN32
#include <winioctl.h>
typedef DWORD conmode;
#  define setattr(fd, t) SetConsoleMode((HANDLE)rb_w32_get_osfhandle(fd), *t)
#  define getattr(fd, t) GetConsoleMode((HANDLE)rb_w32_get_osfhandle(fd), t)
#endif

static ID id_getc, id_ttydev, id_console;

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
#elif defined _WIN32
    *t &= ~ENABLE_ECHO_INPUT;
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
    VALUE func _((VALUE));
    void (*setter) _((conmode *));
{
    OpenFile *fptr;
    int fd1 = -1, fd2 = -1, status = 0;
    conmode t[2];
    VALUE result = Qnil;

    GetOpenFile(io, fptr);
    if (fptr->f) {
	if (set_ttymode(fileno(fptr->f), t+0, setter))
	    fd1 = fileno(fptr->f);
	else
	    status = -1;
    }
    if (fptr->f2 && status == 0) {
	if (set_ttymode(fileno(fptr->f2), t+1, setter))
	    fd2 = fileno(fptr->f2);
	else
	    status = -1;
    }
    if (status == 0) {
	result = rb_protect(func, io, &status);
    }
    if (fptr->f && fd1 == fileno(fptr->f)) {
	if (!setattr(fd1, t+0)) status = -1;
    }
    if (fptr->f2 && fd2 == fileno(fptr->f2)) {
	if (!setattr(fd2, t+1)) status = -1;
    }
    if (status) {
	if (status == -1) rb_sys_fail(0);
	rb_jump_tag(status);
    }
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
console_dev(klass)
    VALUE klass;
{
    VALUE con = 0;
    OpenFile *fptr;

    if (rb_const_defined(klass, id_console)) {
	con = rb_const_get(klass, id_console);
	if (TYPE(con) == T_FILE) {
	    if ((fptr = RFILE(con)->fptr) && fptr->f)
		return con;
	}
	rb_mod_remove_const(klass, ID2SYM(id_console));
    }
    {
	VALUE args[2];
#if defined HAVE_TERMIOS_H || defined HAVE_TERMIO_H
	args[0] = rb_str_new2("/dev/tty");
	args[1] = INT2FIX(O_RDWR);
	con = rb_class_new_instance(2, args, klass);
#elif defined _WIN32
	VALUE out;
	OpenFile *ofptr;

	args[0] = rb_str_new2("CONOUT$");
	args[1] = INT2FIX(O_WRONLY);
	out = rb_class_new_instance(2, args, klass);
	args[0] = rb_str_new2("CONIN$");
	args[1] = INT2FIX(O_RDONLY);
	con = rb_class_new_instance(2, args, klass);
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
    rb_define_method(rb_cIO, "noecho", console_noecho, 0);
    rb_define_singleton_method(rb_cFile, "console", console_dev, 0);
}
