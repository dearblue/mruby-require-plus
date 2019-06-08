#include <mruby.h>
#include <mruby/dump.h>
#include <mruby/irep.h>
#include <mruby/value.h>
#include <stdint.h>
#include <string.h>

#define E_LOAD_ERROR mrb_exc_get(mrb, "LoadError")

static uint32_t
loadu32be(const void *ptr)
{
  const uint8_t *p = (const uint8_t *)ptr;
  return ((uint32_t)p[0] << 24) |
         ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] <<  8) |
         ((uint32_t)p[3] <<  0);
}

static const uint8_t *
check_mruby_binary(mrb_state *mrb, const void *buf, size_t size, mrb_value name)
{
  const struct rite_binary_header *bin = (const struct rite_binary_header *)buf;
  if (size < sizeof(struct rite_binary_header) ||
      size < loadu32be(bin->binary_size)) {
    mrb_raisef(mrb, E_LOAD_ERROR,
               "wrong binary size - %S",
               name);
  }

  if (memcmp(bin->binary_version, RITE_BINARY_FORMAT_VER, sizeof(bin->binary_version)) != 0) {
    mrb_raisef(mrb, E_LOAD_ERROR,
               "wrong binary version - %S (expected \"%S\", but given \"%S\")",
               name,
               mrb_str_new_lit(mrb, RITE_BINARY_FORMAT_VER),
               mrb_str_new(mrb, (const char *)bin->binary_version, sizeof(bin->binary_version)));
  }

  return (const uint8_t *)buf;
}

#if MRUBY_RELEASE_NO < 20002 && !defined(mrb_true_p)
static mrb_irep *
mrb_read_irep_buf(mrb_state *mrb, const void *buf, size_t bufsize, mrb_value name)
{
  check_mruby_binary(mrb, buf, bufsize, name);
  return mrb_read_irep(mrb, (const uint8_t *)buf);
}
#endif

#include "internals.h"
#include <mruby/compile.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <wchar.h>
#include <mruby-aux/mobptr.h>
#include <mruby/dump.h>
#include <mruby/proc.h>

#define LOG0() do { fprintf(stderr, "%s:%d:%s.\n", __FILE__, __LINE__, __func__); } while (0)
#define LOGS(MESG) do { fprintf(stderr, "%s:%d:%s: %s\n", __FILE__, __LINE__, __func__, MESG); } while (0)
#define LOGF(...) do { fprintf(stderr, "%s:%d:%s: ", __FILE__, __LINE__, __func__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while (0)

#define TOKEN2STR_2(T)  #T
#define TOKEN2STR_1(T)  TOKEN2STR_2(T)
#define TOKEN2STR(T)    TOKEN2STR_1(T)

#ifdef _WIN32
# define PATH_SPLITTER ';'
#else
# define PATH_SPLITTER ':'
#endif

//#define DEBUG_FORCE_WITH_WINDOWS_CODE
#define MATERIALIZE_COMPONENTNAME
#include "componentname.c"

#define MATERIALIZE_WHATMYNAME
#include "whatmyname.c"

#ifndef max
# define max(X, Y) ((X) > (Y) ? (X) : (Y))
#endif

#ifndef clamp
# define clamp(N, MIN, MAX) ((N) < (MIN) ? (MIN) : (N) > (MAX) ? (MAX) : (N))
#endif

#if defined(__FreeBSD__) && !defined(HAVE_FDLOPEN) && !defined(WITHOUT_FDLOPEN)
# define HAVE_FDLOPEN 1
#endif

static void make_funcname(MRB, VALUE str, const char name[]);

static void
aux_str_add_pathsep(MRB, VALUE str)
{
  if (needpathsep(RSTRING_PTR(str), RSTRING_LEN(str))) {
    mrb_str_cat_cstr(mrb, str, "/");
  }
}

static void
aux_add_pathsep(char *str, size_t len)
{
  if (needpathsep(str, len)) {
    str[len + 0] = '/';
    str[len + 1] = '\0';
  }
}

static void
make_funcname(MRB, VALUE str, const char name[])
{
  RSTR_SET_LEN(mrb_str_ptr(str), 0);
  mrb_str_cat_cstr(mrb, str, TOKEN2STR(MRUBY_REQUIRE_PLUS_COMMON_PREFIX));

  const char *p = name;
  for (; *p != '\0'; p ++) {
    char ch;
    if (isalnum(*p)) {
      ch = *p;
    } else {
      ch = '_';
    }

    mrb_str_cat(mrb, str, &ch, 1);
  }
}

static const char *
make_initname(MRB, VALUE str, const char name[])
{
  make_funcname(mrb, str, name);
  mrb_str_cat_cstr(mrb, str, TOKEN2STR(MRUBY_REQUIRE_PLUS_INIT_SUFFIX));
  return RSTRING_PTR(str);
}

