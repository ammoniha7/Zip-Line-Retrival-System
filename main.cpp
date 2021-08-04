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
#define CPU_FREQ 80

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
    .slider { -webkit-appearance: none; margin: 14px; width: 360px; height: 25px; background: #FFD65C;
      outline: none; -webkit-transition: .2s; transition: opacity .2s;}
    .slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 35px; background: #003249; cursor: pointer;}
    .slider::-moz-range-thumb { width: 35px; height: 35px; background: #003249; cursor: pointer; }
    .stop_button{background-color: #ff0001; border: none; color: black; padding: 16px 40px;}
    .direction_button{background-color: #00ff00; border: none; color: black; padding: 16px 40px;}
    .go_up_button{background-color: #0000ff; border: none; color: yellow; padding: 16px 40px;}
  </style>
</head>
<body>
  <h2>Zip Line Controls</h2>
  <p>State: %STATE% Direction: %DIRECTION%</p>
  <p><button id="stop_button" class="button stop_button" onclick="buttonClickSend(this)">Stop Motor</button></p>
  <p><button id="direction_button" class="button direction_button" onclick="buttonClickSend(this)">Direction</button></p>
  <p><button id="go_up_button" class="button go_up_button" onclick="buttonClickSend(this)">Go Up</button></p>
  <p><span id="max_speed_slider">Max Speed: %MAXSPEEDVALUE%</span></p>
  <p><input type="range" onchange="sliderSend(this)" id="pwmSlider" min="60" max="255" value="%MAXSPEEDVALUE%" step="1" class="slider"></p>
<script>
function sliderSend(element){
  var sliderValue = element.value;
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/maxspeed?value="+sliderValue, true);
  xhr.send();
}
function buttonClickSend(element){
  var xhr = new XMLHttpRequest();
  if(element.id == "stop_button"){xhr.open("GET", "/stop_click", true);}
  else if(element.id == "direction_button"){xhr.open("GET", "/direction_click", true);}
  else if(element.id == "go_up_button"){xhr.open("GET", "/go_up_click", true);}
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";

class Timer{
public:
  unsigned long currentTime;
  void start(){
    currentTime = millis();
  }
  unsigned long getTime(){
    return millis() - currentTime;
  }
};

int readyLight = 18;
int zipLight = 16;
int recoveryLight = 17;
int buzzer = 27;
int motorDirection = 22;
int motor = 2;
int handleBarSensor = 15;
int State = READY;
bool atTheTop = false;
bool stallTimerStarted = false;
unsigned long stallRotationCount;
int motorAccelerationTimeLimit = 25;
int motorsMaxSpeed = 210;
int recoveryWaitTime = 7000;
Timer recoveryTimer;
Timer atTheTopTimer;
Timer readyTimeOutTimer;
Timer stallTimer;

int odometerLed = 19;
int odometerSensor = 34;
int odometerSensorValue;
unsigned long rotations = 0;
unsigned long beforeRotations;
unsigned long countToTopLimit;
unsigned long countToTopLimitOffset = 20;
bool crossedThreshold = false;
bool shouldCheckAtTheTop = true;
int highThreshold = 3000;
String high_threshold_string = String(highThreshold);
int lowThreshold = 300;
String lower_threshold_string = String(lowThreshold);
String current_ir_string;
Timer odometerTimer;

const char* ssid = "Zip-Line-Controls";
const char* password = "123456789";

int wifiMenu = 1;
String state_string = "Ready";
String direction_string = "Up";
String motor_max_speed_string = String(motorsMaxSpeed);
String motor_direction_state_string;
String motor_direction_string = "Up";
String recover_wait_time_string = String(recoveryWaitTime * 0.001);  //cause actual variable is in miliseconds but wifi is in seconds

String header;
String currentLine;
Timer wifiTimer;
bool wifiStopMotor = false;
bool wifiSkipToRecovery = false;

void countRotations(){
  for(int times=0; times<=4; times++){
  odometerSensorValue = analogRead(odometerSensor);
  if(odometerSensorValue > highThreshold and not crossedThreshold){
    //Serial.println(rotations);
    rotations++;
    crossedThreshold = true;
  }
  else if(odometerSensorValue < lowThreshold){crossedThreshold = false;}
  }
}

void startCount(){
  beforeRotations = rotations;
  Serial.print("Before Rotations: ");
  Serial.println(beforeRotations);
}

void stopCount(){
  if(rotations - beforeRotations < 10){   //This is if someone pulls it at the top and stuff dont actully go down
    countToTopLimit = rotations + 500;
    return;
  }
  countToTopLimit = (rotations + (rotations - beforeRotations)) - countToTopLimitOffset;
  Serial.print("Count limit: ");
  Serial.println(countToTopLimit);
  Serial.print("rotatons: ");
  Serial.println(rotations);
}

bool isAtTheTop(){
  if(shouldCheckAtTheTop){
    if(rotations > countToTopLimit){return true;}
    return false;
  }
  return false;
}

bool isStalled(){
  // If there have been no rotations in less than 160 milliseconds will return true, otherwise false
  if(!stallTimerStarted){
    stallTimerStarted = true;
    stallTimer.start();
    stallRotationCount = rotations;
    return false;
  }
  else if(stallTimer.getTime() > 160){
    stallTimerStarted = false;
    if(rotations - stallRotationCount < 1){
      Serial.println("Stalling!");
      return true;
    }
  }
  return false;
}

bool someoneOn(){
  byte first, second, third;
  first = digitalRead(handleBarSensor);
  second = digitalRead(handleBarSensor);
  third = digitalRead(handleBarSensor);
  if(first == 0 and second == 0 and third == 0){
    return true;
  }
  return false;
}

void turnOnMotor(int speed){
  ledcWrite(ledChannel, speed);
  if(speed == 0){
    Serial.println("Motor is off!");
  }
}

void readyRoutine(){
  digitalWrite(readyLight, HIGH);
  readyTimeOutTimer.start();
  while(not someoneOn()){
    if(readyTimeOutTimer.getTime() > 120000 or wifiSkipToRecovery){
      State = RECOVERY;
      wifiSkipToRecovery = false;
      return;
    }
  }
  State = ZIPPING;
  digitalWrite(readyLight, LOW);
  delay(5);  //helps the sensor/button be more consistint
}

void zippingRoutine(){
  digitalWrite(zipLight, HIGH);
  startCount();
  while(someoneOn()){
    countRotations();
  }
  stopCount();
  Serial.print("Total Rotations for the zip: ");
  Serial.println(rotations - beforeRotations);
  State = RECOVERY;
  digitalWrite(zipLight, LOW);
  delay(5);
}

void stopTheMotor(){
  // This is how recovery exits to ready when either: someone pulls on handlebar, or stop it with wifi.
  turnOnMotor(0);
  digitalWrite(recoveryLight, LOW);
  digitalWrite(buzzer, LOW);
  if(!wifiStopMotor){
    if(atTheTopTimer.getTime() > 2000){countToTopLimitOffset += 2;}
    else if(atTheTopTimer.getTime() < 1000){countToTopLimitOffset -= 2;}
  }
  recoveryTimer.start();
  while(recoveryTimer.getTime() < RECOVERY_TO_READY_AFTER_BEEN_GRABBED_TIME){
  }
  State = READY;
}

void moveToTop(){
  digitalWrite(recoveryLight, HIGH);
  digitalWrite(buzzer, HIGH);
  atTheTop = false;
  recoveryTimer.start();
  while(recoveryTimer.getTime() < recoveryWaitTime){
    if(wifiStopMotor){
      wifiStopMotor = false;
      stopTheMotor();
      return;
    }
  }
  Serial.println("Speeding up motor");
  digitalWrite(buzzer, LOW);
  for(int motorSpeed=50; motorSpeed<=motorsMaxSpeed; motorSpeed++){
    turnOnMotor(motorSpeed);
    recoveryTimer.start();
    while(recoveryTimer.getTime() < motorAccelerationTimeLimit){
      countRotations();
      if(someoneOn()){
        stopTheMotor();
        return;
      }
      if(motorSpeed > motorsMaxSpeed - 30 and isStalled()){
        stopTheMotor();
        return;
      }
    }
  }
  Serial.println("Motor is at full speed");
  while(not someoneOn() and not isStalled()){
    countRotations();
    if(wifiStopMotor){
      break;
    }
    if(isAtTheTop() and not atTheTop){
      turnOnMotor(motorsMaxSpeed - 70);
      Serial.println("Turning motor down because were at the top");
      atTheTop = true;
      atTheTopTimer.start();
    }
  }
  stopTheMotor();
}

void updateVariableStrings(){
  if(State == READY){state_string = "Ready";}
  else if(State == ZIPPING){state_string = "Zipping";}
  else if(State == RECOVERY){state_string = "Recovery";}
}

// Replaces placeholder with button section in your web page
String processor(const String& var){
  //Serial.println(var);
  if (var == "MAXSPEEDVALUE"){
    return String(motorsMaxSpeed);
  }
  else if(var == "STATE"){
    return state_string;
  }
  else if(var == "DIRECTION"){
    return direction_string;
  }
  return String();
}

void setup(){
  //setCpuFrequencyMhz(CPU_FREQ);
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
  delay(3000);
  //Serial.print("Cpu freq: ");
  //Serial.println(getCpuFrequencyMhz());

  WiFi.softAP(ssid, password);
  server.begin();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/maxspeed", HTTP_GET, [] (AsyncWebServerRequest *request) {
    // GET input1 value on <ESP_IP>/slider?value=<inputMessage>
    if (request->hasParam("value")) {
      motor_max_speed_string = request->getParam("value")->value();
      motorsMaxSpeed = motor_max_speed_string.toInt();
      Serial.print("Max speed in now:");
      Serial.println(motor_max_speed_string);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/stop_click", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("stop button clicked");
    if(State == RECOVERY){wifiStopMotor = true;}
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/direction_click", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("direction button clicked");
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/go_up_click", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Go up button Clicked");
    if(State == READY){wifiSkipToRecovery = true;}
    request->send_P(200, "text/html", index_html, processor);
  });
}

void loop(){
  updateVariableStrings();
  Serial.print("State is: ");
  Serial.println(state_string);

  if(State == READY){readyRoutine();}
  else if(State == ZIPPING){zippingRoutine();}
  else if(State == RECOVERY){moveToTop();}
}
