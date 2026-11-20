# STM32 Voice Reminder Clock — V1 Functional Design

**Version: V1.4 · 2026-11-19**  
**Positioning:** A standalone voice reminder clock that can be deployed to a Nucleo F446RE whenever needed. The PC handles configuration, backup, and one-click restoration; the board executes reminders independently.

## 1. Project Objectives and Design Principles

Develop a voice reminder clock based on the **STM32 Nucleo F446RE**. The development board can normally be used for other experiments. When it is needed as an alarm clock, connect it to a PC and use the Python GUI to flash the alarm firmware and restore previously backed-up alarms and audio. After configuration, disconnect it from the PC and keep it powered; the board handles scheduling, voice playback, and stopping alarms via a button.

- **Plug-and-play:** Switch from ordinary experimental firmware to the alarm application whenever needed; the alarm firmware need not remain installed permanently.
- **PC-based configuration:** Use a graphical interface, without Whisper, an LLM, or a voice-dialogue workflow.
- **Independent STM32 operation:** Store the active alarm configuration and voice data in internal Flash.
- **Audio DSP as the learning focus:** Audio preprocessing, ADPCM encoding/decoding, and real-time DAC/DMA playback.
- **Minimal hardware:** Use the Nucleo F446RE and Waveshare 3972 amplifier/speaker module, plus an external regulated 5 V supply and suitable connection cable for standalone operation. No MicroSD card, external RTC, or additional audio circuitry is required.

## 2. System Architecture

```text
+--------------------------------------------------------------------+
| PC · Python GUI                       (connected for setup/management)|
|                                                                    |
|  +--------------------------------------------------------------+  |
|  | Alarm management: Create / Edit / Delete / View / Enable     |  |
|  |                   or disable                                 |  |
|  +--------------------------------------------------------------+  |
|  | Microphone recording           | Reminder text -> TTS        |  |
|  +--------------------------------------------------------------+  |
|  | Audio DSP: Filtering / Resampling / Volume normalization /   |  |
|  |            ADPCM encoding                                     |  |
|  +--------------------------------------------------------------+  |
|  | Local backup: JSON configuration + raw recordings + ADPCM    |  |
|  +--------------------------------------------------------------+  |
|  | Firmware management: Flash / Verify / Restore alarms /       |  |
|  |                      Automatic & manual time sync            |  |
|  +--------------------------------------------------------------+  |
+--------------------------------------------------------------------+
        |                                  |
        | USB virtual COM / UART           | ST-LINK / SWD
        | (alarms, audio, time sync)        | (firmware flashing)
        v                                  v
+--------------------------------------------------------------------+
| STM32 Nucleo F446RE                          (standalone operation) |
|                                                                    |
|  +----------------------------+  +------------------------------+ |
|  | RTC                        |  | Scheduler                    | |
|  | Clock and alarm triggering |  | Multiple-alarm scheduling    | |
|  +----------------------------+  +------------------------------+ |
|  | Flash                      |  | ADPCM Decoder                | |
|  | Alarm data / compressed    |  | Real-time audio decoding     | |
|  | audio storage              |  |                              | |
|  +----------------------------+  +------------------------------+ |
|  | Timer + DMA + DAC          |  | State Machine                | |
|  | Continuous PCM output      |  | Button / LED / run states    | |
|  +----------------------------+  +------------------------------+ |
+--------------------------------------------------------------------+
                              |
                              v
                 +-----------------------------+
                 | Audio amplifier + speaker   |
                 | Recorded or TTS reminders   |
                 +-----------------------------+
```

**PC → board:** ST-LINK/SWD flashes the firmware. USB virtual COM (UART) transfers configuration, compressed audio, status readbacks, and time synchronization. Even if the board is running other experimental firmware, the PC can reflash it with the alarm application over ST-LINK.

**Board → speaker:** The board reads IMA-ADPCM data from Flash, decodes it to PCM, and uses timer-triggered DAC output with DMA for continuous playback through an amplifier and speaker.

## 3. V1 Resource and Functional Boundaries

| Parameter | Provisional V1 specification |
|---|---|
| MCU | STM32F446RE, Cortex-M4F, 180 MHz |
| Flash / SRAM | 512 KB / 128 KB |
| Number of alarms | Maximum 6 |
| Audio per alarm | Maximum 8 seconds |
| Compression format | IMA-ADPCM, 8 kHz, 4-bit, mono |
| Audio storage | Approximately 32 KB per clip; approximately 192 KB for six clips (excluding headers) |
| Storage allocation | Provisional data storage budget: approximately 256 KB for alarm metadata and compressed audio. M2 will determine the final Flash sector allocation based on compiled firmware size and storage requirements. |
| Recurrence | One-time, daily, weekdays, or selected days of the week |
| User interaction | PC GUI; on-board USER button and green LED |
| Connectivity | ST-LINK/SWD + USB UART |

