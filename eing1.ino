// =================================================================
// 🔥 โค้ด Final: แจ้งเตือน Broadcast + หน่วงเวลาปิด + สั่งผ่านแอปได้
// =================================================================

// 1. ตั้งค่า BLYNK 
#define BLYNK_TEMPLATE_ID "TMPL62iShn_VG"
#define BLYNK_TEMPLATE_NAME "eing2"
#define BLYNK_AUTH_TOKEN "DEF4OE3tghZTSRlUX6qEnnvzL1aEgQ2J"

// 2. ตั้งค่า LINE (Broadcast ไม่ต้องใช้ User ID)
const char* LINE_TOKEN = "HHNYB2Q7uIXsk5YSVWXLkpWjVfZb5T+q+Zg9OX+08OUSJqa7S8QdhDJgGJTyB8aUgVXcdQYnPuz6m+yIceonLeF/Gqu8zCrVsHQFRhRnaJXwv20BKk7Sb7BGduuoZOyyCwtfjacSdVNIDyb9KfuxdQdB04t89/1O/w1cDnyilFU="; 

// 3. ตั้งค่า WiFi
char ssid[] = "J-VEC_Teacher"; // หรือ J-VEC_Teacher
char pass[] = "0000011111"; // หรือ 0000011111

// 4. ตั้งค่าความไวและเวลาหน่วง
const int SMOKE_THRESHOLD = 250;  // ลดเกณฑ์ลงเพื่อให้ตรวจจับไวขึ้น
const int DELAY_OFF_TIME = 5000;  // หน่วงเวลาปิด 5 วินาที

// =================================================================

#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiClientSecure.h>

#define MQ2_PIN A0      
#define RELAY_PIN D1    
#define LED_PIN D2      
#define BUZZER_PIN D5  

BlynkTimer timer;
int smokeValue = 0;
bool manualSwitch = false; // ตัวแปรเก็บสถานะปุ่มจากแอป
bool toggleState = false;

unsigned long lastNotifyTime = 0;   
const long notifyInterval = 15000; 

unsigned long stopTimer = 0; // ตัวจับเวลาสำหรับ Delay OFF

// -----------------------------------------------------------------
// ฟังก์ชันส่ง LINE Broadcast (ส่งหาทุกคน)
// -----------------------------------------------------------------
void sendLineMessage(String message) {
  WiFiClientSecure client;
  client.setInsecure(); 

  Serial.println("📢 Broadcasting LINE Message...");

  if (client.connect("api.line.me", 443)) {
    String jsonPayload = "{\"messages\":[{\"type\":\"text\",\"text\":\"" + message + "\"}]}";

    client.println("POST /v2/bot/message/broadcast HTTP/1.1");
    client.println("Host: api.line.me");
    client.println("Authorization: Bearer " + String(LINE_TOKEN));
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonPayload.length());
    client.println();
    client.println(jsonPayload);

    // รอฟังผลจาก Server (เพื่อเช็ค Error)
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") break;
    }
    String response = client.readStringUntil('\n');
    Serial.println("👉 Server Reply: " + response);
    
  } else {
    Serial.println("❌ Connect to LINE failed");
  }
}

// -----------------------------------------------------------------
// ฟังก์ชันหลัก: ตรวจจับควัน + หน่วงเวลาปิด
// -----------------------------------------------------------------
void checkAlarm() {
  smokeValue = analogRead(MQ2_PIN); 
  Blynk.virtualWrite(V0, smokeValue); 
  Serial.print("Smoke: "); Serial.println(smokeValue);

  // 1. เงื่อนไขการ "รีเซ็ตเวลา" (Reset Timer)
  // ถ้าเจอควันเยอะ หรือ กดปุ่มในแอปค้างไว้ -> ให้เวลานับถอยหลังกลับไปเริ่มต้นใหม่เรื่อยๆ
  if (smokeValue > SMOKE_THRESHOLD || manualSwitch == true) {
     stopTimer = millis(); 
  }

  // 2. เงื่อนไขการ "ทำงาน" (Active State)
  // ถ้าเพิ่งผ่านไปไม่ถึง 5 วินาที (นับจากครั้งสุดท้ายที่เจอควันหรือกดปุ่ม) -> ให้ทำงาน
  if (millis() - stopTimer < DELAY_OFF_TIME) { 
    
    // สั่งเปิดทันที
    digitalWrite(RELAY_PIN, HIGH); 
    
    // แจ้งเตือน LINE (เฉพาะกรณีเจอควันจริง และไม่ถี่เกินไป)
    // เราเช็ค smokeValue > threshold อีกที เพื่อกันไม่ให้ส่งไลน์ตอนเรากดปุ่มเล่นเอง
    if (smokeValue > SMOKE_THRESHOLD && (millis() - lastNotifyTime >= notifyInterval)) {
       noTone(BUZZER_PIN); 
       Blynk.logEvent("smoke_alert", "🔥 เจอควัน!");
       
       String msg = "🚨 ไฟไหม้! พบค่าควันและแก๊สพุ่งสูง โปรดระวัง (ค่าควัน: " + String(smokeValue) + ")";
       sendLineMessage(msg); 

       lastNotifyTime = millis();
    }
    
    // ไฟกระพริบ + เสียง
    toggleState = !toggleState;
    if (toggleState) {
       digitalWrite(LED_PIN, HIGH); tone(BUZZER_PIN, 4000); Blynk.virtualWrite(V2, 1);
    } else {
       digitalWrite(LED_PIN, LOW); noTone(BUZZER_PIN); Blynk.virtualWrite(V2, 0);
    }
    
  } else {
    // 3. ถ้าเกิน 5 วินาทีแล้ว (Timeout) -> สั่งปิดทุกอย่าง
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN, LOW); 
    noTone(BUZZER_PIN); 
    Blynk.virtualWrite(V2, 0);
    
    // รีเซ็ตเวลาส่งไลน์ เพื่อให้พร้อมส่งใหม่ในรอบหน้า
    if (smokeValue < (SMOKE_THRESHOLD - 50)) lastNotifyTime = millis() - notifyInterval;
  }
}

// -----------------------------------------------------------------
// รับค่าปุ่มจากแอป Blynk (V1) **สำคัญมาก! ห้ามหาย**
// -----------------------------------------------------------------
BLYNK_WRITE(V1) {
  manualSwitch = param.asInt(); // รับค่า 0 หรือ 1 จากแอป
  checkAlarm(); // เรียกฟังก์ชันตรวจสอบทันทีที่กดปุ่ม เพื่อความไว
}

// -----------------------------------------------------------------
// Setup
// -----------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MQ2_PIN, INPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN); 
  
  lastNotifyTime = millis() - notifyInterval; 

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(500L, checkAlarm); // เช็คทุกๆ 0.5 วินาที
}

void loop() {
  Blynk.run();
  timer.run();
}