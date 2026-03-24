// V3.6
#include "BluetoothSerial.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_bt.h"

BluetoothSerial SerialBT;

// --- KONFIGURASI BATERAI ---
#define TIPE_BATERAI 3 

// --- PIN DEFINITION ---
const int ledPin = 2;       
const int in1 = 26;         
const int in2 = 27;         
const int in3 = 14;         
const int in4 = 12;         

// --- PIN LAMPU ---
const int pinLampuDepan = 19;

// Lampu Belakang KIRI
const int pinBlkKiri_Hijau = 25;
const int pinBlkKiri_Merah = 33;

// Lampu Belakang KANAN
const int pinBlkKanan_Hijau = 17; 
const int pinBlkKanan_Merah = 16; 

// --- SETUP BUZZER 
const int passiveBuzzer = 32;
const int activeBuzzer = 4;

// --- SENSOR ---
const int trigPin = 5;     
const int echoPin = 18;    
const int irKiriPin = 36;   
const int irKananPin = 23;

// --- VARIABEL ---
int motorSpeed;     
int BATAS_MAX;      
int BATAS_MIN;      
int STEP_GIGI;      

// --- STEERING MAPPING ---
int steeringGear = 1; 
float steeringDivisor = 1.5;

char command = '0';
char stateGerak = 'S';

bool isPaused = false;     
bool isRearBlocked = false;    
bool wasLeftBlocked = false;   
bool wasRightBlocked = false;  

long duration;
int distance = 0; 
int lastValidDistance = 999;
unsigned long lastInfoTime = 0; 

// --- VARIABEL SUARA & LAMPU ---
bool isSirenOn = false;           
unsigned long lastSirenTime = 0;  
bool sirenHigh = false;           
bool isFrontLightOn = false; 

// Logic Klakson
bool isHonking = false;           
unsigned long honkStartTime = 0;  
const int DURASI_KLAKSON = 400;

// --- PROTOTIPE FUNGSI ---
void bunyiStartEngine();
void bunyiBluetooth();
void bunyiTurunGigi();
void bunyiNaikGigi();
void bunyiBeep(int frequency);
void setSteeringSensitivity(int gear);
void maju();
void mundur();
void belokKiri();
void belokKanan();
void berhenti();
void eksekusiPerintah(char cmd);
int getRawDistance();
void updateSensorUltrasonik();
void updateSensorInfrared();
void updateMotor();
void updateSuara();

void setup() {
  Serial.begin(115200);
  
  if (TIPE_BATERAI == 3) { 
    BATAS_MAX = 220; BATAS_MIN = 70; STEP_GIGI = 30; 
  } else { 
    BATAS_MAX = 255; BATAS_MIN = 100; STEP_GIGI = 39; 
  }
  motorSpeed = BATAS_MIN;

  pinMode(ledPin, OUTPUT); digitalWrite(ledPin, LOW); 
  pinMode(in1, OUTPUT); pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT); pinMode(in4, OUTPUT);

  // --- SETUP LAMPU ---
  pinMode(pinLampuDepan, OUTPUT);
  digitalWrite(pinLampuDepan, HIGH); 

  pinMode(pinBlkKiri_Hijau, OUTPUT);
  pinMode(pinBlkKiri_Merah, OUTPUT);
  digitalWrite(pinBlkKiri_Hijau, HIGH);
  digitalWrite(pinBlkKiri_Merah, LOW);

  pinMode(pinBlkKanan_Hijau, OUTPUT);
  pinMode(pinBlkKanan_Merah, OUTPUT);
  digitalWrite(pinBlkKanan_Hijau, HIGH);
  digitalWrite(pinBlkKanan_Merah, LOW);

  // --- SETUP BUZZER ---
  pinMode(passiveBuzzer, OUTPUT);
  pinMode(activeBuzzer, OUTPUT);
  digitalWrite(activeBuzzer, LOW);

  pinMode(trigPin, OUTPUT); pinMode(echoPin, INPUT);
  pinMode(irKiriPin, INPUT); pinMode(irKananPin, INPUT);
  
  SerialBT.begin("MobilGanteng"); 
  Serial.println("BLUETOOTH READY! Waiting for Pairing...");
  
  SerialBT.register_callback([](esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
    if (event == ESP_SPP_SRV_OPEN_EVT) {
      Serial.println("CONNECTED");
      digitalWrite(ledPin, HIGH);
      bunyiBluetooth();
    } else if (event == ESP_SPP_CLOSE_EVT) {
      Serial.println("DISCONNECTED");
      digitalWrite(ledPin, LOW);  
      stateGerak = 'S';
      berhenti();                 
    }
  });

  esp_err_t err = esp_bredr_tx_power_set(ESP_PWR_LVL_P3, ESP_PWR_LVL_P3);
  bunyiStartEngine();
  
  setSteeringSensitivity(1);

  if (digitalRead(irKiriPin) == LOW) { 
    wasLeftBlocked = true; //Serial.println("!!! AWAS : KIRI NABRAK !!!"); 
  }
  if (digitalRead(irKananPin) == LOW) { 
    wasRightBlocked = true; //Serial.println("!!! AWAS : KANAN NABRAK !!!"); 
  }
}