The GUI must check **the number of alarms, each recording's duration, and total audio size**. The limits above are initial design targets, not proof that six eight-second clips will fit. The final implementation must verify Flash erase sectors, firmware size, and data layout. **M2 decision:** Freeze the exact firmware/data sector allocation only after measuring the compiled firmware size and checking the six-clip storage budget. Do not treat a provisional partition as final. Each alarm can reference one audio clip; shared audio may be supported later.

**RTC clock source:** The target board has been identified from its rear label as **NUCLEO-F446RE (MB1136-F446RE-C04)**. This board revision includes an onboard **32.768 kHz LSE crystal**. Configure the RTC to use **LSE rather than LSI** in STM32CubeMX and verify successful oscillator startup during M3. No additional RTC crystal, external RTC module, or board modification is required for the LSE configuration. RTC time retention across a complete power loss is still not provided (see §7.1).

### 3.1 Pin Assignment

| STM32 Pin | Nucleo Board Pin | Function |
|---|---|---|
| PA4 | Arduino A2 | DAC Channel 1 output → Waveshare 3972 H2 INPUT (DAC signal in schematic; also available on P1 pin 5) |
| PA5 | Arduino D13 | GPIO output → onboard green LED (LD2) |
| PC13 | USER button (B1) | Stop audio playback |
| 5V | 5V | Waveshare 3972 H1 POWER (5 V input for the LM386 amplifier) |
| GND | GND | Waveshare 3972 H1 POWER (GND); common ground with the audio input |

**Pin conflict:** PA5 is shared by DAC Channel 2 and the onboard green LED (LD2). Use only **DAC Channel 1 on PA4** for audio output; keep PA5 configured as GPIO output for LD2. The Waveshare schematic shows the LM386 supply on **H1 POWER (5 V/GND)**; the **3.3 V pin on P1 does not power the amplifier**. H2 INPUT provides the DAC audio input and GND; its GND shares the module common ground.

**Standalone power:** Use a regulated external 5 V source connected to E5V (CN7 pin 6) and GND, with JP5 set to E5V (pins 2–3) and JP1 removed. Observe the E5V input rating (4.75–5.25 V, maximum 500 mA). **Operating workflow:** Power E5V first and leave it on during PC configuration, firmware flashing, UART transfer, and standalone operation. Connect the PC USB cable only after external power is present; disconnect PC USB after configuration without interrupting E5V. With JP5 on E5V, PC USB alone does not power the target MCU. Do not switch JP5 back and forth for normal setup. Verify this arrangement during M1 before treating standalone operation as validated; boot/playback testing does not by itself prove compliance with the current limit.

### 3.2 Audio Output Electrical Constraints

The Waveshare 3972 integrates an LM386 audio amplifier and a speaker.

- **Supply voltage:** Use 5 V for the Waveshare 3972. The LM386M-1 requires at least 4 V for operation within its specified supply range.
- **Audio input:** STM32 PA4 (DAC Channel 1) connects to the module's audio input (labelled DAC in the schematic).
- **Input conditioning:** The module already includes AC coupling and input attenuation (confirmed from the official Waveshare schematic). No additional external components are required for the initial prototype.
- **Output amplitude:** A full-scale 0–3.3 V DAC waveform may overdrive the LM386 and cause clipping.
- **Software volume control:** Scale the decoded PCM amplitude before converting it to unsigned 12-bit DAC samples, preserving the DAC midpoint at approximately 2048.

**M1 Audio Gain Test:** Start with a low software gain (e.g., 0.03–0.05, not 1.0), gradually increase it, and evaluate the resulting playback volume, speech intelligibility, and audible clipping or distortion. This starting range is only a precaution based on the LM386 input attenuation and nominal gain, not a validated operating limit. Determine and document a suitable default gain that provides clear voice playback without noticeable distortion.

## 4. PC-Side Functional Design

### 4.1 GUI Startup and Device Identification

