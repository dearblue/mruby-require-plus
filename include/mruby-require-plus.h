#ifndef MRUBY_REQUIRE_PLUS_H
#define MRUBY_REQUIRE_PLUS_H 1

#include <mruby.h>

MRB_BEGIN_DECL

#define MRUBY_REQUIRE_PLUS_NAME_MAKE2(P, N, S)  P ## N ## S
#define MRUBY_REQUIRE_PLUS_NAME_MAKE1(P, N, S)  MRUBY_REQUIRE_PLUS_NAME_MAKE2(P, N, S)
#define MRUBY_REQUIRE_PLUS_NAME_MAKE(P, N, S)   MRUBY_REQUIRE_PLUS_NAME_MAKE1(P, N, S)

#define MRUBY_REQUIRE_PLUS_COMMON_PREFIX        mrb_
#define MRUBY_REQUIRE_PLUS_INIT_PREFIX          MRUBY_REQUIRE_PLUS_COMMON_PREFIX
#define MRUBY_REQUIRE_PLUS_INIT_SUFFIX          _require_plus_init
#define MRUBY_REQUIRE_PLUS_FINAL_PREFIX         MRUBY_REQUIRE_PLUS_COMMON_PREFIX
#define MRUBY_REQUIRE_PLUS_FINAL_SUFFIX         _require_plus_final
#define MRUBY_REQUIRE_PLUS_IREP_PREFIX          MRUBY_REQUIRE_PLUS_COMMON_PREFIX
#define MRUBY_REQUIRE_PLUS_IREP_SUFFIX          _require_plus_irep
#define MRUBY_REQUIRE_PLUS_INITIALIZE(NAME)     MRUBY_REQUIRE_PLUS_NAME_MAKE(MRUBY_REQUIRE_PLUS_INIT_PREFIX, NAME, MRUBY_REQUIRE_PLUS_INIT_SUFFIX)
#define MRUBY_REQUIRE_PLUS_FINALIZE(NAME)       MRUBY_REQUIRE_PLUS_NAME_MAKE(MRUBY_REQUIRE_PLUS_FINAL_PREFIX, NAME, MRUBY_REQUIRE_PLUS_FINAL_SUFFIX)
#define MRUBY_REQUIRE_PLUS_IREP(NAME)           MRUBY_REQUIRE_PLUS_NAME_MAKE(MRUBY_REQUIRE_PLUS_IREP_PREFIX, NAME, MRUBY_REQUIRE_PLUS_IREP_SUFFIX)

typedef void mruby_require_plus_init_func(mrb_state *mrb);
typedef void mruby_require_plus_final_func(mrb_state *mrb);

/* Ruby の `require "feature"` を模した処理を行います */
MRB_API mrb_bool mruby_require_plus_require(mrb_state *mrb, const char *feature);

/*
 * `vfs` の中の `feature` を読み込みます。拡張子は自動で補完されます。
 * `$LOAD_PATH` に含まれていない VFS を与えることが出来ますが、内部からの `require_relative` は失敗するでしょう。
 */
MRB_API mrb_bool mruby_require_plus_require_with_vfs(mrb_state *mrb, const char *feature, mrb_value vfs);

/* Ruby の `load "feature"` を模した処理を行います */
MRB_API mrb_bool mruby_require_plus_load(mrb_state *mrb, const char *feature);

/*
 * `vfs` の中の `feature` を読み込みます。拡張子は補完されません。
 * `$LOAD_PATH` に含まれていない VFS を与えることが出来ますが、内部からの `require_relative` は失敗するでしょう。
 */
MRB_API mrb_bool mruby_require_plus_load_with_vfs(mrb_state *mrb, const char *feature, mrb_value vfs);

/*
 * `$LOAD_PATH` に VFS を追加します。
 * `whence` は挿入位置です。`-1` で最後尾に追加することになります。
 */
MRB_API void mruby_require_plus_add_loadpath(mrb_state *mrb, mrb_value vfs, int whence);

/*
 * VFS ハンドラオブジェクトを追加します。
 */
MRB_API void mruby_require_plus_entry_vfs(mrb_state *mrb, mrb_value vfs_handler);

/*
 * 読み込み可能とする最大バイト数を取得します。
 */
MRB_API size_t mruby_require_plus_loadsize_max(mrb_state *mrb);

/*
 * 読み込み可能とする最大バイト数を設定します。
 */
MRB_API void mruby_require_plus_set_loadsize_max(mrb_state *mrb, size_t bytesize);

MRB_END_DECL

#endif /* MRUBY_REQUIRE_PLUS_H */
