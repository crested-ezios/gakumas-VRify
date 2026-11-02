**English** | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md) | [日本語](README.ja.md)

# gakumas-VRify

An unofficial VR mod for **Gakuen IDOLM@STER, Windows x64 DMM edition**, based on [gkms-localify-dmm](https://git.chinosk6.cn/chinosk/gkms-localify-dmm).

This branch contains the current **GakuLoader plugin integration**, built as `GakuVRify.dll`. It includes the Localify camera/render bridge and the Motion begin-camera bridge. Source builds use Plugin mode by default; the original standalone proxy remains available explicitly.

[Build from source](BUILDING.md) · [Plugin setup and interfaces](docs/GAKULOADER_PLUGIN.md) · [Camera compatibility](docs/MOTION_CAMERA_COEXIST_20261027.md) · [Source ownership](UPSTREAM.md)

## Before you play

- The game was not designed for VR. Characters may disappear or appear in the wrong place, and the image may flicker.
- **VR motion and flashing images can cause discomfort. Stop if you feel unwell.**
- Review the applicable rules, including the [user agreement (Japanese)](https://legal.bandainamcoent.co.jp/terms/nejp), before using a local game modification. You are responsible for your account and use of the mod.
- This is a fan project, unaffiliated with the upstream project or the game's rights holders.

## Use the plugin integration

Use a matching GakuLoader installation and plugin package. For a source build, follow [BUILDING.md](BUILDING.md); the public Loader SDK header is included, and no private Loader implementation is needed to compile the plugin.

Close the game before installing. The plugin package places `GakuVRify.dll` in `gakumas-local/plugins/`, the OpenXR loader beside the game executable, and VR settings under `gakumas-vr/`. GakuLoader is installed separately. See the [plugin guide](docs/GAKULOADER_PLUGIN.md) for package contents, configuration preservation and recovery.

The generic installer conservatively rejects full Unity VR alongside Localify/Motion because it cannot verify a matching external provider pair. The shared bridges are implemented; this guard is not an automatic paired-component upgrade mechanism.

The [upstream Releases](https://github.com/KagaminTheMirror/gakumas-VRify/releases) distribute the **standalone proxy**. They are a separate workflow and do not establish that a package contains this branch's plugin integration. Do not combine the standalone proxy and GakuLoader's proxy in one installation.

## Compatibility and known limitations

- Historical headset testing covered Quest 3 with Virtual Desktop. Other devices and all scenes are not certified by that result.
- A paired build produced a normal headset picture and completed one Motion playback; see the dated [camera/render report](docs/CAMERA_COEXIST_20261027.md).
- **Known issue:** Motion manual camera movement/rotation was accepted without VR; Motion camera control becomes inactive with VR active. The shared begin-camera bridge does not resolve this remaining scheduling issue. See [the exact scope](docs/MOTION_CAMERA_COEXIST_20261027.md).
- Source builds, synthetic checks, installed file hashes and game/headset acceptance are separate evidence. Historical reports do not certify a newly compiled DLL. Hot unload is unsupported.

## Quest controller guide

![Quest controller guide in English](docs/controller-guide/exports/gakumas-vrify-quest-guide-en.png)

## Development and license

The public checks use synthetic fixtures and a mock host. Optional production-host/Localify/Motion integration checks require separately supplied source trees; see [BUILDING.md](BUILDING.md).

The project is distributed under [GPL-3.0](LICENSE). The vendored Loader SDK retains its [MIT license](sdk/gakuloader/LICENSE). Other components and controller illustrations retain their own terms; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Game assets and game installations are not included.
