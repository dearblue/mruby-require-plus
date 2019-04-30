#!ruby

module RequirePlus
  module Kernel
    def require(feature)
      #p Central.get_upper_frame
      $:.each do |vfs|
        ret = Central.trial_require(vfs, feature)
        return ret unless ret.nil?
      end

      raise LoadError, "cannot load such file - #{feature}"
    end

    def require_relative(feature)
      upper = Central.get_upper_frame[0]
      #p Kernel.caller(0, 1)[0]
      #p upper
      (vfs, dirname) = Central.findvfs(upper)
      #puts "#{__FILE__}(#{__LINE__})#{__method__}" => [vfs, dirname, feature]
      raise LoadError, "mismatch VFS by #{upper}" unless vfs

      path = Central.makepath(dirname, feature)
      ret = Central.trial_require(vfs, path)
      return ret unless ret.nil?

      raise LoadError, "cannot load such file - #{feature}"
    end

    def load(file)
      # 現在の作業ディレクトリから探索する
      case Central.extname(file)
      when ".rb"
        vfs = VFSFile.new(Dir.pwd)
        return Central.load_as_rb(vfs, file)
      #when ".mrb"
      #  return Central.load_as_mrb(vfs, file)
      else
        raise LoadError, file
      end
    end
  end

  module Central
    SOTYPES = [".so"] unless const_defined?(:SOTYPES)

    def Central.trial_require(vfs, feature)
      case
      when rb = Central.find_rbfile(vfs, feature)
        Central.load_as_rb(vfs, rb)
        true
      when mrb = Central.find_mrbfile(vfs, feature)
        Central.load_as_mrb(vfs, mrb)
        true
      when so = Central.find_sofile(vfs, feature)
        Central.load_as_so(vfs, so)
        true
      else
        nil
      end
    end

    def Central.find_rbfile(vfs, feature)
      return feature if feature = Central.find_file(vfs, feature, ".rb")
    end

    def Central.find_mrbfile(vfs, feature)
      return feature if feature = Central.find_file(vfs, feature, ".mrb")
    end

    def Central.find_sofile(vfs, feature)
      return feature if feature = Central.find_file(vfs, feature, SOTYPES)
    end

    def Central.load_common(vfs, feature)
      #p [__method__, __LINE__, vfs, feature]
      signature = make_signature(vfs, feature)
      #p [__method__, __LINE__, vfs, signature]
      if $".include? signature
        false
      else
        #puts "#{__FILE__}(#{__LINE__})#{__method__}" => [vfs, feature]
        yield signature
        #puts "#{__FILE__}(#{__LINE__})#{__method__}" => [vfs, feature]
        $" << signature
        true
      end
    end

    def Central.load_as_rb(vfs, rb)
      vfs = SystemVFS.new(vfs) if vfs.kind_of?(String)

      load_common(vfs, rb) do |sig|
        #puts "#{__FILE__}(#{__LINE__})#{__method__}" => [vfs, sig]
        compile_from_rb(vfs, rb, sig, vfs.read(rb))
      end
    end

    def Central.load_as_mrb(vfs, mrb)
      vfs = SystemVFS.new(vfs) if vfs.kind_of?(String)

      load_common(vfs, mrb) do |sig|
        #puts "#{__FILE__}(#{__LINE__})#{__method__}" => [vfs, sig]
        load_from_mrb(vfs, mrb, sig, vfs.read(mrb))
      end
    end

    def Central.load_as_so(vfs, so)
      vfs = SystemVFS.new(vfs) if vfs.kind_of?(String)

      load_common(vfs, so) do |sig|
        #if vfs.respond_to?(:load_shared_object)
        #  vfs.load_shared_object(so, sig)
        #else
          load_shared_object(vfs, so, sig, vfs.read(so))
        #end
      end
    end

    def Central.findvfs(file)
      $:.each do |vfs|
        prefix = make_prefix(vfs)
        prefix << "/"
        #p [__FILE__, __LINE__, prefix, file]
        if file[prefix]
          subpath = file[prefix.size..-1]
          if Central.file?(vfs, subpath)
            return [vfs, Central.dirname(subpath)]
          end
        end
      end

      raise LoadError, "cannot infer basepath"
    end

    def Central.find_file(vfs, file, exts)
      if vfs.kind_of?(String)
        vfs = SystemVFS.new(vfs)
      end

      deep_each(exts) do |ext|
        return file if extname(file) == ext &&
                       vfs.file?(file) &&
                       vfs.size(file) < RequirePlus.loadsize_max
      end

      deep_each(exts) do |ext|
        t = file + ext
        return t if vfs.file?(t) &&
                    vfs.size(t) < RequirePlus.loadsize_max
      end

      nil
    end

    def Central.file?(vfs, subpath)
      case vfs
      when nil, ""
        raise "wrong load path - #{vfs.inspect}"
      when String
        ;
      when SystemVFS
        vfs = vfs.basepath
      else
        return vfs.file?(subpath)
      end

      File.file? makepath(vfs, subpath)
    end

    def Central.make_prefix(vfs)
      case vfs
      when nil, ""
        raise "wrong load path - #{vfs.inspect}"
      when String
        vfs.dup
      when SystemVFS
        vfs.basedir.dup
      else
        "VFS:#<#{vfs.to_path}>"
      end
    end

    def Central.make_signature(vfs, path)
      makepath(make_prefix(vfs), path)
    end

    def Central.deep_each(obj, &block)
      case obj
      when Array, Range, Enumerator
        obj.each do |o|
          case obj
          when Array, Range, Enumerator
            deep_each(o, &block)
          else
            yield o
          end
        end
      else
        yield obj
      end

      obj
    end

    const_set :SystemVFS, Class.new(Struct.new(:basedir))

    class SystemVFS
      const_set :BasicStruct, superclass

      def file?(path)
        File.file? File.join(basedir, path)
      end

      def size(path)
        File.size File.join(basedir, path)
      end

      def read(path)
        File.open(File.join(basedir, path), "rb") { |f| f.read }
      end

      def load_shared_object(so, sig)
        # c で実装する
      end
    end
  end
end

module Kernel
  include RequirePlus::Kernel
end

class Object
  include Kernel
end