1. Check whether the local backup file exists in the default directory and is valid.
2. Detect whether the Nucleo F446RE is connected via USB/ST-LINK.
3. If alarm firmware is running, read the board's alarm list and display synchronization status.
4. If another experimental firmware is running, display the **Install Alarm Firmware and Restore Backup** action. Do not interpret “non-alarm firmware” as an “empty alarm database.”
5. Once alarm firmware is detected and UART communication is established, **automatically synchronize RTC time**, verify it by reading it back, and display the result. If synchronization fails, display the error and a **Time Sync** retry action. If an automatic time-sync request receives `BUSY` during an update, retry it after the update completes rather than interrupting the transfer.
6. If the PC and board hold different alarm data, the local PC configuration takes precedence. The GUI asks whether to overwrite the board's data with the PC configuration. Upload only after user confirmation; do not perform bidirectional merging or automatic overwriting. 

### 4.2 Creating and Editing Alarms

Users work with three core business attributes: **reminder content, alarm time, and recurrence**. Internally, the system also stores an ID, enabled state, audio reference, and the necessary scheduling fields.

| Attribute | Description |
|---|---|
| Reminder content | Enter text to generate TTS, or record the user's own voice through the PC microphone |
| Alarm time | A one-time alarm needs a full date and time; a recurring alarm needs a time and, where necessary, a start date |
| Recurrence | One-time, daily, weekdays, or selected days of the week |

**Interaction flow:**

1. Enter the reminder name/content, date and time, and recurrence rule.
2. Select TTS or recording; recordings are made on the PC.
3. Perform the required audio processing on the PC, convert to the specified format, and encode as ADPCM.
4. Allow the user to preview the decoded version of the final encoded audio to check intelligibility and distortion.
5. Validate the limits: 6 alarms, 8 seconds per clip, and total audio storage capacity.
6. Save changes to the local PC backup. If the board is already running alarm firmware, upload and synchronize the changes.
7. The GUI shows progress for the entire dataset upload, including metadata-only edits that require unchanged audio to be resent. The board writes the data to Flash, verifies it, and returns the result. Only then does the PC display success or a specific error.

Offline edits must be clearly marked **Pending Sync**. “Saved on PC” must not be presented as “Device Updated.”

### 4.3 Viewing, Updating, Deleting, and Disabling Alarms

The GUI lists all alarms with their names, times, recurrence rules, enabled states, audio durations, and estimated next trigger times. It supports changing an alarm's time or recurrence without rerecording audio, previewing audio, disabling/re-enabling alarms, deleting individual alarms, and clearing all alarms. A standard confirmation dialog is shown before deletion.

After the board successfully deletes or updates an alarm, the GUI updates the synchronization status. If transmission or Flash programming fails, it shows an error and permits retrying, rather than reporting false success.

### 4.4 One-Click Firmware Installation and Backup Restoration

**Action button: Install Alarm Firmware and Restore Backup.**

1. Confirm the target is an F446RE through ST-LINK.
2. Warn that flashing will replace the currently installed firmware. Confirm that the PC's local alarm backup is available and valid before proceeding with restoration. Do not imply that the board can be used to create a backup.
3. Use `STM32_Programmer_CLI` (or an equivalent flashing tool) to flash and verify the prebuilt alarm firmware.
4. Once the alarm firmware starts, upload all alarm metadata and ADPCM audio over UART.
5. Check Flash data consistency using CRC and/or readback.
6. **Automatic time synchronization:** Obtain the current date, time, and time zone/local time basis from the PC; set the STM32 RTC and verify by reading it back.
7. Only after all steps succeed, show **Alarm Ready** in the GUI; the board returns to its normal idle LED pattern. Detailed success/failure information remains in the GUI.

The previously installed firmware does not need to cooperate with UART operations, even if it is not the alarm application. ST-LINK handles reflashing. **M5 decision:** Evaluate an optional fixed-address firmware magic/version structure readable over SWD for identifying installed firmware before reflashing. Do not require this in V1.4; even with SWD identification, a successful UART handshake is still required to establish that the alarm application is running and responsive.

### 4.5 RTC Time Synchronization (Automatic + Manual)

**Purpose:** Calibrate the STM32 RTC from the PC clock whenever alarm firmware is running and the board is connected. Provide manual synchronization for debugging and error recovery.

| Scenario | GUI behavior | Board behavior |
|---|---|---|
| Alarm firmware detected and UART handshake succeeds | **Automatically synchronize**; show “Synchronizing time” | Receive the time-setting command and update the RTC |
| One-click flashing/restoration completes | Synchronize and verify before displaying “Alarm Ready” | Set and read back the RTC |
| User clicks **Time Sync** | Run the same workflow manually; allow retry at any time | Accept the same time-setting command |
| Synchronization succeeds | Show “Synchronized,” the board's readback time, and the most recent successful sync time | Return status and readback time |
| Synchronization fails or times out | Display the reason clearly and allow retry; **do not display “Ready”** | Return an error code or remain in the awaiting-time-sync state |

