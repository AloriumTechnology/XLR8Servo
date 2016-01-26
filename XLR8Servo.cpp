/*--------------------------------------------------------------------
 Copyright (c) 2015 Alorim Technology.  All right reserved.
 This file is part of the Alorium Technology XLR8 Servo library.
 Written by Matt Weber (Matthew.D.Weber@ieee.org) of
   Alorium Technology (info@aloriumtech.com) using the same interface as
   the Arduino Servo library by Michael Margolis, but the XLR8
   implementation is modified to take advantage of the FPGA hardware
   acceleration available on the XLR8 board.

 XLR8 Servo is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as
 published by the Free Software Foundation, either version 3 of
 the License, or (at your option) any later version.
 XLR8 Servo is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
 You should have received a copy of the GNU Lesser General Public
 License along with XLR8 Servo.  If not, see
 <http://www.gnu.org/licenses/>.
 --------------------------------------------------------------------*/

#if defined(ARDUINO_ARCH_AVR)

#include <avr/interrupt.h>
#include <Arduino.h>

#include "XLR8Servo.h"

// Architecture specific
#if defined (__AVR_ATmega328P__)
#  define XLR8_SVCR    _SFR_MEM8(0xFA)
#  define XLR8_SVPWL   _SFR_MEM8(0xFC)
#  define XLR8_SVPWH   _SFR_MEM8(0xFD)
#  define XLR8_SVPW    _SFR_MEM16(0xFC)
#  define XLR8_SVEN  7 
#  define XLR8_SVDIS 6 
#  define XLR8_SVUP  5  
#else
#   warning "XLR8Servo not implemented for selected device"
#endif

static servo_t servos[MAX_SERVOS];                          // static array of servo structures
static volatile int8_t Channel[1 ];             // counter for the servo being pulsed for each timer (or -1 if refresh interval)

uint8_t ServoCount = 0;                                     // the total number of attached servos

#define SERVO_MIN() (MIN_PULSE_WIDTH - this->min * 4)  // minimum value in uS for this servo
#define SERVO_MAX() (MAX_PULSE_WIDTH - this->max * 4)  // maximum value in uS for this servo

Servo::Servo()
{
  if( ServoCount < MAX_SERVOS) {
    this->servoIndex = ServoCount++;                    // assign a servo index to this instance
	  servos[this->servoIndex].microseconds = DEFAULT_PULSE_WIDTH;   // store default values
  }
  else
    this->servoIndex = INVALID_SERVO ;  // too many servos
}

uint8_t Servo::attach(int pin)
{
  return this->attach(pin, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
}

uint8_t Servo::attach(int pin, int min, int max)
{
  if((this->servoIndex < MAX_SERVOS ) && (pin < MAX_SERVOS)) {
    pinMode( pin, OUTPUT) ;                                   // set servo pin to output
    servos[this->servoIndex].Pin.nbr = pin;
    // todo min/max check: abs(min - MIN_PULSE_WIDTH) /4 < 128
    this->min  = (MIN_PULSE_WIDTH - min)/4; //resolution of min/max is 4 uS
    this->max  = (MAX_PULSE_WIDTH - max)/4;
    // Start the hardware
    uint8_t oldSREG = SREG;
    cli();
    XLR8_SVPW = servos[this->servoIndex].microseconds;
    XLR8_SVCR = (1 << XLR8_SVEN) | (1 << XLR8_SVUP) | (servos[this->servoIndex].Pin.nbr & B11111);
    servos[this->servoIndex].Pin.isActive = true;
    SREG = oldSREG;
  }
  return this->servoIndex ;
}

void Servo::detach()
{
  servos[this->servoIndex].Pin.isActive = false;
  XLR8_SVCR = (1 << XLR8_SVDIS) | (servos[this->servoIndex].Pin.nbr & B11111);
}

void Servo::write(int value)
{
  if(value < MIN_PULSE_WIDTH)
  {  // treat values less than 544 as angles in degrees (valid values in microseconds are handled as microseconds)
    if(value < 0) value = 0;
    if(value > 180) value = 180;
    value = map(value, 0, 180, SERVO_MIN(),  SERVO_MAX());
  }
  this->writeMicroseconds(value);
}

void Servo::writeMicroseconds(int value)
{
  // calculate and store the values for the given channel
  byte channel = this->servoIndex;
  if( (channel < MAX_SERVOS) )   // ensure channel is valid
  {
    if( value < SERVO_MIN() )          // ensure pulse width is valid
      value = SERVO_MIN();
    else if( value > SERVO_MAX() )
      value = SERVO_MAX();

    uint8_t oldSREG = SREG;
    cli();
    servos[channel].microseconds = value;
    // Copy new value to the hardware
    XLR8_SVPW = servos[channel].microseconds;
    XLR8_SVCR = (1 << XLR8_SVUP) | (servos[channel].Pin.nbr & B11111);
    SREG = oldSREG;
  }
}

int Servo::read() // return the value as degrees
{
  return  map( this->readMicroseconds()+1, SERVO_MIN(), SERVO_MAX(), 0, 180);
}

int Servo::readMicroseconds()
{
  unsigned int pulsewidth;
  if( this->servoIndex != INVALID_SERVO )
    pulsewidth = servos[this->servoIndex].microseconds;
    // Hardware value should match software. Do this check for now, but could remove
    //  it later for better performance    
    unsigned int pulsewidth_check;
    XLR8_SVCR = (servos[this->servoIndex].Pin.nbr & B11111);
    pulsewidth_check = XLR8_SVPW;
    if (pulsewidth != pulsewidth_check) {pulsewidth = 32767;} // set to max for error indication
  else
    pulsewidth  = 0;

  return pulsewidth;
}

bool Servo::attached()
{
  // Hardware value should match software. Here's how to check the hardware if you need to. 
  //book enabled_check   
  //XLR8_SVCR = (servos[this->servoIndex].Pin.nbr & B11111);
  //enabled_check = (XLR8_SVCR & (1 << XLR8_SVEN));
  //servos[this->servoIndex].Pin.isActive = enabled_check;
  return servos[this->servoIndex].Pin.isActive ;
}

#endif // ARDUINO_ARCH_AVR

