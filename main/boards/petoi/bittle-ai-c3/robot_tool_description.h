#ifndef PETOI_ROBOT_TOOL_DESCRIPTION_H_
#define PETOI_ROBOT_TOOL_DESCRIPTION_H_
namespace petoi {
inline constexpr const char* kRobotToolDescription =
    "Physical robot motion (not screen expression). Always call for body/head motion. "
    "Code only; one call/request, up to 4 comma-separated actions. Never retry transmitted "
    "commands. Single result confirms TX; sequence tokens do not prove physical completion. "
    "Gaits: kwkF walk, kbk back, ktrF run, kcrF crawl, kvtL/kvtR turn left/right. "
    "Gait number <200 is steps(default3), >=200 is ms; turn number is degrees(default90). "
    "Basic: kup stand, ksit sit, d rest, khi greet, kstr stretch, kgdb good, kpu pushup, kjmp "
    "jump. "
    "Emotion: kang angry, khg hug, kfiv highfive, kchr cheer, knd nod, kwh shake head, kzz sleepy. "
    "Skills: kpd play dead, kbf backflip, kff frontflip, khds handstand, kmw moonwalk, krl roll, "
    "kbx box, kkc kick. Interactive: kcmh come, khsk handshake, khu raise hand, ksnf sniff, "
    "kscrh scratch, kdg dig, kpee pee. Joints: m0 head(+left/-right), m8 left arm, m9 right arm "
    "(arms negative=forward). Examples: ksit; kwkF 3; m0 45; m8 -30; kup,ksit,d.";
}
#endif
