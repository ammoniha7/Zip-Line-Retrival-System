#include <WiFi.h>

#define READY 1
#define ZIPPING 2
#define RECOVERY 3
#define ledChannel 0
#define resolution 8
#define freq 5000
#define RECOVERY_TO_READY_AFTER_BEEN_GRABBED_TIME 1700
#define CPU_FREQ 80

WiFiServer server(80);

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

int readyLight = 32;
int zipLight = 25;
int recoveryLight = 26;
int buzzer = 27;
int motorDirection = 22;
int motor = 21;
int handleBarSensor = 14;
int State = READY;
bool atTheTop = false;
bool stall_timer_started = false;
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
String direction_string = "Forward";
String motor_max_speed_string = String(motorsMaxSpeed);
String motor_direction_state_string;
String motor_direction_string = "Forward";
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
  Serial.prinln(beforeRotations);
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
  if(rotations > countToTopLimit){return true;}
  else{return false;}
}




void checkClient(){                        
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            if (header.indexOf("GET /directionon") >= 0){  
              Serial.println("direction on");
              motor_direction_state_string = "on";
              motor_direction_string = "Forward";
              digitalWrite(motorDirection, HIGH);
              Serial.println("Direction is HIGH");
            }
            else if (header.indexOf("GET /directionoff") >= 0){
              Serial.println("direction off");
              motor_direction_state_string = "off";
              motor_direction_string = "Backward";
              digitalWrite(motorDirection, LOW);
              Serial.println("Direction is LOW");
            }
            //Stop button
            else if (header.indexOf("GET /stopmotor") >= 0 and State == RECOVERY) {
              wifiStopMotor = true;
            }
            //max speed slider
            else if(header.indexOf("GET /?maxspeed=")>=0) {
              int max1 = header.indexOf('=');
              int max2 = header.indexOf('&');
              motor_max_speed_string = header.substring(max1+1, max2);
              motorsMaxSpeed = motor_max_speed_string.toInt();
              Serial.print("Max Speed is now: ");
              Serial.println(motor_max_speed_string);
            }
            //lower threshold slider
            else if(header.indexOf("GET /?lowerthreshold=")>=0) {
              int lower1 = header.indexOf('=');
              int lower2 = header.indexOf('&');
              lower_threshold_string = header.substring(lower1+1, lower2);
              lowThreshold = lower_threshold_string.toInt();
              Serial.print("Lower Threshold: ");
              Serial.println(lowThreshold);
            }
            //upper threshold slider
            else if(header.indexOf("GET /?higherthreshold=")>=0) {
              int upper1 = header.indexOf('=');
              int upper2 = header.indexOf('&');
              high_threshold_string = header.substring(upper1+1, upper2);
              highThreshold = high_threshold_string.toInt();
              Serial.print("Upper Threshold: ");
              Serial.println(highThreshold);
            }
            else if(header.indexOf("GET /?recoverywaittime=")>=0) {
              int pos1 = header.indexOf('=');
              int pos2 = header.indexOf('&');
              recover_wait_time_string = header.substring(pos1+1, pos2);
              recoveryWaitTime = recover_wait_time_string.toInt() * 1000;
              Serial.print("Recover wait time is now: ");
              Serial.println(recover_wait_time_string);
            }
            else if(header.indexOf("GET /skiptorecovery")>=0){
              wifiSkipToRecovery = true;
            }


            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 17px; margin: 2px; cursor: pointer;}");
            client.println(".stopbutton {background-color: #e62117;}");
            client.println(".spencerbutton {background-color: #f2eb18;}");
            client.println(".refreshbutton {background-color: #bb5843;}");
            client.println(".skiptorecoverybutton {background-color: #775cb7;}");
            client.println(".directionbutton {background-color: #0b45d9;}</style></head>");
            client.println("<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial; margin-left:auto; margin-right:auto;}");
            client.println(".slider { width: 300px; }</style>");
            client.println("<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js\"></script>");
            client.println("<body>");
            // Web Page Heading Text
            client.println("<h1>Zip Line Controls</h1>");
            client.println("<p> State : " + state_string + "</p>");
            if (motor_direction_state_string=="off"){
              client.println("<p><a href=\"/directionon\"><button class=\"button directionbutton\">Backward</button></a></p>");
            } else {
              client.println("<p><a href=\"/directionoff\"><button class=\"button directionbutton\">Forward</button></a></p>");
            } 
            //Stop button
            client.println("<p><a href=\"/stopmotor\"><button class=\"button stopbutton\">Stop</button></a></p>");
            client.println("<p><a href=\"/spencer\"><button class=\"button spencerbutton\">Spencer</button></a></p>");
            //Motor speed slider
            client.println("<p>Recovery Wait Time: <span id=\"servoPos\"></span></p>");          
            client.println("<input type=\"range\" min=\"0\" max=\"50\" class=\"slider\" id=\"servoSlider\" onchange=\"servo(this.value)\" value=\""+recover_wait_time_string+"\"/>");

            client.println("<script>var slider = document.getElementById(\"servoSlider\");");
            client.println("var servoP = document.getElementById(\"servoPos\"); servoP.innerHTML = slider.value;");
            client.println("slider.oninput = function() { slider.value = this.value; servoP.innerHTML = this.value; }");
            client.println("$.ajaxSetup({timeout:1000}); function servo(pos) { ");
            client.println("$.get(\"/?recoverywaittime=\" + pos + \"&\"); {Connection: close};}</script>");
            //Max speed slider
            client.println("<p>Max Speed: <span id=\"maPos\"></span></p>");          
            client.println("<input type=\"range\" min=\"100\" max=\"255\" class=\"slider\" id=\"maSlider\" onchange=\"ma(this.value)\" value=\""+ motor_max_speed_string +"\"/>");

            client.println("<script>var slider1 = document.getElementById(\"maSlider\");");
            client.println("var maP = document.getElementById(\"maPos\"); maP.innerHTML = slider1.value;");
            client.println("slider1.oninput = function() { slider1.value = this.value; maP.innerHTML = this.value; }");
            client.println("$.ajaxSetup({timeout:1000}); function ma(pos0) { ");
            client.println("$.get(\"/?maxspeed=\" + pos0 + \"&\"); {Connection: close};}</script>"); 
            

            // IR heading
            current_ir_string = String(analogRead(odometerSensor));
            client.println("<p>Current IR Value - " + current_ir_string + "</p>");
            //Lower threshold slider
            client.println("<p>Lower Threshold: <span id=\"lowerPos\"></span></p>");          
            client.println("<input type=\"range\" min=\"0\" max=\"4095\" class=\"slider\" id=\"lowerSlider\" onchange=\"lower(this.value)\" value=\""+ lower_threshold_string+"\"/>");

            client.println("<script>var slider2 = document.getElementById(\"lowerSlider\");");
            client.println("var lowerP = document.getElementById(\"lowerPos\"); lowerP.innerHTML = slider2.value;");
            client.println("slider2.oninput = function() { slider2.value = this.value; lowerP.innerHTML = this.value; }");
            client.println("$.ajaxSetup({timeout:1000}); function lower(pos1) { ");
            client.println("$.get(\"/?lowerthreshold=\" + pos1 + \"&\"); {Connection: close};}</script>");
            //Upper Threshold Slider 
            client.println("<p>Upper Threshold: <span id=\"upperPos\"></span></p>");          
            client.println("<input type=\"range\" min=\"0\" max=\"4095\" class=\"slider\" id=\"upperSlider\" onchange=\"upper(this.value)\" value=\""+ high_threshold_string+"\"/>");

            client.println("<script>var slider3 = document.getElementById(\"upperSlider\");");
            client.println("var upperP = document.getElementById(\"upperPos\"); upperP.innerHTML = slider3.value;");
            client.println("slider3.oninput = function() { slider3.value = this.value; upperP.innerHTML = this.value; }");
            client.println("$.ajaxSetup({timeout:1000}); function upper(pos2) { ");
            client.println("$.get(\"/?higherthreshold=\" + pos2 + \"&\"); {Connection: close};}</script>");

            client.println("<p><a href=\"/skiptorecovery\"><button class=\"button skiptorecoverybutton\">Skip to Recovery</button></a></p>");
            client.println("<p><a href=\"/refresh\"><button class=\"button refreshbutton\">Refresh</button></a></p>");


            client.println("</body></html>");
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    delay(2);
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}


