/**************************************************************************/
/*!
    @file     PCF85263.cpp
    This is a library for the Low Voltage, Low Power, Factory-Calibrated 
    16-/24-Bit Dual AD7719 Analog to Digital Converter from Analog Devices.
    Written by Fabian Voelker for promesstec GmbH.
    MIT license, all text here must be included in any redistribution.
*/
/**************************************************************************/


#include "PCF85263.h"

#ifdef __AVR__
#include <avr/pgmspace.h>
#elif defined(ESP8266)
#include <pgmspace.h>
#elif defined(ARDUINO_ARCH_SAMD)
// nothing special needed
#elif defined(ARDUINO_SAM_DUE)
#define PROGMEM
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif

/**************************************************************************/
/*!
    @brief Write value to register.
    @param reg register address
    @param val value to write
*/
/**************************************************************************/
void RTC_I2C::write_register(uint8_t reg, uint8_t val) {
  uint8_t buffer[2] = {reg, val};
  i2c_dev->write(buffer, 2);
}

/**************************************************************************/
/*!
    @brief Read value from register.
    @param reg register address
    @return value of register
*/
/**************************************************************************/
uint8_t RTC_I2C::read_register(uint8_t reg) {
  uint8_t buffer[1];
  i2c_dev->write(&reg, 1);
  i2c_dev->read(buffer, 1);
  return buffer[0];
}

/**************************************************************************/
// utility code, some of this could be exposed in the DateTime API if needed
/**************************************************************************/

/**
  Number of days in each month, from January to November. December is not
  needed. Omitting it avoids an incompatibility with Paul Stoffregen's Time
  library. C.f. https://github.com/adafruit/RTClib/issues/114
*/
const uint8_t daysInMonth[] PROGMEM = {31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30};

/**************************************************************************/
/*!
    @brief  Given a date, return number of days since 2000/01/01,
            valid for 2000--2099
    @param y Year
    @param m Month
    @param d Day
    @return Number of days
*/
/**************************************************************************/
static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
  if (y >= 2000U)
    y -= 2000U;
  uint16_t days = d;
  for (uint8_t i = 1; i < m; ++i)
    days += pgm_read_byte(daysInMonth + i - 1);
  if (m > 2 && y % 4 == 0)
    ++days;
  return days + 365 * y + (y + 3) / 4 - 1;
}

/**************************************************************************/
/*!
    @brief  Given a number of days, hours, minutes, and seconds, return the
   total seconds
    @param days Days
    @param h Hours
    @param m Minutes
    @param s Seconds
    @return Number of seconds total
*/
/**************************************************************************/
static uint32_t time2ulong(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
  return ((days * 24UL + h) * 60 + m) * 60 + s;
}

/**************************************************************************/
/*!
    @brief  Constructor from
        [Unix time](https://en.wikipedia.org/wiki/Unix_time).
    This builds a DateTime from an integer specifying the number of seconds
    elapsed since the epoch: 1970-01-01 00:00:00. This number is analogous
    to Unix time, with two small differences:
     - The Unix epoch is specified to be at 00:00:00
       [UTC](https://en.wikipedia.org/wiki/Coordinated_Universal_Time),
       whereas this class has no notion of time zones. The epoch used in
       this class is then at 00:00:00 on whatever time zone the user chooses
       to use, ignoring changes in DST.
     - Unix time is conventionally represented with signed numbers, whereas
       this constructor takes an unsigned argument. Because of this, it does
       _not_ suffer from the
       [year 2038 problem](https://en.wikipedia.org/wiki/Year_2038_problem).
    If called without argument, it returns the earliest time representable
    by this class: 2000-01-01 00:00:00.
    @see The `unixtime()` method is the converse of this constructor.
    @param t Time elapsed in seconds since 1970-01-01 00:00:00.
*/
/**************************************************************************/
DateTime::DateTime(uint32_t t) {
  t -= SECONDS_FROM_1970_TO_2000; // bring to 2000 timestamp from 1970

  ss = t % 60;
  t /= 60;
  mm = t % 60;
  t /= 60;
  hh = t % 24;
  uint16_t days = t / 24;
  uint8_t leap;
  for (yOff = 0;; ++yOff) {
    leap = yOff % 4 == 0;
    if (days < 365U + leap)
      break;
    days -= 365 + leap;
  }
  for (m = 1; m < 12; ++m) {
    uint8_t daysPerMonth = pgm_read_byte(daysInMonth + m - 1);
    if (leap && m == 2)
      ++daysPerMonth;
    if (days < daysPerMonth)
      break;
    days -= daysPerMonth;
  }
  d = days + 1;
}

