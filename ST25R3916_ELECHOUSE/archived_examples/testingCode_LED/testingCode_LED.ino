#include <Arduino.h>
#include <SPI.h>                // SPI 库
#include <rfal_rf.h>            // RFAL 核心
#include <rfal_nfc.h>           // RFAL NFC 层
#include <rfal_nfca.h>          // ISO14443A 支持
#include <rfal_nfcb.h>          // ISO14443B 支持
#include <rfal_nfcv.h>          // ISO15693 支持
#include <rfal_rfst25r3916.h>   // ST25R3916 驱动
#include <st_errno.h>           // 错误码

// SPI 和引脚定义
const int kPinMOSI = 23;
const int kPinMISO = 19;
const int kPinSCK  = 18;
const int kPinSS   = 5;
const int kPinIRQ  = 4;
const int kPinLED  = 2;    // 感应到卡时常亮的 LED

SPIClass gSPI(0);
RfalRfST25R3916Class gReaderHardware(&gSPI, kPinSS, kPinIRQ);
RfalNfcClass gNFCReader(&gReaderHardware);

// 卡片在位标志
volatile bool cardPresent = false;

// 前置声明
const char*      rfalNfcState2Str(rfalNfcState st);
void             printNfcID(const char* typeName, uint8_t* nfcid, uint8_t len);
void             demoNotif(rfalNfcState st);

void setup() {
  Serial.begin(115200);
  while(!Serial);

  // LED 和 SPI 片选引脚初始化
  pinMode(kPinLED, OUTPUT);
  digitalWrite(kPinLED, LOW);
  pinMode(kPinSS, OUTPUT);
  digitalWrite(kPinSS, HIGH);

  // 初始化 VSPI
  gSPI.begin(kPinSCK, kPinMISO, kPinMOSI, kPinSS);

  Serial.println("Initializing ST25R3916 NFC reader...");
  if (gNFCReader.rfalNfcInitialize() != ERR_NONE) {
    Serial.println("ERROR: NFC init failed");
    while (1) { delay(100); }
  }

  // 配置发现参数
  rfalNfcDiscoverParam dp;
  memset(&dp, 0, sizeof(dp));
  dp.compMode      = RFAL_COMPLIANCE_MODE_NFC;
  dp.devLimit      = 1;
  dp.techs2Find    = RFAL_NFC_POLL_TECH_A 
                   | RFAL_NFC_POLL_TECH_B 
                   | RFAL_NFC_POLL_TECH_V;
  dp.notifyCb      = demoNotif;
  dp.totalDuration = 0;        // 连续模式
  dp.wakeupEnabled = false;

  if (gNFCReader.rfalNfcDiscover(&dp) != ERR_NONE) {
    Serial.println("ERROR: Discovery start failed");
    while (1) { delay(100); }
  }
}

void loop() {
  // 推动 RFAL 状态机
  gNFCReader.rfalNfcWorker();

  // 根据卡片状态，常亮或熄灭 LED
  digitalWrite(kPinLED, cardPresent ? HIGH : LOW);
}

// 回调：处理状态变化
void demoNotif(rfalNfcState st)
{
  rfalNfcDevice *nfcDev;
  Serial.print("[CB] State: ");
  Serial.print(rfalNfcState2Str(st));
  Serial.print(" ("); Serial.print(st); Serial.println(")");

  if (st == RFAL_NFC_STATE_ACTIVATED) {
    // 卡片激活：打印 ID，并设置常亮标志
    gNFCReader.rfalNfcGetActiveDevice(&nfcDev);
    if (nfcDev->type == RFAL_NFC_LISTEN_TYPE_NFCA) {
      printNfcID("ISO 14443A", nfcDev->nfcid, nfcDev->nfcidLen);
    }
    else if (nfcDev->type == RFAL_NFC_LISTEN_TYPE_NFCB) {
      printNfcID("ISO 14443B", nfcDev->nfcid, nfcDev->nfcidLen);
    }
    else if (nfcDev->type == RFAL_NFC_LISTEN_TYPE_NFCV) {
      printNfcID("ISO 15693", nfcDev->nfcid, nfcDev->nfcidLen);
    } else {
      Serial.print("Other type "); Serial.println(nfcDev->type, HEX);
    }

    cardPresent = true;

    // 请求去激活并重启发现，以便收到 START_DISCOVERY
    ReturnCode err = gNFCReader.rfalNfcDeactivate(true);
    if (err != ERR_NONE) {
      Serial.print("Deactivate error: "); Serial.println(err);
    }
  }
  else if (st == RFAL_NFC_STATE_START_DISCOVERY) {
    // 发现循环重启：卡片离开，清除常亮标志
    cardPresent = false;
    Serial.println("[CB] Card removed, discovery restarted.");
  }
}

// 打印 UID/PUPI 辅助
void printNfcID(const char* typeName, uint8_t* nfcid, uint8_t len) {
  Serial.print(typeName);
  Serial.print(" Tag Found! ID: ");
  for (uint8_t i = 0; i < len; i++) {
    if (nfcid[i] < 0x10) Serial.print("0");
    Serial.print(nfcid[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

// 状态枚举转字符串
const char* rfalNfcState2Str(rfalNfcState st) {
  switch (st) {
    case RFAL_NFC_STATE_NOTINIT:             return "NOTINIT";
    case RFAL_NFC_STATE_START_DISCOVERY:     return "START_DISCOVERY";
    case RFAL_NFC_STATE_WAKEUP_MODE:         return "WAKEUP_MODE";
    case RFAL_NFC_STATE_POLL_TECHDETECT:     return "POLL_TECHDETECT";
    case RFAL_NFC_STATE_POLL_COLAVOIDANCE:   return "POLL_COLAVOIDANCE";
    case RFAL_NFC_STATE_POLL_SELECT:         return "POLL_SELECT";
    case RFAL_NFC_STATE_POLL_ACTIVATION:     return "POLL_ACTIVATION";
    case RFAL_NFC_STATE_LISTEN_TECHDETECT:   return "LISTEN_TECHDETECT";
    case RFAL_NFC_STATE_LISTEN_COLAVOIDANCE: return "LISTEN_COLAVOIDANCE";
    case RFAL_NFC_STATE_LISTEN_ACTIVATION:   return "LISTEN_ACTIVATION";
    case RFAL_NFC_STATE_LISTEN_SLEEP:        return "LISTEN_SLEEP";
    case RFAL_NFC_STATE_ACTIVATED:           return "ACTIVATED";
    case RFAL_NFC_STATE_DATAEXCHANGE:        return "DATAEXCHANGE";
    default:                                 return "UNKNOWN";
  }
}
