// TODO sensor remap file
#include <mysyslog.h>
#include <mywifi.h>
#include <mywebserver.h>
#include <tempreporter.h>
#include <webupdater.h>
#include <myconfig.h>
#include <mytime.h>

#include <my_secrets.h>

void setup(){
    // start serial port
    Serial.begin(115200);
    Serial.println("Starting...");

    MyCfgInit();
    mytime_setup(MY_TIMEZONE);
    WIFI_init(NULL,true);
    SyslogInit("tempreporter");
    WS_init("/temperatures");

    // start tempreporter service
    TR_init(server);

    // firmware updates
    UD_init(server);
    Serial.println("ready to go");
}

void loop(){ 
#ifdef ESP8266
    time_t next = TR_report_data();
    time_t now = time(NULL);
    if (next > now) {
        delay((next - now)*1000);
    }
#else
    delay(1000);
#endif
}