bool isStalled(){
  //As this is called it will start a timer and if no rotations have been counted in 160 millis then it return true (it's stalled)
  if(!stall_timer_started){
    stall_timer_started = true;
    stallTimer.start();
    stallRotationCount = rotations;
    return false;
  }
  else if(stallTimer.getTime() > 160){
    stall_timer_started = false;
    if(rotations - stallRotationCount < 1){
      Serial.println("Stalling!");
      return true;
    }
    else{return false;}
  }
  return false;
}

bool someoneOn(){
  if(digitalRead(handleBarSensor)){return false;}
  else{return true;}
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
    checkClient();
    if(readyTimeOutTimer.getTime() > 120000 or wifiSkipToRecovery){
      State = RECOVERY;
      wifiSkipToRecovery = false;
      return;
    }
  }
  State = ZIPPING;
  digitalWrite(readyLight, LOW);
  delay(1);
}

void zippingRoutine(){
  digitalWrite(zipLight, HIGH);
  startCount();
  while(someoneOn()){
    countRotations();
    checkClient();
  }
  stopCount();
  Serial.print("Total Rotations for the zip: ");
  Serial.println(rotations - beforeRotations);
  State = RECOVERY;
  digitalWrite(zipLight, LOW);
  delay(1);
}

void recoveryExitRoutine(){
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
    checkClient();
  }
  State = READY;
}

