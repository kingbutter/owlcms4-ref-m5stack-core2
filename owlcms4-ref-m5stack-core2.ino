#include <M5Core2.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <driver/i2s.h>
#include "WebServer.h"
#include "Wire.h"
#include "AXP192.h"
#include "letsencrypt.h"

// Include Images
#include "logo_owlcms.h"

// Define Light Theme Colors
#define LTFT_COLOR1 0xFFFF //TopBar
#define LTFT_TXT2 0x0000 //TopBarTXT
#define LTFT_COLOR2 0xFFFF //TopBarLine
#define LTFT_COLOR3 0xFFFF //Background
#define LTFT_TXT 0x0000 //Text

// Define Dark Theme Colors

#define DTFT_COLOR1 0x1414 //TopBar
#define DTFT_TXT2 0xC618 //TopBarTXT
#define DTFT_COLOR2 0x0000 //TopBarLine
#define DTFT_COLOR3 0x0000 //Background
#define DRED 0xc0c0
#define DGREEN 0x0606
#define DYELLOW 0xe6e6

// Sound Setup
#include "dingdong.h"

// Configure Pins for Sound
#define CONFIG_I2S_BCK_PIN 12
#define CONFIG_I2S_LRCK_PIN 0
#define CONFIG_I2S_DATA_PIN 2
#define CONFIG_I2S_DATA_IN_PIN 34

// Configure Speaker
#define Speak_I2S_NUMBER I2S_NUM_0

// Mic and Speaker Mode definition
#define MODE_MIC 0
#define MODE_SPK 1
#define DATA_SIZE 1024

// LED Include
#include <FastLED.h>

// Set number of LEDs and Pins
#define NUM_LEDS 10
#define DATA_PIN 25
#define CLOCK_PIN 13

// Define the array of leds
CRGB leds[NUM_LEDS];

// MQTT Include
#include "PubSubClient.h"

// Define MQTT Client
PubSubClient mqttClient;
// Definitions for MQTT Client ID
String macAddress;
char mac[50];
char clientId[50];
// Definitions for MQTT connection client
WiFiClient wifiClient;
WiFiClientSecure wifiClientSecure;

// OWLCMS Setup
#define ELEMENTCOUNT(x)(sizeof(x) / sizeof(x[0]))
// Define Field of Play 
char fop[20];
// Define Referee
int referee = 0;
// Define last decision time
unsigned long lastDecision = 0;
// Define human readable time since decision
String lastDecisionReadable;
// Set Good and Bad Lift strings
const char * good_lift = "good";
const char * no_lift = "bad";
// Define SSID for setup AP
const String apSSID = "REF_" + String(referee) + "_SETUP";

// Set IP for captive portal
const IPAddress apIP(192, 168, 4, 1);

// Preferences Setup
// Define Setup Mode Toggle
boolean settingMode;
// Define SSID Option list for setup
String ssidList;
// Define SSID Name and Password
String wifi_ssid;
String wifi_password;
// Define MQTT Server Name, Port, User and Password
String mqttServer;
String mqttPort;
String mqttUserName;
String mqttPassword;
// Define MQTT TLS Toggle
boolean mqttTLS = false;
//Define Sound Toggle
boolean enableSound = false;

// Set Touch areas to create good and bad lift buttons
Button goodLiftBtn(160, 20, 160, 200, "good-lift");
Button noLiftBtn(0, 20, 160, 200, "no-lift");

// Define Status Items
float batVoltage = 0.0;
int wifiBars = 0;
int activeNotification = 0;
boolean chargeState;
boolean decisionDrawn = false;

// Define DNS Server and webserver for Setup
DNSServer dnsServer;
WebServer webServer(80);

// wifi and config store
Preferences preferences;

// Function to print to LCD and Serial
void printLine(String line) {
  Serial.println(line);
  M5.Lcd.println(line);
}

void clearConfig() {
  preferences.remove("WIFI_SSID");
  preferences.remove("WIFI_PASSWD");
  preferences.remove("MQTT_SERVER");
  preferences.remove("MQTT_USER");
  preferences.remove("MQTT_PASS");
  preferences.remove("MQTT_PORT");
  preferences.remove("MQTT_TLS");
  preferences.remove("REFEREE");
  preferences.remove("PLATFORM");  
  preferences.remove("ENABLE_SOUND");
}

