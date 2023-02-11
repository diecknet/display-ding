#include <ESP8266WiFi.h>
#include <U8g2lib.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <config.h>

// Display Definition
// U8G2_R2 == flipped screen
// U8G2_R0 == normal screen
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R2, /* clock=*/ 5, /*data=*/ 4, /* reset=*/ 16); 

// Display size and coordinates counter
int screenSizeX = 128;
int screenSizeY = 32;

const char* ssid = STASSID; // set this in include/config.h
const char* password = STAPSK; // set this in include/config.h
const char* mqtt_server = MQTTSERVER; // set this in include/config.h
const char* mqtt_user = MQTTUSER; // set this in include/config.h
const char* mqtt_password = MQTTPASS; // set this in include/config.h
const char* mqtt_topic = MQTTTOPIC; // set this in include/config.h

WiFiClient espWiFiClient;
PubSubClient mqttClient(espWiFiClient);
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];

// this is used for non blocking reconnect attempts
long lastReconnectAttempt = 0;

// this var saves the last animation cycle that happened (I don't want to animate in every CPU Cycle)
int lastAnimation = 0;

// state of animations
int movingAnimation = 0;
int animationStatus = 0;



// state of dots animation
int dotCount = 0;
String dots = "";

// state of the laundrymachine
// 0 = off
// 1 = on
// 2 = done
int status = 0;

// since when is the machine off?
unsigned long offMillis = 0;

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


void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  // this callback gets executed, when a message arrives via MQTT
  Serial.print("Message arrived via MQTT [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println("");
  
  // parse JSON with ArduinoJson
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);
  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  // fetch values
  status = doc["status"];
  const char* timestamp = doc["timestamp"];

  Serial.println("Status: " + String(status));
  Serial.println("Timestamp: " + String(timestamp));
  if(status == 0 && offMillis == 0) {
    offMillis = millis();
  } else if(status != 0 && offMillis != 0) {
    offMillis = 0;
  }
}

void reconnect() {
  // non blocking reconnect
  long now = millis();
  if(now - lastReconnectAttempt > 5000) {
    Serial.println("lastReconnectAttempt: " + String(lastReconnectAttempt));
    lastReconnectAttempt = now;
    
    // Check if we're connected to WiFi
    if(WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi Connection lost");
    } else {
      Serial.println("WiFi is connected");
      // Check if we're reconnected to MQTT
      if(!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
          Serial.println("MQTT connected");
          // resubscribe
          mqttClient.subscribe(mqtt_topic);
        } else {
          Serial.print("failed, rc=");
          Serial.print(mqttClient.state());
          Serial.println(" try again in 5 seconds");
          Serial.println("WiFi status: " + String(WiFi.status()));

        }
      } else {
        Serial.println("MQTT is connected");
      }
    }
  }
}

void drawLaundryWaves() {
  // start drawing at x=8 until x=19
  for(int i = 8; i < 19; i++ )
  {
    u8g2_uint_t y = 18 + sin((i*0.2)-movingAnimation) * 1.7;
    u8g2.drawPixel(i, y);
  }
  movingAnimation++;
  if(movingAnimation > 32) { movingAnimation = 0; }
}

void flash_screen() {
  // fill whole screen with white rectangle
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 64);
  u8g2.sendBuffer();
  u8g2.clearBuffer();
}

void setup() {
  Serial.begin(115200);
  // setup display
  u8g2.begin();
  u8g2.setFont(u8g2_font_7x14_tf); // this font is used everywhere in the program
  
  flash_screen();

  // setup wifi
  setup_wifi();
  // connect to mqtt server
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
}

void loop() {

  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  // reboot after 33 days
  if(millis() >= 2851200000) {
    flash_screen();
    ESP.restart();
  }

  // if status is OFF for atleast 1 hour, switch screen off/power saving mode
  if(status == 0 && (millis() - offMillis) >= 3600000) {
    u8g2.setPowerSave(1);
  } else {
    u8g2.setPowerSave(0);
  }

  // do animation/draw stuff after atleast this amount of ms
  if((millis() - lastAnimation) >= 50) {
    lastAnimation = millis();
    u8g2.setDrawColor(1);
    if(status != 2 || (status == 2 && animationStatus < 300)) {
          u8g2.drawXBM(0,0,laundrymachine_width,laundrymachine_height,laundrymachine_bits);
    }
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
        if(animationStatus < 300) {
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
        } else {
            if(animationStatus <= 428 && animationStatus >= 364) {
              movingAnimation--; // move animation to the left
            } else {
              movingAnimation++; // move animation to the right
            }
            u8g2.drawXBM(movingAnimation*2,0,laundrymachine_width,laundrymachine_height,laundrymachine_bits);
            u8g2.drawDisc(13+(movingAnimation*2), 18, 5, U8G2_DRAW_ALL);
            
            // reset animation
            if(animationStatus > 428) {
                movingAnimation = 0; 
                animationStatus = 0;
            }
        }
        // increase animation counter
        animationStatus++;

      break;
      case 3:
        u8g2.drawUTF8(28, 12, "Waschmaschine");
        u8g2.drawUTF8(28, 30, "scheduled");
        u8g2.setFont(u8g2_font_unifont_t_symbols);
        u8g2.drawGlyph(102, 30, 0x23f0);
        u8g2.setFont(u8g2_font_7x14_tf);
      break;
    }
    
    u8g2.sendBuffer();
    u8g2.clearBuffer();
  }
}