# 一、腕宝开源链接
- [小智 AI 聊天机器人百科全书](https://ccnphfhqs21z.feishu.cn/wiki/F5krwD16viZoF0kKkvDcrZNYnhb)
- [OSHWHub | 腕宝项目页](https://oshwhub.com/dotnfc/esp32-s3-wrist-gem-xiaoszhi-ai)
- [B站 | 腕宝功能说明](https://www.bilibili.com/video/BV1hqd2Y8E3p)


# 二、编译配置命令

**配置编译目标为 ESP32S3：**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> 腕宝助手
```

**编译：**

```bash
idf.py build
```

**合并固件：**
```bash
idf.py merge-bin -o merged-xiaozhi-wg.bin -f raw
```

---