boolean restoreConfig() {
  wifi_ssid = preferences.getString("WIFI_SSID");
  wifi_password = preferences.getString("WIFI_PASSWD");
  mqttServer = preferences.getString("MQTT_SERVER");
  mqttUserName = preferences.getString("MQTT_USER");
  mqttPassword = preferences.getString("MQTT_PASS");
  mqttPort = preferences.getString("MQTT_PORT");
  mqttTLS = preferences.getBool("MQTT_TLS");
  referee = preferences.getString("REFEREE").toInt();
  preferences.getString("PLATFORM").toCharArray(fop,20);
  enableSound = preferences.getBool("ENABLE_SOUND");

  // Draw Logo
  M5.Lcd.pushImage(280, 0, 40, 40, logo_40);

  // Let the user see the restored configuration
  printLine("WIFI SSID: " + wifi_ssid);
  printLine("WIFI PASS: " + wifi_password);
  printLine("MQTT SERVER:");
  // Check for MQTT TLS and adjust output accordingly
  if (mqttTLS) {
    printLine("mqtts://" + mqttServer + ":" + mqttPort);
  }
  else
  {
    printLine("mqtt://" + mqttServer + ":" + mqttPort);
  }
  printLine("MQTT USER: " + mqttUserName);
  printLine("MQTT PASS: " + mqttPassword);
  printLine("PLATFORM: " + String(fop));
  printLine("REFEREE: " + String(referee));
  delay(2000);

  // Connect to the WiFi
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  // Check to see if we need to fire up the setup AP
  if (wifi_ssid.length() > 0) {
    return true;
  } else {
    return false;
  }
}

// Check If we are connected
boolean checkConnection() {
     // Initialize attempt count
     int count = 0;

  if(WiFi.status() != WL_CONNECTED || !mqttClient.connected()) {
  // We aren't connected, draw the screen with the 40x40 logo
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.pushImage(280, 0, 40, 40, logo_40);
  }
if (WiFi.status() != WL_CONNECTED) {
  // Wifi Is not conected, let's try to connect

  // Keep the user informed
  printLine("Waiting for Wi-Fi connection");
  M5.Lcd.println();
  M5.Lcd.println();
  M5.Lcd.println();
  M5.Lcd.println();

  // Let's try this 30 times
  while (count < 30) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      M5.Lcd.println();
      M5.Lcd.setTextColor(GREEN);
      M5.Lcd.fillRect(0, 20, 280, 20, TFT_BLACK);
      printLine("Connected!");
      break;
    }
    else {
    delay(1000);
    M5.Lcd.fillRect(0, 20, 280, 20, TFT_BLACK);
    M5.Lcd.setTextColor(RED);
    // Define time Left String
    String timeLeft;
    // Do a manual zero pad
    if (count > 20) {
      timeLeft = "0" + String(30 - count);
    }
    else
    {
      timeLeft = String(30 - count);
    }
    M5.Lcd.drawString("Waiting for Connection 00:" + timeLeft, 0, 20, 2);
    Serial.println("Waiting for" + String(count +1) + " of 30 seconds");
    count++;
    }
  }
  if (WiFi.status() != WL_CONNECTED) {
  M5.Lcd.setTextColor(RED);
  M5.Lcd.fillRect(0, 0, 280, 40, TFT_BLACK);
  printLine("Timed out!");
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.println("Press Button to Clear Config");
  M5.Lcd.println("or Tap screen to Reboot");
        Button screenBtn(0, 0, 320, 240, "whole-screen");
      while(1)
      {
        M5.update();
        if (M5.BtnA.wasReleased() || M5.BtnB.wasReleased() || M5.BtnC.wasReleased() )  {
          printLine("Clearing Config");
          M5.Lcd.println("Starting AP Setup mode after restart");
          clearConfig();
          break;
        }
                if (screenBtn.wasReleased())  {
                            printLine("Keeping Config and rebooting");
          break;
        }

       }
  delay(1000);
  ESP.restart();
  return false;
  }
    }

  if(!mqttClient.connected()) {
  M5.Lcd.setTextColor(WHITE);
  count = 0;
  long r = random(1000);
  sprintf(clientId, "owlcms-%ld", r);
  printLine("MQTT Server Set to " + mqttServer + ":" + mqttPort);
  printLine("MQTT Login Set to" + mqttUserName + ":" + mqttPassword);
  printLine("Assigned MQTT Client ID of " + String(clientId));
 printLine("Waiting for MQTT connection");
  while (count < 30) {
    if (mqttClient.connect(clientId, mqttUserName.c_str(), mqttPassword.c_str())) {
      M5.Lcd.setTextColor(GREEN);
      printLine("Connected!");
      char requestTopic[50];
      sprintf(requestTopic, "owlcms/decisionRequest/%s/+", fop);
      mqttClient.subscribe(requestTopic);
      printLine("Subscribed to Request Topic");
      char ledTopic[50];
      sprintf(ledTopic, "owlcms/led/#", fop);
      mqttClient.subscribe(ledTopic);
      printLine("Subscribed to LED Topic");
      char summonTopic[50];
      sprintf(summonTopic, "owlcms/summon/#", fop);
      mqttClient.subscribe(summonTopic);
      printLine("Subscribed to Summon Topic");
      break;
    } else {
      Serial.print("MQTT connection failed, Attempt ");
      Serial.print(count + 1);
      Serial.print(" of 30 rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 second");
      M5.Lcd.setTextColor(RED);
      M5.Lcd.fillRect(0, 140, 320, 20, TFT_BLACK);
      M5.Lcd.println("Failed! Attempt " + String(count + 1) + " of 30");
      delay(5000);
    }
    count++;

  }

  if (!mqttClient.connected()) {
  Serial.println("MQTT Connection Timed out.");
  M5.Lcd.setTextColor(RED);
  M5.Lcd.fillRect(0, 0, 320, 40, TFT_BLACK);
  printLine("Timed out!");
  M5.Lcd.setTextColor(GREEN);
 M5.Lcd.println("Press Button to Clear Config or Tap screen to Reboot");
        Button screenBtn(0, 0, 320, 240, "whole-screen");
      while(1)
      {
        M5.update();
        if (M5.BtnA.wasReleased() || M5.BtnB.wasReleased() || M5.BtnC.wasReleased() )  {
          printLine("Clearing Config");
          M5.Lcd.println("Starting AP Setup mode after restart");
          clearConfig();
          break;
        }
                if (screenBtn.wasReleased())  {
          printLine("Keeping Config and rebooting");
          break;
        }

       }
  delay(1000);
  ESP.restart();
  return false;
  }
  }