static const char *
make_finalname(MRB, VALUE str, const char name[])
{
  make_funcname(mrb, str, name);
  mrb_str_cat_cstr(mrb, str, TOKEN2STR(MRUBY_REQUIRE_PLUS_FINAL_SUFFIX));
  return RSTRING_PTR(str);
}

static const char *
make_irepname(MRB, VALUE str, const char name[])
{
  make_funcname(mrb, str, name);
  mrb_str_cat_cstr(mrb, str, TOKEN2STR(MRUBY_REQUIRE_PLUS_IREP_SUFFIX));
  return RSTRING_PTR(str);
}

#ifdef MRUBY_REQUIRE_PLUS_WITHOUT_SO
#else
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
# include <windows.h>
# define dlopen(N, F)   ((void *)LoadLibraryEx(N))
# define dlfunc(H, N)   ((void *(*)())GetProcAddressA((HMODULE)H, N))
# define dlclose(H)     FreeLibrary((HMODULE)H)

static int
write_file(const char path[], const void *buf, size_t buflen)
{
  HANDLE file;
# ifdef MRB_UTF8_STRING
  //size_t wlen = ....;
  //wchar_t wpath[wlen];
  //path を wpath に変換する
  //m
  //  HANDLE file = CreateFileW(wpath, ....);
# endif
  {
    file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
  }
  if (handle == INVALID_HANDLE_VALUE) { return -1; }
  if (WriteFile(file, buf, (DWORD)buflen, NULL, NULL) == FALSE) { return -1; }
  CloseHandle(file);
  return 0;
}

#else
# include <dlfcn.h>
# ifndef __FreeBSD__
#  define dlfunc(H, N) ((void *(*)())dlsym(H, N))
# endif

static int
write_file(const char path[], const void *buf, size_t buflen)
{
  int fd = open(path, O_RDWR | O_EXCL | O_CREAT | O_TRUNC, 0700);
  write(fd, buf, buflen);
  close(fd);
  return 0;
}

#endif

#define id_loaded_shared_objects(MRB) mrb_intern_lit(MRB, "loaded shared objects@require+")

struct loadso_spec;
typedef void mruby_require_plus_init_f(mrb_state *mrb);
typedef void mruby_require_plus_final_f(mrb_state *mrb);

static void unload_shared_object(MRB, struct loadso_spec *p);
static mrb_value compile_from_rb(MRB, VALUE self);
static mrb_value load_from_mrb(MRB, VALUE self);
static mrb_value load_shared_object(MRB, VALUE self);

/*
 * 片方向リストを用いる。
 *
 * 読み込まれたライブラリは頭に追加されるため、next を辿ると読み込んだ逆順で参照することになる。
 * これは終了処理時にしか利用されず、`mrb_XXX_require_plus_final()` を呼ぶ時に逆順となるので都合が良い。
 */
struct loadso_spec
{
  struct loadso_spec *next;
  void *linkage;
  mruby_require_plus_final_f *final;
};

static VALUE
loadso_free_trial(MRB, VALUE opaque)
{
  struct loadso_spec *p = (struct loadso_spec *)mrb_cptr(opaque);
  p->final(mrb);
  return Qnil;
}

static void
loadso_free(MRB, void *ptr)
{
  struct loadso_spec *p = (struct loadso_spec *)ptr;
  int ai = mrb_gc_arena_save(mrb);
  while (p) {
    struct loadso_spec *next = p->next;
    if (p->final) {
      mrb_protect(mrb, loadso_free_trial, mrb_cptr_value(mrb, p), NULL);
      mrb_gc_arena_restore(mrb, ai);
    }
    if (p->linkage) {
      dlclose(p->linkage);
    }
    mrb_free(mrb, p);
    p = next;
  }
}

static const mrb_data_type loaded_shared_object_type = { "so data@require+", loadso_free };

static void unload_shared_object(MRB, struct loadso_spec *p);

static void
parser_free(MRB, void *ptr)
{
  if (ptr) {
    mrb_parser_free((struct mrb_parser_state *)ptr);
  }
}

static VALUE
aux_exec_toplevel(MRB, struct RProc *proc)
{
  MRB_PROC_SET_TARGET_CLASS(proc, mrb->object_class);
#if MRUBY_RELEASE_NO < 20000
  //mrb->c->ci->target_class = mrb->object_class;
  mrb->c->ci->argc = 0;
  //mrb->c->stack[0] = mrb_top_self(mrb); /* receiver */
  //mrb->c->stack[1] = mrb_nil_value();   /* block */
  return mrb_toplevel_run(mrb, proc);
#else
  mrb->c->ci->mid = 0; /* スタックトレースに `:in METHOD-NAME` が追加されることを防止する */
  return mrb_yield_with_class(mrb, VALUE(proc), 0, NULL, mrb_top_self(mrb), mrb->object_class);
#endif
}

