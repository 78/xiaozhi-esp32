#include "otto_movements.h"

#include <algorithm>

#include "freertos/idf_additions.h"
#include "oscillator.h"

static const char* TAG = "OttoMovements";

#define HAND_HOME_POSITION 45

Otto::Otto() {
    is_otto_resting_ = false;
    has_hands_ = false;
    // 初始化所有舵机管脚为-1（未连接）
    for (int i = 0; i < SERVO_COUNT; i++) {
        servo_pins_[i] = -1;
        servo_trim_[i] = 0;
    }
}

Otto::~Otto() {
    DetachServos();
}

unsigned long IRAM_ATTR millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

void Otto::Init(int left_leg, int right_leg, int left_foot, int right_foot, int left_hand,
                int right_hand) {
    servo_pins_[LEFT_LEG] = left_leg;
    servo_pins_[RIGHT_LEG] = right_leg;
    servo_pins_[LEFT_FOOT] = left_foot;
    servo_pins_[RIGHT_FOOT] = right_foot;
    servo_pins_[LEFT_HAND] = left_hand;
    servo_pins_[RIGHT_HAND] = right_hand;

    // 检查是否有手部舵机
    has_hands_ = (left_hand != -1 && right_hand != -1);

    AttachServos();
    is_otto_resting_ = false;
}

///////////////////////////////////////////////////////////////////
//-- ATTACH & DETACH FUNCTIONS ----------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::AttachServos() {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].Attach(servo_pins_[i]);
        }
    }
}

void Otto::DetachServos() {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].Detach();
        }
    }
}

///////////////////////////////////////////////////////////////////
//-- OSCILLATORS TRIMS ------------------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::SetTrims(int left_leg, int right_leg, int left_foot, int right_foot, int left_hand,
                    int right_hand) {
    servo_trim_[LEFT_LEG] = left_leg;
    servo_trim_[RIGHT_LEG] = right_leg;
    servo_trim_[LEFT_FOOT] = left_foot;
    servo_trim_[RIGHT_FOOT] = right_foot;

    if (has_hands_) {
        servo_trim_[LEFT_HAND] = left_hand;
        servo_trim_[RIGHT_HAND] = right_hand;
    }

    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].SetTrim(servo_trim_[i]);
        }
    }
}

///////////////////////////////////////////////////////////////////
//-- BASIC MOTION FUNCTIONS -------------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::MoveServos(int time, int servo_target[]) {
    if (GetRestState() == true) {
        SetRestState(false);
    }

    final_time_ = millis() + time;
    if (time > 10) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_pins_[i] != -1) {
                increment_[i] = (servo_target[i] - servo_[i].GetPosition()) / (time / 10.0);
            }
        }

        for (int iteration = 1; millis() < final_time_; iteration++) {
            partial_time_ = millis() + 10;
            for (int i = 0; i < SERVO_COUNT; i++) {
                if (servo_pins_[i] != -1) {
                    servo_[i].SetPosition(servo_[i].GetPosition() + increment_[i]);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    } else {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_pins_[i] != -1) {
                servo_[i].SetPosition(servo_target[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(time));
    }

    // final adjustment to the target.
    bool f = true;
    int adjustment_count = 0;
    while (f && adjustment_count < 10) {
        f = false;
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_pins_[i] != -1 && servo_target[i] != servo_[i].GetPosition()) {
                f = true;
                break;
            }
        }
        if (f) {
            for (int i = 0; i < SERVO_COUNT; i++) {
                if (servo_pins_[i] != -1) {
                    servo_[i].SetPosition(servo_target[i]);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            adjustment_count++;
        }
    };
}

void Otto::MoveSingle(int position, int servo_number) {
    if (position > 180)
        position = 90;
    if (position < 0)
        position = 90;

    if (GetRestState() == true) {
        SetRestState(false);
    }

    if (servo_number >= 0 && servo_number < SERVO_COUNT && servo_pins_[servo_number] != -1) {
        servo_[servo_number].SetPosition(position);
    }
}

void Otto::OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                           double phase_diff[SERVO_COUNT], float cycle = 1) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].SetO(offset[i]);
            servo_[i].SetA(amplitude[i]);
            servo_[i].SetT(period);
            servo_[i].SetPh(phase_diff[i]);
        }
    }

    double ref = millis();
    double end_time = period * cycle + ref;

    while (millis() < end_time) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_pins_[i] != -1) {
                servo_[i].Refresh();
            }
        }
        vTaskDelay(5);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

