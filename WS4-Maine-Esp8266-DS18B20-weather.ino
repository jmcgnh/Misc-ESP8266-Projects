#include <OneWire.h>
#include <DallasTemperature.h>

// derived from https://github.com/wero1414/ESPWeatherStation
// GPL2.0 License applies to this code.

#define DEBUG_ESP_WIFI
#define DEBUG_ESP_PORT Serial

// used onewire scanner to discover the correct pin
#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

#define LINNEA_APT
#define PHANTWS6

#include "secret.h" // defines IDs and PASSWDs

// interfaces

// WiFi //
#include <ESP8266WiFi.h>
const char* ssid     = MYSSID;
const char* password = SSIDPASSWD;
const int sleepTimeS = 600; // in seconds; 18000 for Half hour, 300 for 5 minutes etc.
const char vfname[] =  __FILE__ ;
const char vtimestamp[] =  __DATE__ " " __TIME__;
const char versionstring[] = "20190708.0220.1";

#if defined( WUNDERGROUND) | defined(WUNDERGROUND_ME)
/////////////// Weather Underground ////////////////////////
char wu_host [] = "weatherstation.wunderground.com";
char wu_WEBPAGE [] = "/weatherstation/updateweatherstation.php";
char wu_ID [] = MYWUID;
char wu_PASSWORD [] = WUPASSWD;
char WU_cert_fingerprint[] = "12 DB BB 24 8E 0F 6F D4 63 EC 45 DD 5B ED 37 D7 6F B1 5F E5";
#endif

#if defined(PHANT01) || defined(PHANTWS6)
/////////////// Phant ////////////////////////
char logHost [] = MYPHANTHOST;
char logWebPage [] = MYPHANTWEBPAGE;
char phantPubKey [] = MYPHANTPUBKEY;
char phantPrivKey [] = MYPHANTPRIVKEY;
char logCertFingerprint [] = PHANTSHA1FINGERPRINT;
#endif



/////////////IFTTT/////////////////////// not currently used
//const char* host = "maker.ifttt.com";//dont change
//const String IFTTT_Event = "YourEventName";
//const int puertoHost = 80;
//const String Maker_Key = "YourMakerKey";
//String conexionIF = "POST /trigger/"+IFTTT_Event+"/with/key/"+Maker_Key +" HTTP/1.1\r\n" +
//                  "Host: " + host + "\r\n" +
//                  "Content-Type: application/x-www-form-urlencoded\r\n\r\n";
//////////////////////////////////////////

unsigned long delayTime;

void setup()
{
  int wifiwaitcount = 0;
  int wifitrycount = 0;
  int wifistatus = WL_IDLE_STATUS;

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  delay(1000);

  
  Serial.println();
  Serial.print("file: ");
  Serial.println(vfname);
  Serial.print("timestamp (local time): ");
  Serial.println(vtimestamp);
  Serial.println();

  // start sensor
  sensors.begin(); 

  // Connect D0 to RST to wake up
  pinMode(D0, WAKEUP_PULLUP);

  // most of the time we will have been reconnected by now, so check for connection before .begin()
  // this avoids a problem that crops up when calling .begin() while already connected
  if ( (wifistatus = WiFi.status()) == WL_CONNECTED) {
    Serial.print("Connected to ");   Serial.println(WiFi.SSID());
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("Connecting to ");  Serial.println(ssid);
    Serial.println(password);
    WiFi.begin(ssid, password);
    while (((wifistatus = WiFi.status()) != WL_CONNECTED) && (++wifiwaitcount < 60)) {
      Serial.print(".");
      delay(1000);
    }
    Serial.println();
  }
  Serial.print("wifi status= ");     Serial.println(wifistatus);
  Serial.println( "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");

 }

