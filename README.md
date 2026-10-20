# 🛡️ Hardware-Isolated Security Vault

<p align="center">
  <img src="esp32-board.png" alt="ESP32 Hardware Security Vault Board" width="380" />
</p>

<p align="center">
  <strong>An air-gapped cryptographic hardware security token featuring physical button 2FA presence verification, on-chip mbedTLS SHA-256 acceleration, non-volatile flash persistence, and a real-time Web Serial API dashboard.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32%20DevKit%20V1-E7352C?style=for-the-badge&logo=espressif&logoColor=white" alt="ESP32" />
  <img src="https://img.shields.io/badge/Language-C++%20/%20Arduino-00979D?style=for-the-badge&logo=arduino&logoColor=white" alt="Arduino" />
  <img src="https://img.shields.io/badge/Crypto-mbedTLS%20SHA--256-blue?style=for-the-badge&logo=lock&logoColor=white" alt="mbedTLS" />
  <img src="https://img.shields.io/badge/Interface-Web%20Serial%20API-F7C255?style=for-the-badge&logo=googlechrome&logoColor=black" alt="Web Serial" />
  <img src="https://img.shields.io/badge/Storage-ESP32%20NVS%20Flash-4B8BBE?style=for-the-badge" alt="NVS Storage" />
  <img src="https://img.shields.io/badge/Security-Physical%202FA%20Airgap-brightgreen?style=for-the-badge" alt="Physical 2FA" />
</p>

---

## 📌 Executive Summary

Traditional software password managers and browser-based authenticators remain vulnerable to host-level compromises: keyloggers, remote access trojans (RATs), malicious browser extensions, and memory scraper tools. If the host operating system is compromised, in-memory cryptographic keys and passwords can be extracted silently without user awareness.

The **Hardware-Isolated Security Vault** solves this problem by moving the root of trust out of the host PC and into an **isolated microcontroller silicon boundary (ESP32)**. 

### Key Tenet: Zero-Trust Host Architecture
* **Physical Proof of Presence (2FA)**: Sensitive operations cannot be triggered silently by background malware. Every cryptographic commit or verification requires an operator to depress an onboard tactile button (`GPIO 25` interrupt).
* **100% On-Chip Cryptography**: SHA-256 hashing is executed strictly inside ESP32 hardware via `mbedTLS`. The host browser never sees, generates, or validates hashes.
* **Autonomous Hardware Verdict**: Verification decisions (ACCEPT or REJECT) are rendered solely by the ESP32 Moore Finite State Machine (FSM). The web application is purely a display terminal and cannot override or spoof authorization.
* **On-Chip NVS Persistence**: Passwords are saved as cryptographically strong hashes directly into ESP32 Non-Volatile Storage (`vault_data` partition). Zero data is cached in cookies, local storage, or host disks.

---

## 🧠 System Architecture

The architecture decouples the untrusted host workstation from the trusted hardware execution environment:

```mermaid
flowchart TD
    subgraph Host ["💻 Host Workstation (Untrusted Zone)"]
        UI["Cyberpunk Web Dashboard\n(vault_1.html / index.html)"]
        WSerial["Browser Web Serial Driver\n(115200 Baud 8N1)"]
        UI <-->|JSON/UART Strings| WSerial
    end

    subgraph Hardware ["🔒 ESP32 Security Vault (Trusted Silicon Zone)"]
        UART["Hardware UART Controller\n(Serial0 @ 115200)"]
        FSM["Moore Finite State Machine\n(IDLE / ARMED Engine)"]
        ISR["Hardware Interrupt Handler\n(GPIO 25 Tactile Button)"]
        CRYPTO["mbedTLS Hardware Engine\n(SHA-256 Cipher Unit)"]
        NVS["Flash Memory (Preferences)\n['vault_data' Partition]"]
        PERIPH["Feedback Peripherals\n(GPIO 26 LED & GPIO 27 Buzzer)"]
        
        WSerial <==== USB Serial Connection ====> UART
        UART -->|CREATE / VERIFY| FSM
        ISR -->|Single/Double Click Event| FSM
        FSM -->|Compute Digest| CRYPTO
        CRYPTO -->|Query / Save Master Hash| NVS
        FSM -->|Status Telemetry & Tones| PERIPH
        FSM -->|SUCCESS:hash / ERROR:msg| UART
    end
```