void loop() {
  updateSensorUltrasonik();
  updateSensorInfrared();
  updateSuara();

  if (millis() - lastInfoTime > 1000) {
    if (distance > 20) {
      //String info = "Jarak: " + String(distance) + " cm | Steer: Lv " + String(steeringGear);
      //if (SerialBT.hasClient()) SerialBT.println(info);
      //Serial.println(info);
    }
    lastInfoTime = millis();
  }

  if (SerialBT.available()) {
    String incomingData = "";
    while (SerialBT.available()) {
      char c = (char)SerialBT.read();
      incomingData += c;
    }
    incomingData.trim(); 

    // Cek Emergency Stop
    if (incomingData.equalsIgnoreCase("STOP")) {
       Serial.println("!!! EMERGENCY STOP !!!");
       stateGerak = 'S'; berhenti();
       SerialBT.disconnect(); return;
    }

    for (int i = 0; i < incomingData.length(); i++) {
        char c = incomingData.charAt(i);

        if (c == 'W') { 
            isPaused = true; 
            stateGerak = 'S';
            //Serial.println("STATUS: PAUSED"); 
            continue;
        }
        if (c == 'w') { 
            isPaused = false; 
            //Serial.println("STATUS: START"); 
            continue;
        }
        command = c;

        eksekusiPerintah(c);
    }
  }
  
  updateMotor();
}

int getRawDistance() {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long dur = pulseIn(echoPin, HIGH, 25000);
  
  if (dur == 0) {
    if (lastValidDistance < 20) return 0; 
    else return 999;
  }
  return dur * 0.034 / 2;
}

void updateSensorUltrasonik() {
  int r1 = getRawDistance(); delay(2);
  int r2 = getRawDistance(); delay(2);
  int r3 = getRawDistance();

  int fixDist = r2;
  if ((r1 <= r2 && r2 <= r3) || (r3 <= r2 && r2 <= r1)) {
    fixDist = r2;
  } else if ((r2 <= r1 && r1 <= r3) || (r3 <= r1 && r1 <= r2)) {
    fixDist = r1;
  } else {
    fixDist = r3;
  }

  distance = fixDist;

  if (distance != 999) {
    lastValidDistance = distance;
  }
  
  if (distance < 15 && distance >= 0) {
    if (!isRearBlocked) {
      //String msg = "!!! STOP : BELAKANG ADA OBJEK (" + String(distance) + "cm) !!!";
      //Serial.println(msg);
      //if(SerialBT.hasClient()) SerialBT.println(msg);
      isRearBlocked = true; 
    }
  } else {
    if (isRearBlocked) {
      if (distance > 20) {
        //String msg = ">>> AMAN : OBJEK JAUH <<<";
        //Serial.println(msg);
        //if(SerialBT.hasClient()) SerialBT.println(msg);
        isRearBlocked = false; 
      }
    }
  }
}

