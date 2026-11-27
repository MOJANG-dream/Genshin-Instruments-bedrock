# Changelog

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
