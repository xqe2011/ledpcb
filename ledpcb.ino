#include "common.h"

void setup() {
    LED_Setup();
    Touch_Setup();
    Sound_Setup();
    IR_Setup();
    USBSerial_println("Welcome to NuoZi Nya LED System!");
    USBSerial_println("Author: wk, 2011");
}

void loop() {
    LED_Loop();
    Touch_Loop();
    Sound_Loop();
    IR_Loop();
}
