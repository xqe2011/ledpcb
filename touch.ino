#include <TouchKey.h>
#include "common.h"

extern const LED_TaskInfo irSendStart;
extern const LED_TaskInfo irSendStop;
extern volatile uint8_t clockCompensation;
static uint8_t isIRSendEnable = 0;
static uint32_t irTouchDebounceTime = 0;
static uint32_t lastTouchTickTime = 0;

void Touch_Setup()
{
    TouchKey_begin((1 << 0) | (1 << 1) | (1 << 2));

    // In most cases you don't need to adjust parameters. But if you do, test with TouchKeyTuneParameter example
    /*
      //refer to AN3891 MPR121 Baseline System for more details
      TouchKey_SetMaxHalfDelta(5);      //increase if sensor value are more noisy
      TouchKey_SetNoiseHalfDelta(2);    //If baseline need to adjust at higher rate, increase this value
      TouchKey_SetNoiseCountLimit(10);  //If baseline need to adjust faster, increase this value
      TouchKey_SetFilterDelayLimit(5);  //make overall adjustment slopwer

      TouchKey_SetTouchThreshold(100);  //Bigger touch pad can use a bigger value
      TouchKey_SetReleaseThreshold(80); //Smaller than touch threshold
    */
}

void Touch_Loop()
{
    uint32_t now = millis();
     if (now - lastTouchTickTime > 9) {
        lastTouchTickTime = now;
        clockCompensation += TOUCH_CLOCK_COMPENSATION;
        TouchKey_Process();
        uint8_t result = TouchKey_Get();
        /* 开关红外功能 */
        if (result & (1 << TOUCH_RIGHT_HAND) && result & (1 << TOUCH_LEFT_HAND)) {
            /* 按住3s才触发 */
            if (irTouchDebounceTime == 0) {
                irTouchDebounceTime = now;
            }
            if (now - irTouchDebounceTime < 3000) {
                return;
            }
            isIRSendEnable = !isIRSendEnable;
            /* 播放声音 */
            if (isIRSendEnable) {
                Sound_Play(SOUND_IR_ENABLE);
            } else {
                Sound_Play(SOUND_IR_DISABLE);
            }
            LED_SetIntervalIRSendEnable(isIRSendEnable);
            irTouchDebounceTime = 0;
        } else {
            irTouchDebounceTime = 0;
        }
    }
}
