//----------------------------------------------------------------------
//    Sistem Irigasi Cerdas dan Monitoring Sawah 4.0 IPB University
//                           (Node Aktuator)
//
//    Author        : Muhammad Hafiduddin
//    Version       : 1.0 (2022)
//    Supervised by : Dr. Karlisa Priandana, S.T., M.Eng.
//                    Auriza Rahmad Akbar, S.Komp., M.Kom.
//
//    c 2022 Computer Science IPB University
//----------------------------------------------------------------------

//#define THINGER_SERIAL_DEBUG

#include <ThingerESP8266.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiUdp.h>


// konfigurasi Thinger.io
#define USERTHINGER "______________"  // username akun Thinger.io
#define DEVICE_ID "_____________"     // ID perangkat
#define DEVICE_CRED "_______________" // password perangkat

// konfigurasi WiFi
#define SSID "__________________"     // SSID WiFi
#define WIFIPASS "_________________"  // password WiFi
#define USER "__________________"     // username akun
#define ACCPASS "_________________"   // password akun

// Inisialisasi variabel dari library
WiFiClient client;
HTTPClient http;
LiquidCrystal_I2C lcd(0x27, 20, 4);
ThingerESP8266 thing(USERTHINGER, DEVICE_ID, DEVICE_CRED);
Servo servo;

// inisialisasi variabel 
int currentState = 0;
bool reset = false;
const int interval = 5000;

// proses login ke wifi kampus
void loginWifi() {
  http.begin(client, "http://1.1.1.3/ac_portal/login.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
  String body = "opr=pwdLogin&userName=";
  body += USER;
  body += "&pwd=";
  body += ACCPASS;
  body += "&rememberPwd=1";
    
  int httpcode = http.POST(body);
  if (httpcode > 0){
    Serial.println(http.getString());
  }
  http.end();
}

// ------------------------------------------ MAIN FUNCTION -----------------------------------------
void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  // tampilkan nama alat di LCD
  lcd.clear();  
  lcd.print("   SISTEM IRIGASI   ");
  lcd.setCursor(0, 1);
  lcd.print("       CERDAS       ");
  lcd.setCursor(0, 2);
  lcd.print("   Sawah 4.0 IPB    ");
  lcd.setCursor(0, 3);
  lcd.print("  (Node Aktuator)   ");
  delay(3000);
  
  pinMode(14, OUTPUT);

  // hubungkan wifi
  WiFi.begin(SSID, WIFIPASS);
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    lcd.clear();
    lcd.print("Menghubungkan...");
    lcd.setCursor(0, 1);
    lcd.print("ke Wifi ");
    lcd.print(SSID);
    delay(500);
  }
  Serial.println("Terhubung!");
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print("     Terhubung!     ");
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  loginWifi();

  // jalankan dan set servo ke 0 derajat
  servo.write(((float(currentState)*2)*0.9));

  // set tampilan lcd
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Open Percentage :");
  lcd.setCursor(0, 1);
  lcd.print(currentState);
  lcd.print("% (");
  lcd.print(currentState*0.9);
  lcd.print((char)223);
  lcd.print(")");

  lcd.setCursor(0, 2);
  lcd.print("WiFi RSSI :");
  lcd.setCursor(0, 3);
  lcd.print(WiFi.RSSI());

  // dapatkan data keterbukaan aktuator dari Thinger.io
  thing["aktuator"] << [](pson& in){    
    if (!in.is_empty()){
      
      currentState = in;

      lcd.setCursor(0, 0);
      lcd.print("Open Percentage :");
      lcd.setCursor(0, 1);
      lcd.print(currentState);
      lcd.print("% (");
      lcd.print(currentState*0.9);
      lcd.print((char)223);
      lcd.print(")");
    }
    
    // servo digerakkan sesuai nilai di server
    // derajat pada servo = servo.write(2*value)
    // nilai dikoversi dari persentase (0-100) ke derajat sudut (0-90)
    // konversi dilakukan dengan mengalikan nilai dengan 0.9
    servo.attach(14);
    Serial.println(float(currentState));
    servo.write(((float(currentState)*2)*0.9));
    delay(2000);
    servo.detach();
  };
}

int hitMillis = millis();

// inisialisasi kondisi wifi diskonek atau tidak
bool wifidisconnect = false;

void loop() {
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    lcd.clear();
    lcd.print(" Menghubungkan ke.. ");
    lcd.setCursor(0, 1);
    lcd.print("Wifi ");
    lcd.print(SSID);

    wifidisconnect = true;
    delay(500);
  }

  //jika WiFi terkoneksi kembali
  if (wifidisconnect == true){
    Serial.println("Terhubung!");
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("     Terhubung!     ");

    loginWifi();
    
    wifidisconnect = false;
    delay(2000);
  }

  if (millis() - hitMillis >= interval){
    hitMillis = millis();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Open Percentage :");
    lcd.setCursor(0, 1);
    lcd.print(currentState);
    lcd.print("% (");
    lcd.print(currentState*0.9);
    lcd.print((char)223);
    lcd.print(")");
    lcd.setCursor(0, 2);
    lcd.print("WiFi RSSI :");
    lcd.setCursor(0, 3);
    lcd.print(WiFi.RSSI());
  }
  
  thing.handle();
}
