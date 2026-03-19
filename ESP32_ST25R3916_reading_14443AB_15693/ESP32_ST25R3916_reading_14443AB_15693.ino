#include <Arduino.h>
#include <SPI.h> // Include SPI library

// Core RFAL and ST25R3916 Headers (based on stm32duino/ST25R3916 v1.1.0)
#include <rfal_rf.h>         // RF Abstraction Layer core
#include <rfal_nfc.h>        // RFAL NFC Layer (discovery, activation, data exchange)
#include <rfal_nfca.h>       // Include specific types if needed (usually brought in by rfal_nfc.h)
#include <rfal_nfcb.h>       // Include specific types if needed
#include <rfal_nfcv.h>       // Include specific types if needed
#include <rfal_rfst25r3916.h> // RFAL Driver specific for ST25R3916
#include <st_errno.h>        // ST Error Codes

// ESP32-WROOM-32 board SPI pin configuration
// *** IMPORTANT: Verify these pins match your ESP32 board and wiring! ***
const int kPinMOSI = 23;
const int kPinMISO = 19;
const int kPinSCK  = 18;
const int kPinSS   = 5;
const int kPinIRQ  = 4;
const int kPinLED  = 2; // Optional LED for visual indication

// Use VSPI peripheral for SPI communication on ESP32
SPIClass gSPI(VSPI);

// Hardware Abstraction Layer for ST25R3916 using specific SPI and pins
RfalRfST25R3916Class gReaderHardware(&gSPI, kPinSS, kPinIRQ);
// NFC Layer Abstraction using the hardware layer
RfalNfcClass gNFCReader(&gReaderHardware);

