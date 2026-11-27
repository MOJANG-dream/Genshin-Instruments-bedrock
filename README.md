# Genshin Instruments · Bedrock

[原神乐器](https://github.com/StavWasPlayZ/Genshin-Instruments) 的 Bedrock 移植版，
以原生模组形式运行在 [LeviLamina](https://levimc.org/LeviLamina/) 上。

支持 7 种乐器、21 键三列网格、按半音移调、鼠标滑奏，以及多人共享演奏。
声音走服务端 `PlaySoundPacket` 与原版 `sound_definitions.json`，客户端不需要额外的自定义包。

## 乐器

| id | 名称 | 采样 |
| --- | --- | --- |
| `windsong_lyre` | Windsong Lyre 风物之诗琴 | 21 |
| `floral_zither` | Floral Zither 镜花之琴 | 21 |
| `vintage_lyre` | Vintage Lyre 老旧的诗琴 | 21 |
| `glorious_drum` | Arataki's Great and Glorious Drum 荒泷·盛世豪鼓 | don / ka |
| `nightwind_horn` | Nightwind Horn 晚风圆号 | 14 |
| `ukulele` | Ukulele 悠可琴 | 21 |
| `djem_djem_drum` | Djem Djem Drum 金贝鼓 | 8 |

## 操作

- `V` 打开默认乐器界面，`B` 循环切换乐器（可在 `config.json` 改键）
- 手持乐器物品右键同样会打开界面（需要行为包在世界里启用）
- 网格键位：
  - 第一列 `Z X C V B N M`
  - 第二列 `A S D F G H J`
  - 第三列 `Q W E R T Y U`
- `Shift` / `Alt` 实时升降八度，`Tab` 切换乐器，`Esc` 关闭
- 鼠标左键点按琴键，按住拖动可滑奏

## 构建

推送即触发 [`.github/workflows/build.yml`](.github/workflows/build.yml)：

- 先 `git clone --depth 1` 上游 Java 仓库（音频采样与图标是 GPL-3.0 素材，不入库），
  再由 `tools/build_packs.py` 生成资源包 / 行为包；
- `windows-latest` + xmake + LLVM 构建 `client` 与 `server` 两个目标；
- 产物在 Actions 运行页的 **Artifacts** 里，解压后直接把 `genshin-instruments/`
  丢进 `mods\`（客户端）或 `plugins\`（服务端）。

本地构建需要 VS2022、Windows SDK、xmake 与 LLVM（clang-cl）：

```powershell
git clone --depth 1 https://github.com/StavWasPlayZ/Genshin-Instruments.git upstream
python tools\build_packs.py upstream
xmake f -c -y --target_type=client
xmake -y
```

产物在 `bin\genshin-instruments\`：`genshin-instruments.dll`、`manifest.json`、
`packs\RP\genshin_instruments\`、`packs\BP\genshin_instruments\`。

## 安装

1. 解压产物到 `mods\genshin-instruments\`（服务端为 `plugins\genshin-instruments\`）
2. 资源包会自动压进客户端资源包栈；行为包需要在世界的「行为包」里启用，
   物品才会出现（不改世界设置也可以，用 `V` 键开界面）
3. `lip install github.com/MOJANG-dream/Genshin-Instruments-bedrock` 也可以，
   前提是对应版本已经发布 release

> 资源包 / 行为包的 uuid + version 一旦改动内容就要同步 bump
> （`src/pack/ResourcePacks.cpp` 与两个 `manifest.json`），否则客户端会用缓存里的旧包。

## 配置

`config/config.json`，首次启动自动生成：

| 键 | 默认 | 说明 |
| --- | --- | --- |
| `openKey` | `86` (`V`) | 打开乐器界面的按键码 |
| `cycleKey` | `66` (`B`) | 循环切换乐器的按键码 |
| `defaultInstrument` | `windsong_lyre` | `V` 打开的乐器 |
| `playLocally` | `true` | 本机播放（零延迟） |
| `relayToNearby` | `true` | 同步给附近玩家 |
| `relayRadius` | `32` | 同步半径（格） |
| `useStereo` | `false` | 有立体声采样时用立体声 |
| `showKeyLabels` | `true` | 琴键上显示键位 |
| `showBackground` | `true` | 绘制背景板 |
| `transposeOctaves` | `0` | 全局移调（-2..2） |
| `enableDebugLog` | `false` | 调试日志 |

## 网络协议

| 包 | 方向 | 作用 |
| --- | --- | --- |
| `HelloPacket` | C→S | 客户端声明支持自定义包 |
| `OpenInstrumentPacket` | S→C | 打开界面（含乐器 id 与移调） |
| `PlayNotePacket` | C→S | 上报一次按键 |
| `CloseInstrumentPacket` | C→S | 关闭界面 |

服务端只对握过手的连接发自定义包，原版客户端不会收到无法解析的 runtime packet。

## 未移植

长按持续音（Nightwind Horn hold）、鼓类专用界面、MIDI 输入、共享界面、
`ACCURATE_NOTES` 音准修正、第三人称手持模型。

## 许可

GPL-3.0，与上游保持一致；音频与图标素材来自上游，构建时下载，不随仓库分发。