**Interaction and verification flow:**

1. The GUI uses the firmware identifier and UART handshake to confirm that the board is indeed running alarm firmware. Merely plugging in USB or detecting ST-LINK **does not mean** a UART synchronization connection has been established.
2. The PC obtains the current time in the **Europe/Berlin** time zone and sends the corresponding local calendar date and wall-clock time in `SET_TIME`. V1 stores and schedules against Europe/Berlin **local time** in the RTC, rather than UTC. The PC must not silently use an unrelated system time zone; it must convert from the PC time zone to Europe/Berlin before transmission.
3. The STM32 validates the request and sets the RTC, returning execution status. After successful RTC setting, it writes a validity marker to a backup register; the PC then issues `GET_TIME` to read back the board time. A marker does not by itself guarantee accurate time.
4. The GUI compares the readback with current PC time, allowing a small tolerance for normal serial communication delay. On success, it shows “RTC Synchronized” and the last sync time; on failure, it offers a retry.
5. The GUI has a permanently available **Time Sync** button. Automatic and manual synchronization use the same protocol path, avoiding duplicate implementations.

**RTC initialization and validity (M3):** Use an RTC backup-domain register as a validity marker written only after a successful time setting. On boot, inspect the marker before performing calendar setup; preserve the RTC calendar on ordinary MCU resets when the backup domain remains valid. When the marker is absent/invalid, enter `WAITING_FOR_TIME_SYNC` and require PC time synchronization. CubeMX-generated initialization code must not unconditionally overwrite a valid RTC date/time or reset its backup domain. A retained marker indicates prior synchronization and backup-domain continuity, **not** independently verified clock accuracy.

**Daylight-saving-time limitation (V1):** The STM32 does not autonomously apply Europe/Berlin daylight-saving transitions while disconnected. After a spring/fall transition, its wall-clock time may be one hour wrong until the next successful PC time sync. During this interval, recurring alarms may sound one hour early or late. The PC corrects the RTC on reconnection; V1 makes no guarantee of accurate alarms across an unattended DST transition.

**Important limitation:** Automatic/manual synchronization can correct the RTC only while the board is connected to the PC. Under the documented E5V-first workflow (§3.1), disconnecting PC USB **does not interrupt target power or invalidate the RTC**, provided external E5V remains present. A real interruption of E5V (with no backup supply) can still invalidate RTC time. V1 does not add an RTC backup battery or a DS3231, and **PC synchronization is not a power-loss time-retention solution**. When the RTC time is invalid or uncalibrated, show an awaiting-time-sync state and avoid silently triggering alarms against an incorrect absolute time.

### 4.6 Local PC Backup

SQLite is unnecessary. Use JSON metadata and separate audio files:

```text
voice-reminder/
├── firmware/
│   └── alarm_f446re.bin
├── backup/
│   ├── alarms.json
│   ├── original_audio/
│   │   ├── alarm_001.wav
│   │   └── alarm_002.wav
│   └── encoded_audio/
│       ├── alarm_001.adpcm
│       └── alarm_002.adpcm
└── app/
```

| Backup state | Application behavior |
|---|---|
| Valid backup exists | Load normally |
| Local backup missing (including missing default directory) | Report the missing backup and allow creation of a new empty alarm database; do not automatically import from or overwrite existing board data |
| File exists but is corrupted | Report an error and retain the corrupted file; do not overwrite it automatically |
| PC and board differ | Ask whether to overwrite the board with the PC configuration; after confirmation, upload in one direction without bidirectional merging |

**Responsibilities:** The PC backup supports long-term retention and recovery across firmware changes. The board's Flash data is the configuration actually executed when disconnected from the PC.

## 5. STM32-Side Functions and State Logic

### 5.1 Core Modules

| Module | Responsibility |
|---|---|
| Storage Manager | Store and verify alarm metadata and compressed audio |
| RTC Driver | Use the onboard LSE on the identified MB1136-F446RE-C04 board (verify startup in M3); maintain Europe/Berlin local time, preserve valid backup-domain RTC state across MCU resets, check the synchronization marker, and support UART time setting/readback |
| Alarm Scheduler | Determine the next trigger; handle one-time and recurring alarms |
| Audio Decoder | Decode IMA-ADPCM into PCM |
| Audio Output | Real-time playback through Timer, DMA, and DAC; keep the idle DAC at mid-scale and apply short start/stop fades to reduce clicks |
| UART Protocol | Receive configuration, upload and deletion commands; enforce command availability by board state, return `BUSY` when needed, and report verification results |
| Button Handler | Stop the current reminder with the USER button |
| LED Controller | Indicate operating/operation states without blocking |
| State Machine | Coordinate BOOT / WAITING_FOR_TIME_SYNC / IDLE / PLAYING / UPDATING / ERROR; DISMISS is a playback-stop event/transition |