/**************************************************************************/
/*!
    @brief  Constructor from (year, month, day, hour, minute, second).
    @warning If the provided parameters are not valid (e.g. 31 February),
           the constructed DateTime will be invalid.
    @see   The `isValid()` method can be used to test whether the
           constructed DateTime is valid.
    @param year Either the full year (range: 2000--2099) or the offset from
        year 2000 (range: 0--99).
    @param month Month number (1--12).
    @param day Day of the month (1--31).
    @param hour,min,sec Hour (0--23), minute (0--59) and second (0--59).
*/
/**************************************************************************/
DateTime::DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour,
                   uint8_t min, uint8_t sec) {
  if (year >= 2000U)
    year -= 2000U;
  yOff = year;
  m = month;
  d = day;
  hh = hour;
  mm = min;
  ss = sec;
}

/**************************************************************************/
/*!
    @brief  Copy constructor.
    @param copy DateTime to copy.
*/
/**************************************************************************/
DateTime::DateTime(const DateTime &copy)
    : yOff(copy.yOff), m(copy.m), d(copy.d), hh(copy.hh), mm(copy.mm),
      ss(copy.ss) {}

/**************************************************************************/
/*!
    @brief  Convert a string containing two digits to uint8_t, e.g. "09" returns
   9
    @param p Pointer to a string containing two digits
*/
/**************************************************************************/
static uint8_t conv2d(const char *p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9')
    v = *p - '0';
  return 10 * v + *++p - '0';
}

/**************************************************************************/
/*!
    @brief  Constructor for generating the build time.
    This constructor expects its parameters to be strings in the format
    generated by the compiler's preprocessor macros `__DATE__` and
    `__TIME__`. Usage:
    ```
    DateTime buildTime(__DATE__, __TIME__);
    ```
    @note The `F()` macro can be used to reduce the RAM footprint, see
        the next constructor.
    @param date Date string, e.g. "Apr 16 2020".
    @param time Time string, e.g. "18:34:56".
*/
/**************************************************************************/
DateTime::DateTime(const char *date, const char *time) {
  yOff = conv2d(date + 9);
  // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
  switch (date[0]) {
  case 'J':
    m = (date[1] == 'a') ? 1 : ((date[2] == 'n') ? 6 : 7);
    break;
  case 'F':
    m = 2;
    break;
  case 'A':
    m = date[2] == 'r' ? 4 : 8;
    break;
  case 'M':
    m = date[2] == 'r' ? 3 : 5;
    break;
  case 'S':
    m = 9;
    break;
  case 'O':
    m = 10;
    break;
  case 'N':
    m = 11;
    break;
  case 'D':
    m = 12;
    break;
  }
  d = conv2d(date + 4);
  hh = conv2d(time);
  mm = conv2d(time + 3);
  ss = conv2d(time + 6);
}

/**************************************************************************/
/*!
    @brief  Memory friendly constructor for generating the build time.
    This version is intended to save RAM by keeping the date and time
    strings in program memory. Use it with the `F()` macro:
    ```
    DateTime buildTime(F(__DATE__), F(__TIME__));
    ```
    @param date Date PROGMEM string, e.g. F("Apr 16 2020").
    @param time Time PROGMEM string, e.g. F("18:34:56").
*/
/**************************************************************************/
DateTime::DateTime(const __FlashStringHelper *date,
                   const __FlashStringHelper *time) {
  char buff[11];
  memcpy_P(buff, date, 11);
  yOff = conv2d(buff + 9);
  // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
  switch (buff[0]) {
  case 'J':
    m = (buff[1] == 'a') ? 1 : ((buff[2] == 'n') ? 6 : 7);
    break;
  case 'F':
    m = 2;
    break;
  case 'A':
    m = buff[2] == 'r' ? 4 : 8;
    break;
  case 'M':
    m = buff[2] == 'r' ? 3 : 5;
    break;
  case 'S':
    m = 9;
    break;
  case 'O':
    m = 10;
    break;
  case 'N':
    m = 11;
    break;
  case 'D':
    m = 12;
    break;
  }
  d = conv2d(buff + 4);
  memcpy_P(buff, time, 8);
  hh = conv2d(buff);
  mm = conv2d(buff + 3);
  ss = conv2d(buff + 6);
}