void recoveryRoutine(){
  digitalWrite(recoveryLight, HIGH);
  digitalWrite(buzzer, HIGH);
  atTheTop = false;
  recoveryTimer.start();
  while(recoveryTimer.getTime() < recoveryWaitTime){
    checkClient();
    if(wifiStopMotor){
      wifiStopMotor = false;
      recoveryExitRoutine();
      return;
    }
  }
  Serial.println("Speeding up motor");
  digitalWrite(buzzer, LOW);
  for(int i=50; i<=motorsMaxSpeed; i++){
    turnOnMotor(i);
    recoveryTimer.start();
    while(recoveryTimer.getTime() < motorAccelerationTimeLimit){
      checkClient();
      countRotations();
      if(someoneOn()){
        recoveryExitRoutine();
        return;
      }
      if(i > motorsMaxSpeed - 30 and isStalled()){
        recoveryExitRoutine();
        return;
      }
    }
  }
  Serial.println("Motor is at full speed");
  while(not someoneOn() and not isStalled()){
    checkClient();
    countRotations();
    if(wifiStopMotor){
      break;
    }
    if(isAtTheTop() and not atTheTop){
      turnOnMotor(motorsMaxSpeed - 70);
      Serial.println("Turning motor down because were at the top!");
      atTheTop = true;
      atTheTopTimer.start();
    }
  }
  recoveryExitRoutine();
}

void updateVariableStrings(){
  // I was expecting this function to have more stuff but it only updates the state string
  if(State == READY){state_string = "Ready";}
  else if(State == ZIPPING){state_string = "Zipping";}
  else if(State == RECOVERY){state_string = "Recovery";}
}

void setup(){
  setCpuFrequencyMhz(CPU_FREQ);
  Serial.begin(115200);
  pinMode(33, OUTPUT);
  digitalWrite(33, LOW); //braden shorted this or something...
  analogReadResolution(12);

  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.begin();

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
  Serial.print("Cpu freq: ");
  Serial.println(getCpuFrequencyMhz());
}

void loop(){
  updateVariableStrings();
  Serial.print("State is: ");
  Serial.println(state_string);

  if(State == READY){readyRoutine();}
  else if(State == ZIPPING){zippingRoutine();}
  else if(State == RECOVERY){recoveryRoutine();}
}
