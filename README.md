# mruby-require-plus - extension loader for mruby with Virtual Filesystem support

***早期開発版です。まともな動作は期待できません。***

Ruby で利用される `require` の mruby 版……と言うだけではなく、利用者定義可能な仮想ファイルシステムに対応したものです。

<https://www.atdot.net/~ko1/diary/201212.html#d25> でチラッと出てくる「複数ファイルをまとめる方法を作る（汎用 require フレームワーク）→ VFS 要る？」に刺激されて作成した CRuby 向けの拙作 [invfs](https://github.com/dearblue/ruby-invfs) の概念を mruby へ持ち込みました。


## 注意と制限

 1. メモリをジャブジャブ使います。
 2. (for Microsoft Windows) シフト JIS や CP932 な文字列はこの世界に存在しません。
 3. (for mruby-1.4.0) *`require` された最中における `Fiber` の作成や切り替えは不安定です。意図しない場所でおそらく `SIGSEGV` を引き起こします。*
 4. 親ディレクトリを辿ったり、シンボリックリンクをまたいだりしたパスの指定は上手く扱えません。バグやセキュリティリスクを増大させるでしょう。
 5. *mruby をサンドボックスとして利用することは不可能になります。**確実にセキュリティを低下させます** (ロードパスの制限などは現在予定にありません)。*
 6. 読み込むファイル形式は、拡張子によってのみ識別されます。ファイルの内容を探って形式を認識していません。
 7. マルチバイトに対する対応は不完全です (将来的に改善するかもしれません)。
 8. `MRB_UTF8_STRING` が指定されていても何も注意を払わないし特別な処理も行いません (将来的に改善するかもしれません)。
 9. *`MRB_USE_ETEXT_EDATA` を定義しないで `.mrb` ファイルを読み込ませた場合、バッファ領域の破棄によって `SIGSEGV` を引き起こします。*
10. `require_relative` メソッドを再定義する手段はありません (再定義して `super` しても正しい呼び出し元が取得できないため動作しません)。
11. mruby-require-plus 向けの拡張ライブラリを作成するためのツールは、今のところありません。全部手書きです。
12. `libmruby.a` をそのままリンクすると静的リンクとなるため、関数の実体が実行ファイルと共有オブジェクトファイルとでそれぞれに分裂することになります。うまく組み合わせて下さい。
13. *`.so` ファイルにおける mruby のバージョンやビルド設定を比較する手段はありません。適合しない場合は `SIGSEGV` を引き起こします。*
14. `MRB_TT_DATA` で独自開放関数 (`struct RData` の `dfree` のこと) を登録するライブラリを利用する場合、`mrb_close()` 中にライブラリのファイナライザで `MRB_TT_DATA` オブジェクトの開放処理を行う必要があります。  
    そうでなければライブラリを開放した後に独自開放関数が呼ばれることとなるので、おそらく `SIGSEGV` を起こします。


## できること

  - Ruby API
      - `Kernel#require(feature)` (`feature` is supported `.rb`, `.mrb` and `.so`)
      - `Kernel#require_relative(feature)` (`feature` is supported `.rb`, `.mrb` and `.so`)
      - `Kernel#load(file)` (`file` is ruby script only)
      - (そのうち実装されます) `RequirePlus.regist(vfs)` (aliased from `$:.vfs_regist`)
  - C API  
    そのうち実装されます


## くみこみかた

`build_config.rb` に gem として追加して、mruby をビルドして下さい。

```ruby
MRuby::Build.new do |conf|
  conf.gem "mruby-require-plus", github: "dearblue/mruby-require-plus"
end
```

- - - -

mruby gem パッケージとして依存したい場合、`mrbgem.rake` に記述して下さい。

```ruby
# mrbgem.rake
MRuby::Gem::Specification.new("mruby-XXX") do |spec|
  ...
  spec.add_dependency "mruby-require-plus", github: "dearblue/mruby-require-plus"
end
```

### ビルド設定に関して

  - `MRB_INT16` - 想定していない動作設定です。
  - `MRB_UTF8_STRING` - 想定していない動作設定です。おそらく、不可解な挙動を引き起こします。
  - `MRB_USE_ETEXT_EDATA` - `.mrb` ファイルを組み込み可能とした場合に必要です。未設定である場合は SIGSEGV を引き起こします。
  - `MRUBY_REQUIRE_PLUS_WITHOUT_RB` - (現在は無視されます) `.rb` ファイルの組み込み機能を排除します。
  - `MRUBY_REQUIRE_PLUS_WITHOUT_MRB` - (現在は無視されます) `.mrb` ファイルの組み込み機能を排除します。
  - `MRUBY_REQUIRE_PLUS_WITHOUT_SO` - (現在は無視されます) `.so` ファイルの組み込み機能を排除します。


## つかいかた

### ロードパス

『ロードパス』は CRuby で言うところの `$:` や `$LOAD_PATH` の事です。

もし環境変数 `MRUBYLIB` が設定されていた場合、ロードパスの先頭に追加されます。区切り文字は `":"` です。

