#include <Arduino.h>
#include <PCF85263.h>

PCF85263 rtc;

void setup(void)
{
    Wire.begin();
    Serial.begin(115200);

    if(!rtc.begin())
    {
        Serial.println("RTC not found! Check your wiring.");
        Serial.flush();
        while (1) delay(10);
    }

    // When time needs to be re-set on a previously configured device, the
    // following line sets the RTC to the date & time this sketch was compiled
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2023 at 3am you would call:
    // rtc.adjust(DateTime(2023, 1, 21, 3, 0, 0));

    rtc.start();
    rtc.configure();
}

void loop(void)
{
  DateTime now = rtc.now();
  DateTime future (now + TimeSpan(0,0,1,0));
  rtc.setAlarm(future);
  DateTime AlarmDt1 = rtc.getAlarm();
  delay(100000);
}