#if MRUBY_RELEASE_NO == 10400
struct aux_exec_proc_on_toplevel
{
  struct RProc *proc;
  struct mrb_context *workctx;
  struct mrb_context *origctx;
};

/* これらの具体的な数値は mruby-fiber からのパクリ */
enum {
  context_stack_size = 64,
  context_ci_size = 8,
};

static struct mrb_context *
alloc_context(mrb_state *mrb)
{
  /*
   * XXX: はたしてバージョン間における互換性がどれだけ保たれることやら……。
   *      mruby 1.2, 1.3, 1.4, 1.4.1, 2.0 は host/bin/mruby-gemcut-test が動作することを確認。
   */
  struct mrb_context *c = (struct mrb_context *)mrb_calloc(mrb, 1, sizeof(struct mrb_context));
  c->stbase = (mrb_value *)mrb_calloc(mrb, context_stack_size, sizeof(*c->stbase));
  c->stend = c->stbase + context_stack_size;
  c->stack = c->stbase;
  c->cibase = (mrb_callinfo *)mrb_calloc(mrb, context_ci_size, sizeof(*c->cibase));
  c->ciend = c->cibase + context_ci_size;
  c->ci = c->cibase;
  c->ci->stackent = c->stack;
  //c->ci->proc = mrb->c->ci->proc;
  c->ci->target_class = mrb->object_class;
  //c->fib = mrb->c->fib;

  return c;
}

static VALUE
aux_exec_proc_on_toplevel_trial(MRB, VALUE obj)
{
  struct aux_exec_proc_on_toplevel *args = mrb_cptr(obj);

  args->workctx = alloc_context(mrb);
  args->origctx = mrb->c;
  mrb->c = args->workctx;
  return aux_exec_toplevel(mrb, args->proc);
}

static VALUE
aux_exec_proc_on_toplevel_cleanup(MRB, VALUE obj)
{
  struct aux_exec_proc_on_toplevel *args = mrb_cptr(obj);
  mrb->c = args->origctx;
  if (args->workctx) { mrb_free_context(mrb, args->workctx); }
  return Qnil;
}
#endif

static VALUE
aux_exec_proc_on_toplevel(MRB, struct RProc *proc)
{
  //LOGF("GC arena index: %d", (int)mrb_gc_arena_save(mrb));

#if MRUBY_RELEASE_NO == 10400
  struct aux_exec_proc_on_toplevel args;
  memset(&args, 0, sizeof(args));
  VALUE argsv = mrb_cptr_value(mrb, &args);
  args.origctx = mrb->c;
  args.proc = proc;
  return mrb_ensure(mrb, aux_exec_proc_on_toplevel_trial, argsv, aux_exec_proc_on_toplevel_cleanup, argsv);
#else
  return aux_exec_toplevel(mrb, proc);
#endif
}

static mrb_value
compile_from_rb(MRB, VALUE self)
{
  mrb_value vfs, name;
  const char *signature, *code;
  mrb_int codesize;
  mrb_get_args(mrb, "oSzs", &vfs, &name, &signature, &code, &codesize);

  int ai = mrb_gc_arena_save(mrb);
  mrb_value mob = mrbx_mob_create(mrb);
  mrbc_context *cc = mrbc_context_new(mrb);
  mrbc_filename(mrb, cc, signature);
  mrbx_mob_push(mrb, mob, cc, (mrbx_mob_free_f *)mrbc_context_free);
  struct mrb_parser_state *parser = mrb_parse_nstring(mrb, code, codesize, cc);
  mrbx_mob_push(mrb, mob, parser, parser_free);
  mrb_parser_set_filename(parser, signature);
  struct RProc *proc = mrb_generate_code(mrb, parser);
  mrbx_mob_cleanup(mrb, mob);
  mrb_gc_arena_restore(mrb, ai);
  mrb_gc_protect(mrb, VALUE(proc));

  aux_exec_proc_on_toplevel(mrb, proc);
  mrb_gc_arena_restore(mrb, ai);

  return Qnil;
}

static mrb_value
load_from_mrb(MRB, VALUE self)
{
  mrb_value vfs, name;
  const char *signature;
  const uint8_t *bin;
  mrb_int binsize;
  mrb_get_args(mrb, "oSzs", &vfs, &name, &signature, &bin, &binsize);

  int ai = mrb_gc_arena_save(mrb);
  mrb_value mob = mrbx_mob_create(mrb);
  mrb_irep *irep = mrb_read_irep_buf(mrb, bin, binsize, name);
  if (irep == NULL) {
    check_mruby_binary(mrb, bin, binsize, name);
    mrb_raisef(mrb, E_LOAD_ERROR, "load error - %S", name);
  }
  mrbx_mob_push(mrb, mob, irep, (mrbx_mob_free_f *)mrb_irep_decref);
  struct RProc *proc = mrb_proc_new(mrb, irep);
  mrbx_mob_pop(mrb, mob, irep);
  mrbx_mob_cleanup(mrb, mob);
  mrb_gc_arena_restore(mrb, ai);
  mrb_gc_protect(mrb, VALUE(proc));

  aux_exec_proc_on_toplevel(mrb, proc);
  mrb_gc_arena_restore(mrb, ai);

  return Qnil;
}

