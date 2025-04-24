#include "otto_movements.h"

#include <algorithm>

#include "oscillator.h"

static const char* TAG = "OttoMovements";

Otto::Otto() {
    is_otto_resting_ = false;
}

Otto::~Otto() {
    DetachServos();
}

unsigned long IRAM_ATTR millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

void Otto::Init(int left_leg, int right_leg, int left_foot, int right_foot) {
    servo_pins_[0] = left_leg;
    servo_pins_[1] = right_leg;
    servo_pins_[2] = left_foot;
    servo_pins_[3] = right_foot;

    AttachServos();
    is_otto_resting_ = false;
}

///////////////////////////////////////////////////////////////////
//-- ATTACH & DETACH FUNCTIONS ----------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::AttachServos() {
    servo_[0].Attach(servo_pins_[0]);
    servo_[1].Attach(servo_pins_[1]);
    servo_[2].Attach(servo_pins_[2]);
    servo_[3].Attach(servo_pins_[3]);
}

void Otto::DetachServos() {
    servo_[0].Detach();
    servo_[1].Detach();
    servo_[2].Detach();
    servo_[3].Detach();
}

///////////////////////////////////////////////////////////////////
//-- OSCILLATORS TRIMS ------------------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::SetTrims(int left_leg, int right_leg, int left_foot, int right_foot) {
    servo_[0].SetTrim(left_leg);
    servo_[1].SetTrim(right_leg);
    servo_[2].SetTrim(left_foot);
    servo_[3].SetTrim(right_foot);
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
        for (int i = 0; i < 4; i++)
            increment_[i] = (servo_target[i] - servo_[i].GetPosition()) / (time / 10.0);

        for (int iteration = 1; millis() < final_time_; iteration++) {
            partial_time_ = millis() + 10;
            for (int i = 0; i < 4; i++)
                servo_[i].SetPosition(servo_[i].GetPosition() + increment_[i]);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    } else {
        for (int i = 0; i < 4; i++)
            servo_[i].SetPosition(servo_target[i]);
        vTaskDelay(pdMS_TO_TICKS(time));
    }

    // final adjustment to the target.
    bool f = true;
    int adjustment_count = 0;
    while (f && adjustment_count < 10) {
        f = false;
        for (int i = 0; i < 4; i++) {
            if (servo_target[i] != servo_[i].GetPosition()) {
                f = true;
                break;
            }
        }
        if (f) {
            for (int i = 0; i < 4; i++) {
                servo_[i].SetPosition(servo_target[i]);
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
    int servoNumber = servo_number;
    if (servoNumber == 0) {
        servo_[0].SetPosition(position);
    }
    if (servoNumber == 1) {
        servo_[1].SetPosition(position);
    }
    if (servoNumber == 2) {
        servo_[2].SetPosition(position);
    }
    if (servoNumber == 3) {
        servo_[3].SetPosition(position);
    }
}

void Otto::OscillateServos(int amplitude[4], int offset[4], int period, double phase_diff[4],
                           float cycle = 1) {
    for (int i = 0; i < 4; i++) {
        servo_[i].SetO(offset[i]);
        servo_[i].SetA(amplitude[i]);
        servo_[i].SetT(period);
        servo_[i].SetPh(phase_diff[i]);
    }
    double ref = millis();
    double end_time = period * cycle + ref;

    while (millis() < end_time) {
        for (int i = 0; i < 4; i++) {
            servo_[i].Refresh();
        }
        vTaskDelay(5);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

void Otto::Execute(int amplitude[4], int offset[4], int period, double phase_diff[4],
                   float steps = 1.0) {
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

///////////////////////////////////////////////////////////////////
//-- HOME = Otto at rest position -------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::Home() {
    if (is_otto_resting_ == false) {  // Go to rest position only if necessary

        int homes[4] = {90, 90, 90, 90};  // All the servos at rest position
        MoveServos(500, homes);           // Move the servos in half a second

        is_otto_resting_ = true;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
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
    int up[] = {90, 90, 150, 30};
    MoveServos(period, up);
    int down[] = {90, 90, 90, 90};
    MoveServos(period, down);
}

//---------------------------------------------------------
//-- Otto gait: Walking  (forward or backward)
//--  Parameters:
//--    * steps:  Number of steps
//--    * T : Period
//--    * Dir: Direction: FORWARD / BACKWARD
//---------------------------------------------------------
void Otto::Walk(float steps, int period, int dir) {
    //-- Oscillator parameters for walking
    //-- Hip sevos are in phase
    //-- Feet servos are in phase
    //-- Hip and feet are 90 degrees out of phase
    //--      -90 : Walk forward
    //--       90 : Walk backward
    //-- Feet servos also have the same offset (for tiptoe a little bit)
    int A[4] = {30, 30, 20, 20};
    int O[4] = {0, 0, 4, -4};
    double phase_diff[4] = {0, 0, DEG2RAD(dir * -90), DEG2RAD(dir * -90)};

    //-- Let's oscillate the servos!
    Execute(A, O, period, phase_diff, steps);
}

//---------------------------------------------------------
//-- Otto gait: Turning (left or right)
//--  Parameters:
//--   * Steps: Number of steps
//--   * T: Period
//--   * Dir: Direction: LEFT / RIGHT
//---------------------------------------------------------
void Otto::Turn(float steps, int period, int dir) {
    //-- Same coordination than for walking (see Otto::walk)
    //-- The Amplitudes of the hip's oscillators are not igual
    //-- When the right hip servo amplitude is higher, the steps taken by
    //--   the right leg are bigger than the left. So, the robot describes an
    //--   left arc
    int A[4] = {30, 30, 20, 20};
    int O[4] = {0, 0, 4, -4};
    double phase_diff[4] = {0, 0, DEG2RAD(-90), DEG2RAD(-90)};

    if (dir == LEFT) {
        A[0] = 30;  //-- Left hip servo
        A[1] = 10;  //-- Right hip servo
    } else {
        A[0] = 10;
        A[1] = 30;
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
    int bend1[4] = {90, 90, 62, 35};
    int bend2[4] = {90, 90, 62, 105};
    int homes[4] = {90, 90, 90, 90};

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
    int shake_leg1[4] = {90, 90, 58, 35};
    int shake_leg2[4] = {90, 90, 58, 120};
    int shake_leg3[4] = {90, 90, 58, 60};
    int homes[4] = {90, 90, 90, 90};

    // Changes in the parameters if left leg is chosen
    if (dir == -1) {
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
    int A[4] = {0, 0, height, height};
    int O[4] = {0, 0, height, -height};
    double phase_diff[4] = {0, 0, DEG2RAD(-90), DEG2RAD(90)};

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
    int A[4] = {0, 0, height, height};
    int O[4] = {0, 0, height / 2, -height / 2};
    double phase_diff[4] = {0, 0, DEG2RAD(0), DEG2RAD(0)};

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
    int A[4] = {0, 0, height, height};
    int O[4] = {0, 0, height, -height};
    double phase_diff[4] = {0, 0, 0, 0};

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
    int A[4] = {height, height, 0, 0};
    int O[4] = {0, 0, 0, 0};
    double phase_diff[4] = {DEG2RAD(-90), DEG2RAD(90), 0, 0};

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
    int A[4] = {height, height, height, height};
    int O[4] = {0, 0, height + 4, -height + 4};
    double phase_diff[4] = {DEG2RAD(-90), DEG2RAD(90), DEG2RAD(-90), DEG2RAD(90)};

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

    int A[4] = {0, 0, height, height};
    int O[4] = {0, 0, height / 2 + 2, -height / 2 - 2};
    int phi = -dir * 90;
    double phase_diff[4] = {0, 0, DEG2RAD(phi), DEG2RAD(-60 * dir + phi)};

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
    int A[4] = {25, 25, height, height};
    int O[4] = {0, 0, height / 2 + 4, -height / 2 - 4};
    double phase_diff[4] = {90, 90, DEG2RAD(0), DEG2RAD(-60 * dir)};

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
    int A[4] = {12, 12, height, height};
    int O[4] = {0, 0, height - 10, -height + 10};
    double phase_diff[4] = {DEG2RAD(0), DEG2RAD(180), DEG2RAD(-90 * dir), DEG2RAD(90 * dir)};

    //-- Let's oscillate the servos!
    Execute(A, O, period, phase_diff, steps);
}

void Otto::EnableServoLimit(int diff_limit) {
    for (int i = 0; i < 4; i++) {
        servo_[i].SetLimiter(diff_limit);
    }
}

void Otto::DisableServoLimit() {
    for (int i = 0; i < 4; i++) {
        servo_[i].DisableLimiter();
    }
}