---

## ⚙️ Moore Finite State Machine (FSM)

The embedded firmware runs a deterministic Moore state machine governing access control and hardware authorization:

```mermaid
stateDiagram-v2
    [*] --> IDLE : Power On / Flash Loaded

    state IDLE {
        description: System Standby (LED OFF, Buzzer OFF)
    }

    state ARMED {
        description: LED ON, 100ms Chirp, 5000ms Countdown Timer Active
    }

    IDLE --> ARMED : Rx "CREATE:<payload>" or "VERIFY:<payload>"

    state EvaluateClick <<choice>>
    ARMED --> EvaluateClick : Operator Presses Physical Button

    EvaluateClick --> SUCCESS_ACTION : 1 Click detected (> 1000ms window)
    EvaluateClick --> ABORT_ACTION : 2 Clicks detected (Emergency Abort)
    
    ARMED --> TIMEOUT_ACTION : Inactivity Timer > 5000ms (0 clicks)

    state SUCCESS_ACTION {
        description: Run mbedTLS SHA-256
        sub_create: If CREATE -> Clear NVS, Store Hash, Reset Counters
        sub_verify: If VERIFY -> Compare Hash with NVS Master Hash
    }

    state ABORT_ACTION {
        description: Tx "ERROR:Rejected - User aborted operation!"\n1000ms Buzzer Alert
    }

    state TIMEOUT_ACTION {
        description: Tx "ERROR:Timeout - Physical presence not detected!"\nLog Timeout in NVS | 2000ms Siren
    }

    SUCCESS_ACTION --> IDLE : Double Chirp & SUCCESS Tx -> Reset
    ABORT_ACTION --> IDLE : Reset to Standby
    TIMEOUT_ACTION --> IDLE : Reset to Standby
```

### Physical Operator Gesture Mapping

| Action / Gesture | Physical Response | Hardware Reaction & Outcome |
| :--- | :--- | :--- |
| **1 Click** | Button pressed once, confirmed after 1.0s window | **ACCEPT / COMMIT**: Firmware generates SHA-256 digest. Writes to NVS (if `CREATE`) or evaluates against stored hash (if `VERIFY`). Emits double success chirp. |
| **2 Clicks** | Button double-clicked within armed window | **REJECT / ABORT**: Firmware immediately halts execution, disarms, fires a 1-second warning buzzer tone, and informs host that the operator aborted. |
| **0 Clicks (Timeout)** | No button press detected within 5 seconds | **TIMEOUT / ANTI-BOT ALARM**: Triggers a 2-second continuous siren alarm, increments non-volatile `timeouts` audit log, and aborts transaction. |

---

## 🔌 Hardware Specifications & Pinout

### Bill of Materials (BOM)

| Component | Quantity | Description / Role |
| :--- | :---: | :--- |
| **ESP32 DevKit V1** | 1 | 32-bit Dual-Core Xtensa LX6, built-in Wi-Fi/BT, Hardware Cryptographic Engine |
| **Tactile Pushbutton** | 1 | Physical presence input (Active LOW with internal pull-up) |
| **Green / Yellow 5mm LED** | 1 | "ARMED" visual status indicator |
| **Current Limiting Resistor** | 1 | 220Ω – 330Ω (for LED protection) |
| **5V Active Piezo Buzzer** | 1 | Audio feedback for state arming, alerts, success tones, and alarms |
| **Solderless Breadboard** | 1 | Prototyping workspace |
| **Jumper Wires** | 6–8 | Male-to-Male connections |
| **Micro-USB / USB-C Cable** | 1 | High-speed data cable for power & UART communication |

### Complete Pin Mapping Table

