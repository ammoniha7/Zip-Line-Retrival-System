#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define READY 1
#define ZIPPING 2
#define RECOVERY 3
#define ledChannel 0
#define resolution 8
#define freq 5000
#define RECOVERY_TO_READY_AFTER_BEEN_GRABBED_TIME 1700

AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Zip Line Controls</title>
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 1.4rem;}
    p {font-size: 1rem;}
    body {max-width: 400px; margin:0px auto; padding-bottom: 25px;}
    .slider { -webkit-appearance: none; margin: 14px; width: 360px; height: 25px; background: #203ac9;
      outline: none; -webkit-transition: .2s; transition: opacity .2s;}
    .slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 35px; background: #5dfc4e; cursor: pointer;}
    .slider::-moz-range-thumb { width: 35px; height: 35px; background: #003249; cursor: pointer; }
    .stop_button{background-color: #ff0001; border: none; color: black; padding: 16px 40px;}
    .direction_button{background-color: #00ff00; border: none; color: black; padding: 16px 40px;}
    .go_up_button{background-color: #0000ff; border: none; color: yellow; padding: 16px 40px;}
    .odometer_settings_button{background-color: #c93877; border: none; color: white; padding: 16px 40px;}
    .log_button{background-color: #c96f10; border: none; color: white; padding: 16px 40px;}
  </style>
</head>
<body>
  <h2>Zip Line Controls</h2>
  <p>State: %STATE% Direction: %DIRECTION%</p>
  <p><button id="stop_button" class="button stop_button" onclick="buttonClickSend(this)">Stop Motor</button></p>
  <p><button id="direction_button" class="button direction_button" onclick="buttonClickSend(this)">Direction</button></p>
  <p><button id="go_up_button" class="button go_up_button" onclick="buttonClickSend(this)">Go Up</button></p>
  <p><span>Max Speed: %MAXSPEEDVALUE%</span></p>
  <p><input type="range" onchange="sliderSend(this)" id="maxSpeedSlider" min="100" max="255" value="%MAXSPEEDVALUE%" step="1" class="slider"></p>
  <p><span>After Zip Delay (in seconds): %AFTERZIPDELAY%</span></p>
  <p><input type="range" onchange="sliderSend(this)" id="afterZipDelaySlider" min="0" max="60" value="%AFTERZIPDELAY%" step="1" class="slider"></p>
  <p><button id="odometer_settings_button" class="button odometer_settings_button" onclick="location.href = '/odometersettingsclick';">Odometer Settings</button></p>
  <p><button id="log_button" class="button log_button" onclick="location.href = '/logclick';">Log Page</button></p>
<script>
function sliderSend(element){
  var sliderValue = element.value;
  var xhr = new XMLHttpRequest();
  if(element.id == "maxSpeedSlider"){xhr.open("GET", "/maxspeed?value="+sliderValue);}
  else if(element.id == "afterZipDelaySlider"){xhr.open("GET", "/afterzipdelay?value="+sliderValue);}
  xhr.send();
}
function buttonClickSend(element){
  var xhr = new XMLHttpRequest();
  if(element.id == "stop_button"){xhr.open("GET", "/stopclick");}
  else if(element.id == "direction_button"){xhr.open("GET", "/directionclick");}
  else if(element.id == "go_up_button"){xhr.open("GET", "/goupclick");}
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";

const char odometer_index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Zip Line Controls</title>
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 1.4rem;}
    p {font-size: 1rem;}
    body {max-width: 400px; margin:0px auto; padding-bottom: 25px;}
    .slider { -webkit-appearance: none; margin: 14px; width: 360px; height: 25px; background: #203ac9;
      outline: none; -webkit-transition: .2s; transition: opacity .2s;}
    .slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 35px; background: #5dfc4e; cursor: pointer;}
    .slider::-moz-range-thumb { width: 35px; height: 35px; background: #003249; cursor: pointer;}
    .back_button{background-color: #2aa2d1; border: none; color: black; padding: 16px 40px;}
  </style>
</head>
<body>
  <h2>Odometer Settings</h2>
  <p><span>Current IR Value: %CURRENTIRVALUE%</span></p>
  <p><span>Low Threshold: %LOWTHRESHOLD%</span></p>
  <p><input type="range" onchange="sliderSend(this)" id="lowThresholdSlider" min="1" max="4094" value="%LOWTHRESHOLD%" step="100" class="slider"></p>
  <p><span>High Threshold: %HIGHTHRESHOLD%</span></p>
  <p><input type="range" onchange="sliderSend(this)" id="highThresholdSlider" min="1" max="4094" value="%HIGHTHRESHOLD%" step="100" class="slider"></p>
  <p><button id="back_button" class="button back_button" onclick="location.href = '/back';">Back</button></p>
<script>
function sliderSend(element){
  var sliderValue = element.value;
  var xhr = new XMLHttpRequest();
  if(element.id == "lowThresholdSlider"){xhr.open("GET", "/lowthreshold?value="+sliderValue);}
  else if(element.id == "highThresholdSlider"){xhr.open("GET", "/highthreshold?value="+sliderValue);}
  xhr.send();
}
function buttonClickSend(element){
  var xhr = new XMLHttpRequest();
  if(element.id == "back_button"){xhr.open("GET", "/back");}
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";

const char log_page_index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Zip Line Controls</title>
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 1.4rem;}
    p {font-size: 1rem;}
    body {max-width: 400px; margin:0px auto; padding-bottom: 25px;}
    .clear_button{background-color: #c76f12; border: none; color: black; padding: 16px 40px;}
    .back_button{background-color: #2aa2d1; border: none; color: black; padding: 16px 40px;}
  </style>
</head>
<body>
  <h2>Log</h2>
  <p><button id="clear_button" class="button clear_button" onclick="buttonClickSend(this);">Clear Log</button></p>
  <p><button id="back_button" class="button back_button" onclick="location.href = '/back';">Back</button></p>
  %LOGSTRING%
<script>
function buttonClickSend(element){
  var xhr = new XMLHttpRequest();
  if(element.id == "back_button"){xhr.open("GET", "/back");}
  else if(element.id == "clear_button"){xhr.open("GET", "/clearlog")}
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";

class Timer{
public:
  unsigned long currentTime;
  bool started = false;
  void start(){
    currentTime = millis();
  }
  unsigned long getTime(){
    return millis() - currentTime;
  }
};

class Logger{
public:
  String(logStr) = "";
  String newLine = "<br>";
  void log(const char* message, bool ln=false){
    logStr += message;
    Serial.print(message);
    if(ln){
      logNewLine();
    }
  }
  void log(String string, bool ln=false){
    logStr += string;
    Serial.print(string);
    if(ln){
      logNewLine();
    }
  }
  void log(int i, bool ln=false){
    logStr += String(i);
    Serial.print(i);
    if(ln){
      logNewLine();
    }
  }
  void log(unsigned long l, bool ln=false){
    logStr += String(l);
    Serial.print(l);
    if(ln){
      logNewLine();
    }
  }
  void logNewLine(){
    logStr += " - ";
    logStr += millis() * 0.001;
    logStr += newLine;
    Serial.println();
  }
  void checkLogLength(){
    if(logStr.length() > 1100){
      logStr = logStr.substring(100);
    }
  }
};

Logger logger;

byte readyLight = 18;
byte zipLight = 16;
byte recoveryLight = 17;
byte buzzer = 27;
byte motorDirection = 22;
byte motor = 2;
byte handleBarSensor = 15;
byte handleBarSensorValue = 1;
byte state = READY;
bool atTheTop = false;
bool someonOnStateChanged = false;
unsigned long stallRotationCount;
int motorAccelerationTimeLimit = 25;
byte motorsMaxSpeed = 200;
unsigned long afterZipDelay = 7000;
Timer recoveryTimer;
Timer atTheTopTimer;
Timer readyTimeOutTimer;
Timer stallTimer;
Timer handleBarTimer;

byte odometerLed = 19;
byte odometerSensor = 34;
int odometerSensorValue = 0;
unsigned long rotations = 0;
unsigned long beforeRotations;
unsigned long countToTopLimit;
unsigned long countToTopLimitOffset = 20;
bool crossedThreshold = false;
bool shouldCheckAtTheTop = true;
int highThreshold = 3000;
String highThresholdStr = String(highThreshold);
int lowThreshold = 300;
String lowThresholdStr = String(lowThreshold);
String currentIrStr;
Timer odometerTimer;

const char* ssid = "Zip-Line-Controls";
const char* password = "123456789";

const char* someoneOnLog = "Stopping motor because someone was on";
const char* wifiStopMotorLog = "Stopping motor because wifi";
const char* stalledLog = "Stopping motor because it is stalled";

String stateStr = "Ready";
String directionStr = "Up";
String motorMaxSpeedStr = String(motorsMaxSpeed);
String motorDirectionStr = "Up";
String afterZipDelayStr = String(afterZipDelay * 0.001);  //cause actual variable is in miliseconds but wifi is in seconds

bool wifiStopMotor = false;
bool wifiSkipToRecovery = false;

void countRotations(){
  for(int times=0; times<=4; times++){
  odometerSensorValue = analogRead(odometerSensor);
  if(odometerSensorValue > highThreshold and not crossedThreshold){
    //logger.logln(rotations);
    rotations++;
    crossedThreshold = true;
  }
  else if(odometerSensorValue < lowThreshold){crossedThreshold = false;}
  }
}

void startCount(){
  beforeRotations = rotations;
  logger.log("Before Rotations: ");
  logger.log(beforeRotations, true);
}

void stopCount(){
  if(rotations - beforeRotations < 10){   //This is if someone pulls it at the top and stuff dont actully go down
    countToTopLimit = rotations + 500;
    return;
  }
  countToTopLimit = (rotations + (rotations - beforeRotations)) - countToTopLimitOffset;
  logger.log("Count limit: \n");
  logger.log(countToTopLimit);
  logger.log("Rotatons: \n");
  logger.log(rotations);
}

bool isAtTheTop(){
  if(shouldCheckAtTheTop){
    if(rotations > countToTopLimit){return true;}
  }
  return false;
}

bool isStalled(){
  // If there have been no rotations in less than 160 milliseconds will return true, otherwise false
  if(not stallTimer.started){
    stallTimer.start();
    stallTimer.started = true;
    stallRotationCount = rotations;
    return false;
  }
  else if(stallTimer.getTime() > 160){
    stallTimer.started = false;
    if(rotations - stallRotationCount < 1){
      return true;
    }
  }
  return false;
}

bool someoneOn(int timeLimit){
  if(digitalRead(handleBarSensor) != handleBarSensorValue){
    if(handleBarTimer.getTime() > timeLimit){
      handleBarSensorValue = !handleBarSensorValue;
    }
  }
  else{handleBarTimer.start();}

  if(handleBarSensorValue){return false;}
  return true;
}

void turnOnMotor(int speed){
  ledcWrite(ledChannel, speed);
  if(speed == 0){
    logger.log("MOTOR IS OFF------------------", true);
  }
}

void readyAtTheTop(){
  logger.checkLogLength();
  digitalWrite(readyLight, HIGH);
  readyTimeOutTimer.start();
  while(!someoneOn(1000)){
    if(readyTimeOutTimer.getTime() > 120000 or wifiSkipToRecovery){
      state = RECOVERY;
      wifiSkipToRecovery = false;
      logger.log("Going up the zip line because ready timed out.", true);
      return;
    }
  }
  state = ZIPPING;
  digitalWrite(readyLight, LOW);
}

void movingDown(){
  digitalWrite(zipLight, HIGH);
  startCount();
  while(someoneOn(1000)){
    countRotations();
  }
  stopCount();
  logger.log("Total Rotations that were zipped: ");
  logger.log(rotations - beforeRotations, true);
  state = RECOVERY;
  digitalWrite(zipLight, LOW);
}

void stopTheMotor(){
  turnOnMotor(0);
  digitalWrite(recoveryLight, LOW);
  digitalWrite(buzzer, LOW);
  if(!wifiStopMotor){
    if(atTheTopTimer.getTime() > 2000){countToTopLimitOffset += 2;}
    else if(atTheTopTimer.getTime() < 1000){countToTopLimitOffset -= 2;}
  }
  wifiStopMotor = false;
  recoveryTimer.start();
  while(recoveryTimer.getTime() < RECOVERY_TO_READY_AFTER_BEEN_GRABBED_TIME){}
  state = READY;
}

void moveToTop(){
  digitalWrite(recoveryLight, HIGH);
  digitalWrite(buzzer, HIGH);
  atTheTop = false;
  recoveryTimer.start();
  while(recoveryTimer.getTime() < afterZipDelay){
    if(wifiStopMotor){
      wifiStopMotor = false;
      stopTheMotor();
      logger.log(wifiStopMotorLog, true);
      return;
    }
    if(someoneOn(1000)){
      stopTheMotor();
      logger.log(someoneOnLog, true);
      return;
    }
  }
  logger.log("Begin speeding motor up", true);
  digitalWrite(buzzer, LOW);
  for(int motorSpeed=50; motorSpeed<=motorsMaxSpeed; motorSpeed++){
    turnOnMotor(motorSpeed);
    recoveryTimer.start();
    while(recoveryTimer.getTime() < motorAccelerationTimeLimit){
      countRotations();
      if(someoneOn(50)){
        stopTheMotor();
        logger.log(someoneOnLog, true);
        return;
      }
      if(wifiStopMotor){
        stopTheMotor();
        logger.log(wifiStopMotorLog, true);
        return;
      }
      if(motorSpeed > motorsMaxSpeed - 30 and isStalled()){
        logger.log(stalledLog, true);
        stopTheMotor();
        return;
      }
    }
  }
  logger.log("Motor has reached max speed", true);
  while(true){
    countRotations();
    if(someoneOn(50)){
      logger.log(someoneOnLog, true);
      break;
    }
    if(wifiStopMotor){
      logger.log(wifiStopMotorLog, true);
      break;
    }
    if(isAtTheTop() and not atTheTop){
      turnOnMotor(motorsMaxSpeed - 70);
      logger.log("Turning motor down because were at the top", true);
      atTheTop = true;
      atTheTopTimer.start();
    }
  }
  stopTheMotor();
}

void updateVariableStrings(){
  if(state == READY){stateStr = "Ready";}
  else if(state == ZIPPING){stateStr = "Zipping";}
  else if(state == RECOVERY){stateStr = "Recovery";}
}

// Replaces placeholder with button section in your web page
String processor(const String& var){
  //logln(var);
  if (var == "MAXSPEEDVALUE"){return String(motorsMaxSpeed);}
  else if(var == "STATE"){return stateStr;}
  else if(var == "DIRECTION"){return directionStr;}
  else if(var == "AFTERZIPDELAY"){return afterZipDelayStr;}
  else if(var == "CURRENTIRVALUE"){return String(analogRead(odometerSensor));}
  else if(var == "LOWTHRESHOLD"){return lowThresholdStr;}
  else if(var == "HIGHTHRESHOLD"){return highThresholdStr;}
  else if(var == "LOGSTRING"){return logger.logStr;}
  return String();
}

void setup(){
  Serial.begin(115200);
  pinMode(33, OUTPUT);
  digitalWrite(33, LOW); //braden shorted this or something...
  analogReadResolution(12);

  pinMode(odometerLed, OUTPUT);
  digitalWrite(odometerLed, HIGH);  //this light stays on all the time
  pinMode(handleBarSensor, INPUT_PULLUP);
  pinMode(readyLight, OUTPUT);
  pinMode(zipLight, OUTPUT);
  pinMode(recoveryLight, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(motorDirection, OUTPUT);
  pinMode(motor, OUTPUT);
  digitalWrite(motor, LOW);
  digitalWrite(motorDirection, HIGH);
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(motor, ledChannel);
  ledcWrite(ledChannel, 0);

  WiFi.softAP(ssid, password);
  server.begin();
  delay(3000);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/maxspeed", HTTP_GET, [] (AsyncWebServerRequest *request){
    // GET input1 value on <ESP_IP>/slider?value=<inputMessage>
    if (request->hasParam("value")) {
      motorMaxSpeedStr = request->getParam("value")->value();
      motorsMaxSpeed = motorMaxSpeedStr.toInt();
      logger.log("Max speed in now: ");
      logger.log(motorMaxSpeedStr, true);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/afterzipdelay", HTTP_GET, [] (AsyncWebServerRequest *request){
    // GET input1 value on <ESP_IP>/slider?value=<inputMessage>
    if (request->hasParam("value")) {
      afterZipDelayStr = request->getParam("value")->value();
      afterZipDelay = afterZipDelayStr.toInt() * 1000;
      logger.log("After zip delay is now: ");
      logger.log(afterZipDelayStr, true);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/stopclick", HTTP_GET, [](AsyncWebServerRequest *request){
    if(state == RECOVERY){wifiStopMotor = true;}
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/directionclick", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/goupclick", HTTP_GET, [](AsyncWebServerRequest *request){
    if(state == READY){wifiSkipToRecovery = true;}
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/odometersettingsclick", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", odometer_index_html, processor);
  });

  server.on("/back", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/logclick", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", log_page_index_html, processor);
  });

  server.on("/clearlog", HTTP_GET, [](AsyncWebServerRequest *request){
    //logger.logStr = "";
    Serial.println("CLEARING LOG----");
    request->send_P(200, "text/html", log_page_index_html, processor);
  });

  server.on("/lowthreshold", HTTP_GET, [] (AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      lowThresholdStr = request->getParam("value")->value();
      lowThreshold = lowThresholdStr.toInt();
      logger.log("low threshold is now: ");
      logger.log(lowThresholdStr, true);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/highthreshold", HTTP_GET, [] (AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      highThresholdStr = request->getParam("value")->value();
      highThreshold = highThresholdStr.toInt();
      logger.log("high threshold is now: ");
      logger.log(highThresholdStr, true);
    }
    request->send(200, "text/plain", "OK");
  });
}

void loop(){
  updateVariableStrings();
  logger.log("Current state : ");
  logger.log(stateStr, true);

  if(state == READY){readyAtTheTop();}
  else if(state == ZIPPING){movingDown();}
  else if(state == RECOVERY){moveToTop();}
}
