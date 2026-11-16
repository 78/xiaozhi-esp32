# 历史设计依据

原始资料保留在 `D:\work\xiaozhi-realtime` 的 develop 工作目录。参考提交为 `b3adbe9d1526b3bead436b39492eb63d616fc0e8`，可用 `git show <提交>:<路径>` 查阅，不依赖新 worktree 中是否复制了旧 design 目录。

## 音频结论

来源：`design/waveshare-3.5b-realtime-audio-fix-2026-11-15.md`。Waveshare 3.5B 当前声学条件下，AEC 双讲时抑制近端语音、损失句首和轻声。V1 停用设备 AEC，保留 NSNet2，限定 Manual / Auto。Auto 回复期间停止上行，本地播放排空后恢复，不支持语音插话。

## 调试代理

来源：`design/go-realtime-debug-service.md`。沿用独立 Go WebSocket 代理与诊断 TCP 通道；转发不等待录制处理，保留双向应用消息顺序，鉴权只在内存中转发。V1 增加真实请求标识、显式会话关联和 ES8311 原始输入。历史 AEC 实验不作为 V1 产品能力。当前实现以 `tools/realtime-debug/README.md` 与实施记录为准。