さらに実行ファイル (mruby や mirb、カスタマイズされたユーザプログラムなど) の置かれたディレクトリが取得可能な場合、そのディレクトリを基点として以下の順番で追加されます:

  - `../lib/mruby`
  - `../lib`
  - `./lib/mruby`
  - `./lib`

その他にロードパスを必要とする場合は、利用者が好きに増減することが出来ます。

```ruby
$: << "/usr/local/lib/mruby/1.2.3"
$:.insert 0, "/usr/home/YOURNAME/lib/mruby"
```

#### 仮想ファイルシステム (VFS)

仮想ファイルシステム (VFS) として追加する場合、任意の VFS オブジェクト (クラスやモジュールも含みます) をロードパスに追加して下さい。

```ruby
$: << MyVFS.new
```

この VFS オブジェクトは次のメソッドを利用できなければなりません:

  - `.file?(relative_path) -> true or false`
  - `.size(relative_path) -> nil or integer`
  - `.read(relative_path) -> nil or string`

VFS オブジェクトが `.load_shared_object(soname, signature)` メソッドを定義してある場合、`.so` ファイルの面倒を直接見ることが出来ます。
一時ディレクトリへの書き込みと削除が不要となるため、パフォーマンス・セキュリティの向上が見込めるかもしれません。

### `require`

Ruby とそんなに変わりません。

```ruby
require "xyz"
```

### `require_relative`

Ruby とそんなに変わりませんが、いくつかの制限があります。

```ruby
require_relative "abc/def"
```

  - 実ディレクトリから VFS への指定は出来ません。
  - VFS の中であれば、その VFS 内部のファイルしか辿れません。
  - 親ディレクトリを辿ったり、シンボリックリンクをまたいだりするパスの指定は上手く扱えません。

### `load`

Ruby とそんなに変わりませんが、いくつかの制限があります。

```ruby
load "settings.rb"
```

  - Ruby と同様に拡張子の補完は行いません。
  - `*.mrb` と `*.so` としての読み込みは出来ません (常に `*.rb` として扱われます)。


## 拡張ライブラリ

### ".rb" ファイル

mruby 向けの Ruby スクリプトを用意して読み込むことが出来ます。  
内部で `require` や `require_relative`、`load` を利用することが可能です。

### ".mrb" ファイル

あらかじめ `mrbc` によってコンパイルされた mruby 向けのバイトコードファイルを用意して読み込むことが出来ます。  
内部で `require` や `require_relative`、`load` を利用することが可能です。

***`mrbc` と動作させる mruby 実行プログラムのバージョンを合わせて下さい。***

`mrbc` は既定ではファイル・行番号情報を出力しないため、スタックトレースを辿れません。  
必要であれば `mrbc -g` のようにして `.mrb` ファイルを作成して下さい。

### ".so" ファイル

C などで拡張ライブラリを作成する場合、初期化関数と後処理関数が必要です。任意で mruby バイトコードを格納した irep 変数を持つ事が出来ます。

  - 公開関数・公開変数の型:

    - `void mrb_FEATURE_require_plus_init(mrb_state *mrb)`
    - `void mrb_FEATURE_require_plus_final(mrb_state *mrb)`
    - `const void *mrb_FEATURE_require_plus_irep`  
      (`const uint8_t mrb_FEATURE_require_plus_irep[] = { ... };` という定義でも問題ありません)

  - これらの識別子を生成するためのマクロ:

    - `MRUBY_REQUIRE_PLUS_INITIALIZE(feature name)`
    - `MRUBY_REQUIRE_PLUS_FINALIZE(feature name)`
    - `MRUBY_REQUIRE_PLUS_IREP(feature name)`

  - 利用可能なヘッダファイル:

    - `mruby-require-plus.h`
    - `mruby-require+.h` (`#include "mruby-require-plus.h"` するだけです)


## 環境変数について

### `MRUBY_REQUIRE_PLUS_TMPDIR` / `TMPDIR` / `TEMP` / `TMP`

一時的に使われる `.so` ファイルの書き出しディレクトリを指定することが出来ます。
プログラムを実行するアカウントは、対象ディレクトリの読み書き実行が許可されている必要があります。

これらの環境変数がどれも指定されていない場合、既定ディレクトリは `/tmp` となります。

### `MRUBYLIB`

ロードパスに追加される、ディレクトリの並びです。区切り文字は `:` です。


## 動作状況

  - (可) mruby-1.2.0 on FreeBSD 12.0
  - (可) mruby-1.3.0 on FreeBSD 12.0
  - (制限ありで可) mruby-1.4.0 on FreeBSD 12.0
  - (可) mruby-1.4.1 on FreeBSD 12.0
  - (可) mruby-2.0.0 on FreeBSD 12.0
  - (可) mruby-2.0.1 on FreeBSD 12.0


## Specification

  - Package name: mruby-require-plus
  - Version: 0.0.1.CONCEPT.PREVIEW
  - Product quality: CONCEPT
  - Author: [dearblue](https://github.com/dearblue)
  - Project page: <https://github.com/dearblue/mruby-require-plus>
  - Licensing: [2 clause BSD License](LICENSE)
  - Dependency external mrbgems: (NONE)
  - Bundled C libraries (git-submodules): (NONE)
