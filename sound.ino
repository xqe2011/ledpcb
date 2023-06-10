#include "common.h"
#include <HardwareSerial.h>

static void Sound_Send(uint8_t cmd, uint8_t* data, size_t dataLength)
{
    Serial0_write(0x7E);
    Serial0_write(dataLength + 3);
    Serial0_write(cmd);
    uint8_t sum = 0;
    for (size_t i = 0; i < dataLength; i++) {
        Serial0_write(data[i]);
        sum += data[i];
    }
    Serial0_write(sum);
    Serial0_write(0xEF);
}

void Sound_Play(uint8_t number)
{
    uint8_t data[4];
    /* 把number转换为数字填充到data */
    data[0] = 30;
    data[1] = 30 + (number % 1000) / 100;
    data[2] = 30 + (number % 100) / 10;
    data[3] = 30 + number % 10;
    Sound_Send(0xA0, data, 4);
}

void Sound_Setup()
{
    Serial0_begin(9600);
    uint8_t data[1];
    /* 扬声器输出 */
    data[0] = 0x00;
    Sound_Send(0xB6, data, 1);
    delay(50);
    /* 不循环播放 */
    data[0] = 0x00;
    Sound_Send(0xAF, data, 1);
    delay(50);
    /* 停止当前播放 */
    Sound_Send(0xAB, data, 0);
    delay(50);
    /* 调整音量 */
    data[0] = SOUND_VOLUME;
    Sound_Send(0xAE, data, 0);
    delay(50);
}

void Sound_Loop()
{
}