return true;



}

void startWebServer() {
  if (settingMode) {
    M5.Lcd.fillScreen(TFT_BLACK);

      M5.Lcd.pushImage(100, 80, 120, 120, logo_120);

    M5.Lcd.setTextColor(WHITE);
    printLine("Starting Web Server at " + IpAddressToString(WiFi.softAPIP()));
    }
    else {

    M5.Lcd.fillScreen(DTFT_COLOR3);
    M5.Lcd.fillRect(0, 20, 160, 200, TFT_RED);
    M5.Lcd.fillRect(160, 20, 160, 200, TFT_WHITE);

    Serial.println("Starting Web Server at " + IpAddressToString(WiFi.localIP()));

        // Header 
    M5.Lcd.fillRect(0, 0, 320, 27, DTFT_COLOR1);
    M5.Lcd.drawLine(0, 27, 320, 27, DTFT_COLOR2);
    }
    populateSSIDList();

    webServer.on("/", []() {
      String s = "<h1>owlcms Referee Box</h1><p>Please selecting the SSID from the pulldown to begin</p>";
      s += "<form method=\"get\" action=\"setap\"><label>SSID: </label><select name=\"ssid\">";
      s += ssidList;
      s += "</select><br><br>Wifi Password: <input name=\"pass\" length=64 type=\"password\" value=\""+ wifi_password + "\">";
      s += "<br><br>MQTT Server: <input name=\"mqttserver\" length=64 type=\"text\" value=\""+ mqttServer + "\">";
      s += "<br><br>MQTT Port: <input name=\"mqttport\" length=64 type=\"text\" value=\""+ mqttPort + "\">";
      s += "<br><br>MQTT Use TLS: <input name=\"mqtttls\" type=\"checkbox\"";
      if (mqttTLS)
      {
      s += " checked";
      }
      s += ">";
      s += "<br><br>MQTT User: <input name=\"mqttuser\" length=64 type=\"text\" value=\""+ mqttUserName + "\">";
      s += "<br><br>MQTT Password: <input name=\"mqttpass\" length=64 type=\"password\" value=\""+ mqttPassword + "\">";
      s += "<br><br>Platform: <input name=\"platform\" length=64 type=\"text\" value=\""+ String(fop) + "\">";
      s += "<br><br><label>Referee: </label><select name=\"ref\">";
      s += "<option value=\"1\"";
        if (referee == 1)
        {
          s += " selected";
        }
      s += ">Left (1)</option>";
      s += "<option value=\"2\"";
        if (referee == 2)
        {
          s += " selected";
        }
      s += "> Center (2)</option>";
      s += "<option value=\"3\"";
        if (referee == 3)
        {
          s += " selected";
        }
      s += ">Right (3)</option>";
      s += "</select>";
      s += "<br><br>Enable Sound: <input name=\"enablesound\" type=\"checkbox\"";
      if (enableSound)
      {
      s += " checked";
      }
      s += ">";
      s += "<br><br><input type=\"submit\"></form>";
      webServer.send(200, "text/html", makePage("Settings", s));
    });
    webServer.on("/setap", []() {
      M5.Lcd.fillScreen(TFT_BLACK);
      String ssid = urlDecode(webServer.arg("ssid"));
      printLine("SSID: " + ssid);
      String pass = urlDecode(webServer.arg("pass"));
      printLine("Password: " + pass);
      String mqttserver = urlDecode(webServer.arg("mqttserver"));
      printLine("MQTT Server: " + mqttserver);
      String mqttport = urlDecode(webServer.arg("mqttport"));
      printLine("MQTT Port: " + mqttport);
      boolean mqtttls = webServer.hasArg("mqtttls");
      printLine("MQTT TLS: " + String(mqtttls));
      String mqttuser = urlDecode(webServer.arg("mqttuser"));
      printLine("MQTT User: " + mqttuser);
      String mqttpass = urlDecode(webServer.arg("mqttpass"));
      printLine("MQTT Password: " + mqttpass);
      String ref = urlDecode(webServer.arg("ref"));
      printLine("Ref Number: " + ref);
      String platform = urlDecode(webServer.arg("platform"));
      printLine("Platform: " + platform);
      boolean enablesound = webServer.hasArg("enablesound");
      printLine("Enable Sound: " + String(enablesound));
 


      // Store config
      printLine("Writing Config to unit...");
      preferences.putString("WIFI_SSID", ssid);
      preferences.putString("WIFI_PASSWD", pass);
      preferences.putString("MQTT_SERVER", mqttserver);
      preferences.putString("MQTT_PORT", mqttport);
      preferences.putBool("MQTT_TLS", mqtttls);
      preferences.putString("MQTT_USER", mqttuser);
      preferences.putString("MQTT_PASS", mqttpass);
      preferences.putString("REFEREE", ref);
      preferences.putString("PLATFORM", platform);
      printLine("Write nvr done!");
      M5.Lcd.println("Tap Screen to reboot");
      String s = "<h1>Setup complete.</h1><p>device will be connected to \"";
      s += ssid;
      s += "\" after the restart.<br /><br />Tap Screeen to Reset";
      webServer.send(200, "text/html", makePage("Settings", s));
      Button screenBtn(0, 0, 320, 240, "whole-screen");
      while(1)
      {
        M5.update();
        if (screenBtn.wasReleased())  {
          Serial.println("Screen Button Released");
          break;
        }
       }
      ESP.restart();
    });
    webServer.on("/reset", []() {
      // reset the config
     clearConfig();

      String s = "<h1>Settings were reset.</h1><p>Please reset device.</p>";
      webServer.send(200, "text/html", makePage("Settings Reset", s));
      delay(3000);
      ESP.restart();
    });
    webServer.onNotFound([]() {
      String s = "<h1>AP mode</h1><p><a href=\"/\">Settings</a></p>";
      s += "<p><a href=\"/reset\">Reset Config</a></p>";
      webServer.send(200, "text/html", makePage("AP setup mode", s));
    });
  webServer.begin();
}

