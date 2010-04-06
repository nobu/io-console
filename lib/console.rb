begin
  require 'io/console.so'
rescue LoadError
  require 'dl/import'
  case RUBY_PLATFORM
  when /mswin/, /mingw/,  /win32/
    require 'rbconfig'
    class IO
      module Console
        ENABLE_ECHO_INPUT = 4
        extend DL::Importable
        dlload "kernel32", Config::CONFIG["RUBY_SO_NAME"]
        extern "DWORD SetConsoleMode(HANDLE, DWORD)"
        extern "DWORD GetConsoleMode(HANDLE, DWORD*)"
        extern "HANDLE rb_w32_get_osfhandle(int)"
      end
      def echo=(enable)
        echo_input = Console::ENABLE_ECHO_INPUT
        ttymode {|m| enable ? m | echo_input : m & ~echo_input}
      end
      def echo?
        echo_input = Console::ENABLE_ECHO_INPUT
        (ttymode {|m| break m} & echo_input) != 0
      end
      def raw
        mode = nil
        ttymode {|m| mode = m; 0}
        yield(self)
      ensure
        Console.setConsoleMode(handle, mode) if mode
      end
      def noecho
        mode = nil
        ttymode {|m| mode = m; m & ~Console::ENABLE_ECHO_INPUT}
        yield(self)
      ensure
        Console.setConsoleMode(handle, mode) if mode
      end
      def getch
        raw {getc}
      end
      def iflush
      end
      def oflush
      end
      def ioflush
      end
      private
      def handle
        Console.rb_w32_get_osfhandle(fileno)
      end
      def ttymode
        h = handle
        t = malloc(sizeof("L"))
        Console.getConsoleMode(h, t).nonzero? or raise
        t = yield(t.to_a("L")[0])
        Console.setConsoleMode(h, t).nonzero? or raise
      end
    end
    def IO.console
      @console ||= File.open("CON$", "r+")
    end
  end
end
