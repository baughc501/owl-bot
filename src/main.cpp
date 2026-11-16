#include <Arduino.h>
#include <Servo.h>

enum BehaviorId { PAUSED, IDLE, LISTENING, THINKING, TALKING,
                  HAPPY, CURIOUS, SAD, STARTLED, SLEEPY, STOPPED, BEHAVIOR_COUNT };

struct Pose {
    int16_t head, left, right;   // -1 = leave unchanged
    uint16_t holdMs;
};

struct Behavior {
    const char* name;
    const Pose* poses;
    uint8_t len;
    bool loop;
    BehaviorId next;             // used when loop == false
};

#define COUNT(a) (sizeof(a) / sizeof(a[0]))

const Pose IDLE_P[]      = {{85,180,0,1000},{95,180,0,1000},{90,180,0,8000}};
const Pose LISTENING_P[] = {{75,175,5,1500},{80,175,5,1500}};
const Pose THINKING_P[]  = {{110,-1,-1,700},{70,-1,-1,700}};
const Pose TALKING_P[]   = {{80,170,10,180},{95,175,5,180},{85,170,10,220},{92,175,5,180}};
const Pose HAPPY_P[]     = {{70,10,170,500},{90,20,170,500},{70,10,160,500},{90,20,170,1500},{80,170,10,1500}};
const Pose CURIOUS_P[]   = {{70,-1,-1,500},{110,-1,-1,500},{90,-1,-1,500}};
const Pose SAD_P[]       = {{55,180,0,2500},{60,180,0,1500}};
const Pose STARTLED_P[]  = {{90,100,80,250},{90,180,0,400}};
const Pose SLEEPY_P[]    = {{60,180,0,2500},{65,180,0,2500}};

// Row order MUST match the BehaviorId enum order
const Behavior BEHAVIORS[BEHAVIOR_COUNT] = {
    {"PAUSED",    nullptr,     0,                    false, PAUSED},
    {"IDLE",      IDLE_P,      COUNT(IDLE_P),        true,  IDLE},
    {"LISTENING", LISTENING_P, COUNT(LISTENING_P),   true,  LISTENING},
    {"THINKING",  THINKING_P,  COUNT(THINKING_P),    true,  THINKING},
    {"TALKING",   TALKING_P,   COUNT(TALKING_P),     true,  TALKING},
    {"HAPPY",     HAPPY_P,     COUNT(HAPPY_P),       false, IDLE},
    {"CURIOUS",   CURIOUS_P,   COUNT(CURIOUS_P),     false, IDLE},
    {"SAD",       SAD_P,       COUNT(SAD_P),         false, IDLE},
    {"STARTLED",  STARTLED_P,  COUNT(STARTLED_P),    false, IDLE},
    {"SLEEPY",    SLEEPY_P,    COUNT(SLEEPY_P),      true,  SLEEPY},
    {"STOPPED",   nullptr,     0,                    false, STOPPED},
};

const unsigned long SLEEP_AFTER_MS = 60000;

Servo head, leftWing, rightWing;
bool servosAttached = false;

BehaviorId currentId = PAUSED;
const Behavior* cur = &BEHAVIORS[PAUSED];
uint8_t step = 0;
unsigned long nextAt = 0;
unsigned long lastActivity = 0;

void attachServos()
{
    if (servosAttached) return;
    head.attach(10);
    leftWing.attach(8);
    rightWing.attach(9);
    servosAttached = true;
}

void detachServos()
{
    head.detach();
    leftWing.detach();
    rightWing.detach();
    servosAttached = false;
}

void startBehavior(BehaviorId id)
{
    if (id == STOPPED) detachServos();
    else attachServos();

    currentId = id;
    cur = &BEHAVIORS[id];
    step = 0;
    nextAt = millis();
}

void updateBehavior()
{
    if (cur->len == 0) return;
    if ((long)(millis() - nextAt) < 0) return;

    if (step >= cur->len) {
        if (cur->loop) step = 0;
        else { startBehavior(cur->next); return; }
    }

    const Pose& p = cur->poses[step++];
    if (p.head  >= 0) head.write(p.head);
    if (p.left  >= 0) leftWing.write(p.left);
    if (p.right >= 0) rightWing.write(p.right);
    nextAt = millis() + p.holdMs;
}

void handleSerial()
{
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    for (uint8_t i = 0; i < BEHAVIOR_COUNT; i++) {
        if (cmd == BEHAVIORS[i].name) {
            lastActivity = millis();
            startBehavior((BehaviorId)i);
            Serial.print("OK ");
            Serial.println(cmd);
            return;
        }
    }
    Serial.println("Unknown command");
}

void setup()
{
    Serial.begin(115200);
    Serial.setTimeout(50);
    delay(2000);
    Serial.println("Bubo Ready");

    attachServos();
    head.write(90);
    leftWing.write(180);
    rightWing.write(0);

    lastActivity = millis();
    startBehavior(PAUSED);
}

void loop()
{
    handleSerial();

    // Reflex: fall asleep if left alone
    if (currentId == IDLE && millis() - lastActivity > SLEEP_AFTER_MS)
        startBehavior(SLEEPY);

    updateBehavior();
}