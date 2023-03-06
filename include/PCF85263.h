/**************************************************************************/
/*!
    @file     PCF85263.h
    This is a library for the Tiny Real-Time Clock/calendar with alarm function, 
    battery switch-over, time stamp input, and I2C-bus.
    Written by Fabian Voelker for promesstec GmbH.
    MIT license, all text here must be included in any redistribution.
*/
/**************************************************************************/


#ifndef __PCF85263_H__
#define __PCF85263_H__


#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#include <Adafruit_I2CDevice.h>
#include <SPI.h>
#include <Wire.h>

class TimeSpan;

#define SECONDS_PER_DAY             86400L    //< 60 * 60 * 24
#define SECONDS_FROM_1970_TO_2000   946684800 //< Unixtime for 2000-01-01 00:00:00, useful for initialization


class DateTime {
public:
  DateTime(uint32_t t = SECONDS_FROM_1970_TO_2000);
  DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour = 0,
           uint8_t min = 0, uint8_t sec = 0);
  DateTime(const DateTime &copy);
  DateTime(const char *date, const char *time);
  DateTime(const __FlashStringHelper *date, const __FlashStringHelper *time);
  DateTime(const char *iso8601date);
  bool isValid() const;
  char *toString(char *buffer) const;

  /*!
      @brief  Return the year.
      @return Year (range: 2000--2099).
  */
  uint16_t year() const { return 2000U + yOff; }
  /*!
      @brief  Return the month.
      @return Month number (1--12).
  */
  uint8_t month() const { return m; }
  /*!
      @brief  Return the day of the month.
      @return Day of the month (1--31).
  */
  uint8_t day() const { return d; }
  /*!
      @brief  Return the hour
      @return Hour (0--23).
  */
  uint8_t hour() const { return hh; }

  uint8_t twelveHour() const;
  /*!
      @brief  Return whether the time is PM.
      @return 0 if the time is AM, 1 if it's PM.
  */
  uint8_t isPM() const { return hh >= 12; }
  /*!
      @brief  Return the minute.
      @return Minute (0--59).
  */
  uint8_t minute() const { return mm; }
  /*!
      @brief  Return the second.
      @return Second (0--59).
  */
  uint8_t second() const { return ss; }

  uint8_t dayOfTheWeek() const;

  /* 32-bit times as seconds since 2000-01-01. */
  uint32_t secondstime() const;

  /* 32-bit times as seconds since 1970-01-01. */
  uint32_t unixtime(void) const;

  /*!
      Format of the ISO 8601 timestamp generated by `timestamp()`. Each
      option corresponds to a `toString()` format as follows:
  */
  enum timestampOpt {
    TIMESTAMP_FULL, //!< `YYYY-MM-DDThh:mm:ss`
    TIMESTAMP_TIME, //!< `hh:mm:ss`
    TIMESTAMP_DATE  //!< `YYYY-MM-DD`
  };
  String timestamp(timestampOpt opt = TIMESTAMP_FULL) const;

  DateTime operator+(const TimeSpan &span) const;
  DateTime operator-(const TimeSpan &span) const;
  TimeSpan operator-(const DateTime &right) const;
  bool operator<(const DateTime &right) const;

  /*!
      @brief  Test if one DateTime is greater (later) than another.
      @warning if one or both DateTime objects are invalid, returned value is meaningless
      @see use `isValid()` method to check if DateTime object is valid
      @param right DateTime object to compare
      @return True if the left DateTime is later than the right one, false otherwise
  */
  bool operator>(const DateTime &right) const { return right < *this; }

  /*!
      @brief  Test if one DateTime is less (earlier) than or equal to another
      @warning if one or both DateTime objects are invalid, returned value is meaningless
      @see use `isValid()` method to check if DateTime object is valid
      @param right DateTime object to compare
      @return True if the left DateTime is earlier than or equal to the right one, false otherwise
  */
  bool operator<=(const DateTime &right) const { return !(*this > right); }

  /*!
      @brief  Test if one DateTime is greater (later) than or equal to another
      @warning if one or both DateTime objects are invalid, returned value is meaningless
      @see use `isValid()` method to check if DateTime object is valid
      @param right DateTime object to compare
      @return True if the left DateTime is later than or equal to the right one, false otherwise
  */
  bool operator>=(const DateTime &right) const { return !(*this < right); }
  bool operator==(const DateTime &right) const;

  /*!
      @brief  Test if two DateTime objects are not equal.
      @warning if one or both DateTime objects are invalid, returned value is meaningless
      @see use `isValid()` method to check if DateTime object is valid
      @param right DateTime object to compare
      @return True if the two objects are not equal, false if they are
  */
  bool operator!=(const DateTime &right) const { return !(*this == right); }

