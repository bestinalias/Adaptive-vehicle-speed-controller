#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Clcd.h>
#include <SPI.h>
#include <SD.h>
#include <TMRpcm.h>

#define D4 4  // HT12D Data pins
#define D5 5
#define D7 7
#define D8 8

#define ENA 3  // Motor PWM control (moved from D6 to D3)
#define SPEED_UP 2  // Button to increase speed
#define SPEED_DOWN A1  // Button to decrease speed (moved from D3 to A1)
#define VT_PIN A2  // Valid Transmission Pin

#define SD_CS 6  // SD Card Chip Select
#define AUDIO_PIN 10  // Speaker output pin

#define ENTER_AUDIO "voice2.wav"  
#define EXIT_AUDIO "voice3.wav"  

int currentSpeed = 0;  // Default motor speed (0 km/hr)
int limitSpeed = 0;  // Speed limit from RF module
bool speedChanged = true;S
bool inZone = false;

hd44780_I2Clcd lcd;  
TMRpcm audio;  

void setup() {
    pinMode(D4, INPUT);
    pinMode(D5, INPUT);
    pinMode(D7, INPUT);
    pinMode(D8, INPUT);
    pinMode(VT_PIN, INPUT);  
    pinMode(ENA, OUTPUT);
    pinMode(SPEED_UP, INPUT_PULLUP);
    pinMode(SPEED_DOWN, INPUT_PULLUP);

    Serial.begin(9600);
    lcd.begin(16, 2);
    lcd.print(F("System Init..."));
    delay(1500);
    lcd.clear();

    Serial.println(F("Initializing SD Card..."));
    if (!SD.begin(SD_CS)) { 
        Serial.println(F("SD Card Initialization Failed!"));
    } else {
        Serial.println(F("SD Card Ready!"));
    }

    audio.speakerPin = AUDIO_PIN;
    audio.setVolume(5);  // Set default volume
    updateMotor();
    updateLCD();
}

void loop() {
    handleButtonPress();
    bool isVT_High = digitalRead(VT_PIN) == HIGH;
    int newLimitSpeed = isVT_High ? receiveSpeedLimit() : 0;

    if (newLimitSpeed != limitSpeed) {
        limitSpeed = newLimitSpeed;
        updateLCD();
        
        if (limitSpeed > 0 && !inZone) {  
            Serial.println(F("Entering Restricted Zone..."));
            playAudio(ENTER_AUDIO);
            inZone = true;
        } 
        else if (limitSpeed == 0 && inZone) {  
            Serial.println(F("Exiting Restricted Zone..."));
            playAudio(EXIT_AUDIO);
            inZone = false;
        }
    }

    Serial.print(F("VT: ")); Serial.print(isVT_High);
    Serial.print(F(" | Speed Limit: ")); Serial.println(limitSpeed);

    if (limitSpeed > 0 && currentSpeed > limitSpeed) {
        adjustSpeed(limitSpeed);
    }

    if (speedChanged) {
        updateMotor();
        updateLCD();
        speedChanged = false;
    }

    delay(200);
}

void playAudio(const char *fileName) {
    Serial.print(F("Checking file: ")); Serial.println(fileName);

    if (SD.exists(fileName)) { 
        Serial.println(F("File Found! Trying to play..."));
        audio.play(fileName);
    } else {
        Serial.println(F("Error: File not found on SD card!"));
    }
}

int receiveSpeedLimit() {
    int receivedValue = (digitalRead(D8) << 3) | (digitalRead(D7) << 2) | (digitalRead(D5) << 1) | digitalRead(D4);
    Serial.print(F("Received Data: ")); Serial.println(receivedValue, BIN);
    
    switch (receivedValue) {
        case 0b0111: return 20;  
        case 0b1011: return 40;  
        case 0b1101: return 50;  
        case 0b1110: return 60;  
        default: return 0;   
    }
}

void handleButtonPress() {
    if (digitalRead(SPEED_UP) == LOW) {
        increaseSpeed();
        delay(300);
    }
    if (digitalRead(SPEED_DOWN) == LOW) {
        decreaseSpeed();
        delay(300);
    }
}

void increaseSpeed() {
    if (currentSpeed < 80) {  
        currentSpeed += 10;
        speedChanged = true;
        Serial.print(F("Speed Increased: ")); Serial.println(currentSpeed);
        
        // Stop unwanted sounds if VT is LOW
        if (digitalRead(VT_PIN) == LOW) {
            audio.stopPlayback();
        }
    }
}

void decreaseSpeed() {
    if (currentSpeed > 0) {  
        currentSpeed -= 10;
        speedChanged = true;
        Serial.print(F("Speed Decreased: ")); Serial.println(currentSpeed);
        
        // Stop unwanted sounds if VT is LOW
        if (digitalRead(VT_PIN) == LOW) {
            audio.stopPlayback();
        }
    }
}

void adjustSpeed(int newSpeed) {
    Serial.print(F("Adjusting Speed to ")); Serial.println(newSpeed);
    
    while (currentSpeed > newSpeed) {
        currentSpeed = max(currentSpeed - 5, newSpeed);
        updateMotor();
        updateLCD();
        Serial.print(F("Current Speed: ")); Serial.println(currentSpeed);
        delay(300);
    }
}

void updateMotor() {
    int motorPWM = map(currentSpeed, 0, 80, 0, 255);
    Serial.print(F("Motor PWM: ")); Serial.println(motorPWM);
    analogWrite(ENA, motorPWM);
}

void updateLCD() {
    lcd.setCursor(0, 0);
    lcd.print(F("Speed: "));
    lcd.print(currentSpeed);
    lcd.print(F("km/h    "));

    lcd.setCursor(0, 1);
    lcd.print(F("Limit: "));
    if (limitSpeed > 0) {
        lcd.print(limitSpeed);
        lcd.print(F("km/h    "));
    } else {
        lcd.print(F("No Limit "));
    }
}