static struct loadso_spec *
prepare_linkage(MRB)
{
  mrb_value loadedso = mrb_gv_get(mrb, id_loaded_shared_objects(mrb));
  mrb_data_check_type(mrb, loadedso, &loaded_shared_object_type);
  struct loadso_spec *p = (struct loadso_spec *)DATA_PTR(loadedso);

  /*
   * 最初にロードする時か、以前ロードを試みて成功した時に新しい領域を確保する。
   * 以前失敗した場合は NULL で埋めてあるだけなので、再利用する。
   */
  if (p == NULL || p->linkage != NULL) {
    struct loadso_spec *o = (struct loadso_spec *)mrb_calloc(mrb, 1, sizeof(struct loadso_spec));
    o->next = p;
    DATA_PTR(loadedso) = p = o;
  }

  return p;
}

static void so_rmdir_and_free_heap(MRB, void *ptr) { rmdir((const char *)ptr); mrb_free(mrb, ptr); }
static void so_file_delete_and_free_heap(MRB, void *ptr) { unlink((const char *)ptr); mrb_free(mrb, ptr); }
static void so_fd_close(MRB, void *ptr) { close((int)(uintptr_t)ptr); }
static void so_dl_close(MRB, void *ptr) { dlclose(ptr); }

static const char *
make_tmpdir(MRB, VALUE mob)
{
  /* template は C++ のキーワードなので…… */
  static const char temprate[] = "mruby-require+XXXXXXXXXXXXXXXXXXXXXXXX";
  const char *tmproot;

  if ((tmproot = getenv("MRUBY_REQUIRE_PLUS_TMPDIR")) == NULL &&
      (tmproot = getenv("TMPDIR")) == NULL &&
      (tmproot = getenv("TEMP")) == NULL &&
      (tmproot = getenv("TMP")) == NULL) {
    tmproot = "/tmp";
  }

  char *s = (char *)mrbx_mob_malloc(mrb, mob, strlen(tmproot) + strlen("/") + strlen(temprate) + 1);
  s[0] = '\0';
  strcat(s, tmproot);
  aux_add_pathsep(s, strlen(s));
  strcat(s, temprate);
  mktemp(s);
  if (mkdir(s, 0700) != 0) { return NULL; }
  mrbx_mob_push(mrb, mob, s, so_rmdir_and_free_heap);

  return s;
}

static const char *
make_tmpname(MRB, VALUE mob, const char tmpdir[], const char name[])
{
  char *tmpname = (char *)mrbx_mob_malloc(mrb, mob, strlen(tmpdir) + strlen("/") + strlen(name) + 1);

  tmpname[0] = '\0';
  strcat(tmpname, tmpdir);
  strcat(tmpname, "/");
  strcat(tmpname, name);

  return tmpname;
}

static void *
masquerade_dlopen(MRB, VALUE mob, const char name[], const void *bin, size_t binsize)
{
  const char *tmpdir = make_tmpdir(mrb, mob);
  if (tmpdir == NULL) { return NULL; }

  const char *tmpname = make_tmpname(mrb, mob, tmpdir, name);
  if (tmpname == NULL) { return NULL; }

  enum {
#ifdef HAVE_FDLOPEN
    open_mode = O_RDWR | O_CREAT | O_TRUNC | O_EXCL | O_EXLOCK | O_NOFOLLOW | O_DIRECT
#else
    open_mode = O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_EXLOCK | O_NOFOLLOW | O_DIRECT
#endif
  };

  int fd = open(tmpname, open_mode, 0700);
  if (fd == -1) { return NULL; }
  mrbx_mob_push(mrb, mob, (void *)tmpname, so_file_delete_and_free_heap);
  mrbx_mob_push(mrb, mob, (void *)(uintptr_t)fd, so_fd_close);
  errno = 0;

#ifdef HAVE_FDLOPEN
  mrbx_mob_free(mrb, mob, (void *)tmpname);
  if (write(fd, bin, binsize) != binsize) { return NULL; }
  //fcntl(fd, F_SETFL, O_RDONLY | O_EXLOCK | O_NOFOLLOW);
  void *handle = fdlopen(fd, RTLD_NOW);
  mrbx_mob_free(mrb, mob, (void *)(uintptr_t)fd);
#else
  if (write(fd, bin, binsize) != binsize) { return NULL; }
  mrbx_mob_free(mrb, mob, (void *)(uintptr_t)fd);
  void *handle = dlopen(tmpname, RTLD_NOW);
  mrbx_mob_free(mrb, mob, (void *)tmpname);
#endif

  if (handle) {
    mrbx_mob_push(mrb, mob, handle, so_dl_close);
  }

  return handle;
}

