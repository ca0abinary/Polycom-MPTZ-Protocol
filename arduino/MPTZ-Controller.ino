// MPTZ Controller
// Display: 2 row 16 column HD44780 with PCF8574 I2C controller
// Input  : Rotary encoder (CLK, DI, SW, VCC, GND)
// Camera : Serial (9600 8-E-1)
// Tested with MPTZ-6
#include <Arduino.h>
#include <HD44780_LCD_PCF8574.h>
#include <EncoderTool.h>
// https://github.com/ljbeng/SoftwareSerialParity
#include <SoftwareSerialParity.h>

using namespace EncoderTool;

// Defines
// LCD A4 = SCL, A5 = SDA
#define LCD_ROWS        2
#define LCD_COLS        16
#define LCD_I2C_ADDRESS 0x27
#define ROTARY_PIN_A    PIN_A1
#define ROTARY_PIN_B    PIN_A2
#define ROTARY_BUTTON   PIN_A3
#define SERIAL_RX       2
#define SERIAL_TX       3
#define MAX_STATE       5
#define MPTZ_PAN        0x04
#define MPTZ_TILT       0x05
#define MPTZ_ZOOM       0x02

// Devices
PolledEncoder rotary;
SoftwareSerialParity serial(SERIAL_RX, SERIAL_TX);
HD44780LCD lcd(LCD_ROWS, LCD_COLS, LCD_I2C_ADDRESS);

// Globals
byte pan  = 0x06, pan_step  = 0xC0;
byte tilt = 0x02, tilt_step = 0x00;
byte zoom = 0x08, zoom_step = 0x00;
byte state = 0, state_changed = 0, value_changed = 0;

void setup() {
  // Init software serial port to 9600 8-E-1
  serial.begin(9600, EVEN);

  // Init rotary encoder
  rotary.begin(ROTARY_PIN_A, ROTARY_PIN_B, ROTARY_BUTTON);
  rotary.setCountMode(CountMode::halfAlt);

  // Init 16x2 LCD
  lcd.PCF8574_LCDInit(LCDCursorTypeOnBlink);
  lcd.PCF8574_LCDClearScreen();
  lcd.PCF8574_LCDBackLightSet(true);

  wait_for_camera();

  // Init to default state
  mptz_center();
  process_state();
  draw();
}

void loop() {
  value_changed = 0;
  state_changed = 0;

  rotary.tick();

  if (rotary.valueChanged()) {
    value_changed = 1;
  }

  if (rotary.buttonChanged() && rotary.getButton() == 0) {
    state++;
    if (state > MAX_STATE) {
      state = 0;
    }
    state_changed = 1;
  }

  if (value_changed == 1 || state_changed == 1) {
    process_state();
    draw();
  }
}

void draw() {
  lcd.PCF8574_LCDResetScreen(LCDCursorTypeOnBlink);

  // Draw static elements
  lcd.PCF8574_LCDGOTO(LCDLineNumberOne, 0);
  lcd.print("Pan : ");
  lcd.PCF8574_LCDGOTO(LCDLineNumberOne, 12);
  lcd.print("Z:");
  lcd.PCF8574_LCDGOTO(LCDLineNumberTwo, 0);
  lcd.print("Tilt: ");
  lcd.PCF8574_LCDGOTO(LCDLineNumberTwo, 12);
  lcd.print("Z:");

  // Pan
  lcd.PCF8574_LCDGOTO(LCDLineNumberOne, 6);
  PrintHex8(pan);
  lcd.PCF8574_LCDMoveCursor(LCDMoveRight, 1);
  PrintHex8(pan_step);

  // Tilt
  lcd.PCF8574_LCDGOTO(LCDLineNumberTwo, 6);
  PrintHex8(tilt);
  lcd.PCF8574_LCDMoveCursor(LCDMoveRight, 1);
  PrintHex8(tilt_step);

  // Zoom
  lcd.PCF8574_LCDGOTO(LCDLineNumberOne, 14);
  PrintHex8(zoom);
  lcd.PCF8574_LCDGOTO(LCDLineNumberTwo, 14);
  PrintHex8(zoom_step);

  // Set cursor position
  switch (state) {
    case 0: // Pan Step
      lcd.PCF8574_LCDGOTO(LCDLineNumberOne, 0);
      lcd.PCF8574_LCDMoveCursor(LCDMoveRight, 7);
      break;
    case 1: // Pan Sub-step
      lcd.PCF8574_LCDGOTO(LCDLineNumberOne, 0);
      lcd.PCF8574_LCDMoveCursor(LCDMoveRight, 10);
      break;
    case 2: // Tilt Step
      lcd.PCF8574_LCDGOTO(LCDLineNumberTwo, 0);
      lcd.PCF8574_LCDMoveCursor(LCDMoveRight, 7);
      break;
    case 3: // Tilt Sub-step
      lcd.PCF8574_LCDGOTO(LCDLineNumberTwo, 0);
      lcd.PCF8574_LCDMoveCursor(LCDMoveRight, 10);
      break;
    case 4: // Zoom Step
      lcd.PCF8574_LCDGOTO(LCDLineNumberOne, 0);
      lcd.PCF8574_LCDMoveCursor(LCDMoveRight, 15);
      break;
    case 5: // Zoom Sub-step
      lcd.PCF8574_LCDGOTO(LCDLineNumberTwo, 0);
      lcd.PCF8574_LCDMoveCursor(LCDMoveRight, 15);
      break;
  }
}