### 5.2 Basic Board Execution Flow

```text
BOOT
  └─ Validate stored dataset and RTC validity
       ├─ Dataset invalid ────────────────────→ ERROR
       ├─ RTC invalid ────────────────→ WAITING_FOR_TIME_SYNC
       │                                   └─ Successful SET_TIME → IDLE
       └─ Both valid ─────────────────────────→ IDLE
                                                 │
                       ┌─────────────────────────┴─────────────────┐
                       │                                           │
              Upload accepted                              Alarm becomes due
                       │                                           │
                    UPDATING                                    PLAYING
              (erase/write/verify)                      (ADPCM → PCM → DMA/DAC)
                       │                                  └─ Dismiss or timeout
              ┌────────┴──────────────┐                              │
              │                       │                              │
       Verified success       Failure or inactivity timeout          │
              │                       │                              │
       RTC valid?          Original dataset still valid?             │
       ├─ Yes → IDLE        ├─ Yes → previous valid state             │
       └─ No → WAITING_     └─ No  → ERROR                             │
                 FOR_TIME_SYNC          (await full restore)          │
                                                                  IDLE

ERROR retains UART diagnostic and full-dataset restoration support.
No alarm is scheduled from an invalid dataset. On successful restore,
return to IDLE only if RTC is valid, otherwise WAITING_FOR_TIME_SYNC.
```

- **Fired occurrence guard (all alarms):** Keep a RAM-based record of the most recently fired scheduled occurrence for **each alarm**, keyed by alarm ID, relevant alarm configuration, and the **scheduled occurrence date and time** (not actual playback start). Check this guard before dispatching both one-time and recurring alarms, including reminders deferred by an update. Preserve it across `SET_TIME`, including backward clock corrections, so the same recorded occurrence is not played twice. Clear or replace the guard only when the corresponding alarm is genuinely changed/replaced or deleted, not at every scheduler recalculation. A single last-occurrence record does not guarantee de-duplication after large clock rollbacks spanning multiple earlier occurrences. The guard is volatile: V1 does **not** guarantee exactly-once execution after MCU resets or power cycles.
- **One-time alarms:** Once their scheduled date/time has passed, they are expired and normally will not trigger again. No completion flag is written to Flash; the record remains until modified or deleted in the PC GUI. After an MCU reset or power cycle, the volatile guard is lost, so repeat execution is possible if the RTC re-enters the alarm's trigger window. RTC time must be valid.
- **Recurring alarms:** Dismissing or timing out a playback ends only the current scheduled occurrence; the alarm remains enabled and its next future occurrence is calculated. The fired occurrence guard also prevents the most recently dispatched daily/weekly occurrence from repeating after a short backward time sync. A timeout does not disable the alarm.
- **Simultaneous alarms:** Queue due reminders and play them one by one. A button press or timeout stops only the current playback; already queued reminders continue in sequence.
- **Playback timeout:** Without a USER-button press, replay the reminder with approximately 2 seconds between repeats, but **automatically stop the current occurrence after a maximum of 5 minutes** (measured from playback start). The 5-minute limit is fixed for V1 and does not require a GUI setting.
- **Missed alarms during power loss:** Do **not** replay past occurrences after a restart or after time is restored. A one-time alarm already in the past is expired; recurring alarms resume from their next future occurrence. V1 does not provide a missed-alarm history.
- **Alarms during an update:** Reject a new upload while `PLAYING` or while another update is underway; uploads may begin from `IDLE` without an imminent-alarm admission margin. While `UPDATING`, suspend playback. Record the update-start time and sufficient pre-update alarm scheduling information in RAM; after successful verification, read the RTC again and determine which alarms became due in the elapsed update window. Replay each eligible occurrence once **only if its alarm still exists and is enabled in the verified dataset**. Do not rely solely on RTC interrupts during Flash erase/program operations. Do not recover these events after a reboot or failed dataset verification; report the failed update instead of using unverified data. Every dispatched occurrence, including a deferred recurring occurrence, updates its RAM fired guard.
- **Invalid RTC:** `WAITING_FOR_TIME_SYNC` does not schedule or play reminders. Do not treat a stale time reference saved in Flash as current time. Time sync and status queries remain available; a successful verified `SET_TIME` transitions to `IDLE` if the dataset is valid.
- **Upload inactivity timeout:** While `UPDATING`, abort if no valid chunk or expected protocol activity arrives within an M2-defined inactivity timeout (initial target: a few seconds **between chunks**, excluding expected erase/program/verification latency). A disconnected PC must not leave the device stuck in `UPDATING`. If no Flash modification occurred and the previous verified dataset remains intact, return to the previous valid state. Once erasure/programming has invalidated the original dataset, timeout or verification failure enters `ERROR`; do not resume scheduling until a complete dataset has been restored and verified. Preserve diagnostic/status responses and the full-restore command in `ERROR`.
- **Update completion:** After a successful verified upload, return to `IDLE` only if both the RTC and stored dataset are valid; otherwise return to `WAITING_FOR_TIME_SYNC` if only RTC is invalid, or `ERROR` if the dataset is invalid. An aborted upload may return to the previous valid state **only** when the original dataset is demonstrably intact. `DISMISS` is a short event, not a separate long-running state.