void populateSSIDList()
{
  ssidList = "";
  bool foundSelected = false;
   int n = WiFi.scanNetworks();
    ssidList += "<option value=\"" + wifi_ssid + "\" selected>" + wifi_ssid + "</option>";
 for (int i = 0; i < n; ++i) {
    ssidList += "<option value=\"";
    ssidList += WiFi.SSID(i);
    ssidList += "\">";
    ssidList += WiFi.SSID(i);
    ssidList += "</option>";
  }
}
void setupMode() {
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  delay(100);
 
  delay(100);
  printLine("");
  delay(100);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSSID.c_str());
  WiFi.mode(WIFI_MODE_AP);
  startWebServer();
  printLine("Starting Access Point at: " + String(apSSID));
  dnsServer.start(53, "*", apIP);

}

String makePage(String title, String contents) {
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += "</body></html>";
  return s;
}

String urlDecode(String input) {
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}

void drawScreen() {

  M5.Lcd.setTextColor(WHITE);

  // Wifi signal bars

  float RSSI = 0.0;
  int bars;
  RSSI = WiFi.RSSI();

  if (RSSI >= -55) {
    bars = 5;
  } else if (RSSI < -55 & RSSI >= -65) {
    bars = 4;
  } else if (RSSI < -65 & RSSI >= -70) {
    bars = 3;
  } else if (RSSI < -70 & RSSI >= -78) {
    bars = 2;
  } else if (RSSI < -78 & RSSI >= -82) {
    bars = 1;
  } else {
    bars = 0;
  }

  // print signal bars

  if (bars != wifiBars) {
    M5.Lcd.fillRect(280, 0, 40, 26, DTFT_COLOR1);
    for (int b = 0; b <= bars; b++) {
      M5.Lcd.fillRect(281 + (b * 6), 23 - (b * 4), 5, b * 4, DTFT_TXT2);
    }
    wifiBars = bars;
  }

  // Battery stat

  // reading battery voltage
  float battery_voltage = M5.Axp.GetBatVoltage();
  String voltage = String(battery_voltage);

  if (batVoltage != battery_voltage || chargeState != M5.Axp.isCharging() ) {
    // battery symbol

    M5.Lcd.fillRect(4, 7, 28, 2, DTFT_TXT2);
    M5.Lcd.fillRect(4, 19, 28, 2, DTFT_TXT2);
    M5.Lcd.fillRect(4, 7, 2, 12, DTFT_TXT2);
    M5.Lcd.fillRect(32, 7, 2, 14, DTFT_TXT2);
    M5.Lcd.fillRect(34, 11, 3, 6, DTFT_TXT2);

    // battery level symbol
    if (battery_voltage <= 2.99) {
      M5.Lcd.fillRect(4, 7, 28, 2, DRED);
      M5.Lcd.fillRect(4, 19, 28, 2, DRED);
      M5.Lcd.fillRect(4, 7, 2, 14, DRED);
      M5.Lcd.fillRect(32, 7, 2, 14, DRED);
      M5.Lcd.fillRect(34, 11, 3, 6, DRED);
      //M5.Lcd.drawString(voltage, 45, 8, 1);
    }

    if (battery_voltage >= 3.0) {
      M5.Lcd.fillRect(6, 9, 26, 10, DTFT_COLOR1);
      M5.Lcd.fillRect(6, 9, 4, 10, DRED);
      //M5.Lcd.drawString(voltage, 45, 8, 1);
    }
    if (battery_voltage >= 3.2) {
      M5.Lcd.fillRect(6, 9, 26, 10, DTFT_COLOR1);
      M5.Lcd.fillRect(6, 9, 6, 10, DRED);
      //M5.Lcd.drawString(voltage, 45, 8, 1);
    }
    if (battery_voltage >= 3.4) {
      M5.Lcd.fillRect(6, 9, 26, 10, DTFT_COLOR1);
      M5.Lcd.fillRect(6, 9, 11, 10, DYELLOW);
      //M5.Lcd.drawString(voltage, 45, 8, 1);
    }
    if (battery_voltage >= 3.6) {
      M5.Lcd.fillRect(6, 9, 26, 10, DTFT_COLOR1);
      M5.Lcd.fillRect(6, 9, 16, 10, DYELLOW);
      //M5.Lcd.drawString(voltage, 45, 8, 1);
    }
    if (battery_voltage >= 3.8) {
      M5.Lcd.fillRect(6, 9, 26, 10, DTFT_COLOR1);
      M5.Lcd.fillRect(6, 9, 21, 10, DGREEN);
      //M5.Lcd.drawString(voltage, 45, 8, 1);
    }
    if (battery_voltage >= 4.0) {
      M5.Lcd.fillRect(6, 9, 26, 10, DTFT_COLOR1);
      M5.Lcd.fillRect(6, 9, 26, 10, DGREEN);
      //M5.Lcd.drawString(voltage, 45, 8, 1);
    }
    if (battery_voltage >= 4.60) {
      M5.Lcd.fillRect(6, 9, 26, 10, DTFT_COLOR1);
      M5.Lcd.fillRect(6, 9, 26, 10, DGREEN);
      //M5.Lcd.drawString("CHG", 10, 7, 1);
    }
    if (battery_voltage >= 4.85) {
      M5.Lcd.fillRect(6, 9, 26, 10, DTFT_COLOR1);
      M5.Lcd.fillRect(6, 9, 26, 10, CYAN);
      //M5.Lcd.drawString("USB", 10, 7, 1);
    }
    batVoltage = battery_voltage;
if (M5.Axp.isCharging()) {
M5.Lcd.drawLine(15,6,22,6,TFT_BLACK);
M5.Lcd.drawLine(15,7,16,7,TFT_BLACK);
M5.Lcd.drawLine(21,7,22,7,TFT_BLACK);
M5.Lcd.drawLine(15,8,16,8,TFT_BLACK);
M5.Lcd.drawLine(20,8,21,8,TFT_BLACK);
M5.Lcd.drawLine(14,9,15,9,TFT_BLACK);
M5.Lcd.drawLine(19,9,20,9,TFT_BLACK);
M5.Lcd.drawLine(14,10,15,10,TFT_BLACK);
M5.Lcd.drawLine(19,10,20,10,TFT_BLACK);
M5.Lcd.drawLine(14,11,15,11,TFT_BLACK);
M5.Lcd.drawLine(18,11,19,11,TFT_BLACK);
M5.Lcd.drawLine(13,12,14,12,TFT_BLACK);
M5.Lcd.drawLine(18,12,21,12,TFT_BLACK);
M5.Lcd.drawLine(13,13,16,13,TFT_BLACK);
M5.Lcd.drawLine(19,13,21,13,TFT_BLACK);
M5.Lcd.drawLine(13,14,16,14,TFT_BLACK);
M5.Lcd.drawLine(19,14,20,14,TFT_BLACK);
M5.Lcd.drawLine(15,15,16,15,TFT_BLACK);
M5.Lcd.drawLine(18,15,19,15,TFT_BLACK);
M5.Lcd.drawLine(15,16,18,16,TFT_BLACK);
M5.Lcd.drawLine(14,17,17,17,TFT_BLACK);
M5.Lcd.drawLine(14,18,17,18,TFT_BLACK);
M5.Lcd.drawLine(14,19,16,19,TFT_BLACK);
M5.Lcd.drawLine(13,20,15,20,TFT_BLACK);
M5.Lcd.drawLine(13,21,14,21,TFT_BLACK);
}

chargeState = M5.Axp.isCharging();    
  }

M5.Lcd.drawString("Referee " + String(referee), 100, 0, 4);
}

