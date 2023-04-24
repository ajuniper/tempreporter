// TODO sensor remap file
#include <WiFi.h>
#include "time.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <SPIFFS.h>

#include <my_secrets.h>
static const char* ssid     = MY_WIFI_SSID;
static const char* password = MY_WIFI_PASSWORD;

// syslog stuff
static WiFiUDP udpClient;
static Syslog syslog(udpClient, MY_SYSLOG_SERVER, 514, "templog", "templog", LOG_DAEMON);

static AsyncWebServer server(80);

static const char* ntpServer = "pool.ntp.org";
static const long  gmtOffset_sec = 0;
static const int   daylightOffset_sec = 3600;

//flag to use from web update to reboot the ESP
static bool shouldReboot = false;

//Your Domain name with URL path or IP address with path
static const char* serverName = MY_INFLUX_DB;
static const char* authtoken = MY_INFLUX_AUTHTOKEN;

// Data wire is plugged TO GPIO 4
#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
static OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
static DallasTemperature sensors(&oneWire);

static const int max_sensors = 10;
struct sensorAddr {
    String str;
    DeviceAddress da;
};
static sensorAddr sensorAddrs[max_sensors];

// Number of temperature devices found
static int numberOfDevices;

static time_t disconnecttime = 0;

static char post_data[50 * max_sensors];

static const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
</head>
<body>
  <h1>ESP Web Server</h1>
  <table border="1">
  <tr><th align="left">Sensor</th><th>Reading</th></tr>
  %TEMPPLACEHOLDER%
  </table>
  <p>Build date: %BUILDDATE% %BUILDTIME%</p>
</body>
</html>
)rawliteral";

static String processor(const String& var){
    //Serial.println(var);
    if(var == "TEMPPLACEHOLDER"){
        String temps = "";
        DeviceAddress da; 
        char das[20];
        // Loop through each device, print out temperature data
        for(int i=0;i<numberOfDevices; i++){
            // Search the wire for address
            float tempC = sensors.getTempC(sensorAddrs[i].da);
            temps+="<tr><td>";
            temps+=sensorAddrs[i].str;
            temps+="</td><td>";
            temps+=tempC;
            temps+="</td></tr>";
        }
        return temps;
    } else if (var == "BUILDDATE") {
        return __DATE__;
    } else if (var == "BUILDTIME") {
        return __TIME__;
    }
    return String();
}

static void serve_root_get(AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
}

static void serve_sensor_get(AsyncWebServerRequest * request) {
    String x;
    AsyncWebServerResponse *response = nullptr;
    // GET /sensor?id=XXX
    if (request->hasParam("id")) {
        x = request->getParam("id")->value();
        int i;
        for(i=0; i<numberOfDevices; ++i) {
            if (x == sensorAddrs[i].str) {
                float tempC = sensors.getTempC(sensorAddrs[i].da);
                x = tempC;
                response = request->beginResponse(200, "text/plain", x);
                break;
            }
        }
        if (i == numberOfDevices) {
            response = request->beginResponse(404, "text/plain", "Sensor id not found");
        }
    } else {
        response = request->beginResponse(400, "text/plain", "Sensor id missing");
    }
    response->addHeader("Connection", "close");
    request->send(response);
}

static void serve_update_page(AsyncWebServerRequest *request, const String & msg, bool connclose=false) {
    String m = "<html><body>";
    if (!msg.isEmpty()) {
        m+="<p>";
        m+=msg;
        m+="</p>";
    }
    m+="<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form><p/><p><a href=\"/reboot\">Reboot</a></p></body></html>";
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", m);
    if (connclose) {
        response->addHeader("Connection", "close");
    }
    request->send(response);
}

static void serve_update_get(AsyncWebServerRequest *request) {
    serve_update_page(request,"");
}

static void serve_update_post(AsyncWebServerRequest *request){
    shouldReboot = !Update.hasError();
    syslog.logf("update starting %d",shouldReboot);
    String m="Update completed ";
    if (shouldReboot) {
        m+="OK";
    } else {
        m+="badly: ";
        m+=Update.errorString();
    }

    serve_update_page(request, m, true);
}

static void serve_update_post_body(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if(!index){
        Serial.printf("Update Start: %s\n", filename.c_str());
        syslog.logf(LOG_DAEMON|LOG_INFO,"Update Start: %s", filename.c_str());
        if(!Update.begin())
        {
            Update.printError(Serial);
            syslog.logf(LOG_DAEMON|LOG_INFO,"Update failed: %s",Update.errorString());
        }
    }
    if(!Update.hasError()){
        if(Update.write(data, len) != len){
            Update.printError(Serial);
            syslog.logf(LOG_DAEMON|LOG_INFO,"Update failed: %s",Update.errorString());
        }
    }
    if(final){
        if(Update.end(true)){
            Serial.printf("Update Success: %uB\n", index+len);
            syslog.logf(LOG_DAEMON|LOG_INFO,"Update success");
        } else {
            Update.printError(Serial);
            syslog.logf(LOG_DAEMON|LOG_INFO,"Update failed: %s",Update.errorString());
        }
    }
}

static void serve_reboot_get(AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<html><body>Rebooting... bye bye...</body></html>");
    syslog.logf(LOG_DAEMON|LOG_CRIT,"Restarting");
    Serial.printf("Restarting");
    delay(1000);
    ESP.restart();
}

