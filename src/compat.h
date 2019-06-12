#ifndef MRUBY_REQUIRE_PLUS_COMPAT_H
#define MRUBY_REQUIRE_PLUS_COMPAT_H 1

#include <mruby.h>
#include <mruby/proc.h>

#if MRUBY_RELEASE_NO < 10400
# ifndef MRB_PROC_SET_TARGET_CLASS
#  define MRB_PROC_SET_TARGET_CLASS(PROC, CLASS) ((PROC)->target_class = (CLASS))
# endif
#endif

#endif /* MRUBY_REQUIRE_PLUS_COMPAT_H */
