# MCP ベースのチャットボット

（日本語 | [中文](README_zh.md) | [English](README.md)）

## はじめに

👉 [人間：AIにカメラを装着 vs AI：その場で飼い主が3日間髪を洗っていないことを発見【bilibili】](https://www.bilibili.com/video/BV1bpjgzKEhd/)

👉 [手作りでAIガールフレンドを作る、初心者入門チュートリアル【bilibili】](https://www.bilibili.com/video/BV1XnmFYLEJN/)

シャオジーAIチャットボットは音声インタラクションの入口として、Qwen / DeepSeekなどの大規模モデルのAI能力を活用し、MCPプロトコルを通じてマルチエンド制御を実現します。

<img src="docs/mcp-based-graph.jpg" alt="MCPであらゆるものを制御" width="320">

## 最近の更新

- メインラインはESP-IDF v6.0以降へ移行し、推奨安定版はv6.0.2です。全157リリースバリアントはESP-IDF v6.0.1でビルド検証済みです。
- MQTTとBluFiの暗号処理をPSA Cryptoへ移行し、IDF 6のコンポーネント分割およびサードパーティ依存関係にも対応しました。
- オーディオパイプラインの並行処理、MQTT/UDPパケット検証、リリースマトリクス選択処理を強化しました。
- ESP-IDF v5.5は、ESP32-P4 v3より前のシリコンを含む旧ハードウェア互換用途にのみ残しています。詳細な互換性とボード検証状況は、[ESP-IDF 6.0移行ガイド](docs/esp-idf-6-migration.md)を参照してください。

### 実装済み機能

- Wi-Fi、有線Ethernet、USB RNDIS、およびML307/EC801EまたはNT26 Cat.1 4Gに対応し、一部のボードではWi-Fiと4Gを切り替え可能
- [ESP-SR](https://github.com/espressif/esp-sr)によるオフライン音声ウェイクアップとカスタムウェイクワード
- 2種類の通信方式：[WebSocket](docs/websocket.md)と[MQTT + UDP](docs/mqtt-udp.md)
- Opusオーディオストリーミングにより、従来のストリーミングASR + LLM + TTS構成とRealtimeエンドツーエンド音声モデルの両方に対応。AEC対応ハードウェアではリアルタイム全二重対話が可能
- 話者認識、現在話している人を識別 [3D Speaker](https://github.com/modelscope/3D-Speaker)
- OLED / LCDディスプレイで絵文字や豊かな感情表現を表示し、一部のボードではカメラによる視覚入力にも対応
- バッテリー表示と電源管理
- 38言語の画面表示に対応し、音声プロンプトはローカライズ済みリソースを優先して、未収録時は英語へフォールバック
- ESP32、ESP32-C3、ESP32-C5、ESP32-C6、ESP32-S3、ESP32-P4チッププラットフォーム
- ホットスポット、音響信号、BluFiによるWi-Fiプロビジョニング
- デバイス側MCPによるデバイス制御（音量・明るさ調整、アクション制御など）
- クラウド側MCPで大規模モデル能力を拡張（スマートホーム制御、PCデスクトップ操作、知識検索、メール送受信など）
- カスタマイズ可能なウェイクワード、フォント、絵文字、チャット背景、オンラインWeb編集に対応 ([カスタムアセットジェネレーター](https://github.com/78/xiaozhi-assets-generator))

## ハードウェア

### ブレッドボード手作り実践

Feishuドキュメントチュートリアルをご覧ください：

👉 [「シャオジーAIチャットボット百科事典」](https://ccnphfhqs21z.feishu.cn/wiki/F5krwD16viZoF0kKkvDcrZNYnhb?from=from_copylink)

ブレッドボードのデモ：

![ブレッドボードデモ](docs/v1/wiring2.jpg)

### 137のボードディレクトリと157のリリースバリアントに対応（一部のみ表示）

- <a href="https://oshwhub.com/li-chuang-kai-fa-ban/li-chuang-shi-zhan-pai-esp32-s3-kai-fa-ban" target="_blank" title="立創・実戦派 ESP32-S3 開発ボード">立創・実戦派 ESP32-S3 開発ボード</a>
- <a href="https://github.com/espressif/esp-box" target="_blank" title="楽鑫 ESP32-S3-BOX3">楽鑫 ESP32-S3-BOX3</a>
- <a href="https://docs.m5stack.com/zh_CN/core/CoreS3" target="_blank" title="M5Stack CoreS3">M5Stack CoreS3</a>
- <a href="https://docs.m5stack.com/en/atom/Atomic%20Echo%20Base" target="_blank" title="AtomS3R + Echo Base">M5Stack AtomS3R + Echo Base</a>
- <a href="https://gf.bilibili.com/item/detail/1108782064" target="_blank" title="マジックボタン2.4">マジックボタン2.4</a>
- <a href="https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm" target="_blank" title="微雪電子 ESP32-S3-Touch-AMOLED-1.8">微雪電子 ESP32-S3-Touch-AMOLED-1.8</a>
- <a href="https://github.com/Xinyuan-LilyGO/T-Circle-S3" target="_blank" title="LILYGO T-Circle-S3">LILYGO T-Circle-S3</a>
- <a href="https://oshwhub.com/tenclass01/xmini_c3" target="_blank" title="エビ兄さん Mini C3">エビ兄さん Mini C3</a>
- <a href="https://oshwhub.com/movecall/cuican-ai-pendant-lights-up-y" target="_blank" title="Movecall CuiCan ESP32S3">CuiCan AIペンダント</a>
- <a href="https://github.com/WMnologo/xingzhi-ai" target="_blank" title="無名科技Nologo-星智-1.54">無名科技Nologo-星智-1.54TFT</a>
- <a href="https://www.seeedstudio.com/SenseCAP-Watcher-W1-A-p-5979.html" target="_blank" title="SenseCAP Watcher">SenseCAP Watcher</a>
- <a href="https://www.bilibili.com/video/BV1BHJtz6E2S/" target="_blank" title="ESP-HI 超低コストロボット犬">ESP-HI 超低コストロボット犬</a>

<div style="display: flex; justify-content: space-between;">
  <a href="docs/v1/lichuang-s3.jpg" target="_blank" title="立創・実戦派 ESP32-S3 開発ボード">
    <img src="docs/v1/lichuang-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/espbox3.jpg" target="_blank" title="楽鑫 ESP32-S3-BOX3">
    <img src="docs/v1/espbox3.jpg" width="240" />
  </a>
  <a href="docs/v1/m5cores3.jpg" target="_blank" title="M5Stack CoreS3">
    <img src="docs/v1/m5cores3.jpg" width="240" />
  </a>
  <a href="docs/v1/atoms3r.jpg" target="_blank" title="AtomS3R + Echo Base">
    <img src="docs/v1/atoms3r.jpg" width="240" />
  </a>
  <a href="docs/v1/magiclick.jpg" target="_blank" title="マジックボタン2.4">
    <img src="docs/v1/magiclick.jpg" width="240" />
  </a>
  <a href="docs/v1/waveshare.jpg" target="_blank" title="微雪電子 ESP32-S3-Touch-AMOLED-1.8">
    <img src="docs/v1/waveshare.jpg" width="240" />
  </a>
  <a href="docs/v1/lilygo-t-circle-s3.jpg" target="_blank" title="LILYGO T-Circle-S3">
    <img src="docs/v1/lilygo-t-circle-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/xmini-c3.jpg" target="_blank" title="エビ兄さん Mini C3">
    <img src="docs/v1/xmini-c3.jpg" width="240" />
  </a>
  <a href="docs/v1/movecall-cuican-esp32s3.jpg" target="_blank" title="CuiCan">
    <img src="docs/v1/movecall-cuican-esp32s3.jpg" width="240" />
  </a>
  <a href="docs/v1/wmnologo_xingzhi_1.54.jpg" target="_blank" title="無名科技Nologo-星智-1.54">
    <img src="docs/v1/wmnologo_xingzhi_1.54.jpg" width="240" />
  </a>
  <a href="docs/v1/sensecap_watcher.jpg" target="_blank" title="SenseCAP Watcher">
    <img src="docs/v1/sensecap_watcher.jpg" width="240" />
  </a>
  <a href="docs/v1/esp-hi.jpg" target="_blank" title="ESP-HI 超低コストロボット犬">
    <img src="docs/v1/esp-hi.jpg" width="240" />
  </a>
</div>

## ソフトウェア

### ファームウェア書き込み

初心者の方は、まず開発環境を構築せずに書き込み可能なファームウェアを使用することをおすすめします。

ファームウェアはデフォルトで公式 [xiaozhi.me](https://xiaozhi.me) サーバーに接続します。個人ユーザーはアカウント登録でQwenリアルタイムモデルを無料で利用できます。

👉 [初心者向けファームウェア書き込みガイド](https://ccnphfhqs21z.feishu.cn/wiki/Zpz4wXBtdimBrLk25WdcXzxcnNS)

### 開発環境

- Cursor または VSCode
- ESP-IDFプラグインをインストールし、[ESP-IDF v6.0.2](https://github.com/espressif/esp-idf/releases/tag/v6.0.2)を優先して使用してください。v6.0以降の安定版を推奨し、ESP-IDF v5.5.2は旧ハードウェアとの互換性維持にのみ使用します
- LinuxはWindowsよりも優れており、コンパイルが速く、ドライバの問題も少ない
- 本プロジェクトはGoogle C++コードスタイルを採用、コード提出時は準拠を確認してください

### 開発者ドキュメント

- [ESP-IDF 6.0移行ガイド](docs/esp-idf-6-migration.md) - SDK互換性、コンポーネント変更、旧ハードウェア対応、ボード検証状況
- [カスタム開発ボードガイド](docs/custom-board.md) - シャオジーAI用のカスタム開発ボード作成方法
- [MCPプロトコルIoT制御使用法](docs/mcp-usage.md) - MCPプロトコルでIoTデバイスを制御する方法
- [MCPプロトコルインタラクションフロー](docs/mcp-protocol.md) - デバイス側MCPプロトコルの実装方法
- [MQTT + UDP ハイブリッド通信プロトコルドキュメント](docs/mqtt-udp.md)
- [詳細なWebSocket通信プロトコルドキュメント](docs/websocket.md)

## 大規模モデル設定

すでにシャオジーAIチャットボットデバイスをお持ちで、公式サーバーに接続済みの場合は、[xiaozhi.me](https://xiaozhi.me) コンソールで設定できます。

👉 [バックエンド操作ビデオチュートリアル（旧インターフェース）](https://www.bilibili.com/video/BV1jUCUY2EKM/)

## 関連オープンソースプロジェクト

個人PCでサーバーをデプロイする場合は、以下のオープンソースプロジェクトを参照してください：

- [xinnan-tech/xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server) Pythonサーバー
- [joey-zhou/xiaozhi-esp32-server-java](https://github.com/joey-zhou/xiaozhi-esp32-server-java) Javaサーバー
- [AnimeAIChat/xiaozhi-server-go](https://github.com/AnimeAIChat/xiaozhi-server-go) Golangサーバー
- [hackers365/xiaozhi-esp32-server-golang](https://github.com/hackers365/xiaozhi-esp32-server-golang) Golangサーバー

シャオジー通信プロトコルを利用した他のクライアントプロジェクト：

- [huangjunsen0406/py-xiaozhi](https://github.com/huangjunsen0406/py-xiaozhi) Pythonクライアント
- [TOM88812/xiaozhi-android-client](https://github.com/TOM88812/xiaozhi-android-client) Androidクライアント
- [100askTeam/xiaozhi-linux](http://github.com/100askTeam/xiaozhi-linux) 百問科技提供のLinuxクライアント
- [78/xiaozhi-sf32](https://github.com/78/xiaozhi-sf32) 思澈科技のBluetoothチップファームウェア
- [QuecPython/solution-xiaozhiAI](https://github.com/QuecPython/solution-xiaozhiAI) 移遠提供のQuecPythonファームウェア

## プロジェクトについて

これはエビ兄さんがオープンソースで公開しているESP32プロジェクトで、MITライセンスのもと、誰でも無料で、商用利用も可能です。

このプロジェクトを通じて、AIハードウェア開発を理解し、急速に進化する大規模言語モデルを実際のハードウェアデバイスに応用できるようになることを目指しています。

ご意見やご提案があれば、いつでもIssueを提出するか、[Discord](https://discord.gg/C759fGMBcZ) または QQグループ：1011329060 にご参加ください。

## スター履歴

<a href="https://star-history.com/#78/xiaozhi-esp32&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date" />
 </picture>
</a> 
