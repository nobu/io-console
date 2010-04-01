begin
  require 'io/console.so'
rescue LoadError
  require 'dl/import'
  case RUBY_PLATFORM
  when /mswin/, /mingw/,  /win32/
    require 'rbconfig'
    class IO
      module Console
        extend DL::Importable
        dlload "kernel32", Config::CONFIG["RUBY_SO_NAME"]
        extern "DWORD SetConsoleMode(HANDLE, DWORD)"
        extern "DWORD GetConsoleMode(HANDLE, DWORD*)"
        extern "HANDLE rb_w32_get_osfhandle(int)"
      end
      def handle
        Console.rb_w32_get_osfhandle(fileno)
      end
      def ttymode(m)
        h = handle
        t = malloc(sizeof("L"))
        Console.getConsoleMode(h, t) or raise
        t = yield(t.to_a("L")[0])
        Console.setConsoleMode(h, t) or raise
      end
    end
    def File.console
      @console ||= open("CON$", "r+")
    end
  end
end