void PrintHex8(uint8_t data)
{
  char tmp[16];
  sprintf(tmp, "%.2X", data); 
  lcd.print(tmp);
}

void process_state() {
  byte rotary_value = rotary.getValue();
  switch (state) {
    // Pan Step
    case 0:
      if (state_changed == 1) {
        rotary.setLimits(0x00, 0x0f);
        rotary.setValue(pan);
      } else {
        pan = rotary_value;
        mptz_move(MPTZ_PAN, pan, pan_step);
      }
      break;
    // Pan Sub-step
    case 1:
      if (state_changed == 1) {
        rotary.setLimits(0x00, 0xcf);
        rotary.setValue(pan_step);
      } else {
        pan_step = rotary_value;
        mptz_move(MPTZ_PAN, pan, pan_step);
      }
      break;
    // Tilt Step
    case 2:
      if (state_changed == 1) {
        rotary.setLimits(0x01, 0x02);
        rotary.setValue(tilt);
      } else {
        tilt = rotary_value;
        mptz_move(MPTZ_TILT, tilt, tilt_step);
      }
      break;
    // Tilt Sub-step
    case 3:
      if (state_changed == 1) {
        rotary.setLimits(0x00, 0xff);
        rotary.setValue(tilt_step);
      } else {
        tilt_step = rotary_value;
        mptz_move(MPTZ_TILT, tilt, tilt_step);
      }
      break;
    // Zoom Step
    case 4:
      if (state_changed == 1) {
        rotary.setLimits(0x01, 0x11);
        rotary.setValue(zoom);
      } else {
        zoom = rotary_value;
        mptz_move(MPTZ_ZOOM, zoom, zoom_step);
      }
      break;
    // Zoom Sub-step
    case 5:
      if (state_changed == 1) {
        rotary.setLimits(0x00, 0xff);
        rotary.setValue(zoom_step);
      } else {
        zoom_step = rotary_value;
        mptz_move(MPTZ_ZOOM, zoom, zoom_step);
      }
      break;
  }
}

void wait_for_camera() {
  lcd.PCF8574_LCDClearScreen();
  lcd.PCF8574_LCDGOTO(LCDLineNumberOne, 0);
  lcd.print("Wait for cam");
  lcd.PCF8574_LCDGOTO(LCDLineNumberTwo, 0);
  lcd.print("Click to break");
  lcd.PCF8574_LCDGOTO(LCDLineNumberOne, 14);

  // Clear buffer
  while (serial.available() > 0) {
    serial.read();
  }

  // Wait for camera to signal
  while (true) {
    rotary.tick();

    if (serial.available() > 0) {
      byte b = serial.read();
      PrintHex8(b);
      lcd.PCF8574_LCDMoveCursor(LCDMoveLeft, 2);
      if (b == 0xe0) {
        mptz_init();
        break;
      } 
    } else if (rotary.buttonChanged() && rotary.getButton() == 0) {
      state_changed = 1;
      break;
    }
  }
}

void mptz_init() {
  serial.write(0x81);
  serial.write(0x40);
  serial.flush();
  delay(100);
}

void mptz_center() {
  mptz_move(MPTZ_PAN, pan, pan_step);
  mptz_move(MPTZ_TILT, tilt, tilt_step);
  mptz_move(MPTZ_ZOOM, zoom, zoom_step);
}

void mptz_move(byte cmd, byte msb, byte lsb) {
  serial.write(0x84);
  serial.write(0x43);
  serial.write(cmd);
  serial.write(msb);
  serial.write(lsb);
  serial.flush();
  delay(100);
}
