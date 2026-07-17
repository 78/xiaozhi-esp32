#ifndef DOG_CONTROL_H
#define DOG_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

// 舵机初始化（调用 weixintest2 的 servo_init）
void Dog_ServoInit(void);

// 保留原来的单腿控制（映射到 servo.h 的 API）
void Leg1_SetAngle(int angle);  // 右前 FR
void Leg2_SetAngle(int angle);  // 右后 BR
void Leg3_SetAngle(int angle);  // 左后 BL
void Leg4_SetAngle(int angle);  // 左前 FL

// 保留复位
void Dog_ResetAll(void);

// 参数化步态（新增：支持任意步数）
void Dog_ForwardSteps(int steps);      // 前进 N 步
void Dog_BackwardSteps(int steps);    // 后退 N 步
void Dog_TurnLeftSteps(int steps);    // 左转 N 步
void Dog_TurnRightSteps(int steps);   // 右转 N 步

// 持续行走控制（新增）
void Dog_WalkStart(void);             // 开始持续前进
void Dog_WalkStop(void);              // 停止行走并归位
int  Dog_GetWalkState(void);          // 获取行走状态

// MCP 工具注册
void InitMachineDog(void);

// ---- 摇摆控制（角度参数已内置，half_ms/step_ms 可设） ----
void Dog_SwingFB(int cycles, int half_ms);                  // 前后摇摆 (amp=15)
void Dog_SwingLR(int dir, int cycles, int half_ms);         // 左右摇摆 (amp=45, dir=1右倾先/-1左倾先)
void Dog_SwingTwist(int cycles, int half_ms);               // 旋转摇摆 (amp=25)
void Dog_SwingUpDown(int cycles, int half_ms);              // 上下摇摆 (amp=35)
void Dog_SwingSideLeft(int cycles, int half_ms);            // 左侧侧摇 (amp=20, in_deg=20)
void Dog_SwingSideRight(int cycles, int half_ms);           // 右侧侧摇 (amp=20, in_deg=20)
void Dog_SwingWave(int cycles, int step_ms);                // 波浪步 (amp=20)
void Dog_SwingMarch(int cycles, int half_ms);               // 原地踏步 (amp=20)
void Dog_SwingNod(int cycles, int half_ms);                 // 侧向点头 (amp=18)
void Dog_SwingTremble(int cycles, int half_ms);             // 颤抖 (amp=8)
void Dog_BodySway(int fb, int lr);             // 摇杆实时姿态控制
void Dog_DanceLong(void);                       // 长跳舞：8种动作随机顺序各一次（除侧向点头和颤抖）
void Dog_DanceShort(void);                      // 短跳舞：4种基础摇摆随机取3种
void Dog_Shutdown(void);                       // 关机：拉低 IO3 两次使 IP5306 断电
void Dog_EmergencyStop(void);                 // 紧急停止：立即中断所有运动并归位

#ifdef __cplusplus
}
#endif

#endif