void updateSensorInfrared() {
  int bacaKiri = digitalRead(irKiriPin);
  int bacaKanan = digitalRead(irKananPin);

  if (bacaKiri == LOW && !wasLeftBlocked) {
    //String msg = "!!! AWAS : KIRI NABRAK !!!";
    //Serial.println(msg); if(SerialBT.hasClient()) SerialBT.println(msg);
    wasLeftBlocked = true;
  } else if (bacaKiri == HIGH && wasLeftBlocked) {
    //String msg = ">>> AMAN : OBJEK KIRI JAUH <<<";
    //Serial.println(msg); if(SerialBT.hasClient()) SerialBT.println(msg);
    wasLeftBlocked = false;
  }

  if (bacaKanan == LOW && !wasRightBlocked) {
    //String msg = "!!! AWAS : KANAN NABRAK !!!";
    //Serial.println(msg); if(SerialBT.hasClient()) SerialBT.println(msg);
    wasRightBlocked = true;
  } else if (bacaKanan == HIGH && wasRightBlocked) {
    //String msg = ">>> AMAN : OBJEK KANAN JAUH <<<";
    //Serial.println(msg); if(SerialBT.hasClient()) SerialBT.println(msg);
    wasRightBlocked = false;
  }
}

void eksekusiPerintah(char cmd) {
  switch (cmd) {
    case 'F': stateGerak = 'F'; break; 
    case 'B': stateGerak = 'B'; break; 
    case 'L': stateGerak = 'L'; break;
    case 'R': stateGerak = 'R'; break;
    case 'S': stateGerak = 'S'; break;
    
    // SIRINE
    case 'X': case 'x': 
      isSirenOn = !isSirenOn; 
      if (!isSirenOn) {
        noTone(passiveBuzzer);
        digitalWrite(pinBlkKiri_Merah, LOW);
        digitalWrite(pinBlkKiri_Hijau, HIGH);
        digitalWrite(pinBlkKanan_Merah, LOW);
        digitalWrite(pinBlkKanan_Hijau, HIGH);
      } else {
        //Serial.println("MODE SIRINE: ON");
      }
      break;

    // LAMPU DEPAN
    case 'U': case 'u':
      isFrontLightOn = !isFrontLightOn;
      if (isFrontLightOn) {
        digitalWrite(pinLampuDepan, LOW); 
        //Serial.println("Lampu Depan: ON");
      } else {
        digitalWrite(pinLampuDepan, HIGH);
        //Serial.println("Lampu Depan: OFF");
      }
      break;

    // KLAKSON
    case 'Y': 
       if (isHonking == false) {
         digitalWrite(activeBuzzer, HIGH);
         honkStartTime = millis(); 
         isHonking = true;         
       }
      break;

    // GIGI SPEED
    case '1': 
      motorSpeed -= STEP_GIGI;
      if (motorSpeed < BATAS_MIN) motorSpeed = BATAS_MIN; 
      if (!isSirenOn) bunyiTurunGigi(); 
      Serial.println("Gigi Turun -> PWM: " + String(motorSpeed));
      if(SerialBT.hasClient()) SerialBT.println("Speed Down: " + String(motorSpeed));
      break;

    case '2': 
      motorSpeed += STEP_GIGI;
      if (motorSpeed > BATAS_MAX) motorSpeed = BATAS_MAX; 
      if (!isSirenOn) bunyiNaikGigi();
      Serial.println("Gigi Naik -> PWM: " + String(motorSpeed));
      if(SerialBT.hasClient()) SerialBT.println("Speed Up: " + String(motorSpeed));
      break;

    // GIGI STEERING
    case '3': 
      steeringGear--;
      if (steeringGear < 1) steeringGear = 1;
      setSteeringSensitivity(steeringGear);
      break;

    case '4': 
      steeringGear++;
      if (steeringGear > 4) steeringGear = 4;
      setSteeringSensitivity(steeringGear);
      break;
  }
}

void updateMotor() {
  if (isPaused) { berhenti(); return; }

  if (stateGerak == 'B' && isRearBlocked) { berhenti(); return; }

  switch (stateGerak) {
    case 'F': maju(); break;
    case 'B': mundur(); break;
    case 'L': belokKiri(); break;
    case 'R': belokKanan(); break;
    case 'S': berhenti(); break;
  }
}

