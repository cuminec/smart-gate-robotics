#include <Arduino.h>
#include <ESP32Servo.h>

// ── Pin Definitions ──────────────────────────────────────────
#define TRIG_PIN       2
#define ECHO_PIN      15
#define SERVO_PIN     13
#define LED_RED       12
#define LED_GREEN     14
#define LED_BLUE      27

// ── Configuration ────────────────────────────────────────────
#define MAX_DISTANCE_CM      500   // Maximum sensor range (cm)
#define DETECT_DISTANCE_CM   250   // Vehicle detection threshold (cm)
#define GATE_OPEN_ANGLE        0   // Servo angle = Gate OPEN
#define GATE_CLOSE_ANGLE      90   // Servo angle = Gate CLOSED
#define GATE_HOLD_MS        5000   // How long gate stays open (ms)
#define SWEEP_STEP_DELAY_MS   12   // Delay per degree during sweep (ms)
#define BLINK_INTERVAL_MS    200   // LED blink period while moving (ms)
#define SENSOR_SAMPLE_DELAY   80   // Main loop polling interval (ms)

// ── Gate State Machine ───────────────────────────────────────
enum GateState {
  STATE_CLOSED,    // Gate closed, waiting for vehicle
  STATE_OPENING,   // Servo sweeping 90° → 0°
  STATE_OPEN,      // Gate held open, timing 5 s
  STATE_CLOSING    // Servo sweeping 0° → 90°
};

// ── Globals ──────────────────────────────────────────────────
Servo       gateServo;
GateState   gateState    = STATE_CLOSED;
unsigned long openTimestamp = 0;   // When gate reached full-open
int         currentAngle  = GATE_CLOSE_ANGLE;

// ── RGB LED Helpers ──────────────────────────────────────────
void rgbOff() {
  digitalWrite(LED_RED,   LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE,  LOW);
}

// Set one colour (pass pin: LED_RED / LED_GREEN / LED_BLUE)
void rgbSolid(uint8_t pin) {
  rgbOff();
  digitalWrite(pin, HIGH);
}

// ── Distance Measurement ─────────────────────────────────────
/**
 * Sends a 10 µs trigger pulse and measures echo duration.
 * Returns distance in centimetres, capped at MAX_DISTANCE_CM.
 */
long measureDistanceCm() {
  // Ensure TRIG is LOW before pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(4);

  // 10 µs HIGH pulse
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // pulseIn timeout = 2 * MAX_DISTANCE_CM * 58 µs ≈ 58 000 µs for 500 cm
  long duration = pulseIn(ECHO_PIN, HIGH, 58000UL);

  if (duration == 0) {
    // No echo received — object beyond range or sensor error
    return MAX_DISTANCE_CM;
  }

  // Speed of sound: distance (cm) = duration (µs) / 58
  long distance = duration / 58;
  return constrain(distance, 0, MAX_DISTANCE_CM);
}

// ── Smooth Servo Sweep with LED Blink ────────────────────────
/**
 * Sweeps the servo from `fromAngle` to `toAngle` one degree at a time.
 * While moving, the specified LED colour blinks at BLINK_INTERVAL_MS.
 * @param ledPin  The RGB pin to blink (LED_RED or LED_BLUE)
 */
void sweepServo(int fromAngle, int toAngle, uint8_t ledPin) {
  int       step         = (toAngle > fromAngle) ? 1 : -1;
  bool      ledState     = false;
  unsigned long lastBlink = millis();

  for (int angle = fromAngle; angle != toAngle + step; angle += step) {
    gateServo.write(angle);
    currentAngle = angle;

    // Non-blocking blink logic
    if (millis() - lastBlink >= BLINK_INTERVAL_MS) {
      ledState  = !ledState;
      lastBlink  = millis();
      if (ledState) {
        rgbSolid(ledPin);
      } else {
        rgbOff();
      }
    }

    delay(SWEEP_STEP_DELAY_MS);
  }

  // Make sure the LED is ON (solid) after sweep completes
  rgbOff();
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println("  Smart Gate System — Booting...");
  Serial.println("========================================");

  // GPIO setup
  pinMode(TRIG_PIN,   OUTPUT);
  pinMode(ECHO_PIN,   INPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_BLUE,   OUTPUT);
  rgbOff();

  // Servo initialisation (ESP32Servo needs a timer slot)
  ESP32PWM::allocateTimer(0);
  gateServo.setPeriodHertz(50);           // Standard 50 Hz servo
  gateServo.attach(SERVO_PIN, 500, 2400); // 500–2400 µs pulse range

  // Move to known CLOSED position on startup
  Serial.println("  Initialising: Moving servo to CLOSED (90°)...");
  gateServo.write(GATE_CLOSE_ANGLE);
  currentAngle = GATE_CLOSE_ANGLE;
  delay(800);  // Allow servo to reach position

  gateState = STATE_CLOSED;
  Serial.println("  Gate: CLOSED  |  Waiting for vehicle...\n");
}

// ── Main Loop ────────────────────────────────────────────────
void loop() {
  long distance = measureDistanceCm();

  // ── Serial debug output ──────────────────────────────────
  Serial.print("[State: ");
  switch (gateState) {
    case STATE_CLOSED:  Serial.print("CLOSED "); break;
    case STATE_OPENING: Serial.print("OPENING"); break;
    case STATE_OPEN:    Serial.print("OPEN   "); break;
    case STATE_CLOSING: Serial.print("CLOSING"); break;
  }
  Serial.print("]  Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // ── State Machine ────────────────────────────────────────
  switch (gateState) {

    // ── CLOSED: Poll sensor, open on vehicle detection ──────
    case STATE_CLOSED:
      rgbOff();
      if (distance <= DETECT_DISTANCE_CM) {
        Serial.println(">>> Vehicle detected! Opening gate...");
        gateState = STATE_OPENING;
        // No delay here — handled in STATE_OPENING
      }
      break;

    // ── OPENING: Sweep 90° → 0° with RED blink ──────────────
    case STATE_OPENING:
      sweepServo(GATE_CLOSE_ANGLE, GATE_OPEN_ANGLE, LED_RED);
      rgbSolid(LED_GREEN);          // GREEN solid — gate is fully open
      openTimestamp = millis();
      gateState     = STATE_OPEN;
      Serial.println(">>> Gate fully OPEN. Holding for 5 seconds...");
      break;

    // ── OPEN: Hold open 5 s, then check if path is clear ────
    case STATE_OPEN:
      rgbSolid(LED_GREEN);          // Stay GREEN while open

      if (millis() - openTimestamp >= GATE_HOLD_MS) {
        // Re-measure before deciding to close
        long checkDist = measureDistanceCm();
        if (checkDist > DETECT_DISTANCE_CM) {
          Serial.println(">>> Path clear. Closing gate...");
          gateState = STATE_CLOSING;
        } else {
          // Vehicle still present — extend hold time
          Serial.println(">>> Vehicle still present. Extending hold...");
          openTimestamp = millis();   // Reset 5-second timer
        }
      }
      break;

    // ── CLOSING: Sweep 0° → 90° with BLUE blink ─────────────
    case STATE_CLOSING:
      sweepServo(GATE_OPEN_ANGLE, GATE_CLOSE_ANGLE, LED_BLUE);
      rgbOff();
      gateState = STATE_CLOSED;
      Serial.println(">>> Gate fully CLOSED. System ready.\n");
      break;
  }

  delay(SENSOR_SAMPLE_DELAY);
}
