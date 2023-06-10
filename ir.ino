#include "common.h"

#define IR_BYTES_COUNT 4

static uint16_t timerCount = 0;
static uint8_t receivingFlag = 0;
static uint8_t isReceived = 0;
static uint8_t receivedBitNum = 0;
static uint8_t receivedData[IR_BYTES_COUNT] = {0};
static uint16_t sendIntervals[3 + IR_BYTES_COUNT * 8];
static uint8_t sendIntervalNum = 0;
static uint8_t sendCountdown = 0;
static uint8_t sendPinFlag = 0;

#pragma save
#pragma nooverlay
void recvPinCallback() {
    uint16_t timerCount38k = timerCount / 2;
    /* 软件读出上一帧数据后再开始解析前导码 */
    if (!isReceived && timerCount38k > 462 && timerCount38k < 564) {
        receivingFlag = 1;
        isReceived = 0;
        receivedBitNum = 0;
    } else if (receivingFlag) {
        if (timerCount38k > 78 && timerCount38k < 94) {
            receivedData[receivedBitNum / 8] |= 1 << (receivedBitNum % 8);
        } else if (timerCount38k > 39 && timerCount38k < 47){
            receivedData[receivedBitNum / 8] &= ~(1 << (receivedBitNum % 8));
        } else {
            /* 无效数据丢弃 */
            receivingFlag = 0;
            receivedBitNum = 0;
            return;
        }
        receivedBitNum++;
        /* 接受完成，通知应用层 */
        if (receivedBitNum == 32) {
            receivingFlag = 0;
            isReceived = 1;
        }
    }
    timerCount = 0;
}
#pragma restore

void Timer2Interrupt(void) __interrupt (INT_NO_TMR2)
{
    TF2 = 0;
    timerCount++;
    if (sendCountdown != 0) sendCountdown--;
    if (sendIntervalNum != sizeof(sendIntervals) && sendCountdown == 0) {
        sendCountdown = sendIntervals[sendIntervalNum++];
        sendPinFlag = !sendPinFlag;
        digitalWrite(IR_SEND_PIN, sendPinFlag);
    }
}

void IR_Send(uint8_t* data)
{
    /* 先关闭中断 */
    EA = 0;
    /* 前导码 */
    sendIntervals[0] = 342;
    sendIntervals[1] = 171;
    uint8_t i = 0;
    for (i = 0; i < IR_BYTES_COUNT * 8; i++) {
        sendIntervals[2 * i + 2] = 21;
        sendIntervals[2 * i + 3] = data[i / 8] & (1 << (i % 8)) ? 64 : 21;
    }
    sendIntervals[2 * i + 4] = 40;
    EA = 1;
}

void IR_Setup()
{
    /* 初始化红外接收中断 */
    pinMode(IR_RECV_PIN, INPUT_PULLUP);
    pinMode(IR_SEND_PIN, OUTPUT);
    digitalWrite(IR_SEND_PIN, 0);
    attachInterrupt(IR_SEND_PIN_INT_NUM, recvPinCallback, FALLING);
    /* 初始化定时器，使用定时器2的计数模式 */
    T2MOD |= 0x40;
	TH2 = 0x00;
	TL2 = 0x00;
    RCAP2H = 0xFF;
    RCAP2L = 0xB1;
    ET2 = 1;
    TR2 = 1;
}

uint8_t tmpData[] = {0xA1, 0xA2, 0xA3, 0xA4};
void IR_Loop()
{
    if (isReceived) {
        isReceived = 0;
        USBSerial_print("Received: 0x");
        USBSerial_print(receivedData[0], 16);
        USBSerial_print(receivedData[1], 16);
        USBSerial_print(receivedData[2], 16);
        USBSerial_print(receivedData[3], 16);
        USBSerial_println("");
    }
    if (USBSerial_available()) {
        uint8_t c = USBSerial_read();
        if (c == 's') {
            IR_Send(tmpData);
        }
    }
}