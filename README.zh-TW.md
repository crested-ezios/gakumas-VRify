[English](README.md) | [简体中文](README.zh-CN.md) | **繁體中文** | [日本語](README.ja.md)

# gakumas-VRify

為《學園偶像大師》（学園アイドルマスター）加入 VR 支援的非官方 Mod，**適用於 Windows x64 DMM 版**，基於 [gkms-localify-dmm](https://git.chinosk6.cn/chinosk/gkms-localify-dmm)。

此分支包含目前的 **GakuLoader 外掛整合版 `GakuVRify.dll`**，已實作 Localify camera/render 與 Motion begin-camera 共用橋接。原始碼預設建置 Plugin；原獨立代理版保留為明確選項。

[原始碼建置](BUILDING.md) · [外掛安裝與介面](docs/GAKULOADER_PLUGIN.md) · [鏡頭相容範圍](docs/MOTION_CAMERA_COEXIST.md) · [原始碼歸屬](UPSTREAM.md)

## 使用前請閱讀

- 遊戲本身並非為 VR 設計，可能出現角色缺失、位置錯誤及畫面閃爍。
- **請注意暈眩與閃爍風險。如感到不適，請停止使用。**
- 使用本機遊戲修改前，請查閱包含[使用者條款（日文）](https://legal.bandainamcoent.co.jp/terms/nejp)在內的相關規則，並對自己的帳號與使用後果負責。
- 本專案為粉絲自製，與上游專案及遊戲相關方沒有隸屬或背書關係。

## 使用外掛整合版

請使用配套的 GakuLoader 與外掛套件。自行編譯請參閱[建置指南](BUILDING.md)；發布原始碼隨附公開 Loader SDK；開發倉庫預設使用相鄰的 GakuLoader SDK。編譯外掛不需要私有 Loader 實作。

安裝前關閉遊戲，並使用配套的完整安裝包。將 `GakuVRify.dll` 放入 `GakuLoader/plugins/`，`openxr_loader.dll` 放在遊戲執行檔旁。全新設定使用 `GakuLoader/Configs/gaku.vrify/`，狀態和診斷記錄分別使用 `GakuLoader/PluginData/gaku.vrify/`、`GakuLoader/Logs/gaku.vrify/`。詳見[全新安裝指南](docs/FRESH_INSTALL_GAKULOADER.md)。

既有 VR 設定可繼續原位使用；變更布局前請備份。[歷史遷移文件](docs/CONFIG_MIGRATION_V2.md)與目前原生遷移工具仍以舊 `gakumas-local` 為目標，不能用作新 `GakuLoader/` 根目錄的安裝工具。

通用的 `package-plugin.ps1` 和 `install-plugin.ps1` 仍使用舊 `gakumas-local` 套件布局，不適用於新根的全新安裝。通用安裝程式也無法驗證外部 provider 是否配套，因此仍保守拒絕完整 Unity VR 與 Localify/Motion 混裝。

[上游 Releases](https://github.com/KagaminTheMirror/gakumas-VRify/releases) 提供的是**獨立代理版**，屬於另一條安裝流程，不能據此認定包含此分支的外掛整合。請勿在同一安裝中混用獨立代理與 GakuLoader 代理。

## 相容性與已知限制

- 歷史頭戴裝置測試涵蓋 Quest 3 + Virtual Desktop，不代表其他裝置或所有場景已驗收。
- 一組配套建置曾確認頭戴裝置畫面正常，並完成一次 Motion 動作播放，範圍見[相機共用報告](docs/CAMERA_COEXIST.md)。
- **已知問題：**沒有 VR 時的 Motion 手動鏡頭移動、旋轉已確認；啟用 VR 後，Motion 鏡頭控制仍會失效。begin-camera 共用橋接尚未解決剩餘的排程問題，詳見[鏡頭共存紀錄](docs/MOTION_CAMERA_COEXIST.md)。
- 原始碼建置、合成檢查、磁碟檔案校驗與遊戲／頭戴裝置驗收分別記錄。歷史報告不自動認證新編譯的 DLL；不支援熱卸載。

## Quest 控制器操作指南

![Quest 控制器操作指南（繁體中文）](docs/controller-guide/exports/gakumas-vrify-quest-guide-zh-TW.png)

## 開發與授權

公開檢查使用合成測試資料與 mock host。選用的正式宿主、Localify、Motion 原始碼整合測試需要另行提供對應原始碼，見[建置指南](BUILDING.md)。

專案採用 [GPL-3.0](LICENSE)。Loader SDK 保留 MIT 授權；其他元件及控制器插圖保留各自條款，見[第三方通知](THIRD_PARTY_NOTICES.md)。倉庫不包含遊戲資源或遊戲安裝檔案。