static mrb_value
load_shared_object(MRB, VALUE self)
{
  mrb_value vfs, name;
  const char *signature, *bin;
  mrb_int binsize;
  mrb_get_args(mrb, "oSzs", &vfs, &name, &signature, &bin, &binsize);

  int ai = mrb_gc_arena_save(mrb);
  VALUE mob = mrbx_mob_create(mrb);

  mrb_str_strlen(mrb, mrb_str_ptr(name)); /* 途中に NUL が含まれていないことが保証される */
  void *handle = masquerade_dlopen(mrb, mob, RSTRING_PTR(name), bin, binsize);
  if (handle == NULL) { goto raise_exc; }

  {
    const char *rootterm, *dirterm, *basename, *extname, *nameterm;
    splitpath(RSTRING_PTR(name), RSTRING_LEN(name), &rootterm, &dirterm, &basename, &extname, &nameterm);
    VALUE base;
    if (nameterm - extname == 3 && memcmp(extname, ".so", 3) == 0) {
      base = mrb_str_new(mrb, basename, extname - basename);
    } else {
      base = mrb_str_new(mrb, basename, nameterm - basename);
    }
    VALUE funcname = mrb_str_new(mrb, NULL, 0);
    mruby_require_plus_init_f *init = (mruby_require_plus_init_f *)dlfunc(handle, make_initname(mrb, funcname, RSTRING_PTR(base)));
    mruby_require_plus_final_f *final = (mruby_require_plus_init_f *)dlfunc(handle, make_finalname(mrb, funcname, RSTRING_PTR(base)));
    const void *irepbin = dlsym(handle, make_irepname(mrb, funcname, RSTRING_PTR(base)));

    if (init == NULL && irepbin == NULL) { goto raise_exc; }

    mrbx_mob_pop(mrb, mob, handle);
    mrb_gc_arena_restore(mrb, ai);
    mrb_gc_protect(mrb, mob);

    struct loadso_spec *p = prepare_linkage(mrb);
    p->linkage = handle;
    p->final = final;

    if (init) {
      init(mrb);
      mrb_gc_arena_restore(mrb, ai);
      mrb_gc_protect(mrb, mob);
    }

    if (irepbin) {
      mrb_irep *irep = mrb_read_irep(mrb, (const uint8_t *)irepbin);
      mrbx_mob_push(mrb, mob, irep, (mrbx_mob_free_f *)mrb_irep_decref);
      struct RProc *proc = mrb_proc_new(mrb, irep);
      mrbx_mob_pop(mrb, mob, irep);
      mrbx_mob_cleanup(mrb, mob);

      mrb_gc_arena_restore(mrb, ai);
      mrb_gc_protect(mrb, VALUE(proc));
      aux_exec_proc_on_toplevel(mrb, proc);
    } else {
      mrbx_mob_cleanup(mrb, mob);
    }

    mrb_gc_arena_restore(mrb, ai);

    return Qnil;
  }

raise_exc:
  mrbx_mob_cleanup(mrb, mob);
  mrb_gc_arena_restore(mrb, ai);
  mrb_raisef(mrb, E_LOAD_ERROR, "failed load %S (in %S)", name, vfs);
  return Qnil; /* not reached */
}

static VALUE
joinpath(MRB, VALUE str, mrb_int argc, const VALUE argv[], bool *istermsep)
{
  /* FIXME: Windows だと ["C:", "/file"] の場合に "C:file" となってしまう */

  for (; argc > 0; argc --, argv ++) {
    VALUE tmp = *argv;

    switch (mrb_type(tmp)) {
    default:
      tmp = mrb_funcall_argv(mrb, tmp, SYMBOL("to_path"), 0, NULL);
      mrb_check_type(mrb, tmp, MRB_TT_STRING);
      /* fall through */
    case MRB_TT_STRING:
      {
        const char *s = RSTRING_PTR(tmp);
        mrb_int slen = RSTRING_LEN(tmp);

        if (mrb_nil_p(str)) {
          str = mrb_str_new(mrb, s, slen);
          *istermsep = !needpathsep(s, slen);
        } else if (slen > 0) {
          if (!(!!*istermsep ^ !!ispathsep(s[0]))) {
            /* 末尾が "/" で終わっておりかつ追加しようとしている文字列が "/" で始まる場合、
             * あるいは連結する時に "/" を必要とする場合は処理する */
            if (*istermsep) {
              s ++;
              slen --;
            } else {
              mrb_str_cat_cstr(mrb, str, "/");
            }
          }

          if (slen > 0) {
            *istermsep = ispathsep(s[slen - 1]);
            mrb_str_cat(mrb, str, s, slen);
          }
        }
      }

      break;
    case MRB_TT_ARRAY:
      str = joinpath(mrb, str, ARY_LEN(mrb_ary_ptr(tmp)), ARY_PTR(mrb_ary_ptr(tmp)), istermsep);
      break;
    }
  }

  return str;
}

