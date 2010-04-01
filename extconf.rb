require 'mkmf'

ok = false
hdr = nil
case
when macro_defined?("_WIN32", "")
  ok = true
when %w"termios.h termio.h".any? {|h| hdr = h if have_header(h)}
  ok = true
  have_func("cfmakeraw", hdr)
end
if ok
  create_makefile("io/console")
end
