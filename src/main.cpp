#include <Arduino.h>
#include <Servo.h>

enum Behavior { PAUSED, IDLE, CURIOUS, HAPPY };

void idle();
void curious();
void happy();
void paused();

Servo head;
Servo leftWing;
Servo rightWing;

Behavior currentBehavior = PAUSED;

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    head.attach(10);
    leftWing.attach(8);
    rightWing.attach(9);
}

void loop()
{
    switch(currentBehavior)
    {
        case IDLE:
            idle();
            break;

        case CURIOUS:
            curious();
            break;

        case HAPPY:
            happy();
            break;

        case PAUSED:
            paused();
            break;
    }
}

//Functions
void curious()
{
    head.write(70);
    delay(500);

    head.write(110);
    delay(500);

    head.write(90);
    delay(500);
}

void happy()
{
    leftWing.write(10);
    rightWing.write(170);
    head.write(70);
    delay(500);

    leftWing.write(20);
    rightWing.write(170);
    head.write(90);
    delay(500);

    leftWing.write(10);
    rightWing.write(160);
    head.write(70);
    delay(500);

    leftWing.write(20);
    rightWing.write(170);
    head.write(90);
    delay(1500);

    leftWing.write(170);
    rightWing.write(10);
    head.write(80);
    delay(1500);

}

void paused()
{
    head.write(80);
    leftWing.write(180);
    rightWing.write(0);

    delay(1000);
}

void idle()
{
    leftWing.write(180);
    rightWing.write(0);

    head.write(85);
    delay(1000);

    head.write(95);
    delay(1000);

    head.write(90);
    delay(10000);
}