protected:
  uint8_t yOff; ///< Year offset from 2000
  uint8_t m;    ///< Month 1-12
  uint8_t d;    ///< Day 1-31
  uint8_t hh;   ///< Hours 0-23
  uint8_t mm;   ///< Minutes 0-59
  uint8_t ss;   ///< Seconds 0-59
};

/**************************************************************************/
/*!
    @brief  Timespan which can represent changes in time with seconds accuracy.
*/
/**************************************************************************/
class TimeSpan {
public:
  TimeSpan(int32_t seconds = 0);
  TimeSpan(int16_t days, int8_t hours, int8_t minutes, int8_t seconds);
  TimeSpan(const TimeSpan &copy);

  /*!
      @brief  Number of days in the TimeSpan e.g. 4
      @return int16_t days
  */
  int16_t days() const { return _seconds / 86400L; }
  /*!
      @brief  Number of hours in the TimeSpan
              This is not the total hours, it includes the days
              e.g. 4 days, 3 hours - NOT 99 hours
      @return int8_t hours
  */
  int8_t hours() const { return _seconds / 3600 % 24; }
  /*!
      @brief  Number of minutes in the TimeSpan
              This is not the total minutes, it includes days/hours
              e.g. 4 days, 3 hours, 27 minutes
      @return int8_t minutes
  */
  int8_t minutes() const { return _seconds / 60 % 60; }
  /*!
      @brief  Number of seconds in the TimeSpan
              This is not the total seconds, it includes the days/hours/minutes
              e.g. 4 days, 3 hours, 27 minutes, 7 seconds
      @return int8_t seconds
  */
  int8_t seconds() const { return _seconds % 60; }
  /*!
      @brief  Total number of seconds in the TimeSpan, e.g. 358027
      @return int32_t seconds
  */
  int32_t totalseconds() const { return _seconds; }

  TimeSpan operator+(const TimeSpan &right) const;
  TimeSpan operator-(const TimeSpan &right) const;

protected:
  int32_t _seconds; ///< Actual TimeSpan value is stored as seconds
};


/**************************************************************************/
/*!
    @brief  A generic I2C RTC base class. DO NOT USE DIRECTLY
*/
/**************************************************************************/
class RTC_I2C {
protected:
  /*!
      @brief  Convert a binary coded decimal value to binary. RTC stores time/date values as BCD.
      @param val BCD value
      @return Binary value
  */
  static uint8_t bcd2bin(uint8_t val) { return val - 6 * (val >> 4); }
  /*!
      @brief  Convert a binary value to BCD format for the RTC registers
      @param val Binary value
      @return BCD value
  */
  static uint8_t bin2bcd(uint8_t val) { return val + 6 * (val / 10); }
  Adafruit_I2CDevice *i2c_dev = NULL; ///< Pointer to I2C bus interface
  uint8_t read_register(uint8_t reg);
  void write_register(uint8_t reg, uint8_t val);
};





#define PCF85263_ADDRESS            0x51    //< I2C address for PCF8563

/* Time - Registers */
#define PCF85263_100TH_SECONDS      0x00    //< PCF85263-Register 100th seconds
#define PCF85263_SECOND             0x01    //< PCF85263-Register seconds
#define PCF85263_MINUTE             0x02    //< PCF85263-Register minutes
#define PCF85263_HOUR               0x03    //< PCF85263-Register hours
#define PCF85263_DAY                0x04    //< PCF85263-Register days
#define PCF85263_WEEKDAY            0x05    //< PCF85263-Register weekdays
#define PCF85263_MONTH              0x06    //< PCF85263-Register months
#define PCF85263_YEAR               0x07    //< PCF85263-Register years

/* Alarm1 Time - Registers */
#define PCF85263_ALM1_SECONDS       0x08    //< PCF85263-Register Alarm1 seconds
#define PCF85263_ALM1_MINUTE        0x09    //< PCF85263-Register Alarm1 minutes
#define PCF85263_ALM1_HOUR          0x0A    //< PCF85263-Register Alarm1 hours
#define PCF85263_ALM1_DAY           0x0B    //< PCF85263-Register Alarm1 days
#define PCF85263_ALM1_MONTH         0x0C    //< PCF85263-Register Alarm1 months

