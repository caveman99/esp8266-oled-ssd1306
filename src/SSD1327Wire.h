/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 by ThingPulse, Daniel Eichhorn
 * Copyright (c) 2018 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ThingPulse invests considerable time and money to develop these open source libraries.
 * Please support us by buying our products (and not the clones) from
 * https://thingpulse.com
 *
 */

#ifndef SSD1327Wire_h
#define SSD1327Wire_h

#define SSD1327_SETCOLUMN 0x15

#define SSD1327_SETROW 0x75

#define SSD1327_SETCONTRAST 0x81

#define SSD1327_SEGREMAP 0xA0
#define SSD1327_SETSTARTLINE 0xA1
#define SSD1327_SETDISPLAYOFFSET 0xA2
#define SSD1327_NORMALDISPLAY 0xA4
#define SSD1327_DISPLAYALLOFF 0xA6
#define SSD1327_SETMULTIPLEX 0xA8
#define SSD1327_REGULATOR 0xAB
#define SSD1327_DISPLAYOFF 0xAE
#define SSD1327_DISPLAYON 0xAF

#define SSD1327_PHASELEN 0xB1
#define SSD1327_DCLK 0xB3
#define SSD1327_PRECHARGE2 0xB6
#define SSD1327_PRECHARGE 0xBC
#define SSD1327_SETVCOM 0xBE

#define SSD1327_FUNCSELB 0xD5

#define SSD1327_CMDLOCK 0xFD

#include "OLEDDisplay.h"
#include <Wire.h>

//--------------------------------------

class SSD1327Wire : public OLEDDisplay {
  private:
      uint8_t             _address;
      int             _sda;
      int             _scl;
      bool                _doI2cAutoInit = false;
      TwoWire*            _wire = NULL;
      int                 _frequency;

  public:
    /**
     * Create and initialize the Display using Wire library
     *
     * Beware for retro-compatibility default values are provided for all parameters see below.
     * Please note that if you don't wan't SD1306Wire to initialize and change frequency speed ot need to 
     * ensure -1 value are specified for all 3 parameters. This can be usefull to control TwoWire with multiple
     * device on the same bus.
     * 
     * @param _address I2C Display address
     * @param _sda I2C SDA pin number, default to -1 to skip Wire begin call
     * @param _scl I2C SCL pin number, default to -1 (only SDA = -1 is considered to skip Wire begin call)
     * @param g display geometry dafault to generic GEOMETRY_128_64, see OLEDDISPLAY_GEOMETRY definition for other options
     * @param _i2cBus on ESP32 with 2 I2C HW buses, I2C_ONE for 1st Bus, I2C_TWO fot 2nd bus, default I2C_ONE
     * @param _frequency for Frequency by default Let's use ~700khz if ESP8266 is in 160Mhz mode, this will be limited to ~400khz if the ESP8266 in 80Mhz mode
     */
    SSD1327Wire(uint8_t _address, int _sda = -1, int _scl = -1, OLEDDISPLAY_GEOMETRY g = GEOMETRY_128_128, HW_I2C _i2cBus = I2C_ONE, int _frequency = 700000) {
      setGeometry(g);

      this->_address = _address;
      this->_sda = _sda;
      this->_scl = _scl;
#if !defined(ARDUINO_ARCH_ESP32)
      this->_wire = &Wire;
#else
      this->_wire = (_i2cBus==I2C_ONE) ? &Wire : &Wire1;
#endif
      this->_frequency = _frequency;
    }

    bool connect() {
#if !defined(ARDUINO_ARCH_ESP32) && !defined(ARDUINO_ARCH8266)
      _wire->begin();
#else
      // On ESP32 arduino, -1 means 'don't change pins', someone else has called begin for us.
      if(this->_sda != -1)
        _wire->begin(this->_sda, this->_scl);
#endif
      // Let's use ~700khz if ESP8266 is in 160Mhz mode
      // this will be limited to ~400khz if the ESP8266 in 80Mhz mode.
      if(this->_frequency != -1)
        _wire->setClock(this->_frequency);
      return true;
    }