/**************************************************************************/
/*!
    @brief  Constructor for creating a DateTime from an ISO8601 date string.
    This constructor expects its parameters to be a string in the
    https://en.wikipedia.org/wiki/ISO_8601 format, e.g:
    "2020-06-25T15:29:37"
    Usage:
    ```
    DateTime dt("2020-06-25T15:29:37");
    ```
    @note The year must be > 2000, as only the yOff is considered.
    @param iso8601dateTime
           A dateTime string in iso8601 format,
           e.g. "2020-06-25T15:29:37".
*/
/**************************************************************************/
DateTime::DateTime(const char *iso8601dateTime) {
  char ref[] = "2000-01-01T00:00:00";
  memcpy(ref, iso8601dateTime, min(strlen(ref), strlen(iso8601dateTime)));
  yOff = conv2d(ref + 2);
  m = conv2d(ref + 5);
  d = conv2d(ref + 8);
  hh = conv2d(ref + 11);
  mm = conv2d(ref + 14);
  ss = conv2d(ref + 17);
}

/**************************************************************************/
/*!
    @brief  Check whether this DateTime is valid.
    @return true if valid, false if not.
*/
/**************************************************************************/
bool DateTime::isValid() const {
  if (yOff >= 100)
    return false;
  DateTime other(unixtime());
  return yOff == other.yOff && m == other.m && d == other.d && hh == other.hh &&
         mm == other.mm && ss == other.ss;
}

/**************************************************************************/
/*!
    @brief  Writes the DateTime as a string in a user-defined format.
    The _buffer_ parameter should be initialized by the caller with a string
    specifying the requested format. This format string may contain any of
    the following specifiers:
    | specifier | output                                                 |
    |-----------|--------------------------------------------------------|
    | YYYY      | the year as a 4-digit number (2000--2099)              |
    | YY        | the year as a 2-digit number (00--99)                  |
    | MM        | the month as a 2-digit number (01--12)                 |
    | MMM       | the abbreviated English month name ("Jan"--"Dec")      |
    | DD        | the day as a 2-digit number (01--31)                   |
    | DDD       | the abbreviated English day of the week ("Mon"--"Sun") |
    | AP        | either "AM" or "PM"                                    |
    | ap        | either "am" or "pm"                                    |
    | hh        | the hour as a 2-digit number (00--23 or 01--12)        |
    | mm        | the minute as a 2-digit number (00--59)                |
    | ss        | the second as a 2-digit number (00--59)                |
    If either "AP" or "ap" is used, the "hh" specifier uses 12-hour mode
    (range: 01--12). Otherwise it works in 24-hour mode (range: 00--23).
    The specifiers within _buffer_ will be overwritten with the appropriate
    values from the DateTime. Any characters not belonging to one of the
    above specifiers are left as-is.
    __Example__: The format "DDD, DD MMM YYYY hh:mm:ss" generates an output
    of the form "Thu, 16 Apr 2020 18:34:56.
    @see The `timestamp()` method provides similar functionnality, but it
        returns a `String` object and supports a limited choice of
        predefined formats.
    @param[in,out] buffer Array of `char` for holding the format description
        and the formatted DateTime. Before calling this method, the buffer
        should be initialized by the user with the format string. The method
        will overwrite the buffer with the formatted date and/or time.
    @return A pointer to the provided buffer. This is returned for
        convenience, in order to enable idioms such as
        `Serial.println(now.toString(buffer));`
*/
/**************************************************************************/

