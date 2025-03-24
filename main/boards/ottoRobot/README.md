<p align="center">
  <img width="80%" align="center" src="../../../docs/ottoRobot.png"alt="logo">
</p>
  <h1 align="center">
  ottoRobot
</h1>

## 简介

otto 机器人是一个开源的人形机器人平台，具有多种动作能力和互动功能。本项目基于 ESP32 实现了 otto 机器人的控制系统，并加入小智ai。
- <a href="www.ottodiy.com" target="_blank" title="otto官网">otto官网</a>

## 硬件
- <a href=" https://oshwhub.com/txp666/otto_esp32s3" target="_blank" title="立创开源">立创开源</a>

| 组件        | 购买链接                                      |
|------------|----------------------------------------------|
| 主控板       | [ESP32-S3-WROOM-1U-N16R8](https://item.taobao.com/item.htm?_u=o20q7cgbd495&id=701702373214&pisk=gcV7_CZtb3xS4-GpRYbqlNUeMRGQNZ5N9egLSydyJbh-dBUzf2RELT0BOmaOr0WhEkwbvoDr2DkEO2ZZmzRFrz7COyUt2Xyr42E4Aovz4WoU06agfYRzJW8unlz9a7WoTBGotXINb15akzcn96J1JcKkH2gGT2n-ybcvb0TPB15arrTryNzA_7l5Fs3ypBEKex3xzm3-vQ3Rlj3ryXd-pQpvDmmx94Hpejdx-2xJ2kd8kj3tJ2dJw0HxMVmxvXHJeZaxm2GKvb2D526Sqz_-_Oz8mhZLymOp97CnFc1ZDq0bG1H-bzFWxHYnPYiTymsMjm6jHyDQteAIdzegxVEJ2GkuJ-ZjPX12efUbplg0w1-So8zYxmU6RC4-N2Mt9xTp9rG8nxFr61Kjo-Uzp7qWRB3ua5kIjx_ptvlY_AeTVebgkb3LxAVFjQm8JPVgQf12efUbplwC4hdZf4TXdEMMOqiNlZ9HK9vRfBsRT8Xteq0S3Z_X2JD-oqiNlZt8KY3mPm7fl3eh.&spm=a1z09.2.0.0.18762e8dduCKjA) |
| 舵机        | SG90舵机180°|
| 电池        | [103450/2000毫安](https://detail.tmall.com/item.htm?_u=o20q7cgb7e8f&id=638394813102&pisk=gmVY_nVItgjcuvNxrrWkspWZJd7uHT44zozBscmDCuE8XyL0nAmiCfEUvAOG3jDt6uiuim4mGVM_jkwmc-mM65EzDIAiGlftfyyP7m00iF3_Fodgnrm0eFh4Z-AimilT5kcOxMfhtrzq7fshxh0pU2GIJIg6jctWVf0TEr9DRrzq_cTkf927uhKwDgYs5c_-NV3BlfMj13OS4V0XClijF3gK8fG_fxM5NV0KfIOXGapS5qOXlcOsN0gi5VGs1lG7P0us1qZs1bydX0TjjCeCgin7S1XyTBP-kxn9s0dTjSOnFDtKVCdtDjDvnriJ1CN8E-FBFm6erclm0rgYb6RqGAeUSVEAMIZT87r-Pk19tmaz-JD4G6JZpjirBSGp5sa-MmHQMo8Xika85JD8Zw1EMjibIjzMJi48Mo44wPY6Fjh0evFxO6-jj84L6VFNb_EL87r-Pk1OcglLtWeELHmKSK_RydJZh47H6DdMx_E_P4nhk1pwQY7ryDbRydJZUau-xZQ9QdkPz&spm=a1z09.2.0.0.18762e8dduCKjA) |
| 喇叭        | [2030腔体-4Ω3W](https://item.taobao.com/item.htm?_u=o20q7cgbc715&id=684864879619&pisk=g-vbshYd8r4bbNBTfsmPVDKaeB66G0kEBls9xhe4XtBA1RK2RGyN_jj_CatKmZliiFOWWUfVkN5NCGTFrnyZmnosChKdkORVuGLy5UV2ud7wzAtHRsy2XdrDZe-L3KlcQR6cIO3E8vke2nXGBAPIXwU0wMInQ5BOktX88ZZq9vkem3ZVDD--LKWjG4Ib6RLOM_IR0aIAW-IxV_IGvOeA6-F8yabRBiCTMTeRjG4YkFIT2gIcYRUTHSFRei7RWOQvW3n5baQtZYECfj_DcmBETnLoay-VPRe9Fg3GJnwUQg9PcHQBDKaU8cjfGwKAP24hPb1Jj19gjR16DBYGvEUx8sx9NUsfCYPAp3O9RgvxLrskg3LGyHZ_KnBff6BONle9VtdlwKWKpzskwnR9nEZ_Biv2L1_hNceG_TpF9BLbjDxRHGLGtdu3nZK9xpAFdYPAp3O9RCsPDJ7CcuN_V621VwoSV5VisS3QCqHwiXCAqi1rV0avssIlVwoSVrdGMgjfz0i7kCf..&spm=a1z09.2.0.0.18762e8dduCKjA) |
| 开关        | [5.8*5.8侧按自锁](https://detail.tmall.com/item.htm?_u=o20q7cgbe1aa&id=631520524805&pisk=gUTLs_TfoADntwTdsBogrooPMulMJcAe_pRbrTX3Vdp9eIFHxgXlVap2HgaoYwjRyd6M-9AhR_sWEKthdHXuyUpwpy4lRL0RFI-Za9fH-7CWfpUkxBXHX7QeSH4lKv7JNKb-nx0moBRFaaMmn8fYbs_fgTZ7ETw_fafJjBZ3GBRFzTNgFmx6T8ezpPPCNTGO5_CbOasCPRa1Q_fCFL6CfR1Aga_WFMss5_fAFyaQRfE1N_ECNa1Cf11FNk_5PL6s6_W1Oa_SaUDRCW6uyngEpOOufSzblFCdvTUlBzL2JrjOh8W_yzBdVMEHOOU7PFKMvEZ1dmPGUwXeO6pruy7C2nRlwp3LRLd27dC6h4UcdCRDmM8IuuSAROSJRZFSRCQdpiTpLWaHhCdWmGLiGq7dR9tl8QVxKCLpKB89Z7Z1JwxODeQjuJ6kjILRwUkrWKd27dC6h4HC4uLDknS4ntCuAfhT4uSC_j4RHz2mWLO16tcKyurPbf5OnfhT4uSZ_1Bm9bEz4GlN.&spm=a1z09.2.0.0.18762e8dduCKjA) |
| 舵机排针        | [2.54MM 双排](https://item.taobao.com/item.htm?_u=o20q7cgb0210&id=700748964957&pisk=gzTY_OTC-YD0ER8AZIouSkoN9llkH0A2aKRQSOX0fLpJBBFDmGXGfNpepGai0ZjOWL6kotAcl1s6IptchnXgWFpyHr4Gld0OCB-r_tfDo5C6VKUMmIXD25Q2rn4Gnx791pbtKv0n-IRV_NMnKRfL46_Cts1bIsw5VNf9qIZ0OIRV7ONuCDxWgReanmN11OGRP1CQGNsf58a5U14fCd6fV81dTN_6CisSP1fdCrablbe5_1wb5PZbV_1c9-_1fd15VT515s915bWSBTNfIPKIuxBWslh4ur8RMiBTSTzOneahq9G1HP4GHpsYmI685PTJqnLQVtiUZO7c3I1vQ2zVlGKes1pxkr99TUJRNpgT-tOytHj2l2rNvZ6PXE_L1qORktIBkKPbopOJ1HjJrXgFkZ6XjZRg9xAJkKA2yCV_VZQD2MLAR2yfI3ApW1LZQ4ppTUJRNpgthgrp-eKFYJXdsoG-wlrNc_l3W9UgK4p6N_BnMVEabglPw9c-wlrN4b5RKjhTblSra&spm=a1z09.2.0.0.18762e8dduCKjA) |
| 喇叭连接端子        | [立贴 1.25mm](https://item.taobao.com/item.htm?_u=o20q7cgba7d2&id=528734911747&pisk=gwk7OlgT7abSTZwdA7x4chnFHfwQNnJwpMZKjDBPv8e8O9nr5kWUTQqCdri9qzRHZ2GjJqVzyyPUdkgao0Weq0-BdDnTyJlzzk3qRqfrzvzEupii57WrvvSoiVoOUYRuL9wuKJLw7dJZD023pp56vPQlklEwLTE8w82A7zsy6dJZqmsz2hovQYyWP0Zl993LwSEY4rE8JTEJcSELbJB89TCAkr4YpuFdw-BYxkbRy2E8MiETx9I8w7QYDlUYJJF8vSKbxrE8pJH6DGZBNzmW2RRZ2qf1_czfp9HbDe4ZVp64AAZ5wyn7lFhoHu_gW0Ufp9UrV09Kc085ImyoCVZZzKBS5cDmlo39dEwnoX3LAqpG88cZrAVjzCBxD-4Y6XFX2LnbFzN_05sFZ8GKrvNoNMxiD83msPVJiEqjUAPQSS_vymoby5GIzEXLzfixl5kVuOynoX3LAqQ54GBa5usfOiNGdoawcn1htBfJ59TJLbATwoq7gntfyXV8moawcn_-t7E0Vr-Xcahh.&spm=a1z09.2.0.0.18762e8dduCKjA&skuId=4981522281625) |
| lcd        | [1.54寸240*240](https://item.taobao.com/item.htm?_u=o20q7cgb39a6&id=666130334299&pisk=gg2Tsb2SEwbiCd2txl5HnQ5q2ISnB6qaYPrWnq0MGyULkuKgsj0mGxUzyj9c_AcxHyg3IVqiCmGbozNi550DH-UUWdvmCrXxluPF0VmgInnbOPp0sl0g9nHaK5vmSNkYczDAraXlElrZ0x_lrEmJT0MSqhgXoVGBAxmYtlOMVlrZ3qtHl_VQbEL2Sy-scqsKdmnWfxGshe9I8mvjcrgsOei-4xMblfGCdmm-ld96CMdI20m6CxgsRDixXhMjhrGCvmuIfxMjC9QxRhgDH76NW2ZDXUO2gJnt6qpm8KhTnLcjPQgBHK3Offu5E2pXhJnTxzG1BLCmP7uiZ4aCKdHYAmUi-z6WpvG8aomYWOpsIWNUQA2hyEcTkVG0pXL610HtWbwsOTj3YjNLpA21uQr3vVG-KWfFSjDTW7ur167Ukk3glRhBWw00Zk2s9z_HprlYaomYWOp_PgkLE8FraU0-mCsdvIRq1DSkH4pDr9UbAD3lXtd2gXSEv4jdvIRqTMoKrGIvgIlFY&spm=a1z09.2.0.0.18762e8dduCKjA) |
| 喇叭驱动        | [MAX98357A](https://item.taobao.com/item.htm?_u=o20q7cgb874d&id=730761245954&pisk=gax4sEcYImn4QnfAo3sazeIFnzsA5GlIsh11IdvGhsfmWtigbdJhCxhtHUWM1QHtHi9G_CR5wl6skfEwaI9HlZO6l1XGZQvbc1iA7CJB9CZsDE6gbQ9OSCtwBlWMICHA1x3WHKIOjXGCjD9vHL3BWkx4mamke94gi4_0u0JjFXGIAcu0EGlqOIidAdolL9jGnNfiqYW1IS4DsNXuE_BOISXgmYkPwOWgnZXDE_XhpGVciGVurO6_jr4cnTDPB_XGjGAGqYWtaM9DIuWCoAkSUJlTRYp1tKfzjkydHZD6iPZbcdBcuh2AwlXW4t7VtKxjj5VVE3tPRawsNgvpPIXPY0acxL8MTeAtRzI2IEAOzIh8z_TkPdfGrJZ9UN52rnbzIlvke17B7Il4ksYXawBlrRodFB1kHnYrB7vDOsScEainLLbMPnQpcbqVxepfcFAtRzI2IEjP4BVOExP6XEPg7ZXRU6MrUQqmjrfcqkoQWPQlvT5IhtaTWZXRU6MfhPUOr9BPOx6f.&spm=a1z09.2.0.0.18762e8dduCKjA) |
| 麦克风        | [INMP441](https://item.taobao.com/item.htm?abbucket=13&id=630204400000&ns=1&pisk=gpMiUbjuQfP_sGLL9vw6wTezSSddLRwb1qBYk-U2TyzCBSzv1qVmk2NqBRnt-ru-ooHtHxcCn0ijBhwvfRi_h-8JyLeqfcwXKR-Szx2U8uEDHOWa0RwU_MtppLp-fDjTb3neeAegQtZOgtu4_WrUfyrN3rz2xWr80-WagO5F8yaU3ozV_kyU0uEauruVTyrbDrW4u-oEYoZ43ru43HmURoe4ubqC3zDqdv8FOdoSOqTjLlVgzczZXcHFp5CsfyfAGvc3jzJ8-tWqKloKzaEPEIUrNPF8z2JdI-mUmVUKzLW3L7cSDo0MIOytTXM_BvLlk5007SH3tN5aqR4gaAPeeHnQgXD3BA8fNDn3772K9BTTMR0iNzNwOegmxP3qIW7MJrh-vAPnzeBnlScSDo0MIOuP4M1FaBS1hk-xLs1b_kZ3yuCmfO8ZOczyxHfW15rQXUKHxs1b_kZ9yHxhNNNaAlLR.&priceTId=2147840117427949199974875e13de&spm=a21n57.1.hoverItem.5&utparam=%7B%22aplus_abtest%22%3A%22e67127b4d7e13c7260e3a29fcc711cac%22%7D&xxc=taobaoSearch) |


注：以上链接仅供参考，您可以根据自己的需求在各大电商平台搜索相应组件。完整套件也可在Otto官网或淘宝/阿里巴巴搜索"Otto机器人套件"购买。

## 功能概述

otto 机器人具有丰富的动作能力，包括行走、转向、跳跃、摇摆等多种舞蹈动作。

### 动作

| 指令名称    | 描述             | 参数                                              |
|------------|-----------------|---------------------------------------------------|
| Walk       | 行走             | steps: 步数<br>speed: 速度 (越小越快500-3000)<br>direction: 方向 (1=前进, -1=后退) |
| Turn       | 转向            | steps: 步数<br>speed: 速度 <br>direction: 方向 (1=左转, -1=右转) |
| Stop       | 停止所有动作并回到初始位置 | 无                                        |
| Jump       | 跳跃            | steps: 步数<br>speed: 速度       |
| Swing      | 摇摆            | steps: 步数<br>speed: 速度 <br>height: 高度 (0-50) |
| Moonwalk   | 太空步          | steps: 步数<br>speed: 速度 <br>height: 高度 (15-40)<br>direction: 方向 (1=左, -1=右) |
| Bend       | 弯曲            | steps: 步数<br>speed: 速度 <br>direction: 方向 (1=左, -1=右) |
| ShakeLeg   | 抖腿            | steps: 步数<br>speed: 速度 <br>direction: 方向 (1=左腿, -1=右腿) |
| UpDown     | 上下运动        | steps: 步数<br>speed: 速度 <br>height: 高度 (0-90) |
| TiptoeSwing | 脚尖摇摆       | steps: 步数<br>speed: 速度 <br>height: 高度 (0-50) |
| Jitter     | 抖动           | steps: 步数<br>speed: 速度 <br>height: 高度 (5-25) |
| AscendingTurn | 上升转弯     | steps: 步数<br>speed: 速度 <br>height: 高度 (5-15) |
| Crusaito   | 混合步态       | steps: 步数<br>speed: 速度 <br>height: 高度 (20-50)<br>direction: 方向 (1=左, -1=右) |
| Flapping   | 拍打动作       | steps: 步数<br>speed: 速度 <br>height: 高度 (10-30)<br>direction: 方向 (1=前, -1=后) |

### 对话指令
例：向前走/向前走5步/快速向前
说明：小智控制机器人动作是创建新的任务在后台控制，如让机器人向前走10步，设置完，小智会进入听取语音指令状态，此时机器人仍在向前走，可以通过 "停止" 语音指令 停下otto