| Peripheral | ESP32 GPIO Pin | Mode | Connection Detail |
| :--- | :---: | :---: | :--- |
| **Tactile Button (2FA)** | `GPIO 25` | `INPUT_PULLUP` | Connect one terminal to `GPIO 25`, other terminal to `GND`. Interrupt triggered on `FALLING` edge. |
| **Visual LED Indicator** | `GPIO 26` | `OUTPUT` | Connect Anode (+) to `GPIO 26` through a 220Ω resistor; Cathode (-) to `GND`. |
| **Piezo Audio Buzzer** | `GPIO 27` | `OUTPUT` | Connect Positive (+) lead to `GPIO 27`; Negative (-) lead to `GND`. |
| **UART0 Transmit (TX)** | `GPIO 1 (TX0)` | Serial Tx | Standard USB-UART bridge (CP2102/CH340) onboard. |
| **UART0 Receive (RX)** | `GPIO 3 (RX0)` | Serial Rx | Standard USB-UART bridge (CP2102/CH340) onboard. |
| **Common Ground** | `GND` | Ground | Shared ground rail across all peripherals. |

---

## 📡 Serial Communication Protocol (UART 115200 8N1)

Communication between the Web Serial Dashboard and the ESP32 occurs over high-speed full-duplex UART at **115200 baud, 8 data bits, no parity, 1 stop bit (8N1)**.

### Commands (Host ➔ ESP32)

| Command Format | Parameters | Description |
| :--- | :--- | :--- |
| `CREATE:<plaintext>\n` | `<plaintext>` = Password string | Requests the vault to enter `ARMED` state to encrypt and commit a new master credential. |
| `VERIFY:<plaintext>\n` | `<plaintext>` = Password string | Requests the vault to enter `ARMED` state to verify candidate password against hardware NVS storage. |

### Responses (ESP32 ➔ Host)

| Response Prefix | Example Payload | Meaning |
| :--- | :--- | :--- |
| `ARMED:` | `ARMED: 1 Click = ACCEPT \| 2 Clicks = REJECT` | Vault has entered armed state; awaiting tactile interaction. |
| `SUCCESS:<hash>` | `SUCCESS:2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c...` | Operation confirmed by physical button press; returns the on-chip calculated SHA-256 hash. |
| `ERROR:<reason>` | `ERROR:Mismatch - Wrong Password!` | Verification failed: Candidate hash does not match stored master hash. |
| `ERROR:<reason>` | `ERROR:Timeout - Physical presence not detected!` | 5000ms elapsed without user presence confirmation. |
| `ERROR:<reason>` | `ERROR:Rejected - User aborted operation!` | Operator double-clicked the button to intentionally decline. |
| `ERROR:<reason>` | `ERROR:No password has been set yet!` | Verify was called before any password was initialized in flash. |
| `ALERT:` | `ALERT: Total physical timeouts: 4` | Telemetry log regarding persistent security audit counts. |

---

## 🔒 Security & Cryptographic Mechanisms

```
+---------------------------------------------------------------------------------+
|                               ESP32 SILICON BOUNDARY                            |
|                                                                                 |
|   +-------------------+      +-------------------+      +-------------------+   |
|   |   Raw Password    | ---> |  mbedTLS SHA-256  | ---> |   64-Char Digest  |   |
|   |  (Volatile RAM)   |      |  Hardware Engine  |      |  (Cleaned Post-Op)|   |
|   +-------------------+      +-------------------+      +-------------------+   |
|                                                                   |             |
|                                                                   v             |
|                                                       +---------------------+   |
|                                                       |  NVS Flash Storage  |   |
|                                                       |    "vault_data"     |   |
|                                                       |  [Key: saved_hash]  |   |
|                                                       +---------------------+   |
+---------------------------------------------------------------------------------+
```

1. **Hardware-Accelerated SHA-256 (`mbedtls/md.h`)**:
   Instead of basic software hashing, the firmware leverages the ESP32's native cryptographic hardware pipeline via the ESP-IDF `mbedtls` cryptographic library.