static bool
split_frame_info(const char *info, const size_t len, const char **pathterm, int *line)
{
  const char *beg = info;
  const uintptr_t end = (uintptr_t)info + len;
  const char *path;
  int linenum;
  *pathterm = NULL;

retry:
  for (;; info ++) {
    if (end - (uintptr_t)info <= 0 || *info == '\0') {
      return *pathterm != NULL && *pathterm - beg > 0;
    } else if (*info == ':') {
      path = info ++;
      break;
    }
  }

retry_line:
  if (end - (uintptr_t)info <= 0 || !isdigit((uint8_t)*info)) {
    goto retry;
  }

  linenum = 0;
  for (;; info ++) {
    if (end - (uintptr_t)info <= 0 || *info == '\0') {
      if (linenum > 0) {
        *pathterm = path;
        *line = linenum;
        return *pathterm - beg > 0;
      } else {
        return false;
      }
    } else if (*info == ':') {
      info ++;
      break;
    } else if (!isdigit((uint8_t)*info)) {
      goto retry;
    } else {
      linenum = linenum * 10 + digittoint((uint8_t)*info);
    }
  }

  if (end - (uintptr_t)info < 3 ||
      info[0] != 'i' || info[1] != 'n' || info[2] != ' ') {
    path = info - 1;
    goto retry_line;
  }
  info += 3;

  for (;; info ++) {
    if (end - (uintptr_t)info <= 0 || *info == '\0') {
      *pathterm = path;
      *line = linenum;
      return *pathterm - beg > 0;
    } else if (*info == ':') {
      *pathterm = path;
      *line = linenum;
      path = info ++;
      goto retry_line;
    }
  }
}

static VALUE
split_frame_info_value(MRB, VALUE frame)
{
  mrb_check_type(mrb, frame, MRB_TT_STRING);
  const char *info = RSTRING_PTR(frame);
  size_t len = RSTRING_LEN(frame);
  const char *pathterm;
  int line;

  if (!split_frame_info(info, len, &pathterm, &line)) {
    return Qnil;
  }

  return MRBX_TUPLE(mrb_str_new(mrb, info, pathterm - info), mrb_fixnum_value(line));
}

static VALUE
ext_split_frame_info(MRB, VALUE self)
{
  mrb_get_args(mrb, "");
  return split_frame_info_value(mrb, self);
}

static VALUE
ext_get_upper_frame(MRB, VALUE self)
{
  mrb_get_args(mrb, "");
  VALUE bt = mrb_get_backtrace(mrb);
  VALUE uf = mrb_ary_ref(mrb, bt, 1);
  if (!mrb_string_p(uf)) { return Qnil; }
  return split_frame_info_value(mrb, uf);
}

static VALUE
ext_makepath(MRB, VALUE self)
{
  mrb_int argc;
  VALUE *argv;

  switch (mrb_get_args(mrb, "*", &argv, &argc)) {
  case 0:
    return mrb_str_new(mrb, NULL, 0);
  case 1:
    return mrb_to_str(mrb, argv[0]);
  }

  bool istermsep = false;

  return joinpath(mrb, Qnil, argc, argv, &istermsep);
}

static VALUE
ext_dirname(MRB, VALUE self)
{
  VALUE arg0;
  mrb_get_args(mrb, "S", &arg0);
  const char *path = RSTRING_PTR(arg0);
  size_t len = RSTRING_LEN(arg0);
  const char *rootterm, *dirterm, *basename, *extname, *nameterm;
  splitpath(path, len, &rootterm, &dirterm, &basename, &extname, &nameterm);

  if (dirterm == path) {
    return mrb_str_new_lit(mrb, ".");
  } else {
    return mrb_str_new(mrb, path, dirterm - path);
  }
}

static VALUE
ext_basename(MRB, VALUE self)
{
  VALUE arg0, arg1 = Qnil;
  mrb_get_args(mrb, "S|S", &arg0, &arg1);
  const char *path = RSTRING_PTR(arg0);
  size_t len = RSTRING_LEN(arg0);
  const char *rootterm, *dirterm, *basename, *extname, *nameterm;
  splitpath(path, len, &rootterm, &dirterm, &basename, &extname, &nameterm);

  if (nameterm == path) {
    return mrb_str_new_lit(mrb, ".");
  } else if (!mrb_nil_p(arg1)) {
    if (strcmp(".*", RSTRING_PTR(arg1)) == 0 ||
        strncmp(extname, RSTRING_PTR(arg1), nameterm - extname) == 0) {
      return mrb_str_new(mrb, basename, extname - basename);
    }
  }

  return mrb_str_new(mrb, basename, nameterm - basename);
}

