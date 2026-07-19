
⏺ 不支持 mTLS。三种底层实现都只做到「单向 TLS 或更弱」,没有任何一处配置客户端证书 + 私钥。

  结论与证据

  1. 抽象接口层就堵死了 mTLS 的入口

  managed_components/78__esp-ml307/include/mqtt.h:20:

  virtual bool Connect(broker_address, broker_port, client_id, username, password) = 0;

  Connect 签名只有 client_id / username / password,根本没有传 client cert + private key 的参数。上层
  MqttProtocol::StartMqttClient(mqtt_protocol.cc:67-73)也只从 NVS 读 endpoint / client_id / username / password。设备身份靠的是 MQTT 应用层 username/password
  认证,跑在 TLS 加密信道之上,而不是 mTLS 的证书互认。

  2. 三个底层实现的 TLS 强度(都以 port == 8883 作为开 SSL 的开关)

  ┌──────────────────────┬─────────────────┬──────────────────────────────────────────────────────────────────────┬───────────────────────┐
  │         实现         │    板子类型     │                            8883 端口行为                             │         强度          │
  ├──────────────────────┼─────────────────┼──────────────────────────────────────────────────────────────────────┼───────────────────────┤
  │ esp_mqtt.cc:27-29    │ WiFi (esp-mqtt) │ MQTT_TRANSPORT_OVER_SSL + crt_bundle_attach(内置 CA bundle 验服务端) │ 单向 TLS              │
  ├──────────────────────┼─────────────────┼──────────────────────────────────────────────────────────────────────┼───────────────────────┤
  │ ml307_mqtt.cc:83     │ ML307 模组      │ AT+MQTTCFG="ssl",id,1 仅开 SSL,无 CA/client cert                     │ 单向 TLS(甚至未配 CA) │
  ├──────────────────────┼─────────────────┼──────────────────────────────────────────────────────────────────────┼───────────────────────┤
  │ ec801e_mqtt.cc:87-88 │ EC801E 模组     │ AT+QSSLCFG="seclevel",2,0 → seclevel=0 不校验证书                    │ 纯加密、无认证(最弱)  │
  └──────────────────────┴─────────────────┴──────────────────────────────────────────────────────────────────────┴───────────────────────┘

  esp_mqtt.cc 里只设了 broker.verification.crt_bundle_attach(客户端验服务端),完全没有 credentials.authentication.certificate / .key(服务端验客户端)——这正是
  mTLS 缺失的那一半。

  如果要支持 mTLS,需要改三层

  1. 接口层:给 Mqtt 加 client cert / key(及自定义 CA)的 setter,或扩展 Connect。
  2. 各实现填入:
    - esp_mqtt → credentials.authentication.certificate + .key,以及可选 verification.certificate(自定义 CA)。
    - ml307 → SSL context 里导入 client cert/key 后再 AT+MQTTCFG="ssl"。
    - ec801e → AT+QSSLCFG="cacert"/"clientcert"/"clientkey",并把 seclevel 从 0 改成 2(双向认证)。
  3. 上层:从烧录的证书分区 / NVS 读出设备证书喂进去。

  简单说:现状是「TLS 加密 + username/password 认证」,要做成 mTLS 需要从接口签名往下整条链路改造。