2. **Flash Memory Security (`Preferences.h`)**:
   - The master digest is stored under the non-volatile `"vault_data"` namespace.
   - Values persist across complete power cuts and restarts.
   - When a new password is created, `vaultPrefs.clear()` wipes all previously saved hashes and telemetry records to prevent stale metadata retention.
3. **Hardware Anti-Tamper & Brute-Force Auditing**:
   - Every timeout is logged into non-volatile storage (`timeouts` key).
   - Every password mismatch increments `mismatches` in flash.
   - At system boot, the ESP32 dumps these counter values to the console so operators immediately detect if unauthorized attempts were made while they were away.
4. **Debounced Hardware Interrupt (`IRAM_ATTR isrButton`)**:
   - Configured with a 250ms software debounce lock to eliminate mechanical contact chatter.
   - High-priority interrupt service routine placed in IRAM ensures instantaneous responsiveness regardless of CPU workload.

---

## 💻 Web Dashboard (Zero-Driver Cyberpunk GUI)

The project includes a dashboard (`vault_1.html` / `index.html`) featuring:
* **Direct Web Serial Interface**: Connects directly to `/dev/ttyUSB0` or `COMx` natively from Chrome/Edge without installing serial drivers or middleware servers.
* **Dual-Pane View Architecture**:
  * **View 1 (Home Page)**: System status, feature telemetry tags, architectural overview, and hardware gateway pairing.
  * **View 2 (Main Page)**: Interactive cryptographic controls with password strength toggles, real-time status badges, and an integrated serial telemetry console.
* **Dynamic Terminal Console**: Displays real-time UART logs with millisecond timestamps, highlighting cryptographic events, hardware timeouts, and authentication verdicts in retro phosphor green and amber.

---

## 🚀 Getting Started

### 1. Hardware Assembly

Wire your components to the ESP32 according to the pinout:
1. Connect one leg of the **Pushbutton** to `GPIO 25`, and the other leg to `GND`.
2. Connect the **LED** Anode (long leg) to `GPIO 26` via a 220Ω resistor, and Cathode (short leg) to `GND`.
3. Connect the **Buzzer** positive pin to `GPIO 27`, and negative pin to `GND`.
4. Plug the ESP32 into your computer via a Micro-USB or USB-C data cable.

### 2. Flashing the Firmware