/* Alarm2 Time - Registers */
#define PCF85263_ALM2_SECONDS       0x0D    //< PCF85263-Register Alarm2 seconds
#define PCF85263_ALM2_MINUTE        0x0E    //< PCF85263-Register Alarm2 minutes
#define PCF85263_ALM2_WEEKDAY       0x0F    //< PCF85263-Register Alarm2 weekday

/* Alarm Enable - Register */
#define PCF85263_ALMEN              0x10    //< PCF85263-Register Alarm Enable

/* Timestamp 1 - Registers */
#define PCF85263_TSTMP1_SECONDS     0x11    //< PCF85263-Register Timestamp1 seconds
#define PCF85263_TSTMP1_MINUTE      0x12    //< PCF85263-Register Timestamp1 minutes
#define PCF85263_TSTMP1_HOUR        0x13    //< PCF85263-Register Timestamp1 hours
#define PCF85263_TSTMP1_DAY         0x14    //< PCF85263-Register Timestamp1 days
#define PCF85263_TSTMP1_MONTH       0x15    //< PCF85263-Register Timestamp1 months
#define PCF85263_TSTMP1_YEAR        0x16    //< PCF85263-Register Timestamp1 years

/* Timestamp 2 - Registers */
#define PCF85263_TSTMP2_SECONDS     0x17    //< PCF85263-Register Timestamp2 seconds
#define PCF85263_TSTMP2_MINUTE      0x18    //< PCF85263-Register Timestamp2 minutes
#define PCF85263_TSTMP2_HOUR        0x19    //< PCF85263-Register Timestamp2 hours
#define PCF85263_TSTMP2_DAY         0x1A    //< PCF85263-Register Timestamp2 days
#define PCF85263_TSTMP2_MONTH       0x1B    //< PCF85263-Register Timestamp2 months
#define PCF85263_TSTMP2_YEAR        0x1C    //< PCF85263-Register Timestamp2 years

/* Timestamp 3 - Registers */
#define PCF85263_TSTMP3_SECONDS     0x1D    //< PCF85263-Register Timestamp3 seconds
#define PCF85263_TSTMP3_MINUTE      0x1E    //< PCF85263-Register Timestamp3 minutes
#define PCF85263_TSTMP3_HOUR        0x1F    //< PCF85263-Register Timestamp3 hours
#define PCF85263_TSTMP3_DAY         0x20    //< PCF85263-Register Timestamp3 days
#define PCF85263_TSTMP3_MONTH       0x21    //< PCF85263-Register Timestamp3 months
#define PCF85263_TSTMP3_YEAR        0x22    //< PCF85263-Register Timestamp3 years

/* Timestamp Control - Register */
#define PCF85263_TSTMP_Control      0x23    //< PCF85263-Register Timestamp Control

/* Control & Function - Registers */
#define PCF85263_OFFSET             0x24    //< PCF85263-Register Offset
#define PCF85263_OSC                0x25    //< PCF85263-Register Oscillator
#define PCF85263_BATSW              0x26    //< PCF85263-Register Battery Switch
#define PCF85263_PINIO              0x27    //< PCF85263-Register Pin IO Config
#define PCF85263_FUNCT              0x28    //< PCF85263-Register Function
#define PCF85263_INTAEN             0x29    //< PCF85263-Register INT A Enable
#define PCF85263_INTBEN             0x2A    //< PCF85263-Register INT B Enable
#define PCF85263_FLAGS              0x2B    //< PCF85263-Register Flags
#define PCF85263_RAM                0x2C    //< PCF85263-Register RAM Byte
#define PCF85263_WD                 0x2D    //< PCF85263-Register Watchdog 
#define PCF85263_STOPEN             0x2E    //< PCF85263-Register STOP Enable
#define PCF85263_RESETS             0x2F    //< PCF85263-Register Resets


class PCF85263 : RTC_I2C
{
public:
  ~PCF85263();
  bool begin(TwoWire *wireInstance = &Wire);
  void start(void);
  void stop(void);
  void adjust(const DateTime &dt);
  DateTime now();

};


#endif