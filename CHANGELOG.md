# Changelog

## 0.1.4

声音与贴图：资源包现在真的会被游戏加载。

- 之前只把包注册进包仓库（`addCustomResourcePackPath`），但没有触发重扫，也没有让游戏认为它该被启用，
  所以游戏里既没有物品图标（紫黑格），`playUI` 请求的音效事件也不存在 —— 按键被吞、没有声。
  现在启动时把 `packs/RP` 与 `packs/BP` 复制到游戏自己的开发包目录（`development_resource_packs` /
  `development_behavior_packs`）并调用 `refreshPacks()`，开发包对所有世界默认启用。
- 发声改成走服务端 `PlaySoundPacket`，并且**默认也发回给演奏者本人**（`relayToSelf`），
  这条是原版通路，客户端一定会播；本地 `playUI` 保留为可选（`playLocally`，默认关，避免双声）。
- 界面按 Java 版补充：每个键显示音名（三列七行是 C..B，Djem Djem Drum 用它的 `NOTE_LAYOUT`），
  非打击乐器的网格后面叠了五线谱横线。

## 0.1.3

按 Java 版对齐不同乐器的琴键布局，Djem Djem Drum 不再是三列七行。

- Djem Djem Drum 改成 Java 版的两行四键：上排 `A S D F`、下排 `Q W E R`，八个格子一一对应它的八个采样，
  不再像三列七行那样把采样循环升八度（之前日志里 `pitch 2.000` 就是这个问题）。
- 网格尺寸改为按乐器定义（`InstrumentDef::rows/cols/rowMajor`），面板宽高随之按乐器计算，
  以后要加夜风号的两列布局、或者恢复鼓面专用界面都有地方落。
- 服务端与网络包对 `cell` 的校验改成按乐器格子数判断。

## 0.1.2

实机反馈「没有界面、没有声音」，这一版先加诊断日志定位，不改变功能。

- 客户端：记录每次按键的 keyCode、打开界面时每道门禁的结果（当前屏幕名、`createCommonDialogInfoScreen` 是否返回空）、
  `UIScene::$render` 钩子是否命中我们的场景、`BeforeUIRenderEvent` 是否与场景视图匹配、面板是否真的开始绘制。
- 发声：记录 `playUI` 请求的事件名、音量、音高与返回的 handle（0 表示引擎没有起播）。
- 服务端：记录握手（`hello`）、物品右键（是否因为客户端未握手而被跳过）、以及下发的打开/音符请求。
- 日志全部带 `[gi]` 前缀，过滤 `genshin-instruments` 即可。

## 0.1.1

修复 LeviLauncher 导入时报「manifest.json 无效或缺少必要字段」。

- 发布包的 zip 里，模组自己的 `manifest.json` 现在写在最前面。此前 `zip -r` 把
  `packs/RP/genshin_instruments/manifest.json` 排在前面，启动器会把资源包清单当成模组清单读取。
- 构建产物里的模组版本不再是 `0.0.0`：release 用 tag 去掉 `v` 作为 `MOD_VERSION`。

## 0.1.0

首个可运行的 Bedrock / LeviLamina 移植版。

### 新增

- 7 种乐器：风物之诗琴、镜花之琴、老旧的诗琴、荒泷盛世豪鼓、晚风圆号、悠可琴、金贝鼓。
- 原生 2D 乐器界面：3 列 × 7 行网格，底部为 C，向上到 B；`Z X C V B N M` / `A S D F G H J` /
  `Q W E R T Y U` 三列键位，与 Java 版 `keyMap` 一致。
- 鼠标点按与按住拖动滑奏（glissando），`Shift` / `Alt` 实时升降半音，`Tab` 切换乐器，`Esc` 关闭。
- 本机零延迟播放（`SoundEngine::playUI`）；`relayToNearby` 打开时由服务端用原版
  `PlaySoundPacket` 把音符转给半径内的其他玩家，原版客户端也能听到。
- 资源包（193 段采样 + 物品图标）在构建时从上游 Java 仓库生成并注入客户端资源栈；
  行为包只负责注册 7 个物品，`V` / `B` 键路径完全不需要行为包。
- `config/config.json` 可改开键、切换键、默认乐器、音量、同步半径、移调等。

### 已知限制

- 未移植：Nightwind Horn 长按持续音、鼓类专用界面、MIDI 输入、多人共享同一界面、
  `ACCURATE_NOTES` 音准修正、第三人称手持模型。
- 音频与图标为上游 GPL-3.0 素材，构建时下载，不随仓库分发。