void Otto::Execute(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                   double phase_diff[SERVO_COUNT], float steps = 1.0) {
    if (GetRestState() == true) {
        SetRestState(false);
    }

    int cycles = (int)steps;

    //-- Execute complete cycles
    if (cycles >= 1)
        for (int i = 0; i < cycles; i++)
            OscillateServos(amplitude, offset, period, phase_diff);

    //-- Execute the final not complete cycle
    OscillateServos(amplitude, offset, period, phase_diff, (float)steps - cycles);
    vTaskDelay(pdMS_TO_TICKS(10));
}

//---------------------------------------------------------
//-- Execute2: 使用绝对角度作为振荡中心
//--  Parameters:
//--    amplitude: 振幅数组（每个舵机的振荡幅度）
//--    center_angle: 绝对角度数组（0-180度），作为振荡中心位置
//--    period: 周期（毫秒）
//--    phase_diff: 相位差数组（弧度）
//--    steps: 步数/周期数（可为小数）
//---------------------------------------------------------
void Otto::Execute2(int amplitude[SERVO_COUNT], int center_angle[SERVO_COUNT], int period,
                    double phase_diff[SERVO_COUNT], float steps = 1.0) {
    if (GetRestState() == true) {
        SetRestState(false);
    }

    // 将绝对角度转换为offset（offset = center_angle - 90）
    int offset[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++) {
        offset[i] = center_angle[i] - 90;
    }

    int cycles = (int)steps;

    //-- Execute complete cycles
    if (cycles >= 1)
        for (int i = 0; i < cycles; i++)
            OscillateServos(amplitude, offset, period, phase_diff);

    //-- Execute the final not complete cycle
    OscillateServos(amplitude, offset, period, phase_diff, (float)steps - cycles);
    vTaskDelay(pdMS_TO_TICKS(10));
}

///////////////////////////////////////////////////////////////////
//-- HOME = Otto at rest position -------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::Home(bool hands_down) {
    if (is_otto_resting_ == false) {  // Go to rest position only if necessary
        // 为所有舵机准备初始位置值
        int homes[SERVO_COUNT];
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (i == LEFT_HAND || i == RIGHT_HAND) {
                if (hands_down) {
                    // 如果需要复位手部，设置为默认值
                    if (i == LEFT_HAND) {
                        homes[i] = HAND_HOME_POSITION;
                    } else {                                  // RIGHT_HAND
                        homes[i] = 180 - HAND_HOME_POSITION;  // 右手镜像位置
                    }
                } else {
                    // 如果不需要复位手部，保持当前位置
                    homes[i] = servo_[i].GetPosition();
                }
            } else {
                // 腿部和脚部舵机始终复位
                homes[i] = 90;
            }
        }

        MoveServos(700, homes);
        is_otto_resting_ = true;
    }

    vTaskDelay(pdMS_TO_TICKS(200));
}

bool Otto::GetRestState() {
    return is_otto_resting_;
}

void Otto::SetRestState(bool state) {
    is_otto_resting_ = state;
}