**UART command availability by state (V1):** A command may be supported in a state without being answerable immediately during a blocking Flash operation. The PC must handle `BUSY` or a bounded timeout and retry only when appropriate; no asynchronous command queue is required.

| Board state | `GET_TIME` / `STATUS` | `SET_TIME` | `START_UPLOAD` |
|---|---|---|---|
| `IDLE` | Allowed | Allowed | Allowed |
| `PLAYING` | Allowed | Allowed; do not interrupt current playback, and preserve fired guards | Reject with `BUSY` |
| `WAITING_FOR_TIME_SYNC` | Allowed | Allowed; transition to `IDLE` only when RTC and dataset are valid | Allowed; scheduling remains paused until RTC is valid |
| `UPDATING` | Respond when Flash is not busy; otherwise delay or report `BUSY` | Reject with `BUSY`; PC retries after update | Reject with `BUSY` |
| `ERROR` | Diagnostic queries if communication remains available | Allowed if RTC/UART are operational; remain in ERROR until dataset recovery succeeds | Full-dataset recovery upload allowed |

The PC's automatic and manual RTC synchronization use this same rule. `SET_TIME` must not be silently applied mid-update, and a clock correction during playback must not restart the currently active reminder. Full packet definitions and error/timeout encoding remain M2 decisions.

### 5.3 Operation Feedback and LED

Keep LED indications simple. Detailed device/operation status, time-sync results, and errors are displayed in the PC GUI when connected; LD2 does not encode a separate pattern for every command.

| Board condition | Green LD2 LED | PC GUI (when connected) |
|---|---|---|
| Normal idle (`IDLE`) | Brief blink every few seconds | Ready and synchronized status |
| Data update (`UPDATING`) | Slow repeating blink | Upload progress and verified result |
| Attention required (`WAITING_FOR_TIME_SYNC`, invalid dataset, or `ERROR`) | Fast repeating blink | Specific reason and corrective action |
| Reminder playback (`PLAYING`) | Distinct periodic blink | Playback status, if connected |

After a successful upload, time synchronization, or button dismissal, LD2 resumes the pattern for the resulting board state. The PC must wait for a response confirming that the board has **actually finished writing and verifying Flash** before reporting success. LED control must be non-blocking; avoid `HAL_Delay()` so it does not interfere with real-time playback.

## 6. Audio DSP Responsibilities: PC vs. Board

| Function | PC | STM32 |
|---|---|---|
| Microphone recording | `sounddevice` | Not needed |
| Text-to-speech | Piper TTS or another local engine | Not needed |
| High-pass filtering (as needed) | SciPy | Not needed |
| Anti-alias filtering and resampling | SciPy | Not needed |
| Volume normalization | NumPy | Not needed |
| IMA-ADPCM encoding | FFmpeg or a custom reference implementation | Not needed |
| IMA-ADPCM decoding | Reference implementation for testing/preview | Custom C implementation |
| Timed continuous playback | Preview through standard PC audio device | STM32 Timer + DMA + DAC |
| Amplification and acoustic output | Not needed | External amplifier/speaker module |

**M1 ADPCM format and verification:** Define one exact on-Flash IMA-ADPCM representation, including whether the data is WAV-style blocked IMA (with per-block headers), block sizes, initial predictor/step-index handling, and nibble order. FFmpeg output must not be treated as a headerless raw nibble stream without verification. Keep FFmpeg or a reference encoder on the PC; writing a new encoder is optional, not a V1 requirement. Compile the **same STM32 C decoder** for PC-side tests and compare its decoded PCM sample-for-sample with a trusted reference decoder using identical ADPCM test vectors. Bit-exactness applies to decoding the same encoded input, not to recovering the original uncompressed recording.