void ledOn() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Red;
  }
  FastLED.show();
  M5.Axp.SetLed(1);
}

void ledOff() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
  M5.Axp.SetLed(0);
}

String decisionRequestTopic = String("owlcms/decisionRequest/") + fop;
String summonTopic = String("owlcms/summon/") + fop;
String ledTopic = String("owlcms/led/") + fop;
void callback(char * topic, byte * message, unsigned int length) {

  String stTopic = String(topic);
  Serial.print("Message arrived on topic: " );
  Serial.print(stTopic);
  Serial.print("; Message: ");

  String stMessage;
  // convert byte to char
  for (int i = 0; i < length; i++) {
    stMessage += (char) message[i];
  }
  Serial.println(stMessage);

  int refIndex = stTopic.lastIndexOf("/") + 1;
  String refString = stTopic.substring(refIndex);
  int ref13Number = refString.toInt();

  if (ref13Number == 0 || ref13Number == referee) {
    if (stTopic.startsWith(decisionRequestTopic)) {
      changeReminderStatus(stMessage.startsWith("on"));
    } else if (stTopic.startsWith(summonTopic)) {
      changeSummonStatus(stMessage.startsWith("on"));
    } else if (stTopic.startsWith(ledTopic)) {
      changeSummonStatus(stMessage.startsWith("on"));
    }
  }
}

