# -*- ruby -*-
Gem::Specification.new do |s|
  s.name = "io-console"
  s.version = "0.2.2"
  s.date = "2010-04-02"
  s.summary = "Console interface"
  s.email = "nobu@ruby-lang.org"
  s.description = ""
  s.authors = ["Nobu Nakada"]
  s.require_path = %[.]
  s.files = %w[console.c extconf.rb lib/console.rb]
  s.extensions = %w[extconf.rb]
end