**M1 playback transitions:** Hold the DAC at approximately mid-scale (12-bit value 2048) when idle, and apply short fade-in/fade-out ramps when starting and stopping playback to limit audible clicks. Check sound quality and the default software gain during M1. An external RC low-pass filter may be investigated only if actual listening tests indicate a need; it is **not** part of the V1 hardware requirements.

**Technical focus:** Predictive coding, quantization error, audio sampling and resampling, real-time streaming decoding, DMA double buffering, audio distortion, and CPU/RAM measurements. CMSIS-DSP may be used later for audio algorithm experiments, but V1 should not mandate FFT or unnecessary noise suppression/AGC.

## 7. Known Limitations and Engineering Risks

### 7.1 RTC Cannot Retain Time Through Complete Power Loss

V1 does not use a DS3231 or a VBAT backup battery. With the documented **continuous E5V supply** (§3.1), connecting or disconnecting the PC USB cable does **not** interrupt target power, so the RTC keeps running during normal configuration-to-standalone transitions. However, a real loss of external E5V power can stop or reset the RTC because no backup supply is present. The last PC synchronization cannot establish how long the board remained unpowered. On the next PC connection, the GUI must automatically synchronize the RTC and provide a manual **Time Sync** button with readback verification. Firmware must detect and report invalid RTC time; **unattended reminders cannot be guaranteed across a complete power interruption**. VBAT coin-cell support (including any necessary board solder-bridge changes) is a possible future enhancement, **not** part of V1.

### 7.2 Flash Updates Require Sector Erasure

The F446RE's internal Flash cannot be overwritten arbitrarily like a regular file system. V1 may favor a **complete PC backup + whole-dataset packaging and restoration on the board** strategy, with capacity checks, CRC verification, and error reporting. **Whole-dataset update tradeoff:** Even a metadata-only edit (such as changing an alarm time) may require erasing the data sector(s) and retransmitting all audio, including unchanged clips. The PC GUI must show progress and final verification status rather than implying such edits are instantaneous. M2 shall select/test the UART baud rate and measure actual end-to-end update duration; no particular transfer time or number of erased sectors is assumed before M2. If an upload is interrupted, rebuilding from the PC backup is acceptable; not all updates are guaranteed to be atomic.

**Erase/program safety:** Sector erase and programming can stall execution from internal Flash, delaying interrupt handling and UART reception. **Do not erase/program Flash while audio playback is active.** Use an acknowledged, flow-controlled UART transfer: PC requests an update; the board rejects it if busy, erases the required sector(s), acknowledges erase completion, then receives bounded data chunks and acknowledges each completed write/verification step. The PC must not stream data during an erase or assume the board can buffer the entire upload in SRAM. If erasure/programming has started and the original dataset cannot be verified as intact, a failed, interrupted, or timed-out upload leaves the dataset invalid until fully restored and verified. If the upload aborts before any Flash modification, the previous verified dataset may remain valid. Scheduling must never use unverified data. The firmware shall exit `UPDATING` on an inactivity timeout (initially a few seconds without valid protocol progress between chunks, excluding normal Flash-operation latency), with M2 specifying the exact duration and recovery response.

**M2 design decisions:** Determine the exact 512 KB Flash sector allocation based on measured firmware size and the total data budget. Specify the minimum UART framing contract (command/version, payload length, integrity check, ACK/NACK, timeout and retry/duplicate-chunk handling) and a dataset-level identifier/checksum suitable for comparing PC and board configurations without reading back every audio byte. The concrete packet layout, checksum/sequence-number representation, and upload inactivity timeout are **not fixed in V1.4**; implement and test them in M2, including an unplugged-PC mid-upload test and verified-dataset recovery.

### 7.3 Volume and Audio Quality Require Testing

8 kHz/4-bit ADPCM prioritizes storage capacity. The amplifier/speaker choice and whether the alarm is sufficiently loud must be evaluated by listening tests. 8 kHz and 16 kHz speech quality can be compared, but a higher sample rate reduces the total audio duration that can be stored. The audio output level must match the amplifier's input range.

## 8. Development Milestones

