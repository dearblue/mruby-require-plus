#!ruby

$: << File.join(MRUBY_ROOT, "lib")
require "mruby/source"

require "yaml"

config = YAML.load <<'YAML'
  common:
    gems:
    #- :mgem: mruby-require
    - :core: mruby-sprintf
    - :core: mruby-print
    - :core: mruby-bin-mrbc
    - :core: mruby-bin-mirb
    - :core: mruby-bin-mruby
    - :core: mruby-kernel-ext
    - :core: mruby-enum-ext
    - :core: mruby-enum-lazy
    - :core: mruby-random
  builds:
    host:
      defines: MRB_INT64
    host-int32:
      defines: MRB_INT32
    host-nan-int16:
      defines: [MRB_INT16, MRB_NAN_BOXING]
    host++-word:
      defines: MRB_WORD_BOXING
      c++abi: true
YAML

config["builds"].each_pair do |n, c|
  MRuby::Build.new(n) do |conf|
    toolchain :clang

    conf.build_dir = File.join(__dir__, c["build_dir"] || "build/#{name}")

    enable_debug
    #cc.flags << %w(-fsanitize=address,leak -fno-omit-frame-pointer)
    #linker.flags << %w(-fsanitize=address,leak -fno-omit-frame-pointer)
    enable_test
    enable_cxx_abi if c["c++abi"]

    cc.defines << [*c["defines"]]
    cc.flags << [*c["cflags"]]

    Array(config.dig("common", "gems")).each { |*g| gem *g }
    Array(c["gems"]).each { |*g| gem *g }

    #cc.flags << "-fPIC"
    #cxx.flags << "-fPIC"
    #linker.flags << "-fPIC"

    if MRuby::Source::MRUBY_RELEASE_NO < 20001
      gem core: "mruby-bin-mruby-config"
    end

    if MRuby::Source::MRUBY_RELEASE_NO < 20000
      gem mgem: "mruby-io"
    else
      gem core: "mruby-io"
      gem core: "mruby-metaprog"
    end

    gem __dir__ do |g|
      if g.cc.command =~ /\b(?:g?ccc|clang)\d*\b/
        g.cc.flags << "-std=c11" unless c["c++abi"]
        g.cc.flags << "-pedantic"
        g.cc.flags << "-Wall"
      end
    end
  end
end
