#include <Preferences.h>
#include "mbedtls/md.h"

// --- Pin Definitions ---
#define BUTTON_PIN 25    
#define LED_PIN 26
#define BUZZER_PIN 27

// --- Moore FSM States ---
enum SystemState { IDLE, ARMED };
SystemState currentState = IDLE;

// --- Global Variables & NVS ---
Preferences vaultPrefs;
int timeoutCount = 0;       
int mismatchCount = 0;      
String masterHash = "";
String pendingPayload = "";
String currentCommand = ""; // Tracks if we are Creating or Verifying
unsigned long armedStartTime = 0;
const unsigned long TIMEOUT_MS = 5000; 

// --- Button Tracking Variables ---
volatile int pressCount = 0;
volatile unsigned long lastPressTime = 0;
volatile unsigned long firstPressTime = 0; 

// --- Hardware Interrupt ---
void IRAM_ATTR isrButton() {
  unsigned long currentTime = millis();
  if (currentTime - lastPressTime > 250) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      if (currentState == ARMED) { 
        pressCount++;
        if (pressCount == 1) {
          firstPressTime = currentTime; 
        }
      }
      lastPressTime = currentTime;
    }
  }
}

// --- mbedTLS Hashing ---
String generateSHA256(String payload) {
  byte shaResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *)payload.c_str(), payload.length());
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);
  
  String hashStr = "";
  for(int i = 0; i < 32; i++) {
    char buf[3];
    sprintf(buf, "%02x", shaResult[i]);
    hashStr += buf;
  }
  return hashStr;
}

void logTimeout() {
  timeoutCount++;
  vaultPrefs.putInt("timeouts", timeoutCount); 
  Serial.printf("ALERT: Total physical timeouts: %d\n", timeoutCount);
}

void logMismatch() {
  mismatchCount++;
  vaultPrefs.putInt("mismatches", mismatchCount); 
  Serial.printf("ALERT: Total password mismatches: %d\n", mismatchCount);
}

void setup() {
  Serial.begin(115200);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), isrButton, FALLING);
  
  vaultPrefs.begin("vault_data", false);
  timeoutCount = vaultPrefs.getInt("timeouts", 0);
  mismatchCount = vaultPrefs.getInt("mismatches", 0);
  masterHash = vaultPrefs.getString("saved_hash", "");
  
  Serial.printf("\n[SYSTEM BOOT] Flash Memory Loaded.\n");
  Serial.printf("- Failed Attempt (Timeouts): %d\n", timeoutCount);
  Serial.printf("- Failed Attempt (Mismatches): %d\n", mismatchCount);
  if(masterHash != "") {
    Serial.println("- Stored Hash Detected.");
  }
  
  resetToIdle();
}

void loop() {
  if (currentState == IDLE) {
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      input.trim(); 
      
      // Sequence Detector Update: Separate CREATE and VERIFY
      if (input.startsWith("CREATE:")) {
        pendingPayload = input.substring(7); 
        currentCommand = "CREATE";
        armSystem();
      } 
      else if (input.startsWith("VERIFY:")) {
        pendingPayload = input.substring(7);
        currentCommand = "VERIFY";
        armSystem();
      }
    }
  }
  
  else if (currentState == ARMED) {
    if (pressCount > 0) {
      // 2 Clicks: Reject
      if (pressCount >= 2) {
        digitalWrite(LED_PIN, LOW);
        Serial.println("ERROR:Rejected - User aborted operation!");
        digitalWrite(BUZZER_PIN, HIGH); delay(1000); 
        digitalWrite(BUZZER_PIN, LOW);
        resetToIdle();
      }
      
      // 1 Click: Evaluate
      else if (millis() - firstPressTime > 1000) {
        if (pressCount == 1) {
          digitalWrite(LED_PIN, LOW); 
          String secureHash = generateSHA256(pendingPayload);
          
          // ==========================================
          // BEHAVIOR A: CREATE NEW PASSWORD
          // ==========================================
          if (currentCommand == "CREATE") {
            vaultPrefs.clear(); // Wipes previous hash and logs
            timeoutCount = 0;
            mismatchCount = 0;
            
            vaultPrefs.putString("saved_hash", secureHash);
            masterHash = secureHash; 
            
            Serial.print("SUCCESS:");
            Serial.println(secureHash);
            triggerSuccessSound();
          } 
          
          // ==========================================
          // BEHAVIOR B: VERIFY EXISTING PASSWORD
          // ==========================================
          else if (currentCommand == "VERIFY") {
            if (masterHash == "") {
              Serial.println("ERROR:No password has been set yet!");
              digitalWrite(BUZZER_PIN, HIGH); delay(1000); digitalWrite(BUZZER_PIN, LOW);
            } 
            else if (secureHash == masterHash) {
              Serial.print("SUCCESS:");
              Serial.println(secureHash);
              triggerSuccessSound();
            } 
            else {
              Serial.println("ERROR:Mismatch - Wrong Password!");
              logMismatch();
              digitalWrite(BUZZER_PIN, HIGH); delay(1000); digitalWrite(BUZZER_PIN, LOW);
            }
          }
          resetToIdle();
        }
      }
    }
    
    // Timeout
    else if (millis() - armedStartTime > TIMEOUT_MS && pressCount == 0) {
      digitalWrite(LED_PIN, LOW); 
      Serial.println("ERROR:Timeout - Physical presence not detected!");
      logTimeout(); 
      digitalWrite(BUZZER_PIN, HIGH); delay(2000); digitalWrite(BUZZER_PIN, LOW);
      resetToIdle();
    }
  }
}

// --- Helper Functions ---
void armSystem() {
  currentState = ARMED;
  armedStartTime = millis();
  pressCount = 0; 
  lastPressTime = millis(); 
  digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, HIGH);
  Serial.println("ARMED: 1 Click = ACCEPT | 2 Clicks = REJECT");
}

void triggerSuccessSound() {
  digitalWrite(BUZZER_PIN, HIGH); delay(100);
  digitalWrite(BUZZER_PIN, LOW);  delay(100);
  digitalWrite(BUZZER_PIN, HIGH); delay(100);
  digitalWrite(BUZZER_PIN, LOW);
}

void resetToIdle() {
  currentState = IDLE;
  pressCount = 0;
  pendingPayload = "";
  currentCommand = "";
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
}