static void serve_remap_get(AsyncWebServerRequest *request) {
    String x,y;
    AsyncWebServerResponse *response = nullptr;
    // GET /remap?id=XXX&to=YYY
    if (!request->hasParam("id")) {
        response = request->beginResponse(400, "text/plain", "Sensor id missing");
    } else if (!request->hasParam("to")) {
        response = request->beginResponse(400, "text/plain", "New ID missing");
    } else {
        x = request->getParam("id")->value();
        y = request->getParam("to")->value();
        int i;
        for(i=0; i<numberOfDevices; ++i) {
            if (x == sensorAddrs[i].str) {
                SPIFFS.remove(x);
                File f = SPIFFS.open(x,"w");
                if (f) {
                    f.print(y);
                    close(f);
                    sensorAddrs[i].str = y;
                    response = request->beginResponse(204, "text/plain", y);
                } else {
                    response = request->beginResponse(500, "text/plain", "Failed to open remap file");
                }
                break;
            }
        }
        if (i == numberOfDevices) {
            response = request->beginResponse(404, "text/plain", "Sensor id not found");
        }
    }
    response->addHeader("Connection", "close");
    request->send(response);
}

static void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
    Serial.print("WiFi lost connection. Reason: ");
    Serial.println(info.wifi_sta_disconnected.reason);
    if (disconnecttime == 0) {
        disconnecttime = time(NULL);
    }
    WiFi.reconnect();
}

void setup(){
    char msgbuf[80];
    // start serial port
    Serial.begin(115200);

    Serial.print("Connecting");
    WiFi.begin(ssid, password);
    WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected.");
    Serial.println(WiFi.localIP());

    // Init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // wait for time to be known
    while (time(NULL) < 1000000000) {
        delay(1000);
    }

    // Route for root / web page
    server.on("/", HTTP_GET, serve_root_get);
    server.on("/api", HTTP_GET, serve_sensor_get);

    // firmware updates
    //   // https://github.com/me-no-dev/ESPAsyncWebServer#file-upload-handling
    server.on("/update", HTTP_GET, serve_update_get);
    server.on("/update", HTTP_POST, serve_update_post,serve_update_post_body);  
    server.on("/reboot", HTTP_GET, serve_reboot_get);
    server.on("/remap", HTTP_GET, serve_remap_get);

    // Start up the sensor library
    sensors.begin();

    // Grab a count of devices on the wire
    numberOfDevices = sensors.getDeviceCount();
    if (numberOfDevices > max_sensors) { numberOfDevices = max_sensors; }

    // locate devices on the bus
    sprintf(msgbuf,"Found %d devices",numberOfDevices);
    Serial.println(msgbuf);
    syslog.logf("Started with %d devices",numberOfDevices);

    // don't care if no FS, open will fail and no remap possible
    SPIFFS.begin();

    // Loop through each device, print out address
    for(int i=0;i<numberOfDevices; i++){
        // Search the wire for address
        if(sensors.getAddress(sensorAddrs[i].da, i)){
            char s[20];
            sprintf(s,"%02x-%02x%02x%02x%02x%02x%02x%02x",
                    sensorAddrs[i].da[0],sensorAddrs[i].da[1],
                    sensorAddrs[i].da[2],sensorAddrs[i].da[3],
                    sensorAddrs[i].da[4],sensorAddrs[i].da[5],
                    sensorAddrs[i].da[6],sensorAddrs[i].da[7]);
            sensorAddrs[i].str = s;
            File f = SPIFFS.open(s, "r");
            if (f) {
                while (f.available()) {
                    int c = f.read();
                    if (c >= ' ') {
                        sensorAddrs[i].str += ((char)f.read());
                    } else {
                        break;
                    }
                }
                f.close();
            }
            sprintf(msgbuf,"Device %d address %s %s", i, s, sensorAddrs[i].str.c_str());
        } else {
            sprintf(msgbuf,"Ghost device at %d", i);
        }
        Serial.println(msgbuf);
        syslog.logf(msgbuf);
    }

    server.begin();
}

void loop(){ 
    time_t now = time(NULL);

    //Check WiFi connection status
    // event handler triggers a reconnect
    if (WiFi.status()!= WL_CONNECTED) {
        Serial.println("Wifi not connected!");
        delay(500);
        return;
    }

    if (disconnecttime != 0) {
        syslog.logf("Wifi connected again after %d seconds", now - disconnecttime);
        disconnecttime = 0;
    }
    sensors.requestTemperatures(); // Send the command to get temperatures
    char * buf = post_data;

    // Loop through each device, print out temperature data
    for(int i=0;i<numberOfDevices; i++){
        // Search the wire for address
        float tempC = sensors.getTempC(sensorAddrs[i].da);
        buf += sprintf(buf, "temperature,t=%s value=%f %ld000000000\n", sensorAddrs[i].str.c_str(),tempC,now); 
    }

    WiFiClient client;
    HTTPClient http;

    // curl -H "Authorization: Token xxx==" -i -XPOST "${influx}${db}" --data-binary @-
    // Your Domain name with URL path or IP address with path
    http.begin(client, serverName);
    // Specify content-type header
    http.addHeader("Authorization", authtoken);
    // Send HTTP POST request
    int httpResponseCode = http.POST(post_data);
    // Free resources
    http.end();

    int d = 60+now-time(NULL);
    if (d > 0) {
        delay(d*1000);
    }
}
