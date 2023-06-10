#ifndef CONFIG_H__
#define CONFIG_H__

#define LED_NUM 5
#define LED_MAX_POINTS 5
#define LED_PIN 35
#define LED_PIN_SEND_FUNCTION neopixel_show_P3_5
#define LED_BRIGHTNESS 64

#define TOUCH_EARS 2
#define TOUCH_LEFT_HAND 1
#define TOUCH_RIGHT_HAND 0

#define IR_SEND_PIN 34
#define IR_RECV_PIN 33
#define IR_RECV_PIN_INT_NUM 1
#define IR_DATA_SOF 0xA1

#define SOUND_VOLUME 31
#define SOUND_IR_ENABLE 1
#define SOUND_IR_DISABLE 2


#endif