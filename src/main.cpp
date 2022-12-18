#include <Arduino.h>
#include <U8g2lib.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <./WiFiCredentials.h>

const char* ssid = STASSID;
const char* password = STAPSK;

// Display Definition
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 5, /*data=*/ 4, /* reset=*/ 16);

ESP8266WebServer server(80);

const int led = 13;

// Display size and coordinates counter
int screenSizeX = 128;
int screenSizeY = 32;

// animation cycle counter (I don't want to animate in every CPU Cycle)
int animationCycle = 0;

// state of the wave moving animation
int wave = 0;
// state of dots animation
int dotCount = 0;
String dots = "";


// state of the laundrymachine
// 0 = off
// 1 = on
// 2 = done
int status = 0;

#define laundrymachine_width 26
#define laundrymachine_height 32
static unsigned char laundrymachine_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0x01, 0x02, 0x00, 0x00, 0x01,
   0x02, 0x00, 0x00, 0x01, 0x02, 0xfe, 0x24, 0x01, 0x02, 0x00, 0x00, 0x01,
   0x02, 0x00, 0x00, 0x01, 0xfe, 0xff, 0xff, 0x01, 0x02, 0x00, 0x00, 0x01,
   0x02, 0x00, 0x00, 0x01, 0x02, 0xf8, 0x00, 0x01, 0x02, 0x06, 0x03, 0x01,
   0x82, 0x01, 0x0c, 0x01, 0x42, 0xf8, 0x10, 0x01, 0x42, 0x06, 0x13, 0x01,
   0x22, 0x01, 0x22, 0x01, 0x12, 0x01, 0x24, 0x01, 0x92, 0x00, 0x48, 0x01,
   0x92, 0x00, 0x48, 0x01, 0x92, 0x00, 0x48, 0x01, 0x92, 0x00, 0x48, 0x01,
   0x12, 0x01, 0x24, 0x01, 0x22, 0x03, 0x22, 0x01, 0x42, 0x04, 0x13, 0x01,
   0x42, 0xf8, 0x10, 0x01, 0x82, 0x01, 0x0c, 0x01, 0x02, 0x06, 0x03, 0x01,
   0x02, 0xf8, 0x00, 0x01, 0x02, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x01,
   0xfe, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00 };

void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/plain", "Curren Status: "+ String(status) +"\r\n");
  digitalWrite(led, 0);
}

void handleUpdateDialog() {
  /* to update without this form use
  curl -F "image=@firmware.bin" <ipadr>/update
  */
  digitalWrite(led, 1);
  server.send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void handlePostData() {
  if (server.method() != HTTP_POST) {
    digitalWrite(led, 1);
    server.send(405, "text/plain", "Method Not Allowed. Please use POST.");
    digitalWrite(led, 0);
  } else {
    digitalWrite(led, 1);
    String message = "";
    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
      if(server.argName(i) == "status") {
        status = server.arg(i).toInt();
        // reset dot animation variables
        dotCount = 0;
        dots = "";
      }

      
    }
    server.send(200, "text/plain", message);
    Serial.println("POST data received: " + message);
    digitalWrite(led, 0);
  }
}

void drawLaundryWaves() {
  // start drawing at x=8 until x=19
  for(int i = 8; i < 19; i++ )
  {
    u8g2_uint_t y = 18 + sin((i*0.2)-wave) * 1.7;
    u8g2.drawPixel(i, y);
  }
  wave++;
  if(wave > 32) { wave = 0; }
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  
  u8g2.begin();
  u8g2.setFont(u8g2_font_7x14_tf); // this font is used everywhere in the program
  u8g2.drawStr(0, 16, "Connecting to WiFi");
  u8g2.sendBuffer();
  

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  u8g2.clearBuffer();

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("waschmaschine-display")) {
    Serial.println("MDNS responder started");
  }



  server.on("/", handleRoot);


  // Firmware Update via Web
  server.on("/uploadfirmware", handleUpdateDialog);
  server.on("/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
        Serial.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield();
    });

  server.on("/api/setdisplay", handlePostData);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

}

void loop(void) {
  server.handleClient();
  MDNS.update();

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("WiFi Connection lost?");
    u8g2.clearBuffer();
    u8g2.drawStr(0, 16, "WiFi Interrupted");
    u8g2.sendBuffer();
    u8g2.clearBuffer();
  }

  // do animation/draw stuff on the display only on cycle 0
  if(animationCycle == 0) {
    u8g2.setDrawColor(1);
    u8g2.drawXBM(0,0,laundrymachine_width,laundrymachine_height,laundrymachine_bits);

    switch(status) {
      case 0:
        u8g2.drawUTF8(28, 12, "Waschmaschine");
        u8g2.drawUTF8(28, 30, "ist aus");
      break;
      case 1:
        drawLaundryWaves();
        u8g2.drawUTF8(28, 12,  "Waschmaschine");
        u8g2.drawUTF8(28, 30,  "läuft");
        u8g2.drawUTF8(64, 30,  dots.c_str());
        if(dotCount > 16) {
          dotCount = 0;
          dots = "";
        } 
        if(dotCount < 3) {
          dots += ".";
        }
        dotCount++;
      break;
      case 2:
        u8g2.drawUTF8(28, 12, "Waschmaschine");
        u8g2.drawUTF8(28, 30, "ist fertig");
        u8g2.drawUTF8(100, 30,  dots.c_str());
        u8g2.drawDisc(13, 18, 5, U8G2_DRAW_ALL);
        if(dotCount > 10) {
          dotCount = 0;
          dots = "";
        }
        if(dotCount < 3) {
          dots += "!";
        }
        dotCount++;
      break;
    }
    
    u8g2.sendBuffer();
    u8g2.clearBuffer();

  }
  if(animationCycle >=16) {
    animationCycle = 0;
  } else {
    animationCycle++;
  }
}