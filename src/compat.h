#ifndef MRUBY_REQUIRE_PLUS_COMPAT_H
#define MRUBY_REQUIRE_PLUS_COMPAT_H 1

#include <mruby.h>

#if MRUBY_RELEASE_NO < 10300
static struct RClass *
mrb_exc_get(mrb_state *mrb, const char *name)
{
    return mrb_class_get(mrb, name);
}
#endif

#include <mruby.h>
#include <mruby/proc.h>

#if MRUBY_RELEASE_NO < 10400
# ifndef MRB_PROC_SET_TARGET_CLASS
#  define MRB_PROC_SET_TARGET_CLASS(PROC, CLASS) ((PROC)->target_class = (CLASS))
# endif
#endif

#include <mruby.h>
#include <mruby/string.h>

#if MRUBY_RELEASE_NO < 20000
static mrb_value
mrb_to_str(mrb_state *mrb, mrb_value obj)
{
    return mrb_str_to_str(mrb, obj);
}
#endif

#include <mruby.h>

#if MRUBY_RELEASE_NO < 20001
MRB_API void mrb_notimplement(mrb_state *mrb);
#endif

#endif /* MRUBY_REQUIRE_PLUS_COMPAT_H */
