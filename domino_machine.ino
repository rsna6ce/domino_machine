#include <IRremote.hpp> // for ver3.7.1
#include <VarSpeedServo.h> 
#include <avr/wdt.h>

#define IR_PIN_NO 3 /* ir */
#define A_PIN_NO 5  /* servo for arm */
#define R_PIN_NO 10 /* servo for right wheel */
#define L_PIN_NO 9 /* servo for left wheel */
VarSpeedServo servo1_a;
VarSpeedServo servo1_r;
VarSpeedServo servo1_l;

// trim for servo individual difference
#define STEERING_TRIM 30

// parameters
#define OFFSET_STEERING_VALUE 1
#define OFFSET_STEERING_MAX 50
#define OFFSET_RUN_MAX 500
#define OFFSET_RUN_VALUE 20
#define RUN_DISTANCE_TIMER 250
#define DELAY_RUN_AFTER_ARM 30
#define DELAY_ARM_AFTER_RUN 100
#define ARM_STEP_ANGLE 4
#define ARM_STEP_INTERVAL 32
#define ARM_ANGLE_MAX 48

// set code ir remote controler
#define IR_STARTSTOP 0xE51AF708
#define IR_UP        0xE41BF708
#define IR_DOWN      0xE619F708
#define IR_RIGHT     0xE11EF708
#define IR_LEFT      0xFD02F708

enum run_state {
    arm_swing_push,
    arm_swing_pull,
    run_start,
    run_stop,
    waiting,
};

bool machine_running = false;
int offset_run = 0;
int offset_steering = STEERING_TRIM;
unsigned long next_millis = 0;
run_state next_state = waiting;
int arm_angle = 0;

/*set speed from -100% to 100%*/
void set_speed_percent(int l, int r, int offset_trim) {
    // convert from percent to angle for servo
    int l_angle = ((l * 90) / 100) + 90;
    int r_angle = -((r * 90) / 100) + 90;
    if (offset_trim < 0) {
        servo1_r.write(r_angle, 100, false);
        delay(-offset_trim);
        servo1_l.write(l_angle, 100, false);
    } else if (offset_trim > 0) {
        servo1_l.write(l_angle, 100, false);
        delay(offset_trim);
        servo1_r.write(r_angle, 100, false);
    } else {
        servo1_l.write(l_angle, 100, false);
        servo1_r.write(r_angle, 100, false);
    }
}

void loop() {
    if (Serial.available() > 0) {
        String sdata = Serial.readStringUntil('\n');
        int idata = sdata.toInt();
        if (sdata=="") {
            machine_running = !machine_running;
        } else if (sdata=="a") {
            // debug arm
            servo1_a.write(50, 50, true);
            servo1_a.write(0, 100, true);
        } else if (sdata=="r") {
            // debug run
            set_speed_percent(100, 100, 0);
            delay(200);
            set_speed_percent(0, 0, 0);
        }
    }

    if (IrReceiver.decode()) {
        Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX);
        uint32_t decoded = IrReceiver.decodedIRData.decodedRawData;
        if (decoded == IR_STARTSTOP) {
            machine_running = !machine_running;
        } else if (decoded == IR_UP && (offset_run <= OFFSET_RUN_MAX-OFFSET_RUN_VALUE)) {
            offset_run += OFFSET_RUN_VALUE;
        } else if (decoded == IR_DOWN && (OFFSET_RUN_VALUE-OFFSET_RUN_MAX <= offset_run)) {
            offset_run -= OFFSET_RUN_VALUE;
        } else if (decoded == IR_RIGHT) {
            offset_steering += OFFSET_STEERING_VALUE;
        } else if (decoded == IR_LEFT) {
            offset_steering -= OFFSET_STEERING_VALUE;
        }
        delay(10);
        IrReceiver.resume();
    }

    if (millis() > next_millis) {
        if (next_state == arm_swing_push) {
            // arm
            arm_angle += ARM_STEP_ANGLE;
            servo1_a.write(arm_angle, 100, false);
            if (arm_angle >= ARM_ANGLE_MAX) {
                next_state = arm_swing_pull;
            }
            next_millis = millis() + ARM_STEP_INTERVAL; //nowait

        } else if (next_state == arm_swing_pull) {
            // arm pull
            servo1_a.write(0, 100, false);
            next_state = run_start;
            arm_angle = 0;
            next_millis = millis() + 100 + DELAY_RUN_AFTER_ARM;

        } else if (next_state == run_start) {
            // run
            set_speed_percent(100, 100, offset_steering);
            next_state = run_stop;
            next_millis = millis() + RUN_DISTANCE_TIMER + offset_run;
            
        } else if (next_state == run_stop) {
            // stop
            set_speed_percent(0, 0, 0);
            next_state = waiting;
            next_millis = millis();
            
        } else if (next_state == waiting) {
            // waiting
            if (machine_running) {
                next_state = arm_swing_push;
                next_millis = millis() + DELAY_ARM_AFTER_RUN;
            }
        }
    }
    wdt_reset();
}

void setup() {
    Serial.begin( 115200 );
    Serial.println( "Hello Arduino!" );
    pinMode(A_PIN_NO,INPUT);
    pinMode(R_PIN_NO,INPUT);
    pinMode(L_PIN_NO,INPUT);
    servo1_a.attach(A_PIN_NO);
    servo1_l.attach(L_PIN_NO);
    servo1_r.attach(R_PIN_NO);
    servo1_a.write(0);
    set_speed_percent(0, 0, 0);
    IrReceiver.begin(IR_PIN_NO, DISABLE_LED_FEEDBACK);

    wdt_disable();
    wdt_enable(WDTO_2S);
}