static VALUE
ext_extname(MRB, VALUE self)
{
  VALUE arg0;
  mrb_get_args(mrb, "S", &arg0);
  const char *path = RSTRING_PTR(arg0);
  size_t len = RSTRING_LEN(arg0);
  const char *rootterm, *dirterm, *basename, *extname, *nameterm;
  splitpath(path, len, &rootterm, &dirterm, &basename, &extname, &nameterm);

  if (extname == nameterm) {
    return Qnil;
  } else {
    return mrb_str_new(mrb, extname, nameterm - extname);
  }
}

static VALUE
ext_split_path(MRB, VALUE self)
{
  const char *rootterm, *dirterm, *basename, *extname, *nameterm;
  mrb_check_type(mrb, self, MRB_TT_STRING);
  size_t pathlen = mrb_str_strlen(mrb, RSTRING(self));
  const char *path = RSTRING_PTR(self);
  splitpath(path, pathlen, &rootterm, &dirterm, &basename, &extname, &nameterm);

  return MRBX_TUPLE(mrb_str_new(mrb, path, rootterm - path),
                    mrb_str_new(mrb, path, dirterm - path),
                    mrb_str_new(mrb, basename, nameterm - basename),
                    mrb_str_new(mrb, extname, nameterm - extname));
}

static VALUE
ext_add_pathsep(MRB, VALUE self)
{
  mrb_check_type(mrb, self, MRB_TT_STRING);
  mrb_str_modify(mrb, RSTRING(self));
  aux_str_add_pathsep(mrb, self);
  return self;
}

static VALUE
ext_whatmyname(MRB, VALUE self)
{
  mrb_get_args(mrb, "");

  char path[PATH_MAX + 1];
  int size = whatmyname(path, sizeof(path));
  if (size > 0) {
    return mrb_str_new(mrb, path, size);
  } else {
    return Qnil;
  }
}

#define DEFAULT_LOADSIZE_MAX     ( 4 << 20) //  4 MiB (default)
#define DEFAULT_LOADSIZE_MINIMUM (16 << 10) // 16 KiB
#define DEFAULT_LOADSIZE_MAXIMUM (64 << 20) // 64 MiB

#define id_loadsize_max SYMBOL("loadsize_max@require+")

MRB_API size_t
mruby_require_plus_loadsize_max(MRB)
{
  VALUE v = mrb_gv_get(mrb, id_loadsize_max);
  if (mrb_float_p(v)) {
    return mrb_float(v);
  } else {
    return mrb_int(mrb, v);
  }
}

MRB_API void
mruby_require_plus_set_loadsize_max(MRB, size_t bytesize)
{
  VALUE v;
  bytesize = clamp(bytesize, DEFAULT_LOADSIZE_MINIMUM, DEFAULT_LOADSIZE_MAXIMUM);
  if (bytesize > MRB_INT_MAX) {
    v = mrb_float_value(mrb, bytesize);
  } else {
    v = mrb_fixnum_value(bytesize);
  }
  mrb_gv_set(mrb, id_loadsize_max, v);
}

static VALUE
rp_loadsize_max(MRB, VALUE self)
{
  mrb_get_args(mrb, "");
  return mrb_gv_get(mrb, id_loadsize_max);
}

static void
init_central(MRB)
{
  mruby_require_plus_set_loadsize_max(mrb, DEFAULT_LOADSIZE_MAX);

  struct RClass *reqpls = mrb_define_module(mrb, "RequirePlus");
  mrb_define_class_method(mrb, reqpls, "loadsize_max", rp_loadsize_max, MRB_ARGS_NONE());

  struct RClass *central = mrb_define_module_under(mrb, reqpls, "Central");
  mrb_define_class_method(mrb, central, "compile_from_rb", compile_from_rb, MRB_ARGS_REQ(3));
  mrb_define_class_method(mrb, central, "load_from_mrb", load_from_mrb, MRB_ARGS_REQ(3));
  mrb_define_class_method(mrb, central, "load_shared_object", load_shared_object, MRB_ARGS_REQ(3));

  mrb_define_class_method(mrb, central, "get_upper_frame", ext_get_upper_frame, MRB_ARGS_ANY());
  mrb_define_class_method(mrb, central, "makepath", ext_makepath, MRB_ARGS_ANY());
  mrb_define_class_method(mrb, central, "dirname", ext_dirname, MRB_ARGS_ANY());
  mrb_define_class_method(mrb, central, "basename", ext_basename, MRB_ARGS_ANY());
  mrb_define_class_method(mrb, central, "extname", ext_extname, MRB_ARGS_ANY());
  mrb_define_class_method(mrb, central, "whatmyname", ext_whatmyname, MRB_ARGS_ANY());
}

