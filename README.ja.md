[English](README.md) | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md) | **日本語**

# gakumas-VRify

『学園アイドルマスター』に VR 対応を追加する非公式 Mod です。**Windows x64 の DMM 版に対応**し、[gkms-localify-dmm](https://git.chinosk6.cn/chinosk/gkms-localify-dmm) をベースにしています。

このブランチは、現在の **GakuLoader プラグイン統合版 `GakuVRify.dll`** です。Localify の camera/render と Motion の begin-camera 共有ブリッジを実装しています。ソースビルドの既定は Plugin モードで、従来の単独プロキシ版は明示的に選択できます。

[ソースビルド](BUILDING.md) · [プラグイン設定とインターフェース](docs/GAKULOADER_PLUGIN.md) · [カメラの互換性](docs/MOTION_CAMERA_COEXIST.md) · [ソースの管理](UPSTREAM.md)

## ご利用前に

- ゲーム自体は VR 向けではないため、キャラクターの欠落、位置ずれ、画面のちらつきが発生する場合があります。
- **VR 酔いや点滅にご注意ください。体調に異変を感じた場合は使用を中止してください。**
- ゲームのローカル改変を利用する前に、[利用規約](https://legal.bandainamcoent.co.jp/terms/nejp)などの関連規則をご確認ください。アカウントおよび利用の結果は、ご自身の責任となります。
- ファンによるプロジェクトであり、上流プロジェクトやゲームの権利者との提携・承認関係はありません。

## プラグイン統合版を使う

対応する GakuLoader とプラグインパッケージを使用してください。ソースからの作成は[ビルド手順](BUILDING.md)を参照してください。公開版には Loader SDK ヘッダーが含まれ、開発版は既定で隣接する GakuLoader SDK を使います。コンパイルに非公開の Loader 実装は必要ありません。

インストール前にゲームを終了し、対応する一式のパッケージを使用してください。`GakuVRify.dll` は `GakuLoader/plugins/`、`openxr_loader.dll` はゲーム実行ファイルの隣に配置します。新規設定は `GakuLoader/Configs/gaku.vrify/`、状態と診断ログは `GakuLoader/PluginData/gaku.vrify/` と `GakuLoader/Logs/gaku.vrify/` に保存します。[新規インストールガイド](docs/FRESH_INSTALL_GAKULOADER.md)を参照してください。

既存の VR 設定は元の場所で使用できます。変更前にバックアップしてください。[過去の移行資料](docs/CONFIG_MIGRATION_V2.md)と現行のネイティブ移行ツールは旧 `gakumas-local` ルートを対象としており、新しい `GakuLoader/` のインストーラーとして使用できません。

汎用の `package-plugin.ps1` と `install-plugin.ps1` も旧 `gakumas-local` 配置を使用します。新規導入には対応する一式のパッケージを使ってください。汎用インストーラーは対応する外部 provider を確認できないため、完全な Unity VR と Localify/Motion の混在も拒否します。

[上流の Releases](https://github.com/KagaminTheMirror/gakumas-VRify/releases) は**単独プロキシ版**の配布です。このブランチのプラグイン統合版とは別の導入手順です。同じインストール先で単独プロキシと GakuLoader のプロキシを混在させないでください。

## 互換性と既知の制限

- 過去のヘッドセット試験は Quest 3 + Virtual Desktop を対象としています。他の機器や全シーンの動作を保証する結果ではありません。
- 対応する一組のビルドで正常なヘッドセット映像と Motion の一回の再生が確認されています。範囲は[カメラ共有の記録](docs/CAMERA_COEXIST.md)を参照してください。
- **既知の問題：**VR なしでの Motion カメラの手動移動・回転は確認済みですが、VR が有効な状態では Motion のカメラ制御が働きません。begin-camera 共有ブリッジだけでは残るスケジューリング問題は解消されません。[詳細](docs/MOTION_CAMERA_COEXIST.md)をご確認ください。
- ソースビルド、合成テスト、ディスク上のファイル検証、ゲーム・ヘッドセットでの確認は別々に記録します。過去の報告が新しくビルドした DLL を認証するわけではありません。ホットアンロードは非対応です。

## Quest コントローラーガイド

![Quest コントローラーガイド（日本語）](docs/controller-guide/exports/gakumas-vrify-quest-guide-ja.png)

## 開発とライセンス

公開テストは合成フィクスチャと mock host を使用します。製品ホスト、Localify、Motion の実装を用いる任意の統合テストには、別途対応するソースが必要です。[ビルド手順](BUILDING.md)を参照してください。

本プロジェクトは [GPL-3.0](LICENSE) で配布します。Loader SDK は MIT ライセンスを保持します。他のコンポーネントとコントローラー画像には各自の条件が適用されます。[第三者通知](THIRD_PARTY_NOTICES.md)を参照してください。ゲームアセットやゲーム本体は含みません。