    void display(void) {
      initI2cIfNeccesary();
#ifdef OLEDDISPLAY_DOUBLE_BUFFER
      uint8_t x, y, out;

      _wire->beginTransmission(_address);
      _wire->write(0x00);
      _wire->write(SSD1327_SETROW);
      _wire->write(0x00);
      _wire->write(0x7F);
      _wire->write(SSD1327_SETCOLUMN);
      _wire->write(0x00);
      _wire->write(0x3F);
      _wire->endTransmission();

      for (y = 0; y < displayHeight; y++) {
        _wire->beginTransmission(_address);
        _wire->write(0x40);
        out = 0;
        for (x = 0; x < displayWidth; x++) {
          // get src pixel in the page based ordering the OLED lib uses FIXME, super inefficent
          auto isset = buffer[x + (y / 8) * displayWidth] & (1 << (y & 7));
          auto dblbuf_isset = buffer_back[x + (y / 8) * displayWidth] & (1 << (y & 7));
          if (isset != dblbuf_isset) {
            if (y % 2) { // odd
              out += isset ? 0x0F : 0x00;
              _wire->write(out);
              out = 0;
            } else { //even
              out = isset ? 0xF0 : 0x00;
            }
          }
        }
        _wire->endTransmission();
      }
      // Copy the Buffer to the Back Buffer
      for (y = 0; y < (displayHeight / 8); y++) {
          for (x = 0; x < displayWidth; x++) {
              uint16_t pos = x + y * displayWidth;
              buffer_back[pos] = buffer[pos];
          }
      }
#else
        uint8_t * p = &buffer[0];
        for (uint8_t y=0; y<8; y++) {
          sendCommand(0xB0+y);
          sendCommand(0x02);
          sendCommand(0x10);
          for( uint8_t x=0; x<8; x++) {
            _wire->beginTransmission(_address);
            _wire->write(0x40);
            for (uint8_t k = 0; k < 16; k++) {
              _wire->write(*p++);
            }
            _wire->endTransmission();
          }
        }
#endif
    }

    void setI2cAutoInit(bool doI2cAutoInit) {
      _doI2cAutoInit = doI2cAutoInit;
    }

  protected:
    // Send all the init commands
    virtual void sendInitCommands()
    {
      _wire->beginTransmission(_address);
      _wire->write(0x00);
      _wire->write(SSD1327_DISPLAYOFF);
      _wire->write(SSD1327_SETCONTRAST);
      _wire->write(0x80);
      _wire->write(SSD1327_SEGREMAP);
      _wire->write(0x51);
      _wire->write(SSD1327_SETSTARTLINE);
      _wire->write(0x00);
      _wire->write(SSD1327_SETDISPLAYOFFSET);
      _wire->write(0x00);
      _wire->write(SSD1327_DISPLAYALLOFF);
      _wire->write(SSD1327_SETMULTIPLEX);
      _wire->write(0x7F);
      _wire->write(SSD1327_PHASELEN);
      _wire->write(0x11);
      _wire->write(SSD1327_DCLK);
      _wire->write(0x00);
      _wire->write(SSD1327_REGULATOR);
      _wire->write(0x01);
      _wire->write(SSD1327_PRECHARGE2);
      _wire->write(0x04);
      _wire->write(SSD1327_SETVCOM);
      _wire->write(0x0F);
      _wire->write(SSD1327_PRECHARGE);
      _wire->write(0x08);
      _wire->write(SSD1327_FUNCSELB);
      _wire->write(0x62);
      _wire->write(SSD1327_CMDLOCK);
      _wire->write(0x12);
      _wire->write(SSD1327_NORMALDISPLAY);
      _wire->write(SSD1327_DISPLAYON);
      _wire->endTransmission();
    }

  private:
	int getBufferOffset(void) {
		return 0;
	}
    inline void sendCommand(uint8_t command) __attribute__((always_inline)){
      _wire->beginTransmission(_address);
      _wire->write(0x00);
      _wire->write(command);
      _wire->endTransmission();
    }

    void initI2cIfNeccesary() {
      if (_doI2cAutoInit) {
#ifdef ARDUINO_ARCH_AVR
        _wire->begin();
#else
        _wire->begin(this->_sda, this->_scl);
#endif
      }
    }

};

#endif