static void
init_solinks(MRB)
{
  struct RData *loaded_shared_objects = mrb_data_object_alloc(mrb, NULL, NULL, &loaded_shared_object_type);
  mrb_gv_set(mrb, id_loaded_shared_objects(mrb), VALUE(loaded_shared_objects));
}

static void
init_loadpath(MRB)
{
  VALUE loadpath = mrb_ary_new(mrb);
  int ai = mrb_gc_arena_save(mrb);
  mrb_gv_set(mrb, SYMBOL("$:"), loadpath);
  mrb_gv_set(mrb, SYMBOL("$LOAD_PATH"), loadpath);

  const char *env = getenv("MRUBYLIB");
  if (env) {
    const char *p = env;
    for (const char *q; (q = strchr(p, PATH_SPLITTER)) != NULL; p = q + 1) {
      if (q - p < 1) { continue; }
      mrb_ary_push(mrb, loadpath, mrb_str_new(mrb, p, q - p));
      mrb_gc_arena_restore(mrb, ai);
    }
    if (*p != '\0') {
      mrb_ary_push(mrb, loadpath, mrb_str_new_cstr(mrb, p));
      mrb_gc_arena_restore(mrb, ai);
    }
  }

  char myname[PATH_MAX + 1];
  int namelen = whatmyname(myname, sizeof(myname));
  if (namelen > 0) {
    const char *rootterm, *dirterm, *basename, *extname, *nameterm;
    /* dirname(myname) */
    splitpath(myname, namelen, &rootterm, &dirterm, &basename, &extname, &nameterm);
    if (dirterm > myname) {
      VALUE tmp;
      VALUE objdir1 = mrb_str_new(mrb, myname, dirterm - myname);
      /* dirname(dirname(myname)) */
      splitpath(myname, dirterm - myname, &rootterm, &dirterm, &basename, &extname, &nameterm);
      if (dirterm > myname) {
        VALUE objdir2 = mrb_str_new(mrb, myname, dirterm - myname);
        mrb_str_cat_cstr(mrb, objdir2, "/lib");
        tmp = mrb_str_dup(mrb, objdir2);
        mrb_str_cat_cstr(mrb, tmp, "/mruby");
        MRB_SET_FROZEN_FLAG(mrb_str_ptr(tmp));
        MRB_SET_FROZEN_FLAG(mrb_str_ptr(objdir2));
        mrb_ary_push(mrb, loadpath, tmp);
        mrb_ary_push(mrb, loadpath, objdir2);
      }
      mrb_str_cat_cstr(mrb, objdir1, "/lib");
      tmp = mrb_str_dup(mrb, objdir1);
      mrb_str_cat_cstr(mrb, tmp, "/mruby");
      MRB_SET_FROZEN_FLAG(mrb_str_ptr(tmp));
      MRB_SET_FROZEN_FLAG(mrb_str_ptr(objdir1));
      mrb_ary_push(mrb, loadpath, tmp);
      mrb_ary_push(mrb, loadpath, objdir1);
    }
  }

  // 呼び出し元関数でアリーナを復帰するので省略
  //mrb_gc_arena_restore(mrb, ai);
}

static void
init_loadedfeatures(MRB)
{
  VALUE features = mrb_ary_new(mrb);
  mrb_gv_set(mrb, SYMBOL("$\""), features);
  mrb_gv_set(mrb, SYMBOL("$LOADED_FEATURES"), features);
}

static void
init_loaderror(MRB)
{
  struct RClass *loaderr = mrb_define_class(mrb, "LoadError", E_SCRIPT_ERROR);
}

void
mrb_mruby_require_plus_gem_init(MRB)
{
  int ai = mrb_gc_arena_save(mrb);

  init_solinks(mrb);
  mrb_gc_arena_restore(mrb, ai);
  init_loadpath(mrb);
  mrb_gc_arena_restore(mrb, ai);
  init_loadedfeatures(mrb);
  mrb_gc_arena_restore(mrb, ai);
  init_loaderror(mrb);
  mrb_gc_arena_restore(mrb, ai);
  init_central(mrb);

  mrb_define_method(mrb, mrb->string_class, "__split_frame_info", ext_split_frame_info, MRB_ARGS_ANY());
  mrb_define_method(mrb, mrb->string_class, "__split_path", ext_split_path, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, mrb->string_class, "__add_pathsep", ext_add_pathsep, MRB_ARGS_NONE());
}

void
mrb_mruby_require_plus_gem_final(MRB)
{
  mrb_value loaded_shareds = mrb_gv_get(mrb, id_loaded_shared_objects(mrb));
  struct loaded_shared_objects *so = (struct loaded_shared_objects *)mrb_data_check_get_ptr(mrb, loaded_shareds, &loaded_shared_object_type);
  if (so == NULL) { return; }
  loadso_free(mrb, so);
  DATA_PTR(loaded_shareds) = NULL;
}
