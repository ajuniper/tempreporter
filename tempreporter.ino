// TODO sensor remap file
#include <mysyslog.h>
#include <mywifi.h>
#include <mywebserver.h>
#include <tempreporter.h>
#include <webupdater.h>

#include <my_secrets.h>

// syslog stuff
const char * syslog_name = "tempreporter";

void setup(){
    // start serial port
    Serial.begin(115200);

    WIFI_init();
    WS_init("/temperatures");

    // wait for time to be known
    while (time(NULL) < 1000000000) {
        delay(1000);
    }

    // start tempreporter service
    TR_init(server);

    // firmware updates
    UD_init(server);
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