// --- Helper function to print UID/PUPI ---
void printNfcID(const char* typeName, uint8_t* nfcid, uint8_t len) {
    Serial.print(typeName);
    Serial.print(" Tag Found! ID: ");
    for (int i = 0; i < len; i++) {
        if (nfcid[i] < 0x10) {
           Serial.print("0"); // Pad with leading zero
        }
        Serial.print(nfcid[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

// Notification Callback Function (Called by RFAL Worker)
// Handles state changes during NFC discovery and activation
void demoNotif(rfalNfcState st)
{
    rfalNfcDevice *nfcDev; // Pointer to store active NFC device info

    Serial.print("Notification Callback - State: ");
    Serial.print(rfalNfcState2Str(st));
    Serial.print(" (");
    Serial.print(st);
    Serial.println(")");


    if (st == RFAL_NFC_STATE_ACTIVATED) {
        // A device has been activated (selected)
        digitalWrite(kPinLED, HIGH); // Turn on LED

        // Get information about the activated device
        gNFCReader.rfalNfcGetActiveDevice(&nfcDev);

        // *** MODIFICATION: Handle different card types ***
        if (nfcDev->type == RFAL_NFC_LISTEN_TYPE_NFCA) {
            printNfcID("ISO 14443A", nfcDev->nfcid, nfcDev->nfcidLen);
        } else if (nfcDev->type == RFAL_NFC_LISTEN_TYPE_NFCB) {
            printNfcID("ISO 14443B", nfcDev->nfcid, nfcDev->nfcidLen); // Usually PUPI, 4 bytes
        } else if (nfcDev->type == RFAL_NFC_LISTEN_TYPE_NFCV) {
            printNfcID("ISO 15693", nfcDev->nfcid, nfcDev->nfcidLen); // Usually UID, 8 bytes
        } else {
            // Optional: Handle other detected tag types if needed
            Serial.print("Detected other tag type: ");
            Serial.println(nfcDev->type, HEX);
            Serial.print("NFCID: ");
             for (int i = 0; i < nfcDev->nfcidLen; i++) {
                 if (nfcDev->nfcid[i] < 0x10) {
                   Serial.print("0");
                 }
                 Serial.print(nfcDev->nfcid[i], HEX);
                 Serial.print(" ");
             }
            Serial.println();
        }

        // Deactivate the device and signal the state machine to return to discovery
        // Use 'true' for continuous reading. This is common for all types here.
        Serial.println("Deactivating device and requesting restart discovery...");
        ReturnCode err = gNFCReader.rfalNfcDeactivate(true); // Use 'true' to restart discovery
         if(err != ERR_NONE) {
              Serial.print("Error during deactivation request: ");
              Serial.println(err);
         } else {
              Serial.println("Device deactivated. Worker will restart discovery.");
         }

        digitalWrite(kPinLED, LOW); // Turn off LED

    } else if (st == RFAL_NFC_STATE_START_DISCOVERY) {
         Serial.println("Discovery loop restarted.");
         digitalWrite(kPinLED, LOW);
    } else if (st == RFAL_NFC_STATE_WAKEUP_MODE) {
         Serial.println("Wake Up mode started.");
    } else if (st == RFAL_NFC_STATE_POLL_TECHDETECT) {
         Serial.println("Polling for devices...");
    }
    // Add other state handling if needed
}


void setup() {
    Serial.begin(115200);
    while(!Serial);
    // *** MODIFICATION: Update title ***
    Serial.println("ESP32 - ST25R3916 Multi-Protocol Reader (A/B/V) - Continuous Mode");
    Serial.println("Based on RFAL from stm32duino/ST25R3916 v1.1.0");
    Serial.println("WARNING: Using STM32-focused libraries on ESP32. Check compatibility.");

    // Print configured pins
    Serial.print("MOSI: "); Serial.println(kPinMOSI);
    Serial.print("MISO: "); Serial.println(kPinMISO);
    Serial.print("SS:   "); Serial.println(kPinSS);
    Serial.print("SCK:  "); Serial.println(kPinSCK);
    Serial.print("IRQ:  "); Serial.println(kPinIRQ);
    Serial.print("LED:  "); Serial.println(kPinLED);

    // Init GPIO
    pinMode(kPinLED, OUTPUT);
    digitalWrite(kPinLED, LOW);
    pinMode(kPinSS, OUTPUT);
    digitalWrite(kPinSS, HIGH);

    // Init SPI
    gSPI.begin(kPinSCK, kPinMISO, kPinMOSI, kPinSS);

    // Initialize the NFC reader stack
    Serial.print("Initializing NFC Reader stack... ");
    ReturnCode init_err = gNFCReader.rfalNfcInitialize();
    if (init_err == ERR_NONE) {
        Serial.println("Initialization successful.");
    } else {
        Serial.print("Initialization failed with error code: ");
        Serial.println(init_err);
        while (1) { delay(100); } // Halt on failure
    }

    // Configure NFC discovery parameters
    rfalNfcDiscoverParam discover_params;
    memset(&discover_params, 0, sizeof(discover_params));
    discover_params.compMode = RFAL_COMPLIANCE_MODE_NFC;
    discover_params.devLimit = 1;
    discover_params.nfcfBR = RFAL_BR_212; // NFC-F specific, keep default
    discover_params.ap2pBR = RFAL_BR_424; // P2P specific, keep default

    // *** MODIFICATION: Set technologies to search for: A, B, and V ***
    discover_params.techs2Find = ( RFAL_NFC_POLL_TECH_A | RFAL_NFC_POLL_TECH_B | RFAL_NFC_POLL_TECH_V );

    discover_params.GBLen = RFAL_NFCDEP_GB_MAX_LEN;
    discover_params.notifyCb = demoNotif;
    discover_params.totalDuration = 1000U; // Adjust duration if needed for multi-protocol search
    discover_params.wakeupEnabled = false; // Continuous polling
    discover_params.wakeupConfigDefault = true;

    // Start discovery using the RfalNfcClass wrapper method
    // *** MODIFICATION: Update print message ***
    Serial.print("Starting NFC Discovery (Polling for ISO14443A/B, ISO15693)... ");
    ReturnCode disc_err = gNFCReader.rfalNfcDiscover(&discover_params);
     if (disc_err == ERR_NONE) {
        Serial.println("Discovery started successfully.");
    } else {
        Serial.print("Failed to start discovery with error code: ");
        Serial.println(disc_err);
        while (1) { delay(100); } // Halt on failure
    }
}

void loop() {
    // Continuously run the RfalNfcClass worker function.
    gNFCReader.rfalNfcWorker();
    // delay(10); // Optional delay, probably not needed
}

// Helper function to convert RFAL NFC state enum to string
const char* rfalNfcState2Str(rfalNfcState st) {
    switch(st) {
        case RFAL_NFC_STATE_NOTINIT: return "NOTINIT";
        case RFAL_NFC_STATE_START_DISCOVERY: return "START_DISCOVERY";
        case RFAL_NFC_STATE_WAKEUP_MODE: return "WAKEUP_MODE";
        case RFAL_NFC_STATE_POLL_TECHDETECT: return "POLL_TECHDETECT";
        case RFAL_NFC_STATE_POLL_COLAVOIDANCE: return "POLL_COLAVOIDANCE";
        case RFAL_NFC_STATE_POLL_SELECT: return "POLL_SELECT";
        case RFAL_NFC_STATE_POLL_ACTIVATION: return "POLL_ACTIVATION";
        case RFAL_NFC_STATE_LISTEN_TECHDETECT: return "LISTEN_TECHDETECT";
        case RFAL_NFC_STATE_LISTEN_COLAVOIDANCE: return "LISTEN_COLAVOIDANCE";
        case RFAL_NFC_STATE_LISTEN_ACTIVATION: return "LISTEN_ACTIVATION";
        case RFAL_NFC_STATE_LISTEN_SLEEP: return "LISTEN_SLEEP";
        case RFAL_NFC_STATE_ACTIVATED: return "ACTIVATED";
        case RFAL_NFC_STATE_DATAEXCHANGE: return "DATAEXCHANGE";
        default: return "UNKNOWN";
    }
}