1. Install the latest [Arduino IDE](https://www.arduino.cc/en/software).
2. Add ESP32 board support:
   - Go to **File** > **Preferences**.
   - In **Additional Boards Manager URLs**, add:  
     `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Go to **Tools** > **Board** > **Boards Manager**, search for `esp32` by Espressif, and click **Install**.
4. Open [sketch_oct18a.ino](file:///c:/Users/Piyush/Downloads/embedded/embedded/sketch_oct18a/sketch_oct18a.ino).
5. Configure your settings under **Tools**:
   - **Board**: `ESP32 Dev Module` (or your specific ESP32 board)
   - **Upload Speed**: `921600` (or `115200`)
   - **Flash Frequency**: `80MHz`
   - **Port**: Select the COM port corresponding to your ESP32 (e.g., `COM3`, `COM5`, or `/dev/ttyUSB0`).
6. Click **Upload** (➡️).
7. Open the **Serial Monitor** at **115200 baud** to verify that the boot sequence completes:
   ```text
   [SYSTEM BOOT] Flash Memory Loaded.
   - Failed Attempt (Timeouts): 0
   - Failed Attempt (Mismatches): 0
   ```

### 3. Launching the Web Interface

No Node.js or server installation is mandatory. You can serve the static files with any standard HTTP server or open them in a Web Serial-supported browser (Google Chrome, Microsoft Edge, Opera, Brave).

#### Option A: Python Quick Server (Recommended)
Run in your terminal from the project directory:
```bash
# Python 3
python -m http.server 8000
```
Then navigate to:
```
http://localhost:8000/vault_1.html
```

#### Option B: VS Code Live Server
Right-click `vault_1.html` or `index.html` and select **"Open with Live Server"**.

---

## 📖 Step-by-Step Usage Guide

### Step 1: Connect to Hardware Gateway
1. Click the glowing **"Connect to ESP32 Gateway"** button in the top navigation bar or home screen.
2. The browser prompt will list connected serial devices. Select your **ESP32** (e.g., *CP2102 USB to UART Bridge Controller* or *CH340*) and click **Connect**.
3. The gateway status dot will turn **solid green**, and the interface will transition to the Main cryptographic panel.

### Step 2: Enroll a Master Password
1. In the **"CREATE A PASSWORD TO ENCRYPT"** card, type your desired master password.
2. Click **"ENCRYPT & COMMIT TO VAULT"**.
3. **Observe the Hardware**:
   - The ESP32 LED illuminates (`GPIO 26`).
   - The buzzer emits a crisp 100ms tone.
   - The web console displays: `[ARMED: 1 Click = ACCEPT | 2 Clicks = REJECT]`.
4. **Physical Action**:
   - **To Approve**: Press the physical tactile button **once**. After 1 second, the vault hashes the password with SHA-256, stores the digest into flash NVS, sounds a double success chirp, and turns off the LED.
   - **To Cancel**: Press the tactile button **twice**. The vault aborts, sounds an alert tone, and keeps previous credentials untouched.

### Step 3: Authenticate / Verify Password
1. Once registered, the **"VERIFY PASSWORD"** card unlocks.
2. Type the candidate password into the input field and click **"AUTHENTICATE WITH VAULT"**.
3. The ESP32 arms again (LED ON, tone sounds).
4. Depress the tactile button **once** to confirm your physical presence.
5. The ESP32 autonomously computes the candidate hash and matches it against NVS storage:
   - **Match**: `SUCCESS:<hash>` emitted, success chime plays, and UI confirms authentication.
   - **Mismatch**: `ERROR:Mismatch` emitted, warning siren plays, and audit counter in NVS increments.

---

## 📂 Project Directory Structure

```text
Hardware-Isolated-Security-Vault/
├── sketch_oct18a/
│   └── sketch_oct18a.ino      # ESP32 C++ firmware (FSM, mbedTLS SHA-256, NVS flash, ISR)
├── vault_1.html               # Formatted Cyberpunk Web Serial GUI & telemetry interface
├── index.html                 # Production Web Serial GUI implementation
├── esp32-board.png            # Visual asset: ESP32 DevKit pinout & board graphic
├── grid-pattern.png           # Visual asset: Futuristic background canvas pattern
├── Home Page.svg              # Vector design reference for Home view
├── Main.svg                   # Vector design reference for Main interactive view
└── README.md                  # Comprehensive project documentation
```

---

## 🛠️ Security Verification & Threat Analysis

| Threat Scenario | Traditional Software Vault | Hardware-Isolated Vault (This Project) |
| :--- | :--- | :--- |
| **Background Keylogger / RAT** | Malware can simulate keystrokes and unlock software silently. | **Mitigated**: Transactions stall in `ARMED` state and abort via timeout unless a human physically presses the hardware switch. |
| **Browser Memory Scraping** | Password & derived keys reside in JavaScript memory heap. | **Mitigated**: Hashing and verification take place solely inside ESP32 silicon. |
| **Tampered Client UI** | Attackers can patch client JS code to return `true` on auth checks. | **Mitigated**: Authentication verdicts are signed and issued by ESP32 hardware via serial; unauthorized actions remain rejected. |
| **Cold Reboot / Power Cut** | Transient data can be corrupted or wiped. | **Mitigated**: ESP32 NVS stores credentials in non-volatile flash wear-levelled partitions. |

---

## 👥 Built By

Developed as an advanced embedded security & hardware isolation project by:

* **Piyush Lohia** — Reg No: `25BCE2527`
* **Yash Agarwal** — Reg No: `25BCE2531`

---

## 📜 License

This project is open-source under the [MIT License](https://opensource.org/licenses/MIT). Feel free to use, modify, and distribute for educational, research, and personal security engineering applications.
