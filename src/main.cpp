#include <Arduino.h>
#include <Servo.h>

enum Behavior { PAUSED, IDLE, CURIOUS, HAPPY };

struct Pose {
    int16_t head;    // -1 = leave unchanged
    int16_t left;
    int16_t right;
    uint16_t holdMs; // how long to hold before the next pose
};

const Pose IDLE_SEQ[] = {
    {85, 180, 0, 1000},
    {95, 180, 0, 1000},
    {90, 180, 0, 10000},
};

const Pose CURIOUS_SEQ[] = {
    {70, -1, -1, 500},
    {110, -1, -1, 500},
    {90, -1, -1, 500},
};

const Pose HAPPY_SEQ[] = {
    {70, 10, 170, 500},
    {90, 20, 170, 500},
    {70, 10, 160, 500},
    {90, 20, 170, 1500},
    {80, 170, 10, 1500},
};

#define COUNT(a) (sizeof(a) / sizeof(a[0]))

Servo head, leftWing, rightWing;

Behavior currentBehavior = PAUSED;
const Pose* seq = nullptr;
size_t seqLen = 0;
size_t step = 0;
unsigned long nextAt = 0;

void startBehavior(Behavior b)
{
    currentBehavior = b;
    step = 0;
    nextAt = millis(); // apply first pose immediately

    switch (b)
    {
        case IDLE:    seq = IDLE_SEQ;    seqLen = COUNT(IDLE_SEQ);    break;
        case CURIOUS: seq = CURIOUS_SEQ; seqLen = COUNT(CURIOUS_SEQ); break;
        case HAPPY:   seq = HAPPY_SEQ;   seqLen = COUNT(HAPPY_SEQ);   break;
        case PAUSED:  seq = nullptr;     seqLen = 0;                  break;
    }
}

void updateBehavior()
{
    if (!seq) return;
    if ((long)(millis() - nextAt) < 0) return; // not time yet

    const Pose& p = seq[step];
    if (p.head  >= 0) head.write(p.head);
    if (p.left  >= 0) leftWing.write(p.left);
    if (p.right >= 0) rightWing.write(p.right);

    nextAt = millis() + p.holdMs;
    step = (step + 1) % seqLen;
}

void handleSerial()
{
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    Serial.print("Received: ");
    Serial.println(cmd);

    if      (cmd == "HAPPY")   startBehavior(HAPPY);
    else if (cmd == "CURIOUS") startBehavior(CURIOUS);
    else if (cmd == "IDLE")    startBehavior(IDLE);
    else if (cmd == "PAUSED")  startBehavior(PAUSED);
    else Serial.println("Unknown command");
}

void setup()
{
    Serial.begin(115200);
    Serial.setTimeout(50);
    delay(2000);
    Serial.println("Owl Ready");

    head.attach(10);
    leftWing.attach(8);
    rightWing.attach(9);

    // Known starting pose so servos don't twitch to random angles
    head.write(90);
    leftWing.write(180);
    rightWing.write(0);

    startBehavior(PAUSED);
}

void loop()
{
    handleSerial();
    updateBehavior();
}