void changeReminderStatus(boolean warn) {
  Serial.print("reminder ");
  Serial.print(warn);
  Serial.print(" ");
  if (warn) {
      ledOn();
      M5.Axp.SetLDOEnable(3, true);
      DingDong();
      delay(500);
      ledOff();
      M5.Axp.SetLDOEnable(3, false);
      DingDong();
      delay(500);
      ledOn();
  } else {
    ledOff();
    M5.Axp.SetLDOEnable(3, false);
  }
}

void changeSummonStatus(boolean warn) {
  Serial.print("summon ");
  Serial.print(warn);
  if (warn) {
      ledOn();
      M5.Axp.SetLDOEnable(3, true);
      DingDong();
      delay(500);
      ledOff();
      M5.Axp.SetLDOEnable(3, false);
      DingDong();
      delay(500);
      ledOn();
  } else {
    ledOff();
    M5.Axp.SetLDOEnable(3, false);
  }
}


void sendDecision(int ref02Number,
  const char * decision) {
  if (referee > 0) {
    // software configuration
    ref02Number = referee - 1;
  }
  char topic[50];
  sprintf(topic, "owlcms/decision/%s", fop);
  char message[32];
  sprintf(message, "%i %s", ref02Number + 1, decision);

  mqttClient.publish(topic, message);
  Serial.print(topic);
  Serial.print(" ");
  Serial.print(message);
  Serial.println(" sent.");
}