void loop() {
  //Check battery // currently not using this code
  //int level = analogRead(A0);
  //level = map(level, 0, 1024, 0, 100);
  //if(level<50)
  //{
  // mandarNot(); //Send IFTT
  // Serial.println("Low battery");
  // delay(500);
  //}

  float tempf = 1000;

  //Get sensor data
  sensors.requestTemperatures(); // Send the command to get temperatures

  float tempc = sensors.getTempCByIndex(0);
  tempf =  (tempc * 9.0) / 5.0 + 32.0;

  //local sensor data report
  Serial.println("+++++++++++++++++++++++++");
  Serial.print("tempF=     ");  Serial.print(tempf);   Serial.println(" *F");
  Serial.print("tempC=     ");  Serial.print(tempc);    Serial.println(" *C");
  Serial.println("vvvvvvvvvvvvvvvvvvvvvvvvvv");

  //Send data to logging site
  Serial.print("sending data to ");  Serial.println(logHost);
 
   // Using HTTPS protocol
   WiFiClientSecure client;
   client.setInsecure(); // See BearSSL documentation
   if (!client.connect(logHost, 443)) {
     Serial.println("Conection Fail");
   }
   if (client.verify(logCertFingerprint, logHost)) {
     Serial.println("certificate matches");
   } else {
     Serial.println("certificate doesn't match");
   }

  String ReqData = "tempf=";     ReqData += tempf;
        ReqData += "\r\n";
  Serial.println("ReqData= " + ReqData);

  String WebReq = "POST ";        WebReq += logWebPage; WebReq += " HTTP/1.1\r\n";
  WebReq += "Host: ";             WebReq += logHost;    WebReq += "\r\n";
  WebReq += "Phant-Private-Key: "; WebReq += phantPrivKey; WebReq += "\r\n";
  WebReq += "Connection: "        "close"        "\r\n";
  WebReq += "Content-Length: ";   WebReq += ReqData.length(); WebReq += "\r\n";
  WebReq += "Content-Type: application/x-www-form-urlencoded\r\n";
  WebReq += "\r\n"; // end of headers
  WebReq += ReqData; // POST data

  Serial.println("WebReq= " + WebReq);

  client.print(WebReq);
  
  Serial.println("-----Response-----");
  while (client.connected())
  {
    if (client.available())
    {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
  }

  // Weather Underground
  //Send data to Weather Underground
  Serial.print("sending data to ");
  Serial.println(wu_host);

   // Using HTTP protocol
   WiFiClient wuclient;
   if (!wuclient.connect(wu_host, 80)) {
     Serial.println("Conection Fail");
   }

  // see http://wiki.wunderground.com/index.php/PWS_-_Upload_Protocol for details
        ReqData = "ID=";         ReqData += wu_ID;
        ReqData += "&PASSWORD=";  ReqData += wu_PASSWORD; 
        ReqData += "&dateutc="    "now";
        ReqData += "&tempf=";     ReqData += tempf;
        ReqData += "&softwaretype=" "ESP8266%20WeatherStation02%20version%20";
        ReqData += versionstring;
        ReqData += "&action="    "updateraw" ;
  Serial.println("ReqData= " + ReqData);

  WebReq = "GET ";        WebReq += wu_WEBPAGE;  WebReq += "?";
  WebReq += ReqData;             WebReq += " HTTP/1.1\r\n";
  WebReq += "Host: ";             WebReq += wu_host;    WebReq += "\r\n";
  WebReq += "Connection: "        "close"        "\r\n";
  WebReq += "\r\n\r\n"; // end of headers

  Serial.println("WebReq= " + WebReq);

  wuclient.print(WebReq);
  
  Serial.println("-----Response-----");
  while (wuclient.connected())
  {
    if (wuclient.available())
    {
      String line = wuclient.readStringUntil('\n');
      Serial.println(line);
    }
  }
 
  Serial.println("----------");
  delay(2000);
  sleepMode();
}


void sleepMode() {
  Serial.print("Going into deep sleep now...");
  ESP.deepSleep(sleepTimeS * 1000000);
}
