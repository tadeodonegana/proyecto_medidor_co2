/*
  Blynk implementation to read data from the MHZ19 CO2 sensor.
  Currently under development.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version. 
*/

#include <MHZ19.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#define BLYNK_PRINT Serial

#ifndef ESP32
#define ESP32
#endif

// Comment the following line if you use SoftwareSerial
#define HWSERIAL

// Base level for C02
int CO2_base = 400;
 
// RGB Led 
const int led_R = 12;
const int led_G = 14;
const int led_B = 27;
const int PWMR_Ch = 0;
const int PWMG_Ch = 1;
const int PWMB_Ch = 2;
const int PWM_Freq = 1000;
const int PWM_Res = 8;

// TX and RX of the MHZ19 sensor pins
const int rx2_pin = 16;
const int tx2_pin = 17;

// Device to MH-Z19 Serial baudrate (should not be changed)
#define BAUDRATE 9600

#ifndef HWSERIAL
  #include <SoftwareSerial.h> 
  SoftwareSerial mySerial(rx2_pin, tx2_pin);
#endif

// Push Button for configuration control of the device
const int btn_PIN = 21;
const int debounceThresh = 70;

struct Button {
  const uint8_t PIN;
  uint32_t timePressed;
  bool down;
  bool event;
};

bool btn_down = false;

Button button1 = {btn_PIN, 0,false, false};

// You should get Auth Token in the Blynk App.
char auth[] = "YOUR_AUTH_TOKEN";

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "YOUR_WIFI_NAME";
char pass[] = "YOUR_WIFI_PASSWORD";

// MHZ19 Sensor instance
MHZ19 mhz19;

BlynkTimer timer;

// Set the RGB LED color
void setRGB_LEDColor(int red, int green, int blue)
{
    ledcWrite(PWMR_Ch, red); 
    ledcWrite(PWMG_Ch, green);
    ledcWrite(PWMB_Ch, blue);
}

// The Interrupt Service Routine is placed in the RAM of the ESP32
void IRAM_ATTR isr_button() {
  static uint32_t lastMillis = 0;

  if (millis() - lastMillis > debounceThresh){ // debounce
    if (!button1.down){
      button1.down = true;
      setRGB_LEDColor (0, 0, 255);  // Blue means warming or configuring
    } else { 
      button1.down = false;
      button1.timePressed = millis() - lastMillis;
      button1.event = true;
    }
  }
  lastMillis = millis();
}

// Calibrate the MHZ19 outdoors to get accurate readings
void calibrate_mhz19()
{
  // The device must remain stable, outdoors for waitingMinutes minutes
  const int waitingMinutes = 21;  
  const long waitingSeconds = waitingMinutes * 60L; 
  long cnt_cal = 0; 

  byte color_intensity = 255;
  
  setRGB_LEDColor (0, 0, color_intensity);  // Blue means warming or configuring

  // Wait while the sensor is calibrating
  while (cnt_cal <= waitingSeconds) { // Wait for waitingMinutes minutes
    ++cnt_cal;
    color_intensity = color_intensity-50; 
    
    setRGB_LEDColor (0, 0, color_intensity);

    delay (1000);
  }
  
  // Take a reading which be used as the zero point for 400 ppm
  mhz19.calibrate();  
  
}

// Send readings to Blynk virtual pins
void send_readings_to_blynk()
{
  int co2ppm = mhz19.getCO2(); // CO2 level (ppm)
  int temp = mhz19.getTemperature(); // Temperature (Celsius)

  // CO2 levels to the RGB led and Blynk notifications
  if (!button1.down) 
  if (co2ppm < CO2_base+300) {
    // Green means low risk
    setRGB_LEDColor (0, 255, 0);}
  else if (co2ppm < CO2_base+400) {
    // Yellow means medium risk
    setRGB_LEDColor (255, 50, 0);}
  else if (co2ppm < CO2_base+600) {
    // Red means high risk
    Blynk.notify("CO2 levels are high (High Risk)");
    setRGB_LEDColor (255, 0, 0);}
  else {
    // Purple means more than high risk
    Blynk.notify("CO2 levels are too high (More than High Risk)");
    setRGB_LEDColor (80, 0, 80);
  } 
  
  // Send data to virtual pins
  Blynk.virtualWrite(5, co2ppm);
  Blynk.virtualWrite(6, temp);
}

void setup()
{
  // Debug console
  Serial.begin(9600);
  
  // Starts the connection  
  Blynk.begin(auth, ssid, pass);

  
  #ifdef HWSERIAL
    Serial2.begin(BAUDRATE, SERIAL_8N1, rx2_pin, tx2_pin);
    mhz19.begin(Serial2);
  #else
    mySerial.begin(BAUDRATE); 
    mhz19.begin(mySerial);
  #endif
  
  mhz19.autoCalibration(false);

  // Led configurations
  ledcAttachPin(led_R, PWMR_Ch);
  ledcSetup(PWMR_Ch, PWM_Freq, PWM_Res);
  ledcAttachPin(led_G, PWMG_Ch);
  ledcSetup(PWMG_Ch, PWM_Freq, PWM_Res);
  ledcAttachPin(led_B, PWMB_Ch);
  ledcSetup(PWMB_Ch, PWM_Freq, PWM_Res);

  setRGB_LEDColor(0, 0, 255);  // Blue means warming or Configuring

  // Button configuration
  pinMode(button1.PIN, INPUT_PULLUP);
  attachInterrupt(button1.PIN, isr_button, CHANGE);

  // Wait 3 mins for warming purposes
  delay(180000);

  // Setup a function to be called every 12 seconds and send data to Blynk
  timer.setInterval(12000L, send_readings_to_blynk);
}

void loop()
{
  int co2ppm = mhz19.getCO2(); // CO2 level (ppm)
  int temp = mhz19.getTemperature(); // Temperature (Celsius)
  
  // Measurements for debugging purposes 
  //Serial.print(co2ppm);
  //Serial.print(","); 
  //Serial.println(temp);
  
  // When the button is pressed for more than 1 second we calibrate the sensor
  if (button1.event) {
      Serial.printf("Button has been pressed for %u millis\n", button1.timePressed);
      if (button1.timePressed < 1000)
        CO2_base = co2ppm;
      else calibrate_mhz19();
      button1.event = false;
  }

  // Initiates BlynkTimer
  Blynk.run();
  timer.run(); 
}