// Sound Functions

bool InitI2SSpeakOrMic(int mode) {
  esp_err_t err = ESP_OK;

  i2s_driver_uninstall(
    Speak_I2S_NUMBER);
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER),

    .sample_rate = 44100,
    .bits_per_sample =
    I2S_BITS_PER_SAMPLE_16BIT,

    .channel_format =
    I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format =
    I2S_COMM_FORMAT_I2S,

    .intr_alloc_flags =
    ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 2,
    .dma_buf_len = 128,
  };
  if (mode == MODE_MIC) {
    i2s_config.mode =
      (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
  } else {
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.use_apll = false; // I2S clock setup.  
    i2s_config.tx_desc_auto_clear =
      true;

  }

  err += i2s_driver_install(Speak_I2S_NUMBER, & i2s_config, 0, NULL);

  i2s_pin_config_t tx_pin_config;

  #if(ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 3, 0))
  tx_pin_config.mck_io_num = I2S_PIN_NO_CHANGE;
  #endif
  tx_pin_config.bck_io_num =
    CONFIG_I2S_BCK_PIN;

  tx_pin_config.ws_io_num = CONFIG_I2S_LRCK_PIN;
  tx_pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
  tx_pin_config.data_in_num = CONFIG_I2S_DATA_IN_PIN;
  err +=
    i2s_set_pin(Speak_I2S_NUMBER, &
      tx_pin_config);
  err += i2s_set_clk(
    Speak_I2S_NUMBER, 44100, I2S_BITS_PER_SAMPLE_16BIT,
    I2S_CHANNEL_MONO);

  return true;
}

void SpeakInit(void) {
  M5.Axp.SetSpkEnable(true);
  InitI2SSpeakOrMic(MODE_SPK);
}

void DingDong(void) {
  if (enableSound) {
  size_t bytes_written = 0;
  i2s_write(Speak_I2S_NUMBER, previewR, 120264, & bytes_written,
    portMAX_DELAY);
  }

}

String IpAddressToString(const IPAddress& ipAddress) {
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ;
}

