// TODO sensor remap file
#include <mysyslog.h>
#include <mywifi.h>
#include <mywebserver.h>
#include <tempreporter.h>
#include <webupdater.h>
#include <myconfig.h>
#include <mytime.h>

#include <my_secrets.h>

/*
Config nodes:
        pin.nosleep = number
*/

// pin is adjacent to 0v and pulled up
// esp8266 = 14, esp32 = 21
int sleepPin = -1;

#define RTC_OFFSET 100
static
#ifdef ESP32
RTC_DATA_ATTR 
#endif
time_t next = 0;

// seconds it takes to come out of reset and get up and running
#define startup_time 6

static const char * handleConfigPin(const char * name, const String & id, int &value) {
    if (id == "nosleep") {
        // all ok, save the value
        sleepPin = value;
        return NULL;
    } else {
        return "sensor type not recognised";
    }
}

void setup(){
    // start serial port
    bool isColdBoot = true;

    Serial.begin(115200);
    Serial.println("Starting...");

    // retrieve next from RTC if wake from sleep
#ifdef ESP8266
    rst_info *resetInfo;
    resetInfo = ESP.getResetInfoPtr();
    Serial.printf("Reset reason %d\n",resetInfo->reason);
    if (resetInfo->reason == REASON_DEEP_SLEEP_AWAKE) {
        // woken from deep sleep, read the timing for the
        // next submission
        ESP.rtcUserMemoryRead(RTC_OFFSET, (uint32_t*)&next, sizeof(next));
        Serial.printf("first sample at %d\n",next);
        //syslogf("first sample at %d\n",next);
        isColdBoot = false;
    }
#else
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();
    Serial.printf("Reset reason %d\n",wakeup_reason);
    // next boot time is stored in eeprom by attribute
    // no special handling required
    Serial.printf("first sample at %d\n",next);
    //syslogf("first sample at %d\n",next);
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        isColdBoot = false;
    }
#endif

    MyCfgInit();

    sleepPin = MyCfgGetInt("trpin","nosleep",-1);
    if (sleepPin > 0) {
        pinMode(sleepPin, INPUT_PULLUP);
    }
    // register our config change handlers
    MyCfgRegisterInt("trpin",&handleConfigPin);

    mytime_setup(MY_TIMEZONE);
    WIFI_init(NULL,true, isColdBoot);
    SyslogInit("tempreporter");
    WS_init("/temperatures");

    // start tempreporter service
    TR_init(server, true, isColdBoot);

    // firmware updates
    UD_init(server);
    Serial.println("ready to go");
    if (isColdBoot) {
        syslogf("ready to go");
    }
}

void loop(){ 
    time_t now = time(NULL);
    if (now < 1000000000) {
        delay(1000);
        return;
    }

    if (next == 0) {
        // first time around
        delay(3000);
    }
    if (now >= next) {
        next = TR_report_data();
        Serial.printf("next sample at %d\n",next);
        //syslogf("next sample at %d",next);
    }
    time_t delaay = next - now;
    // only do deep sleep if pin is pulled low
    if ((sleepPin > -1) && (delaay > 15) && (digitalRead(sleepPin) == 1)) {
        delaay -= startup_time;
        Serial.printf("going to sleep for %d seconds...\n",delaay);
        //syslogf("going to sleep for %d seconds...",delaay);
        delay(1000);
        // TODO save next in RTC
#ifdef ESP8266
        ESP.rtcUserMemoryWrite(RTC_OFFSET, (uint32_t*)&next, sizeof(next));
        ESP.deepSleep(delaay*1000000);
#else
        // next is already stored in the power backed ram
        esp_sleep_enable_timer_wakeup((delaay) * 1000000);
        esp_deep_sleep_start();
#endif
    } else if (delaay > 0) {
        delay(delaay*1000);
    }
}
