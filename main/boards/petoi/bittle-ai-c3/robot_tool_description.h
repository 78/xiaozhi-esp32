#ifndef PETOI_ROBOT_TOOL_DESCRIPTION_H_
#define PETOI_ROBOT_TOOL_DESCRIPTION_H_
namespace petoi {
inline constexpr const char* kRobotToolDescription =
    "机器狗物理动作指令(非屏幕表情)。参数仅含代码，勿含中文。\n"
    "Physical robot dog motion tool. Use one call per request; comma-separate up to four actions "
    "in order. Never retry after transmission. Multi-action replies wait for completion tokens.\n"
    "【铁律】机器狗身体动作必调此器！头部转动用m0或kwh，勿用expression!\n"
    "Gaits: kwkF forward, kbk backward, ktrF run, kvtL turn left, kvtR turn right. Basic: khi "
    "greet, ksit sit, kup stand, d rest, kstr stretch.\n"
    "【步态】kwkF前进 kbk后退 ktrF跑 kcrF爬 kvtL左转 kvtR右转\n"
    "  参≤200为步数(默认3)，>200为毫秒。转向参数=度数(默90)。\n"
    "【基本】khi打招呼 ksit坐 kgdb乖 kpu俯卧撑 kjmp跳 d休 kup起 kstr伸懒腰\n"
    "【情感】kang怒 khg抱 kfiv击掌 kchr庆 knd点头 kwh摇头 kzz困\n"
    "【技能】kpu俯卧撑/push up kpd装死/play dead kbf后翻 kff前翻 khds倒立 kmw太空步 krl翻滚 "
    "kbx拳击 kkc踢\n"
    "【互动】kcmh来 khsk握手 khu举手 ksnf嗅 kscrh抓 kdg挖 kpee尿 kpd装死\n"
    "【头部】kwh摇头/歪头/晃头 knd点头 m0转头(正=左负=右,如m0 45头左转)\n"
    "【关节】m<号> <角> 头0左手8右手9 负=前正=后\n"
    "【范例】坐下→ksit 跑→ktrF 3 俯卧撑→kpu 装死→kpd 歪头→kwh 头左转→m0 45 举左手→m8 -30";
}
#endif
