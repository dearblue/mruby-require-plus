#ifndef MRUBY_REQUIRE_PLUS_INTERNALS_H
#define MRUBY_REQUIRE_PLUS_INTERNALS_H 1

#include "../include/mruby-require-plus.h"
#include <mruby.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/value.h>
#include <mruby/data.h>
#include <mruby/error.h>
#include <mruby-aux.h>
#include <stdlib.h>

#if defined(MRB_INT16)
# pragma message("動作環境として想定していない MRB_INT16 が定義されています。")
#endif

#ifdef MRB_UTF8_STRING
# pragma message("現状では MRB_UTF8_STRING の構成に注意を払っていないため、不可解な挙動を起こす可能性があります。")
#endif

/* Omit ruby script loader (.rb) */
/* #define MRUBY_REQUIRE_PLUS_WITHOUT_RB */

/* Omit mruby byte code loader (.mrb) */
/* #define MRUBY_REQUIRE_PLUS_WITHOUT_MRB */

/* Omit shared object file (.so) */
/* #define MRUBY_REQUIRE_PLUS_WITHOUT_SO */

#if defined(MRUBY_REQUIRE_PLUS_WITHOUT_RB) && \
    defined(MRUBY_REQUIRE_PLUS_WITHOUT_MRB) && \
    defined(MRUBY_REQUIRE_PLUS_WITHOUT_SO)
# error MRUBY_REQUIRE_PLUS_WITHOUT_RB と MRUBY_REQUIRE_PLUS_WITHOUT_MRB、MRUBY_REQUIRE_PLUS_WITHOUT_SO が指定されています。代わりに mruby-require-plus を除外すべきです。
#endif

#ifndef MRB_USE_ETEXT_EDATA
# pragma message("Need ``MRB_USE_ETEXT_EDATA'' configuration in your build_config.rb")
#endif

#include "compat.h"

#endif /* MRUBY_REQUIRE_PLUS_INTERNALS_H */