char *DateTime::toString(char *buffer) const {
  uint8_t apTag =
      (strstr(buffer, "ap") != nullptr) || (strstr(buffer, "AP") != nullptr);
  uint8_t hourReformatted = 0, isPM = false;
  if (apTag) {     // 12 Hour Mode
    if (hh == 0) { // midnight
      isPM = false;
      hourReformatted = 12;
    } else if (hh == 12) { // noon
      isPM = true;
      hourReformatted = 12;
    } else if (hh < 12) { // morning
      isPM = false;
      hourReformatted = hh;
    } else { // 1 o'clock or after
      isPM = true;
      hourReformatted = hh - 12;
    }
  }

  for (size_t i = 0; i < strlen(buffer) - 1; i++) {
    if (buffer[i] == 'h' && buffer[i + 1] == 'h') {
      if (!apTag) { // 24 Hour Mode
        buffer[i] = '0' + hh / 10;
        buffer[i + 1] = '0' + hh % 10;
      } else { // 12 Hour Mode
        buffer[i] = '0' + hourReformatted / 10;
        buffer[i + 1] = '0' + hourReformatted % 10;
      }
    }
    if (buffer[i] == 'm' && buffer[i + 1] == 'm') {
      buffer[i] = '0' + mm / 10;
      buffer[i + 1] = '0' + mm % 10;
    }
    if (buffer[i] == 's' && buffer[i + 1] == 's') {
      buffer[i] = '0' + ss / 10;
      buffer[i + 1] = '0' + ss % 10;
    }
    if (buffer[i] == 'D' && buffer[i + 1] == 'D' && buffer[i + 2] == 'D') {
      static PROGMEM const char day_names[] = "SunMonTueWedThuFriSat";
      const char *p = &day_names[3 * dayOfTheWeek()];
      buffer[i] = pgm_read_byte(p);
      buffer[i + 1] = pgm_read_byte(p + 1);
      buffer[i + 2] = pgm_read_byte(p + 2);
    } else if (buffer[i] == 'D' && buffer[i + 1] == 'D') {
      buffer[i] = '0' + d / 10;
      buffer[i + 1] = '0' + d % 10;
    }
    if (buffer[i] == 'M' && buffer[i + 1] == 'M' && buffer[i + 2] == 'M') {
      static PROGMEM const char month_names[] =
          "JanFebMarAprMayJunJulAugSepOctNovDec";
      const char *p = &month_names[3 * (m - 1)];
      buffer[i] = pgm_read_byte(p);
      buffer[i + 1] = pgm_read_byte(p + 1);
      buffer[i + 2] = pgm_read_byte(p + 2);
    } else if (buffer[i] == 'M' && buffer[i + 1] == 'M') {
      buffer[i] = '0' + m / 10;
      buffer[i + 1] = '0' + m % 10;
    }
    if (buffer[i] == 'Y' && buffer[i + 1] == 'Y' && buffer[i + 2] == 'Y' &&
        buffer[i + 3] == 'Y') {
      buffer[i] = '2';
      buffer[i + 1] = '0';
      buffer[i + 2] = '0' + (yOff / 10) % 10;
      buffer[i + 3] = '0' + yOff % 10;
    } else if (buffer[i] == 'Y' && buffer[i + 1] == 'Y') {
      buffer[i] = '0' + (yOff / 10) % 10;
      buffer[i + 1] = '0' + yOff % 10;
    }
    if (buffer[i] == 'A' && buffer[i + 1] == 'P') {
      if (isPM) {
        buffer[i] = 'P';
        buffer[i + 1] = 'M';
      } else {
        buffer[i] = 'A';
        buffer[i + 1] = 'M';
      }
    } else if (buffer[i] == 'a' && buffer[i + 1] == 'p') {
      if (isPM) {
        buffer[i] = 'p';
        buffer[i + 1] = 'm';
      } else {
        buffer[i] = 'a';
        buffer[i + 1] = 'm';
      }
    }
  }
  return buffer;
}

/**************************************************************************/
/*!
      @brief  Return the hour in 12-hour format.
      @return Hour (1--12).
*/
/**************************************************************************/
uint8_t DateTime::twelveHour() const {
  if (hh == 0 || hh == 12) { // midnight or noon
    return 12;
  } else if (hh > 12) { // 1 o'clock or later
    return hh - 12;
  } else { // morning
    return hh;
  }
}