| Milestone | Deliverable | Learning focus |
|---|---|---|
| M1 | STM32 plays ADPCM speech; USER button stops playback; verify exact ADPCM framing/bit-exact decoding, idle DAC midpoint, click-reduction fades, and default software gain; Validate standalone 5 V power using the selected JP1/JP5 configuration, including boot and audio playback without a PC connection | ADPCM format compatibility, C decoder reference tests, DAC/DMA double buffering and listening tests |
| M2 | Flash audio storage, UART upload, CRC verification; **finalize sector partition and minimal reliable UART protocol**, including command-state responses; select/test baud rate, measure full-dataset update time, and support GUI progress reporting | Flash-sector sizing/erase latency, stop-and-wait transfers, `BUSY`/inactivity-timeout handling, mid-upload disconnect/recovery, end-to-end transfer time and dataset identity |
| M3 | Configure and verify startup of the onboard 32.768 kHz LSE on the confirmed MB1136-F446RE-C04 board; implement backup-register validity marker and reset-safe RTC initialization; multiple-alarm scheduling, recurrence rules, RAM per-occurrence fired guards for one-time and recurring alarms, time-sync rollback tests, missed-event/update-window reconciliation and playback-timeout behavior | RTC validity, local-time/DST limitations, interrupts, scheduling across time sync and updates |
| M4 | PySide6 GUI, TTS, recording, and audio preprocessing | Python GUI, audio data processing |
| M5 | One-click flashing, PC backup, automatic restoration; automatic/manual RTC sync and readback verification; assess optional SWD firmware magic/version detection | ST-LINK, UART handshake, deployment and data consistency |
| M6 | Audio quality, capacity, timing, stability, and abnormal-recovery testing | CPU cycles, SRAM, system testing |

## 9. V1 Acceptance Criteria

- If the local PC backup is absent, an empty database can be created safely without automatically overwriting existing board data. If PC and board data differ, replacement is only permitted after user confirmation and proceeds in one direction: PC → STM32.
- Up to 6 alarms can be created, edited, viewed, deleted, enabled, and disabled, with local backups and board synchronization.
- Recorded or TTS audio can be converted into ADPCM reminder clips of up to 8 seconds; the PC supports previewing and capacity checks.
- The board performs real-time ADPCM decoding, DAC/DMA playback, and USER-button termination, without obvious audio glitches. M1 verifies the specified ADPCM block format using bit-exact reference PCM tests and checks DAC midpoint, click-reduction fades, and default gain.
- If the board is running another experimental application, one-click flashing and full backup restoration can return it to alarm mode.
- When alarm firmware is detected after connection, RTC synchronization starts automatically. The GUI provides a manual **Time Sync** button. Success requires readback verification, and failure must be clearly reported.
- Creating, updating, deleting, flashing, and restoring operations provide success/failure feedback; success must be based on the board's verified result. Even metadata-only edits show whole-dataset upload progress when unchanged audio must be resent; M2 records measured update time.
- Invalid RTC time after power loss, interrupted or inactive Flash uploads, and insufficient storage capacity are explicitly reported rather than hidden. Mid-upload disconnection exits `UPDATING` after a bounded inactivity timeout; if the dataset was modified and is invalid, the board enters `ERROR` and permits a full PC restore without scheduling unverified alarms.
- **Flash updates are rejected while audio playback is active.** During an accepted update, playback is suspended; the PC waits for erase/write acknowledgments and does not stream during sector erasure. Concurrent uploads are rejected; time/status commands follow the documented state table, including `BUSY` responses and retry after an update.
- Unattended playback times out after 5 minutes; power-off missed alarms are not replayed; due events during a successful update are reconstructed from the RTC and update window, without relying solely on interrupts. Within a continuous run, the most recently fired scheduled occurrence of **any** alarm (one-time or recurring) is not replayed after a backward time sync; V1 does not promise exactly-once behavior across MCU resets or large clock rollbacks spanning multiple past occurrences.
- RTC scheduling uses Europe/Berlin local time and clearly documents its unsupported unattended DST transitions. M3 verifies successful startup of the confirmed onboard LSE, retains a valid RTC across ordinary resets, and detects an invalid RTC after loss of backup-domain power using its validity marker.

---

*V1.4 (2026-11-19) consolidates the V1.3 design and subsequent refinements. It confirms the NUCLEO-F446RE C04 board configuration and onboard 32.768 kHz LSE; documents the Waveshare H1/H2 audio connections and continuous-E5V power workflow (subject to M1 validation); and clarifies RTC validity, scheduling, audio verification, Flash updates, UART command handling, and error recovery, including upload inactivity timeout and per-occurrence RAM guards. The final Flash sector allocation and detailed UART protocol remain M2 implementation decisions. Complete external power loss still invalidates RTC time without backup power. No additional functional modules or dedicated measurement equipment are required. Standalone operation uses an external regulated 5 V supply and a suitable power connection cable, as specified in §3.1.*
