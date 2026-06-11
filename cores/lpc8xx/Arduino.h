// SPDX-License-Identifier: BSD-3-Clause
// ArduinoCore-LPC8xx - umbrella header.
#ifndef ARDUINO_CORE_LPC8XX_ARDUINO_H
#define ARDUINO_CORE_LPC8XX_ARDUINO_H

#include <stdint.h>
#include <stddef.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include "variant.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HIGH 0x1
#define LOW  0x0

#define INPUT        0x0
#define OUTPUT       0x1
#define INPUT_PULLUP 0x2

#define CHANGE 1
#define FALLING 2
#define RISING 3

#define NOT_A_PIN 0xFF
#define NOT_AN_INTERRUPT -1

#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
// #define abs(x) ((x)>0?(x):-(x))  // Commented out to avoid conflict with C++ stdlib
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define radians(deg) ((deg)*DEG_TO_RAD)
#define degrees(rad) ((rad)*RAD_TO_DEG)
#define sq(x) ((x)*(x))

#define bit(b) (1UL << (b))
#define bitRead(value, bitnum) (((value) >> (bitnum)) & 0x01)
#define bitSet(value, bitnum) ((value) |= (1UL << (bitnum)))
#define bitClear(value, bitnum) ((value) &= ~(1UL << (bitnum)))
#define bitWrite(value, bitnum, bitvalue) ((bitvalue) ? bitSet(value, bitnum) : bitClear(value, bitnum))

typedef uint8_t byte;
typedef bool boolean;
typedef uint8_t pin_size_t;
typedef uint8_t PinStatus;
typedef uint8_t PinMode;

void pinMode(pin_size_t pin, PinMode mode);
void digitalWrite(pin_size_t pin, PinStatus value);
PinStatus digitalRead(pin_size_t pin);

void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
uint32_t millis(void);
uint32_t micros(void);
void yield(void);
long map(long value, long fromLow, long fromHigh, long toLow, long toHigh);

int analogRead(pin_size_t pin);
void analogReadResolution(int bits);
void analogWrite(pin_size_t pin, int value);
void analogWriteResolution(int bits);

int digitalPinToInterrupt(pin_size_t pin);
void attachInterrupt(int interruptNum, void (*userFunc)(void), int mode);
void detachInterrupt(int interruptNum);

void tone(pin_size_t pin, unsigned int frequency, unsigned long duration);
void noTone(pin_size_t pin);

void init(void);
void initVariant(void);
void init_systick(void);

void setup(void);
void loop(void);

#ifdef __cplusplus
}

#define F(str) (str)

#include "Printable.h"
#include "Print.h"
#include "Stream.h"
#include "WString.h"
#include "HardwareSerial.h"
#include "SPI.h"
#include "Wire.h"

#endif

#endif