///////////////////////////////////////////////////////////////////
//-- PREDETERMINED MOTION SEQUENCES -----------------------------//
///////////////////////////////////////////////////////////////////
//-- Otto movement: Jump
//--  Parameters:
//--    steps: Number of steps
//--    T: Period
//---------------------------------------------------------
void Otto::Jump(float steps, int period) {
    int up[SERVO_COUNT] = {90, 90, 150, 30, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    MoveServos(period, up);
    int down[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    MoveServos(period, down);
}

//---------------------------------------------------------
//-- Otto gait: Walking  (forward or backward)
//--  Parameters:
//--    * steps:  Number of steps
//--    * T : Period
//--    * Dir: Direction: FORWARD / BACKWARD
//--    * amount: 手部摆动幅度, 0表示不摆动
//---------------------------------------------------------
void Otto::Walk(float steps, int period, int dir, int amount) {
    //-- Oscillator parameters for walking
    //-- Hip sevos are in phase
    //-- Feet servos are in phase
    //-- Hip and feet are 90 degrees out of phase
    //--      -90 : Walk forward
    //--       90 : Walk backward
    //-- Feet servos also have the same offset (for tiptoe a little bit)
    int A[SERVO_COUNT] = {30, 30, 30, 30, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 5, -5, HAND_HOME_POSITION - 90, HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(dir * -90), DEG2RAD(dir * -90), 0, 0};

    // 如果amount>0且有手部舵机，设置手部振幅和相位
    if (amount > 0 && has_hands_) {
        // 手臂振幅使用传入的amount参数
        A[LEFT_HAND] = amount;
        A[RIGHT_HAND] = amount;

        // 左手与右腿同相，右手与左腿同相，使得机器人走路时手臂自然摆动
        phase_diff[LEFT_HAND] = phase_diff[RIGHT_LEG];  // 左手与右腿同相
        phase_diff[RIGHT_HAND] = phase_diff[LEFT_LEG];  // 右手与左腿同相
    } else {
        A[LEFT_HAND] = 0;
        A[RIGHT_HAND] = 0;
    }

    //-- Let's oscillate the servos!
    Execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Turning (left or right)
//--  Parameters:
//--   * Steps: Number of steps
//--   * T: Period
//--   * Dir: Direction: LEFT / RIGHT
//--   * amount: 手部摆动幅度, 0表示不摆动
//---------------------------------------------------------
void Otto::Turn(float steps, int period, int dir, int amount) {
    //-- Same coordination than for walking (see Otto::walk)
    //-- The Amplitudes of the hip's oscillators are not igual
    //-- When the right hip servo amplitude is higher, the steps taken by
    //--   the right leg are bigger than the left. So, the robot describes an
    //--   left arc
    int A[SERVO_COUNT] = {30, 30, 30, 30, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 5, -5, HAND_HOME_POSITION - 90, HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(-90), DEG2RAD(-90), 0, 0};

    if (dir == LEFT) {
        A[0] = 30;  //-- Left hip servo
        A[1] = 0;   //-- Right hip servo
    } else {
        A[0] = 0;
        A[1] = 30;
    }

    // 如果amount>0且有手部舵机，设置手部振幅和相位
    if (amount > 0 && has_hands_) {
        // 手臂振幅使用传入的amount参数
        A[LEFT_HAND] = amount;
        A[RIGHT_HAND] = amount;

        // 转向时手臂摆动相位：左手与左腿同相，右手与右腿同相，增强转向效果
        phase_diff[LEFT_HAND] = phase_diff[LEFT_LEG];    // 左手与左腿同相
        phase_diff[RIGHT_HAND] = phase_diff[RIGHT_LEG];  // 右手与右腿同相
    } else {
        A[LEFT_HAND] = 0;
        A[RIGHT_HAND] = 0;
    }

    //-- Let's oscillate the servos!
    Execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Lateral bend
//--  Parameters:
//--    steps: Number of bends
//--    T: Period of one bend
//--    dir: RIGHT=Right bend LEFT=Left bend
//---------------------------------------------------------
void Otto::Bend(int steps, int period, int dir) {
    // Parameters of all the movements. Default: Left bend
    int bend1[SERVO_COUNT] = {90, 90, 62, 35, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int bend2[SERVO_COUNT] = {90, 90, 62, 105, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int homes[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    // Time of one bend, constrained in order to avoid movements too fast.
    // T=max(T, 600);
    // Changes in the parameters if right direction is chosen
    if (dir == -1) {
        bend1[2] = 180 - 35;
        bend1[3] = 180 - 60;  // Not 65. Otto is unbalanced
        bend2[2] = 180 - 105;
        bend2[3] = 180 - 60;
    }

    // Time of the bend movement. Fixed parameter to avoid falls
    int T2 = 800;

    // Bend movement
    for (int i = 0; i < steps; i++) {
        MoveServos(T2 / 2, bend1);
        MoveServos(T2 / 2, bend2);
        vTaskDelay(pdMS_TO_TICKS(period * 0.8));
        MoveServos(500, homes);
    }
}

//---------------------------------------------------------
//-- Otto gait: Shake a leg
//--  Parameters:
//--    steps: Number of shakes
//--    T: Period of one shake
//--    dir: RIGHT=Right leg LEFT=Left leg
//---------------------------------------------------------
void Otto::ShakeLeg(int steps, int period, int dir) {
    // This variable change the amount of shakes
    int numberLegMoves = 2;

    // Parameters of all the movements. Default: Right leg
    int shake_leg1[SERVO_COUNT] = {90, 90, 58, 35, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int shake_leg2[SERVO_COUNT] = {90, 90, 58, 120, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int shake_leg3[SERVO_COUNT] = {90, 90, 58, 60, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int homes[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    // Changes in the parameters if left leg is chosen
    if (dir == LEFT) {
        shake_leg1[2] = 180 - 35;
        shake_leg1[3] = 180 - 58;
        shake_leg2[2] = 180 - 120;
        shake_leg2[3] = 180 - 58;
        shake_leg3[2] = 180 - 60;
        shake_leg3[3] = 180 - 58;
    }

    // Time of the bend movement. Fixed parameter to avoid falls
    int T2 = 1000;
    // Time of one shake, constrained in order to avoid movements too fast.
    period = period - T2;
    period = std::max(period, 200 * numberLegMoves);

    for (int j = 0; j < steps; j++) {
        // Bend movement
        MoveServos(T2 / 2, shake_leg1);
        MoveServos(T2 / 2, shake_leg2);

        // Shake movement
        for (int i = 0; i < numberLegMoves; i++) {
            MoveServos(period / (2 * numberLegMoves), shake_leg3);
            MoveServos(period / (2 * numberLegMoves), shake_leg2);
        }
        MoveServos(500, homes);  // Return to home position
    }

    vTaskDelay(pdMS_TO_TICKS(period));
}

//---------------------------------------------------------
//-- Otto movement: Sit (坐下)
//---------------------------------------------------------
void Otto::Sit() {
    int target[SERVO_COUNT] = {120, 60, 0, 180, 45, 135};
    MoveServos(600, target);
}

//---------------------------------------------------------
//-- Otto movement: up & down
//--  Parameters:
//--    * steps: Number of jumps
//--    * T: Period
//--    * h: Jump height: SMALL / MEDIUM / BIG
//--              (or a number in degrees 0 - 90)
//---------------------------------------------------------
void Otto::UpDown(float steps, int period, int height) {
    //-- Both feet are 180 degrees out of phase
    //-- Feet amplitude and offset are the same
    //-- Initial phase for the right foot is -90, so that it starts
    //--   in one extreme position (not in the middle)
    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height, -height, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(-90), DEG2RAD(90), 0, 0};

    //-- Let's oscillate the servos!
    Execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto movement: swinging side to side
//--  Parameters:
//--     steps: Number of steps
//--     T : Period
//--     h : Amount of swing (from 0 to 50 aprox)
//---------------------------------------------------------
void Otto::Swing(float steps, int period, int height) {
    //-- Both feets are in phase. The offset is half the amplitude
    //-- It causes the robot to swing from side to side
    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height / 2, -height / 2, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(0), DEG2RAD(0), 0, 0};

    //-- Let's oscillate the servos!
    Execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto movement: swinging side to side without touching the floor with the heel
//--  Parameters:
//--     steps: Number of steps
//--     T : Period
//--     h : Amount of swing (from 0 to 50 aprox)
//---------------------------------------------------------
void Otto::TiptoeSwing(float steps, int period, int height) {
    //-- Both feets are in phase. The offset is not half the amplitude in order to tiptoe
    //-- It causes the robot to swing from side to side
    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {0, 0, height, -height, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};

    //-- Let's oscillate the servos!
    Execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Jitter
//--  Parameters:
//--    steps: Number of jitters
//--    T: Period of one jitter
//--    h: height (Values between 5 - 25)
//---------------------------------------------------------
void Otto::Jitter(float steps, int period, int height) {
    //-- Both feet are 180 degrees out of phase
    //-- Feet amplitude and offset are the same
    //-- Initial phase for the right foot is -90, so that it starts
    //--   in one extreme position (not in the middle)
    //-- h is constrained to avoid hit the feets
    height = std::min(25, height);
    int A[SERVO_COUNT] = {height, height, 0, 0, 0, 0};
    int O[SERVO_COUNT] = {0, 0, 0, 0, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(-90), DEG2RAD(90), 0, 0, 0, 0};

    //-- Let's oscillate the servos!
    Execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Ascending & turn (Jitter while up&down)
//--  Parameters:
//--    steps: Number of bends
//--    T: Period of one bend
//--    h: height (Values between 5 - 15)
//---------------------------------------------------------
void Otto::AscendingTurn(float steps, int period, int height) {
    //-- Both feet and legs are 180 degrees out of phase
    //-- Initial phase for the right foot is -90, so that it starts
    //--   in one extreme position (not in the middle)
    //-- h is constrained to avoid hit the feets
    height = std::min(13, height);
    int A[SERVO_COUNT] = {height, height, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height + 4, -height + 4, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(-90), DEG2RAD(90), DEG2RAD(-90), DEG2RAD(90), 0, 0};

    //-- Let's oscillate the servos!
    Execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Moonwalker. Otto moves like Michael Jackson
//--  Parameters:
//--    Steps: Number of steps
//--    T: Period
//--    h: Height. Typical valures between 15 and 40
//--    dir: Direction: LEFT / RIGHT
//---------------------------------------------------------
void Otto::Moonwalker(float steps, int period, int height, int dir) {
    //-- This motion is similar to that of the caterpillar robots: A travelling
    //-- wave moving from one side to another
    //-- The two Otto's feet are equivalent to a minimal configuration. It is known
    //-- that 2 servos can move like a worm if they are 120 degrees out of phase
    //-- In the example of Otto, the two feet are mirrored so that we have:
    //--    180 - 120 = 60 degrees. The actual phase difference given to the oscillators
    //--  is 60 degrees.
    //--  Both amplitudes are equal. The offset is half the amplitud plus a little bit of
    //-   offset so that the robot tiptoe lightly

    int A[SERVO_COUNT] = {0, 0, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height / 2 + 2, -height / 2 - 2, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    int phi = -dir * 90;
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(phi), DEG2RAD(-60 * dir + phi), 0, 0};

    //-- Let's oscillate the servos!
    Execute(A, O, period, phase_diff, steps);
}

//----------------------------------------------------------
//-- Otto gait: Crusaito. A mixture between moonwalker and walk
//--   Parameters:
//--     steps: Number of steps
//--     T: Period
//--     h: height (Values between 20 - 50)
//--     dir:  Direction: LEFT / RIGHT
//-----------------------------------------------------------
void Otto::Crusaito(float steps, int period, int height, int dir) {
    int A[SERVO_COUNT] = {25, 25, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height / 2 + 4, -height / 2 - 4, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {90, 90, DEG2RAD(0), DEG2RAD(-60 * dir), 0, 0};

    //-- Let's oscillate the servos!
    Execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Flapping
//--  Parameters:
//--    steps: Number of steps
//--    T: Period
//--    h: height (Values between 10 - 30)
//--    dir: direction: FOREWARD, BACKWARD
//---------------------------------------------------------
void Otto::Flapping(float steps, int period, int height, int dir) {
    int A[SERVO_COUNT] = {12, 12, height, height, 0, 0};
    int O[SERVO_COUNT] = {
        0, 0, height - 10, -height + 10, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};
    double phase_diff[SERVO_COUNT] = {
        DEG2RAD(0), DEG2RAD(180), DEG2RAD(-90 * dir), DEG2RAD(90 * dir), 0, 0};

    //-- Let's oscillate the servos!
    Execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: WhirlwindLeg (旋风腿)
//--   Parameters:
//--     steps: Number of steps
//--     period: Period (建议100-800毫秒)
//--     amplitude: amplitude (Values between 20 - 40)
//---------------------------------------------------------
void Otto::WhirlwindLeg(float steps, int period, int amplitude) {


    int target[SERVO_COUNT] = {90, 90, 180, 90, 45, 20};
    MoveServos(100, target);
    target[RIGHT_FOOT] = 160;
    MoveServos(500, target);
    vTaskDelay(pdMS_TO_TICKS(1000));

    int C[SERVO_COUNT] = {90, 90, 180, 160, 45, 20};
    int A[SERVO_COUNT] = {amplitude, 0, 0, 0, amplitude, 0};
    double phase_diff[SERVO_COUNT] = {DEG2RAD(20), 0, 0, 0, DEG2RAD(20), 0};
    Execute2(A, C, period, phase_diff, steps);

}

//---------------------------------------------------------
//-- 手部动作: 举手
//--  Parameters:
//--    period: 动作时间
//--    dir: 方向 1=左手, -1=右手, 0=双手
//---------------------------------------------------------
void Otto::HandsUp(int period, int dir) {
    if (!has_hands_) {
        return;
    }

    int target[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == 0) {
        target[LEFT_HAND] = 170;
        target[RIGHT_HAND] = 10;
    } else if (dir == LEFT) {
        target[LEFT_HAND] = 170;
        target[RIGHT_HAND] = servo_[RIGHT_HAND].GetPosition();
    } else if (dir == RIGHT) {
        target[RIGHT_HAND] = 10;
        target[LEFT_HAND] = servo_[LEFT_HAND].GetPosition();
    }

    MoveServos(period, target);
}

//---------------------------------------------------------
//-- 手部动作: 双手放下
//--  Parameters:
//--    period: 动作时间
//--    dir: 方向 1=左手, -1=右手, 0=双手
//---------------------------------------------------------
void Otto::HandsDown(int period, int dir) {
    if (!has_hands_) {
        return;
    }

    int target[SERVO_COUNT] = {90, 90, 90, 90, HAND_HOME_POSITION, 180 - HAND_HOME_POSITION};

    if (dir == LEFT) {
        target[RIGHT_HAND] = servo_[RIGHT_HAND].GetPosition();
    } else if (dir == RIGHT) {
        target[LEFT_HAND] = servo_[LEFT_HAND].GetPosition();
    }

    MoveServos(period, target);
}

//---------------------------------------------------------
//--  手部动作: 挥手
//--  Parameters:
//--  dir: 方向 LEFT/RIGHT/BOTH
//---------------------------------------------------------
void Otto::HandWave(int dir) {
    if (!has_hands_) {
        return;
    }
    if (dir == LEFT) {
        int center_angle[SERVO_COUNT] = {90, 90, 90, 90, 160, 135};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 20, 0};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), 0};
        Execute2(A, center_angle, 300, phase_diff, 5);
    }
    else if (dir == RIGHT) {
        int center_angle[SERVO_COUNT] = {90, 90, 90, 90, 45, 20};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 0, 20};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, DEG2RAD(90)};
        Execute2(A, center_angle, 300, phase_diff, 5);
    }
    else {
        int center_angle[SERVO_COUNT] = {90, 90, 90, 90, 160, 20};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 20, 20};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(90)};
        Execute2(A, center_angle, 300, phase_diff, 5);
    }
}


//---------------------------------------------------------
//-- 手部动作: 大风车
//--  Parameters:
//--    steps: 动作次数
//--    period: 动作周期（毫秒）
//--    amplitude: 振荡幅度（度）
//---------------------------------------------------------
void Otto::Windmill(float steps, int period, int amplitude) {
    if (!has_hands_) {
        return;
    }

    int center_angle[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
    int A[SERVO_COUNT] = {0, 0, 0, 0, amplitude, amplitude};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(90)};
    Execute2(A, center_angle, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- 手部动作: 起飞
//--  Parameters:
//--    steps: 动作次数
//--    period: 动作周期（毫秒），数值越小速度越快
//--    amplitude: 振荡幅度（度）
//---------------------------------------------------------
void Otto::Takeoff(float steps, int period, int amplitude) {
    if (!has_hands_) {
        return;
    }

    Home(true);

    int center_angle[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
    int A[SERVO_COUNT] = {0, 0, 0, 0, amplitude, amplitude};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(-90)};
    Execute2(A, center_angle, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- 手部动作: 健身
//--  Parameters:
//--    steps: 动作次数
//--    period: 动作周期（毫秒）
//--    amplitude: 振荡幅度（度）
//---------------------------------------------------------
void Otto::Fitness(float steps, int period, int amplitude) {
    if (!has_hands_) {
        return;
    }
    int target[SERVO_COUNT] = {90, 90, 90, 0, 160, 135};
    MoveServos(100, target);
    target[LEFT_FOOT] = 20;
    MoveServos(400, target);
    vTaskDelay(pdMS_TO_TICKS(2000));

    int C[SERVO_COUNT] = {90, 90, 20, 90, 160, 135};
    int A[SERVO_COUNT] = {0, 0, 0, 0, 0, amplitude};
    double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};
    Execute2(A, C, period, phase_diff, steps);

}

//---------------------------------------------------------
//-- 手部动作: 打招呼
//--  Parameters:
//--    dir: 方向 LEFT=左手, RIGHT=右手
//--    steps: 动作次数
//---------------------------------------------------------
void Otto::Greeting(int dir, float steps) {
    if (!has_hands_) {
        return;
    }
    if (dir == LEFT) {
        int target[SERVO_COUNT] = {90, 90, 150, 150, 45, 135};
        MoveServos(400, target);
        int C[SERVO_COUNT] = {90, 90, 150, 150, 160, 135};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 20, 0};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};
        Execute2(A, C, 300, phase_diff, steps);
    }
    else if (dir == RIGHT) {
        int target[SERVO_COUNT] = {90, 90, 30, 30, 45, 135};
        MoveServos(400, target);
        int C[SERVO_COUNT] = {90, 90, 30, 30, 45, 20};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 0, 20};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};
        Execute2(A, C, 300, phase_diff, steps);
    }

}

//---------------------------------------------------------
//-- 手部动作: 害羞
//--  Parameters:
//--    dir: 方向 LEFT=左手, RIGHT=右手
//--    steps: 动作次数
//---------------------------------------------------------
void Otto::Shy(int dir, float steps) {
    if (!has_hands_) {
        return;
    }

    if (dir == LEFT) {
        int target[SERVO_COUNT] = {90, 90, 150, 150, 45, 135};
        MoveServos(400, target);
        int C[SERVO_COUNT] = {90, 90, 150, 150, 45, 135};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 20, 20};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(-90)};
        Execute2(A, C, 300, phase_diff, steps);
    }
    else if (dir == RIGHT) {
        int target[SERVO_COUNT] = {90, 90, 30, 30, 45, 135};
        MoveServos(400, target);
        int C[SERVO_COUNT] = {90, 90, 30, 30, 45, 135};
        int A[SERVO_COUNT] = {0, 0, 0, 0, 0, 20};
        double phase_diff[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(-90)};
        Execute2(A, C, 300, phase_diff, steps);
    }
}

//---------------------------------------------------------
//-- 手部动作: 广播体操
//---------------------------------------------------------
void Otto::RadioCalisthenics() {
    if (!has_hands_) {
        return;
    }

    const int period = 1000; 
    const float steps = 8.0; 

    int C1[SERVO_COUNT] = {90, 90, 90, 90, 145, 45};
    int A1[SERVO_COUNT] = {0, 0, 0, 0, 45, 45};
    double phase_diff1[SERVO_COUNT] = {0, 0, 0, 0, DEG2RAD(90), DEG2RAD(-90)};
    Execute2(A1, C1, period, phase_diff1, steps);

    int C2[SERVO_COUNT] = {90, 90, 115, 65, 90, 90};
    int A2[SERVO_COUNT] = {0, 0, 25, 25, 0, 0};
    double phase_diff2[SERVO_COUNT] = {0, 0, DEG2RAD(90), DEG2RAD(-90), 0, 0};
    Execute2(A2, C2, period, phase_diff2, steps);
    
    int C3[SERVO_COUNT] = {90, 90, 130, 130, 90, 90};
    int A3[SERVO_COUNT] = {0, 0, 0, 0, 20, 0};
    double phase_diff3[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};
    Execute2(A3, C3, period, phase_diff3, steps);

    int C4[SERVO_COUNT] = {90, 90, 50, 50, 90, 90};
    int A4[SERVO_COUNT] = {0, 0, 0, 0, 0, 20};
    double phase_diff4[SERVO_COUNT] = {0, 0, 0, 0, 0, 0};
    Execute2(A4, C4, period, phase_diff4, steps);
}

//---------------------------------------------------------
//-- 手部动作: 爱的魔力转圈圈
//---------------------------------------------------------
void Otto::MagicCircle() {
    if (!has_hands_) {
        return;
    }

    int A[SERVO_COUNT] = {30, 30, 30, 30, 50, 50};
    int O[SERVO_COUNT] = {0, 0, 5, -5, 0, 0};
    double phase_diff[SERVO_COUNT] = {0, 0, DEG2RAD(-90), DEG2RAD(-90), DEG2RAD(-90) , DEG2RAD(90)};

    Execute(A, O, 700, phase_diff, 40);
}

//---------------------------------------------------------
//-- 展示动作：串联多个动作展示
//---------------------------------------------------------
void Otto::Showcase() {
    if (GetRestState() == true) {
        SetRestState(false);
    }

    // 1. 往前走3步
    Walk(3, 1000, FORWARD, 50);
    vTaskDelay(pdMS_TO_TICKS(500));

    // 2. 挥挥手
    if (has_hands_) {
        HandWave(LEFT);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // 3. 跳舞（使用广播体操）
    if (has_hands_) {
        RadioCalisthenics();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // 4. 太空步
    Moonwalker(3, 900, 25, LEFT);
    vTaskDelay(pdMS_TO_TICKS(500));

    // 5. 摇摆
    Swing(3, 1000, 30);
    vTaskDelay(pdMS_TO_TICKS(500));

    // 6. 起飞
    if (has_hands_) {
        Takeoff(5, 300, 40);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // 7. 健身
    if (has_hands_) {
        Fitness(5, 1000, 25);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // 8. 往后走3步
    Walk(3, 1000, BACKWARD, 50);
}

void Otto::EnableServoLimit(int diff_limit) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].SetLimiter(diff_limit);
        }
    }
}

void Otto::DisableServoLimit() {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].DisableLimiter();
        }
    }
}