void setup() {
  m5.begin(true, true, true, true);
  Wire.begin();
  M5.Lcd.setSwapBytes(true);
  ledOff();
  M5.Axp.EnableCoulombcounter();
  preferences.begin("wifi-config");

  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setSwapBytes(true);
  M5.Lcd.pushImage(40, 0, 240, 240, logo_240);

  FastLED.addLeds < NEOPIXEL, DATA_PIN > (leds, NUM_LEDS); // GRB ordering is assumed
  FastLED.setBrightness(190);
  M5.Axp.SetSpkEnable(true);
  macAddress = WiFi.macAddress();
  Serial.print("MAC Address: ");
  Serial.print(macAddress);
  macAddress.toCharArray(clientId, macAddress.length() + 1);

  SpeakInit();
  DingDong();
  delay(6000);
  M5.Lcd.fillScreen(TFT_BLACK);





  if (restoreConfig()) {
      // Set up MQTT

      mqttClient.setKeepAlive(20);
      if (mqttTLS) {
      Serial.print("Enabling TLS for mqtt");
      mqttClient.setClient(wifiClientSecure);
      Serial.print("Setting Certificate");
      wifiClientSecure.setCACert(rootCABuff);
      // wifiClientSecure.setInsecure();
      }
      else
      {
      Serial.print("NOT Enabling TLS for mqtt");
      mqttClient.setClient(wifiClient);        
      }
      mqttClient.setServer(mqttServer.c_str(), mqttPort.toInt());
      mqttClient.setCallback(callback);
      delay(10);
    if (checkConnection()) {
      M5.Lcd.pushImage(280, 0, 40, 40, logo_40);
      delay(2000);
      M5.Lcd.fillScreen(TFT_BLACK);
      settingMode = false;
      startWebServer();
      return;
    }
  }
  settingMode = true;
  setupMode();

}

void loop() {
  delay(10);
  M5.update();

  dnsServer.processNextRequest();
  drawScreen();
  webServer.handleClient();
  if (!settingMode) {
     drawLastDecision();
  if (!mqttClient.connected()) {
      mqttClient.setKeepAlive(20);
      if (mqttTLS) {
      Serial.print("Enabling TLS for mqtt");
      mqttClient.setClient(wifiClientSecure);
      Serial.print("Setting Certificate");
      wifiClientSecure.setCACert(rootCABuff);
      // wifiClientSecure.setInsecure();
      }
      else
      {
      Serial.print("NOT Enabling TLS for mqtt");
      mqttClient.setClient(wifiClient);        
      }
      mqttClient.setServer(mqttServer.c_str(), mqttPort.toInt());
      mqttClient.setCallback(callback);
    checkConnection();
  } else {
    mqttClient.loop();
  }
  }
  if (M5.BtnB.pressedFor(5000)) {
    settingMode = true;
    setupMode();
  }
  if (goodLiftBtn.wasReleased() || M5.BtnC.wasReleased()) {
    sendDecision(referee, good_lift);
    lastDecision = millis();
  } else if (noLiftBtn.wasReleased() || M5.BtnA.wasReleased()) {
    sendDecision(referee, no_lift);
    lastDecision = millis();
  }
  M5.Lcd.drawString("IP: " + IpAddressToString(WiFi.localIP()) + " | Last Decision: ", 0, 220, 2);
  M5.Lcd.pushImage(300, 220, 20, 20, logo_20);
}

void drawLastDecision() {
int txtWidth = M5.Lcd.textWidth("IP: " + IpAddressToString(WiFi.localIP()) + " | Last Decision: ");
    if (lastDecision == 0) {
        if (!decisionDrawn){
      M5.Lcd.fillRect(txtWidth + 10, 220, 290 - txtWidth, 20, TFT_BLACK);
      M5.Lcd.drawString("None", txtWidth + 10, 220, 2);
      decisionDrawn = true;
        }
     }
   else if (getReadableTime(lastDecision) != lastDecisionReadable)
   {
    M5.Lcd.fillRect(txtWidth + 10, 220, 290 - txtWidth, 20, TFT_BLACK);
   M5.Lcd.drawString(getReadableTime(lastDecision), txtWidth + 10, 220, 2);
   lastDecisionReadable = getReadableTime(lastDecision);
   }

}

String getReadableTime(unsigned long timeInMillis) {
  unsigned long currentMillis;
  unsigned long seconds;
  unsigned long minutes;
  unsigned long hours;
  unsigned long days;
  String readableTime;

  currentMillis = millis() - timeInMillis;
  seconds = currentMillis / 1000;
  minutes = seconds / 60;
  hours = minutes / 60;
  days = hours / 24;
  currentMillis %= 1000;
  seconds %= 60;
  minutes %= 60;
  hours %= 24;

  if (days > 0) {
    readableTime = String(days) + " ";
  }

  if (hours > 0) {
    readableTime += String(hours) + ":";
  }

  if (minutes < 10) {
    readableTime += "0";
  }
  readableTime += String(minutes) + ":";

  if (seconds < 10) {
    readableTime += "0";
  }
  readableTime += String(seconds) + " ago";
  return readableTime;
}
