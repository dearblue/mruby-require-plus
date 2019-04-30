#!ruby

MRUBY_CONFIG ||= ENV["MRUBY_CONFIG"] || "test_config.rb"
ENV["MRUBY_CONFIG"] = MRUBY_CONFIG

Object.instance_eval { remove_const(:MRUBY_CONFIG) }

MRUBY_BASEDIR ||= ENV["MRUBY_BASEDIR"] || "@mruby"

ENV["INSTALL_DIR"] = File.join(File.dirname(__FILE__), "bin")

rakefile = "#{MRUBY_BASEDIR}/Rakefile"

unless File.exist? rakefile
  $stderr.puts <<-ERR
[[#{__FILE__}]]
\tNot found #{rakefile}.
\tLook again a configuration, and try rake.

\t* MRUBY_CONFIG=#{ENV["MRUBY_CONFIG"]}
\t* MRUBY_BASEDIR=#{MRUBY_BASEDIR}
\t* INSTALL_DIR=#{ENV["INSTALL_DIR"]}
  ERR

  abort 1
end

load rakefile
