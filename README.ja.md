[English](README.md) | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md) | **日本語**

# gakumas-VRify

『学園アイドルマスター』に VR 対応を追加する非公式 Mod です。**Windows x64 の DMM 版に対応**し、[gkms-localify-dmm](https://git.chinosk6.cn/chinosk/gkms-localify-dmm) をベースにしています。

このブランチは、現在の **GakuLoader プラグイン統合版 `GakuVRify.dll`** です。Localify の camera/render と Motion の begin-camera 共有ブリッジを実装しています。ソースビルドの既定は Plugin モードで、従来の単独プロキシ版は明示的に選択できます。

[ソースビルド](BUILDING.md) · [プラグイン設定とインターフェース](docs/GAKULOADER_PLUGIN.md) · [カメラの互換性](docs/MOTION_CAMERA_COEXIST_20261027.md) · [ソースの管理](UPSTREAM.md)

## ご利用前に

- ゲーム自体は VR 向けではないため、キャラクターの欠落、位置ずれ、画面のちらつきが発生する場合があります。
- **VR 酔いや点滅にご注意ください。体調に異変を感じた場合は使用を中止してください。**
- ゲームのローカル改変を利用する前に、[利用規約](https://legal.bandainamcoent.co.jp/terms/nejp)などの関連規則をご確認ください。アカウントおよび利用の結果は、ご自身の責任となります。
- ファンによるプロジェクトであり、上流プロジェクトやゲームの権利者との提携・承認関係はありません。

## プラグイン統合版を使う

対応する GakuLoader とプラグインパッケージを使用してください。ソースからの作成は[ビルド手順](BUILDING.md)を参照してください。公開 Loader SDK ヘッダーを同梱しており、プラグインのコンパイルに非公開の Loader 実装は必要ありません。

インストール前にゲームを終了してください。パッケージは `GakuVRify.dll` を `gakumas-local/plugins/` に、OpenXR loader をゲーム実行ファイルの隣に配置します。新規設定は `gakumas-local/Configs/gaku.vrify/`、状態と診断ログは `PluginData/gaku.vrify/` と `Logs/gaku.vrify/` を使用します。GakuLoader は別途インストールします。[プラグインガイド](docs/GAKULOADER_PLUGIN.md)もご覧ください。

既存設定は、明示的な[計画・バックアップ・移行](docs/CONFIG_MIGRATION_V2_20261102.md)まで元の場所で使用できます。同梱のネイティブツールは古い設定・移行記録・ログを保持します。パッケージには設定例のみを含め、使用中の設定は上書きしません。

汎用インストーラーは外部の Localify/Motion provider の組み合わせを検証できないため、完全な Unity VR との混在を引き続き拒否します。共有ブリッジは実装済みですが、インストーラーが対応する全コンポーネントを自動更新・検証するわけではありません。

[上流の Releases](https://github.com/KagaminTheMirror/gakumas-VRify/releases) は**単独プロキシ版**の配布です。このブランチのプラグイン統合版とは別の導入手順です。同じインストール先で単独プロキシと GakuLoader のプロキシを混在させないでください。

## 互換性と既知の制限

- 過去のヘッドセット試験は Quest 3 + Virtual Desktop を対象としています。他の機器や全シーンの動作を保証する結果ではありません。
- 対応する一組のビルドで正常なヘッドセット映像と Motion の一回の再生が確認されています。範囲は[カメラ共有の記録](docs/CAMERA_COEXIST_20261027.md)を参照してください。
- **既知の問題：**VR なしでの Motion カメラの手動移動・回転は確認済みですが、VR が有効な状態では Motion のカメラ制御が働きません。begin-camera 共有ブリッジだけでは残るスケジューリング問題は解消されません。[詳細](docs/MOTION_CAMERA_COEXIST_20261027.md)をご確認ください。
- ソースビルド、合成テスト、ディスク上のファイル検証、ゲーム・ヘッドセットでの確認は別々に記録します。過去の報告が新しくビルドした DLL を認証するわけではありません。ホットアンロードは非対応です。

## Quest コントローラーガイド

![Quest コントローラーガイド（日本語）](docs/controller-guide/exports/gakumas-vrify-quest-guide-ja.png)

## 開発とライセンス

公開テストは合成フィクスチャと mock host を使用します。製品ホスト、Localify、Motion の実装を用いる任意の統合テストには、別途対応するソースが必要です。[ビルド手順](BUILDING.md)を参照してください。

本プロジェクトは [GPL-3.0](LICENSE) で配布します。同梱 Loader SDK は [MIT ライセンス](sdk/gakuloader/LICENSE)を保持します。他のコンポーネントとコントローラー画像には各自の条件が適用されます。[第三者通知](THIRD_PARTY_NOTICES.md)を参照してください。ゲームアセットやゲーム本体は含みません。