/**************************************************************************/
/*!
    @brief  Return the day of the week.
    @return Day of week as an integer from 0 (Sunday) to 6 (Saturday).
*/
/**************************************************************************/
uint8_t DateTime::dayOfTheWeek() const {
  uint16_t day = date2days(yOff, m, d);
  return (day + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
}

/**************************************************************************/
/*!
    @brief  Return Unix time: seconds since 1 Jan 1970.
    @see The `DateTime::DateTime(uint32_t)` constructor is the converse of
        this method.
    @return Number of seconds since 1970-01-01 00:00:00.
*/
/**************************************************************************/
uint32_t DateTime::unixtime(void) const {
  uint32_t t;
  uint16_t days = date2days(yOff, m, d);
  t = time2ulong(days, hh, mm, ss);
  t += SECONDS_FROM_1970_TO_2000; // seconds from 1970 to 2000

  return t;
}

/**************************************************************************/
/*!
    @brief  Convert the DateTime to seconds since 1 Jan 2000
    The result can be converted back to a DateTime with:
    ```cpp
    DateTime(SECONDS_FROM_1970_TO_2000 + value)
    ```
    @return Number of seconds since 2000-01-01 00:00:00.
*/
/**************************************************************************/
uint32_t DateTime::secondstime(void) const {
  uint32_t t;
  uint16_t days = date2days(yOff, m, d);
  t = time2ulong(days, hh, mm, ss);
  return t;
}

/**************************************************************************/
/*!
    @brief  Add a TimeSpan to the DateTime object
    @param span TimeSpan object
    @return New DateTime object with span added to it.
*/
/**************************************************************************/
DateTime DateTime::operator+(const TimeSpan &span) const {
  return DateTime(unixtime() + span.totalseconds());
}

/**************************************************************************/
/*!
    @brief  Subtract a TimeSpan from the DateTime object
    @param span TimeSpan object
    @return New DateTime object with span subtracted from it.
*/
/**************************************************************************/
DateTime DateTime::operator-(const TimeSpan &span) const {
  return DateTime(unixtime() - span.totalseconds());
}

/**************************************************************************/
/*!
    @brief  Subtract one DateTime from another
    @note Since a TimeSpan cannot be negative, the subtracted DateTime
        should be less (earlier) than or equal to the one it is
        subtracted from.
    @param right The DateTime object to subtract from self (the left object)
    @return TimeSpan of the difference between DateTimes.
*/
/**************************************************************************/
TimeSpan DateTime::operator-(const DateTime &right) const {
  return TimeSpan(unixtime() - right.unixtime());
}

/**************************************************************************/
/*!
    @author Anton Rieutskyi
    @brief  Test if one DateTime is less (earlier) than another.
    @warning if one or both DateTime objects are invalid, returned value is
        meaningless
    @see use `isValid()` method to check if DateTime object is valid
    @param right Comparison DateTime object
    @return True if the left DateTime is earlier than the right one,
        false otherwise.
*/
/**************************************************************************/
bool DateTime::operator<(const DateTime &right) const {
  return (yOff + 2000U < right.year() ||
          (yOff + 2000U == right.year() &&
           (m < right.month() ||
            (m == right.month() &&
             (d < right.day() ||
              (d == right.day() &&
               (hh < right.hour() ||
                (hh == right.hour() &&
                 (mm < right.minute() ||
                  (mm == right.minute() && ss < right.second()))))))))));
}

/**************************************************************************/
/*!
    @author Anton Rieutskyi
    @brief  Test if two DateTime objects are equal.
    @warning if one or both DateTime objects are invalid, returned value is
        meaningless
    @see use `isValid()` method to check if DateTime object is valid
    @param right Comparison DateTime object
    @return True if both DateTime objects are the same, false otherwise.
*/
/**************************************************************************/
bool DateTime::operator==(const DateTime &right) const {
  return (right.year() == yOff + 2000U && right.month() == m &&
          right.day() == d && right.hour() == hh && right.minute() == mm &&
          right.second() == ss);
}

/**************************************************************************/
/*!
    @brief  Return a ISO 8601 timestamp as a `String` object.
    The generated timestamp conforms to one of the predefined, ISO
    8601-compatible formats for representing the date (if _opt_ is
    `TIMESTAMP_DATE`), the time (`TIMESTAMP_TIME`), or both
    (`TIMESTAMP_FULL`).
    @see The `toString()` method provides more general string formatting.
    @param opt Format of the timestamp
    @return Timestamp string, e.g. "2020-04-16T18:34:56".
*/
/**************************************************************************/
String DateTime::timestamp(timestampOpt opt) const {
  char buffer[25]; // large enough for any DateTime, including invalid ones

  // Generate timestamp according to opt
  switch (opt) {
  case TIMESTAMP_TIME:
    // Only time
    sprintf(buffer, "%02d:%02d:%02d", hh, mm, ss);
    break;
  case TIMESTAMP_DATE:
    // Only date
    sprintf(buffer, "%u-%02d-%02d", 2000U + yOff, m, d);
    break;
  default:
    // Full
    sprintf(buffer, "%u-%02d-%02dT%02d:%02d:%02d", 2000U + yOff, m, d, hh, mm,
            ss);
  }
  return String(buffer);
}

/**************************************************************************/
/*!
    @brief  Create a new TimeSpan object in seconds
    @param seconds Number of seconds
*/
/**************************************************************************/
TimeSpan::TimeSpan(int32_t seconds) : _seconds(seconds) {}

/**************************************************************************/
/*!
    @brief  Create a new TimeSpan object using a number of
   days/hours/minutes/seconds e.g. Make a TimeSpan of 3 hours and 45 minutes:
   new TimeSpan(0, 3, 45, 0);
    @param days Number of days
    @param hours Number of hours
    @param minutes Number of minutes
    @param seconds Number of seconds
*/
/**************************************************************************/
TimeSpan::TimeSpan(int16_t days, int8_t hours, int8_t minutes, int8_t seconds)
    : _seconds((int32_t)days * 86400L + (int32_t)hours * 3600 +
               (int32_t)minutes * 60 + seconds) {}

/**************************************************************************/
/*!
    @brief  Copy constructor, make a new TimeSpan using an existing one
    @param copy The TimeSpan to copy
*/
/**************************************************************************/
TimeSpan::TimeSpan(const TimeSpan &copy) : _seconds(copy._seconds) {}

/**************************************************************************/
/*!
    @brief  Add two TimeSpans
    @param right TimeSpan to add
    @return New TimeSpan object, sum of left and right
*/
/**************************************************************************/
TimeSpan TimeSpan::operator+(const TimeSpan &right) const {
  return TimeSpan(_seconds + right._seconds);
}

/**************************************************************************/
/*!
    @brief  Subtract a TimeSpan
    @param right TimeSpan to subtract
    @return New TimeSpan object, right subtracted from left
*/
/**************************************************************************/
TimeSpan TimeSpan::operator-(const TimeSpan &right) const {
  return TimeSpan(_seconds - right._seconds);
}










/**************************************************************************/
/*!
    @brief  Start I2C for the PCF85263 and test succesful connection
    @param  wireInstance pointer to the I2C bus
    @return True if Wire can find PCF85263 or false otherwise.
*/
/**************************************************************************/
bool PCF85263::begin(TwoWire *wireInstance) 
{
  if (i2c_dev)
    delete i2c_dev;
  i2c_dev = new Adafruit_I2CDevice(PCF85263_ADDRESS, wireInstance);
  if (!i2c_dev->begin())
    return false;
  return true;
}

/**************************************************************************/
/*!
    @brief  Resets the STOP bit in register Stop Enable
*/
/**************************************************************************/
void PCF85263::start(void) 
{
  uint8_t stopen = read_register(PCF85263_STOPEN);
  if (stopen & (0b00000001))
    write_register(PCF85263_STOPEN, stopen & ~(0b00000001));
}

/**************************************************************************/
/*!
    @brief  Sets the STOP bit in register Stop Enable
*/
/**************************************************************************/
void PCF85263::stop(void) 
{
  uint8_t stopen = read_register(PCF85263_STOPEN);
  if (!(stopen & (0b00000001)))
    write_register(PCF85263_STOPEN, stopen & (0b00000001));
}

/**************************************************************************/
/*!
    @brief  Sets the STOP bit in register Stop Enable
*/
/**************************************************************************/
void PCF85263::configure(void) 
{
  //Timestamp Control Register Factory settings
  write_register(PCF85263_TSTMP_Control, 0b10000000);

  //PINIO Control Register
  write_register(PCF85263_PINIO, 0b00000010);

  //INTA Control Register
  write_register(PCF85263_INTAEN, 0b00010100);
}

/**************************************************************************/
/*!
    @brief  Set the date and time
    @param dt DateTime to set
*/
/**************************************************************************/
void PCF85263::adjust(const DateTime &dt) 
{
  uint8_t buffer[8] = {PCF85263_SECOND, // start at location 1, SECONDS
                       bin2bcd(dt.second()), bin2bcd(dt.minute()),
                       bin2bcd(dt.hour()),   bin2bcd(dt.day()),
                       bin2bcd(0), // skip weekdays
                       bin2bcd(dt.month()),  bin2bcd(dt.year() - 2000U)};
  i2c_dev->write(buffer, 8);
}

/**************************************************************************/
/*!
    @brief  Get the current date/time
    @return DateTime object containing the current date/time
*/
/**************************************************************************/
DateTime PCF85263::now() 
{
  uint8_t buffer[7];
  buffer[0] = PCF85263_SECOND; // start at location 2, VL_SECONDS
  i2c_dev->write_then_read(buffer, 1, buffer, 7);

  return DateTime(bcd2bin(buffer[6]) + 2000U, bcd2bin(buffer[5] & 0x1F),
                  bcd2bin(buffer[3] & 0x3F), bcd2bin(buffer[2] & 0x3F),
                  bcd2bin(buffer[1] & 0x7F), bcd2bin(buffer[0] & 0x7F));
}

/**************************************************************************/
/*!
    @brief  Set the date and time of alarm1
    @param dt DateTime to set
*/
/**************************************************************************/
void PCF85263::setAlarm(const DateTime &dt) 
{
  uint8_t buffer[6] = {PCF85263_ALM1_SECONDS, // start at location 1, SECONDS
                       bin2bcd(dt.second()), bin2bcd(dt.minute()),
                       bin2bcd(dt.hour()),   bin2bcd(dt.day()),
                       bin2bcd(dt.month())};
  i2c_dev->write(buffer, 6);
}

/**************************************************************************/
/*!
    @brief  Get the current date/time from alarm1
    @return DateTime object containing the current date/time from alarm1
*/
/**************************************************************************/
DateTime PCF85263::getAlarm() 
{
  uint8_t buffer[5];
  buffer[0] = PCF85263_ALM1_SECONDS; // start at location 2, VL_SECONDS
  i2c_dev->write_then_read(buffer, 1, buffer, 5);

  return DateTime(0 + 2000U, bcd2bin(buffer[4] & 0x1F),
                  bcd2bin(buffer[3] & 0x3F), bcd2bin(buffer[2] & 0x3F),
                  bcd2bin(buffer[1] & 0x7F), bcd2bin(buffer[0] & 0x7F));
}

/**************************************************************************/
/*!
    @brief  Enables alarm1 and start the comparison
    @param en True enables alarm1, False disable alarm1
*/
/**************************************************************************/
uint8_t PCF85263::enableAlarm(bool en)
{
  uint8_t alrm_en = read_register(PCF85263_ALMEN);
  if(en)
  {
    write_register(PCF85263_ALMEN, (alrm_en | (0x1F)));
  }
  else
  {
    write_register(PCF85263_ALMEN, (alrm_en & ~(0x1F)));
  }
  alrm_en = read_register(PCF85263_ALMEN);
  return alrm_en;
}


/**************************************************************************/
/*!
    @brief  Set the date and time of Timestamp1
    @param dt DateTime to set
*/
/**************************************************************************/
void PCF85263::setTimestamp1(const DateTime &dt) 
{
  uint8_t buffer[7] = {PCF85263_TSTMP1_SECONDS, // start at location 1, SECONDS
                       bin2bcd(dt.second()), bin2bcd(dt.minute()),
                       bin2bcd(dt.hour()),   bin2bcd(dt.day()),
                       bin2bcd(dt.month()),  bin2bcd(dt.year() - 2000U)};
  i2c_dev->write(buffer, 7);
}

/**************************************************************************/
/*!
    @brief  Get the current date/time from Timestamp1
    @return DateTime object containing the current date/time
*/
/**************************************************************************/
DateTime PCF85263::getTimestamp1()
{
  uint8_t buffer[6];
  buffer[0] = PCF85263_TSTMP1_SECONDS; // start at location 2, VL_SECONDS
  i2c_dev->write_then_read(buffer, 1, buffer, 6);

  return DateTime(bcd2bin(buffer[5]) + 2000U, bcd2bin(buffer[4] & 0x1F),
                  bcd2bin(buffer[3] & 0x3F), bcd2bin(buffer[2] & 0x3F),
                  bcd2bin(buffer[1] & 0x7F), bcd2bin(buffer[0] & 0x7F));
}


/**************************************************************************/
/*!
    @brief  Set the date and time of Timestamp1
    @param dt DateTime to set
*/
/**************************************************************************/
void PCF85263::setTimestamp2(const DateTime &dt) 
{
  uint8_t buffer[7] = {PCF85263_TSTMP2_SECONDS, // start at location 1, SECONDS
                       bin2bcd(dt.second()), bin2bcd(dt.minute()),
                       bin2bcd(dt.hour()),   bin2bcd(dt.day()),
                       bin2bcd(dt.month()),  bin2bcd(dt.year() - 2000U)};
  i2c_dev->write(buffer, 7);
}

/**************************************************************************/
/*!
    @brief  Get the current date/time from Timestamp1
    @return DateTime object containing the current date/time
*/
/**************************************************************************/
DateTime PCF85263::getTimestamp2()
{
  uint8_t buffer[6];
  buffer[0] = PCF85263_TSTMP2_SECONDS; // start at location 2, VL_SECONDS
  i2c_dev->write_then_read(buffer, 1, buffer, 6);

  return DateTime(bcd2bin(buffer[5]) + 2000U, bcd2bin(buffer[4] & 0x1F),
                  bcd2bin(buffer[3] & 0x3F), bcd2bin(buffer[2] & 0x3F),
                  bcd2bin(buffer[1] & 0x7F), bcd2bin(buffer[0] & 0x7F));
}

/**************************************************************************/
/*!
    @brief  Get the current date/time from Timestamp1
    @return DateTime object containing the current date/time
*/
/**************************************************************************/
DateTime PCF85263::getTimestampBatSw()
{
  uint8_t buffer[6];
  buffer[0] = PCF85263_TSTMP3_SECONDS; // start at location 2, VL_SECONDS
  i2c_dev->write_then_read(buffer, 1, buffer, 6);

  return DateTime(bcd2bin(buffer[5]) + 2000U, bcd2bin(buffer[4] & 0x1F),
                  bcd2bin(buffer[3] & 0x3F), bcd2bin(buffer[2] & 0x3F),
                  bcd2bin(buffer[1] & 0x7F), bcd2bin(buffer[0] & 0x7F));
}


void PCF85263::setINTA(bool pulse_mode, bool periodic_int, bool offset_correc_int, bool alarm1_int, bool alarm2_int,
                       bool timestamp_int, bool battery_switch_int, bool watchdog_int)
{
  uint8_t intacon = read_register(PCF85263_INTAEN);
  if(pulse_mode) intacon |= (1 << 7);
  else intacon &= ~(1 << 7);
  if(periodic_int) intacon |= (1 << 6);
  else intacon &= ~(1 << 6);
  if(offset_correc_int) intacon |= (1 << 5);
  else intacon &= ~(1 << 5);
  if(alarm1_int) intacon |= (1 << 4);
  else intacon &= ~(1 << 4);
  if(alarm2_int) intacon |= (1 << 3);
  else intacon &= ~(1 << 3);
  if(timestamp_int) intacon |= (1 << 2);
  else intacon &= ~(1 << 2);
  if(battery_switch_int) intacon |= (1 << 1);
  else intacon &= ~(1 << 1);
  if(watchdog_int) intacon |= (1 << 0);
  else intacon &= ~(1 << 0);

  write_register(PCF85263_INTAEN, intacon);
}

void PCF85263::setINTB(bool pulse_mode, bool periodic_int, bool offset_correc_int, bool alarm1_int, bool alarm2_int,
                       bool timestamp_int, bool battery_switch_int, bool watchdog_int)
{
  uint8_t intbcon = read_register(PCF85263_INTBEN);
  if(pulse_mode) intbcon |= (1 << 7);
  else intbcon &= ~(1 << 7);
  if(periodic_int) intbcon |= (1 << 6);
  else intbcon &= ~(1 << 6);
  if(offset_correc_int) intbcon |= (1 << 5);
  else intbcon &= ~(1 << 5);
  if(alarm1_int) intbcon |= (1 << 4);
  else intbcon &= ~(1 << 4);
  if(alarm2_int) intbcon |= (1 << 3);
  else intbcon &= ~(1 << 3);
  if(timestamp_int) intbcon |= (1 << 2);
  else intbcon &= ~(1 << 2);
  if(battery_switch_int) intbcon |= (1 << 1);
  else intbcon &= ~(1 << 1);
  if(watchdog_int) intbcon |= (1 << 0);
  else intbcon &= ~(1 << 0);

  write_register(PCF85263_INTBEN, intbcon);
}