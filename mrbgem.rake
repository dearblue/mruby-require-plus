#!ruby

$: << File.join(MRUBY_ROOT, "lib")
require "mruby/source"

MRuby::Gem::Specification.new("mruby-require-plus") do |s|
  s.summary = "extension loader for mruby with Virtual Filesystem support"
  version = File.read(File.join(__dir__, "README.md")).scan(/^\s*[\-\*] version: \s*(\d+(?:.\w+)+)/i).flatten[-1]
  s.version = version if version
  s.license = "BSD-2-Clause"
  s.author  = "dearblue"
  s.homepage = "https://github.com/dearblue/mruby-require-plus"

  add_conflict "mruby-require"

  if MRuby::Source::MRUBY_RELEASE_NO > 20000
    add_dependency "mruby-bin-config", core: "mruby-bin-config"
  else
    #add_dependency "mruby-bin-mruby-config", core: "mruby-bin-mruby-config"
  end
  add_dependency "mruby-string-ext", core: "mruby-string-ext"
  add_dependency "mruby-proc-ext", core: "mruby-proc-ext"
  add_dependency "mruby-struct", core: "mruby-struct"
  add_dependency "mruby-aux", github: "dearblue/mruby-aux"

  unless build.cc.defines.flatten.grep(/\AMRUBY_REQUIRE_PLUS_WITHOUT_RB(?:=|\z)/)
    add_dependency "mruby-compile", core: "mruby-compile"
  end

  if cc.command =~ /\b(?:g?cc|clang)\d*\b/
    cc.flags << %w(-Wno-declaration-after-statement)
    #cc.flags << "-fPIC"
    build.cc.flags << "-fPIC"
    build.cxx.flags << "-fPIC"
    build.linker.flags << "-fPIC"
    build.gems.each do |g|
      g.cc.flags      << "-fPIC" rescue nil # 先に enable_test してあると "mruby-test" が nil となっているため
      g.cxx.flags     << "-fPIC" rescue nil
      g.linker.flags  << "-fPIC" rescue nil
    end
    #require "pry"; binding.pry; abort "!"
  end

  build.cc.include_paths << File.join(__dir__, "include")
  build.cxx.include_paths << File.join(__dir__, "include")
end
