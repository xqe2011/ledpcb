#include "common.h"

#define IR_BYTES_COUNT 4

static uint16_t timerCount = 0;
static uint8_t receivingFlag = 0;
static uint8_t isReceived = 0;
static uint8_t receivedBitNum = 0;
static uint8_t receivedData[IR_BYTES_COUNT] = {0};
static uint16_t sendIntervals[4 + IR_BYTES_COUNT * 8 * 2];
static uint8_t sendIntervalNum = 0;
static uint16_t sendCountdown = 0;
static uint8_t sendLevelFlag = 0;
volatile uint8_t ledClockCompensationFlag = 0;

uint16_t debugPrintBuffer[128] = {0};
uint8_t debugPrintCount;
uint32_t debugOutputTime = 0;

#pragma save
#pragma nooverlay
void recvPinCallback() {
    uint16_t timerCount38k = timerCount / 2;
    if (timerCount38k < 1000) {
        debugPrintBuffer[debugPrintCount++] = timerCount38k;
    }
    /* 软件读出上一帧数据后再开始解析前导码 */
    if (!isReceived && timerCount38k > 462 && timerCount38k < 564) {
        receivingFlag = 1;
        isReceived = 0;
        receivedBitNum = 0;
    } else if (receivingFlag) {
        if (timerCount38k > 70 && timerCount38k < 102) {
            receivedData[receivedBitNum / 8] |= 1 << (receivedBitNum % 8);
        } else if (timerCount38k > 35 && timerCount38k < 51){
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
    /* LED时钟补偿 */
    if (ledClockCompensationFlag) {
        timerCount += IR_LED_CLOCK_COMPENSATION;
        if (sendCountdown >= IR_LED_CLOCK_COMPENSATION) {
            sendCountdown -= IR_LED_CLOCK_COMPENSATION;
        } else {
            sendCountdown = 0;
        }
        ledClockCompensationFlag = 0;
    }
    timerCount++;
    if (sendCountdown != 0) sendCountdown--;
    /* 禁止自发自收 */
    if (sendIntervalNum != (sizeof(sendIntervals) / sizeof(sendIntervals[0]))) {
        if (sendCountdown == 0) {
            sendCountdown = sendIntervals[sendIntervalNum++];
            sendLevelFlag = !sendLevelFlag;
        }
    } else {
        IR_RECV_PIN_IT_REG = 1;
    }
    digitalWrite(IR_SEND_PIN, sendLevelFlag && (sendCountdown & 0x01));
}

void IR_Send(uint8_t* data)
{
    USBSerial_println("Sent");
    /* 先关闭中断 */
    EA = 0;
    /* 前导码，下面的常数是计算了程序运行时间的 */
    sendIntervals[0] = 655;
    sendIntervals[1] = 327;
    uint8_t i = 0;
    for (i = 0; i < IR_BYTES_COUNT * 8; i++) {
        sendIntervals[2 * i + 2] = 43;
        sendIntervals[2 * i + 3] = data[i / 8] & (0x01 << (i % 8)) ? 125 : 43;
    }
    /* 循环跳出后仍会执行一次加法 */
    i -= 1;
    sendIntervals[2 * i + 4] = 79;
    sendIntervals[2 * i + 5] = 0;
    sendCountdown = sendIntervals[0];
    sendIntervalNum = 1;
    /* 注意由于我们需要进行位翻转，所以这里初始值应该位0或0xFF */
    sendLevelFlag = 0xFF;
    digitalWrite(IR_SEND_PIN, 0);
    IR_RECV_PIN_IT_REG = 0;
    EA = 1;
}

void IR_Setup()
{
    /* 初始化红外接收中断 */
    pinMode(IR_RECV_PIN, INPUT_PULLUP);
    pinMode(IR_SEND_PIN, OUTPUT);
    digitalWrite(IR_SEND_PIN, 0);
    attachInterrupt(IR_RECV_PIN_INT_NUM, recvPinCallback, FALLING);
    /* 初始化定时器，使用定时器2的计数模式 */
    T2MOD |= 0x40;
	TH2 = 0x00;
	TL2 = 0x00;
    RCAP2H = 0xFF;
    RCAP2L = 0xB1;
    ET2 = 1;
    TR2 = 1;
}

static void IR_NoticeLED()
{
    extern const LED_TaskInfo task1;
    if (receivedData[0] != 0xA1) return;
    uint16_t time = (receivedData[2] << 8) | receivedData[3];
    switch (receivedData[1]) {
        case 1:
            LED_ChangeTask(&task1, time);
            break;
        default:
            USBSerial_println("Cannot recognize IR LED command");
            return;
    }
    USBSerial_print("Synced LED with Task: ");
    USBSerial_print(receivedData[1]);
    USBSerial_print(", time: ");
    USBSerial_println(time);
}

uint8_t tmpData[] = {0xA1, 0xA2, 0xA3, 0xA4};
void IR_Loop()
{
    uint32_t now = millis();
    if (debugOutputTime - now > 1000) {
        debugOutputTime = now;
        if (debugPrintCount > 0) {
            USBSerial_println("Debug:");
            for (uint8_t i = 0; i < debugPrintCount; i++) {
                USBSerial_print(debugPrintBuffer[i]);
                USBSerial_print(" ");
            }
            USBSerial_println("");
            debugPrintCount = 0;
        }
    }
    if (isReceived) {
        USBSerial_print("Received: 0x");
        USBSerial_print(receivedData[0], 16);
        USBSerial_print(receivedData[1], 16);
        USBSerial_print(receivedData[2], 16);
        USBSerial_print(receivedData[3], 16);
        USBSerial_println("");
        IR_NoticeLED();
        isReceived = 0;
    }
    if (USBSerial_available()) {
        uint8_t c = USBSerial_read();
        if (c == 's') {
            IR_Send(tmpData);
        }
    }
}