void updateSuara() {
  // 1. UPDATE SIRINE
  if (isSirenOn) {
    if (millis() - lastSirenTime > 400) { 
      lastSirenTime = millis();
      sirenHigh = !sirenHigh;
      
      if (sirenHigh) {
        tone(passiveBuzzer, 900);
        digitalWrite(pinBlkKiri_Merah, HIGH); 
        digitalWrite(pinBlkKanan_Merah, HIGH);
        digitalWrite(pinBlkKiri_Hijau, LOW); 
        digitalWrite(pinBlkKanan_Hijau, LOW); 
      } else {
        tone(passiveBuzzer, 600);
        digitalWrite(pinBlkKiri_Merah, LOW); 
        digitalWrite(pinBlkKanan_Merah, LOW);
        digitalWrite(pinBlkKiri_Hijau, LOW);
        digitalWrite(pinBlkKanan_Hijau, LOW);
      }        
    }
  }

  // 2. UPDATE KLAKSON
  if (isHonking) {
    if (millis() - honkStartTime > DURASI_KLAKSON) {
      digitalWrite(activeBuzzer, LOW);
      isHonking = false;
    }
  }
}

void maju() {
  analogWrite(in1, motorSpeed); analogWrite(in2, 0);
  analogWrite(in3, 0);          analogWrite(in4, motorSpeed);
}
void mundur() {
  analogWrite(in1, 0);          analogWrite(in2, motorSpeed);
  analogWrite(in3, motorSpeed); analogWrite(in4, 0);
}
void berhenti() {
  analogWrite(in1, 0); analogWrite(in2, 0);
  analogWrite(in3, 0); analogWrite(in4, 0);
}
void belokKiri() {
  int speedLuar, speedDalam;
  if (TIPE_BATERAI == 3) {
    speedLuar = motorSpeed + 40;
    if (speedLuar > BATAS_MAX) speedLuar = BATAS_MAX; 
  } else {
    speedLuar = motorSpeed + 60;
    if (speedLuar > 255) speedLuar = 255; 
  }
  speedDalam = (int)(motorSpeed / steeringDivisor);
  analogWrite(in1, speedLuar);  analogWrite(in2, 0);
  analogWrite(in3, 0);          analogWrite(in4, speedDalam); 
}
void belokKanan() {
  int speedLuar, speedDalam;
  if (TIPE_BATERAI == 3) {
    speedLuar = motorSpeed + 40;
    if (speedLuar > BATAS_MAX) speedLuar = BATAS_MAX; 
  } else {
    speedLuar = motorSpeed + 60;
    if (speedLuar > 255) speedLuar = 255; 
  }
  speedDalam = (int)(motorSpeed / steeringDivisor);
  analogWrite(in1, speedDalam); analogWrite(in2, 0);
  analogWrite(in3, 0);          analogWrite(in4, speedLuar);  
}

void setSteeringSensitivity(int gear) {
  String modeName = "";
  
  switch (gear) {
    case 1:
      steeringDivisor = 1.5;
      modeName = "1 (WIDE)"; 
      if (!isSirenOn) tone(passiveBuzzer, 1047, 80); 
      break;
    case 2:
      steeringDivisor = 3.0;
      modeName = "2 (RALLY)"; 
      if (!isSirenOn) tone(passiveBuzzer, 1175, 80); 
      break; 
    case 3:
      steeringDivisor = 6.0;
      modeName = "3 (SHARP)"; 
      if (!isSirenOn) tone(passiveBuzzer, 1319, 80); 
      break;
    case 4:
      steeringDivisor = 9.0;
      modeName = "4 (PIVOT)"; 
      if (!isSirenOn) tone(passiveBuzzer, 1397, 80); 
      break;
  }
  Serial.println("Set Steer: " + modeName);
  if(SerialBT.hasClient()) SerialBT.println("Steer: " + modeName);
}

void bunyiBeep(int frequency) { 
  tone(passiveBuzzer, frequency, 100); 
}
void bunyiStartEngine() { 
  tone(passiveBuzzer, 200, 200); delay(200);
  tone(passiveBuzzer, 400, 200); delay(200);
  tone(passiveBuzzer, 800, 400); 
}
void bunyiBluetooth() { 
  tone(passiveBuzzer, 1500, 100); delay(150); 
  tone(passiveBuzzer, 2000, 100); 
}
void bunyiTurunGigi() { 
  tone(passiveBuzzer, 300, 400); 
}
void bunyiNaikGigi() { 
  tone(passiveBuzzer, 2000, 400); 
}