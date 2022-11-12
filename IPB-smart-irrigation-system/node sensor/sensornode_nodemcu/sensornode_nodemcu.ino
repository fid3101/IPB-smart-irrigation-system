//----------------------------------------------------------------------
//    Sistem Irigasi Cerdas dan Monitoring Sawah 4.0 IPB University
//                       (Node Sensor : Master)
//
//    Author        : Karyadi Mochtar & Muhammad Hafiduddin
//    Version       : 1.0 (2022)
//    Supervised by : Dr. Karlisa Priandana, S.T., M.Eng.
//                    Dr. Ir. Sri Wahjuni, M.T.
//                    Auriza Rahmad Akbar, S.Komp., M.Kom.
//
//    c 2022 Computer Science IPB University
//----------------------------------------------------------------------

// node sensor bagian master bertugas membaca data yang dikirimkan 
// slave arduino (sensor tanah dan hujan), melakukan pembacaan nilai sensor
// ultrasonik dan sensor DHT22, menampilkannya di LCD,
// dan mengirimkan data ke server thinger.io

//#define THINGER_SERIAL_DEBUG

#include <ThingerESP8266.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <EEPROM.h>

#include "fuzzySugeno.h"

//konfigurasi Thinger.IO
#define USERNAME "____________"              // username akun thinger.io
#define DEVICE_ID "________________"         // ID perangkat
#define DEVICE_CREDENTIAL "_____________"    // Password perangkat

//konfigurasi wifi
#define SSID  "______________"
#define WIFIPASS "_______________"           // wifi password
#define USER "_______________"               // username login wifi
#define ACCPASS "______________"             // password login wifi

//konfigurasi sensor DHT22
#define DHTTYPE DHT22     //tipe sensor DHT
#define DHTPIN 13         //pin untuk sensor DHT

//konfigurasi sensor ultrasonik
#define TRIG 14           //pin trigger sensor ultrasonik
#define ECHO 12           //pin echo sensor ultrasonik
float TUBEHEIGHT = 19.00;  //tinggi sensor ultrasonik dari dasar tabung


// --------------- KONFIGURASI ATURAN UNTUK KONDISI AWD --------------------------
// di kondisi AWD terdapat 4 kondisi yaitu :
// - normal (soil moist 80% - 100%)
// - penyiangan (soil moist 80% - water height 2 cm)
// - pematangan (soil moist 60% - 100%)
// - panen (soil moist sekering-keringnya)
const int normalHeight = 0;

const int weedingDistBound[] = {2, 80};   // batas ketinggian air saat penyiangan {max_of_water_height, min_of_soil_moist}
const int weedingHSTBound[][2] = {
  {10, 12},   // fase penyiangan pertama (hst 10 sampai hst 11)
  {20, 22},   // fase penyiangan kedua (hst 20 sampai hst 21)
  {30, 32},   // fase penyiangan ketiga (hst 30 sampai hst 31)
  {40, 45}    // fase penyiangan keempat (hst 40 sampai hst 44)
  // fase penyiangan lebih dari empat bisa langsung ditambahkan
};

const int ripeningHSTBound[][2] = {
  {45, 55},   // fase pematangan pertama (hst 45 sampai hst 54)
  {75, 85}    // fase pematangan kedua (hst 75 sampai hst 84)
};

const int harvestHSTBound = 85;

// -------------------------------------------------------------------------

//konfigurasi EEPROM
#define EEPROM_SIZE 512

//konfigurasi LCD
LiquidCrystal_I2C lcd(0x27, 20, 4); // (I2C address, rows, columns)

// inisialisasi WiFi Client & HTTP client untuk login wifi
WiFiClient client;
HTTPClient http;

// inisialisasi Thinger.IO device
ThingerESP8266 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);

//inisialisasi protokol WiFi UDP dan NTP client
const int timeOffset = 25200; // GMT + 7
WiFiUDP ntpudp;
NTPClient timentp(ntpudp, "pool.ntp.org", timeOffset);

//inisialisasi DHT22
DHT dht(DHTPIN, DHTTYPE);

// inisialisasi array, counter, dan delaytime
unsigned long previousMillis = 0;
const int interval = 5000;          // delay
String arrData[7];                  // variabel sementara untuk hasil parsing data sensor tanah

//inisialisasi variabel penampung nilai sensor dan kondisi
int nitrogen, phospor, kalium, ec;
float dist, dist1, moisture, temp, rainIntensity, ph, output;
long duration;
int x = 0;

String incomingByte;    //penampung data dari serial

int plantingDate = 0;   //tanggal penanaman padi
int currentDate = 0;    //tanggal sekarang
int HST = 0;            //hari setelah tanam (selisih tanggal sekarang dan tanggal penanaman)


// FUNGSI KONVERSI DATA TANGGAL KE JUMLAH HARI
int convertDate(int date, int month, int year){
  int day = 0;
  // hitung jumlah hari dari tahun 1 sampai tahun kemarin
  // jumlah hari setiap 4 tahun = 365 + 365 + 365 + 366 = 1461
  // misal 2022 % 4 = 2 tahun, maka jumlah hari di tahun sisa bagi 4 adalah 365*2 tahun
  day += ((year-1)/4)*1461 + ((year-1)%4)*365;
  // cek tahun kabisat, jika ya maka bulan ke-2 (februari) memiliki 29 hari
  if (year%4 == 0){
    int dayPerMonth[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    // hitung total hari sejak awal tahun sampai bulan sebelum bulan sekarang
    for (int i = 0; i < month-1; i++){  
      day += dayPerMonth[i];
    }
  } else {
    int dayPerMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int i = 0; i < month-1; i++){
      day += dayPerMonth[i];
    }
  }
  // tambahkan tanggal sekarang sebagai jumlah hari yang telah terlewati di bulan ini
  day += date;
  return day;
}

// FUNGSI RESET TANGGAL PENANAMAN
// akan aktif jika user memencet tombol "SET" di keypad pada slave
// lalu slave akan mengirimkan flag kesini (master) dan diarahkan ke fungsi ini
void resetPlantingDate(){
  String bytes;
  int date, month, year;
  
  lcd.clear();
  
  // lakukan tiga kali input untuk tanggal, bulan, dan tahun secara berurutan
  // proses akan diteruskan jika 3 input tersebut lengkap
  for (int i = 0; i < 3; ){

    // tampilkan tampilan reset tanggal penanaman
    lcd.setCursor(0, 0);
    lcd.print("  Masukkan Tanggal  ");
    lcd.setCursor(0, 1);
    lcd.print("     Penanaman      ");

    lcd.setCursor(5, 3);
    lcd.print(" - "); 

    lcd.setCursor(10, 3);
    lcd.print(" - "); 

    // tangkap data tanggal bulan dan tahun dari arduino (slave) melalui serial
    // jika input keypad di slave benar sesuai aturan, maka angka akan dikirim
    // jika input keypad tidak sesuai, maka slave akan mengirim flag "XX" artinya input salah
    while(Serial.available()>0){
      bytes = Serial.readString();
      if (bytes != "XX"){
        Serial.println(bytes.toInt());
        // jika data merupakan inputan pertama dan data benar, 
        // maka data akan disimpan di variabel date (tanggal) dan ditampilkan di LCD
        if (i == 0) {
          date = bytes.toInt();
          lcd.setCursor(3, 3);
          lcd.print(date);
        }
        // jika data merupakan inputan kedua dan data benar, 
        // maka data akan disimpan di variabel month (bulan) dan ditampilkan di LCD
        if (i == 1){
          month = bytes.toInt();
          lcd.setCursor(8, 3);
          lcd.print(month);
        }
        // jika data merupakan inputan ketiga dan data benar, 
        // maka data akan disimpan di variabel year (tahun) dan ditampilkan di LCD
        if (i == 2) {
          year = bytes.toInt()+2000;
          lcd.setCursor(13, 3);
          lcd.print(year);
        }
        i++;    // indeks ditambah jika data benar, artinya sistem meneruskan ke inputan selanjutnya
      } else {
        // jika data merupakan inputan pertama dan data salah, 
        // maka akan ditampilkan peringatan di LCD dan inputan pertama diulangi
        if (i == 0){
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Tanggal tidak sesuai!");
          lcd.setCursor(0, 2);
          lcd.print("Masukkan lagi tanggal");
          lcd.setCursor(0, 3);
          lcd.print("    yang sesuai!     ");
          delay(2000);
          lcd.clear();
        }
        // jika data merupakan inputan kedua dan data salah, 
        // maka akan ditampilkan peringatan di LCD dan inputan kedua diulangi
        else if (i == 1){
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Bulan tidak sesuai!");
          lcd.setCursor(0, 2);
          lcd.print("Masukkan lagi bulan");
          lcd.setCursor(0, 3);
          lcd.print("    yang sesuai!   ");
          delay(2000);
          lcd.clear();

          lcd.setCursor(3, 3);
          lcd.print(date);
        }
      }
    }
  }
  // jika ketiga inputan sudah sesuai, sistem menunggu flag dari slave berisi string "O"
  // flag dikirim sebagai konfirmasi dari user bahwa inputan sudah sesuai dan siap diterapkan
  for (int i = 0; i < 1; ){
    while (Serial.available()>0){
      incomingByte = Serial.readString();
      if(incomingByte == "O"){
        lcd.clear();
        lcd.print(" Tanggal Penanaman  ");
        lcd.setCursor(0, 1);
        lcd.print("  Berhasil Diubah!  ");
        lcd.setCursor(5, 3);
        lcd.print((String(date)+"-"+String(month)+"-"+String(year)));

        delay(2000);

        // hasil input tanggal penanaman dikonversi ke bentuk jumlah hari
        // dan disimpan di EEPROM agar dapat diterapkan lagi jika sistem dimatikan
        plantingDate = convertDate(date, month, year);
        EEPROM.put(0, plantingDate);
        EEPROM.commit();
        i++; 
      }
    }
  }
}

// FUNGSI MENGALIBRASI TINGGI TABUNG SENSOR ULTRASONIK
// fungsi akan dipanggil jika user menginput 'D' di keypad
void calibrateUltrasonic(){
    lcd.clear();
    lcd.print("Kalibrasi Ultrasonik");
    lcd.setCursor(0, 1);
    lcd.print("angkat tabung sensor");
    lcd.setCursor(0, 2);
    lcd.print("     dari air!      ");
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Sdng Mengalibrasi...");
    delay(1000);

    // proses kalibrasi dilakukan dengan mengukur 
    // jarak antara dasar tabung dengan sensor.
    digitalWrite(TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG, LOW);
    duration = pulseIn(ECHO, HIGH);
    dist1 = duration * 0.034 / 2;
    TUBEHEIGHT = dist1;
    delay(1000);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Kalibrasi Selesai  ");
    lcd.setCursor(0, 2);
    lcd.print("  Tinggi tabung :   ");
    lcd.setCursor(0, 3);
    lcd.print("      ");
    lcd.print(TUBEHEIGHT);
    lcd.print(" cm      ");

    // hasil pengukuran jarak kemudian disimpan di EEPROM
    EEPROM.put(1, TUBEHEIGHT);
    EEPROM.commit();
    
    delay(1000);
}

// FUNGSI LOGIN KE JARINGAN WIFI
// fungsi harus dipanggil ketika menggunakan wifi yang memiliki portal login
// seperti wifi kampus.
// fungsi akan melakukan bypass post http untuk mengirim payload username dan pass akun ke portal
void loginWifi() {
  http.begin(client, "http://1.1.1.3/ac_portal/login.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
  String body = "opr=pwdLogin&userName=";
  body += USER;
  body += "&pwd=";
  body += ACCPASS;
  body += "&rememberPwd=1";
    
  int httpcode = http.POST(body);
  //Serial.println(httpcode);
  if (httpcode > 0){
    Serial.println(http.getString());
  }
  http.end();
}

// FUNGSI PENANGKAP DATA DARI SERIAL UART
// fungsi untuk menangkap seluruh data dari slave melalui serial UART
void receivedata(){
  while(Serial.available()>0){
    incomingByte = Serial.readString();
    // cek apakah data merupakan flag interupt untuk reset tanggal penanaman atau bukan 
    if (incomingByte.substring(incomingByte.length()-3, incomingByte.length()) == "SET" || incomingByte == "SET\n"){
      resetPlantingDate();
    } else if (incomingByte.substring(incomingByte.length()-3, incomingByte.length()) == "CAL" || incomingByte == "CAL\n"){
      calibrateUltrasonic();
    } else {
      if (millis() - previousMillis >= interval){
        previousMillis = millis();
    
        String data;
        data = incomingByte; 
        
        //hapus white space pada data yang dikirim
        data.trim();
        
        if (data){
          int index = 0;
          // parsing data
          for (int i=0; i <= data.length(); i++) {
            char delimiter = '#';
            // masukkan data[i] kedalam arrData[index] jika tidak ada delimiter '#', 
            // jika ditemukan delimiter '#', maka index arrData akan ditambah
            // data setelah delimiter akan dimasukkan ke ardata index selanjutnya
            if (data[i] != delimiter) {
              if (data[i] == '\n') break;
              else arrData[index] += data[i];
            } else {
              index++;
            }
          }
    
          //isi variabel global nilai sensor dengan data serial hasil parsing 
          ph         = arrData[0].toFloat();
          moisture      = arrData[1].toFloat();
          ec         = arrData[2].toInt();
          nitrogen   = arrData[3].toInt();
          phospor    = arrData[4].toInt();
          kalium     = arrData[5].toInt();
          rainIntensity       = map(arrData[6].toFloat(), 610.7, 992.2, 100, 0);        
          // nilai sensor hujan dipetakan ke hasil kalibrasi kasar
          
          //reset kembali nilai array utk menerima data baru
          arrData[0] = "";
          arrData[1] = "";
          arrData[2] = "";
          arrData[3] = "";
          arrData[4] = "";
          arrData[5] = "";
          arrData[6] = "";
        }
      }
    }
  }
}

// ------------------------------------------- MAIN FUNCTION ---------------------------------------------

void setup() {  
  
  Serial.begin(9600);
  dht.begin();
  timentp.begin();

  // inisialisasi pin mode untuk sensor ultrasonik
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // get tanggal penanaman yang tersimpan di EEPROM
  EEPROM.begin(EEPROM_SIZE);
  if(EEPROM.read(0)){
    EEPROM.get(0, plantingDate);
  }
  if(EEPROM.read(1)){
    EEPROM.get(1, TUBEHEIGHT);
  }
  
  // persiapan LCD dan tampilan nama sistem
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("   SISTEM IRIGASI   ");
  lcd.setCursor(0, 1);
  lcd.print("       CERDAS       ");
  lcd.setCursor(0, 2);
  lcd.print("   Sawah 4.0 IPB    ");
  lcd.setCursor(0, 3);
  lcd.print("   (Node Sensor)    ");
  delay(3000);
  lcd.clear();

  // mulai koneksi wifi sesuai SSID dan Password wifi yang telah ditentukan
  WiFi.begin(SSID, WIFIPASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    lcd.clear();
    lcd.print(" Menghubungkan ke.. ");
    lcd.setCursor(0, 1);
    lcd.print("Wifi ");
    lcd.print(SSID);
    delay(500);
  }

  //jika WiFi terkoneksi
  Serial.println("Terhubung!");
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("     Terhubung!     ");

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  loginWifi();
  delay(1250);

  // kirim flag request data ke slave (Arduino) melalui serial
  Serial.println("Ya");

  // data yang dikirim ke thinger.io
  thing["Data"] >> [] (pson & out){
    out["Nitrogen"]   = nitrogen;
    out["Phospor"]    = phospor;
    out["Kalium"]     = kalium;
    out["Suhu"]       = temp;
    out["Moisture"]   = moisture;
    out["EC"]         = ec;
    out["pH"]         = ph;
    out["output"]     = output*100;
    out["WiFiRSSI"]   = WiFi.RSSI();
    out["Distance"]   = dist;
    out["Rain"]       = rainIntensity;
    out["HST"]        = HST;
  };
}

// dapatkan millis saat pertama kali program loop dieksekusi
int hitMillis = millis();
int lcdPrint = 0;

// inisialisasi kondisi wifi diskonek atau tidak
bool wifidisconnect = false;

void loop() {
  // selalu cek koneksi Wifi di setiap loop
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
  
  // panggil fungsi receivedata() untuk mendapatkan data dari arduino via serial
  receivedata();

  // eksekusi semua perintah setiap interval waktu yang telah ditentukan
  if (millis()-hitMillis >= interval){
    hitMillis = millis();

    // get data suhu udara dari sensor DHT
    temp = dht.readTemperature();
    
    // get data ketinggian air dari sensor ultrasonik
    digitalWrite(TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG, LOW);
    duration = pulseIn(ECHO, HIGH);
    dist1 = duration * 0.034 / 2;
    dist = TUBEHEIGHT-dist1;
  
    // get tanggal saat ini dan hitung HST (selisih tanggal penanaman dan tanggal saat ini)
    timentp.update();
    time_t epochTime = timentp.getEpochTime();
    struct tm *ptm = gmtime((time_t *) &epochTime);
    int monthDay = ptm->tm_mday;
    int currentMonth = ptm->tm_mon+1;
    int currentYear = ptm->tm_year+1900;
    currentDate = convertDate(monthDay, currentMonth, currentYear);
    HST = currentDate-plantingDate;

    // cek apakah HST termasuk masa penyiangan atau bukan
    bool stat = false;
    bool tempStat;
    
    for (int i = 0; i < (sizeof(weedingHSTBound) / sizeof(weedingHSTBound[0])); i++){
      tempStat = false;
      if (HST >= weedingHSTBound[i][0] && HST < weedingHSTBound[i][1]) tempStat = true;
      stat = stat || tempStat;
    }
    if (stat){
      // jika masa penyiangan, maka jaga ketinggian air di kelembapan 80% - ketinggian 2cm
      output = 1;
      if (dist>=weedingDistBound[0]) output = 0;
      else if (moisture<=weedingDistBound[1]) output = 1;
    }
     
    // jika selain masa penyiangan, maka eksekusi fuzzy
    else {        
      // cek terlebih dahulu ketinggian air
      // jika lebih dari normalHeight+0.5 maka fuzzy tidak akan dieksekusi
      // +0.5 cm ditambahkan untuk toleransi
      if (dist >= normalHeight+0.5){
         output = 0;
      } else {
        // eksekusi fuzzy jika semua kondisi sudah memenuhi
        // cek kondisi apakah hst termasuk kondisi pematangan
        for (int i = 0; i < (sizeof(ripeningHSTBound) / sizeof(ripeningHSTBound[0])); i++){
          tempStat = false;
          if (HST >= ripeningHSTBound[i][0] && HST < ripeningHSTBound[i][1]) tempStat = true;
          stat = stat || tempStat;
        }
        if (stat){
          // jika kondisi pematangan, panggil fungsi fuzzyResult dari file fuzzySugeno.h 
          // dengan argumen rule_base = "ripening"
          output = fuzzyResult(moisture, temp, rainIntensity, "ripening"); // fuzzyResult(soil_moisture, air_temperature, rain_intensity, rule_base)
        }
        // jika HST termasuk fase panen, maka output diset 0 
        else if (HST>=harvestHSTBound){
          output = 0;
        } 
        // jika tidak termasuk ke fase penyiangan, pematangan, maupun panen, maka kondisi adalah normal (common vegetative)
        // panggil fungsi fuzzyResult dari file fuzzySugeno.h dengan argumen rule_base = normal
        else output = fuzzyResult(moisture, temp, rainIntensity, "normal"); // fuzzyResult(soil_moisture, air_temperature, rain_intensity, rule_base)
      }  
    }

    // tampilkan update kondisi node sensor di LCD
    if (lcdPrint == 0){
      lcdPrint = 1;
      lcd.clear();    
      lcd.print("WiFi   :  ");            
      lcd.print(WiFi.RSSI());
      lcd.setCursor(0, 1);             
      lcd.print("HST    :  ");
      lcd.print(HST);
      lcd.setCursor(0, 2);             
      lcd.print("Output : ");
      lcd.print(output); 
    }

    else if (lcdPrint == 1){
      lcdPrint = 2;
      lcd.clear();    
      lcd.print("Moist     :  ");            
      lcd.print(moisture);
      lcd.print("%");
      lcd.setCursor(0, 1);             
      lcd.print("Water lvl :  ");
      lcd.print(dist);
      lcd.print(" cm");
      lcd.setCursor(0, 2);             
      lcd.print("Temp      :  ");
      lcd.print(temp);
      lcd.print(" C");
      lcd.setCursor(0, 3);             
      lcd.print("Rain      :  ");
      lcd.print(rainIntensity);
      lcd.print("%");
    }

    else if (lcdPrint == 2){
      lcdPrint = 3;
      lcd.clear();    
      lcd.print("EC  :  ");            
      lcd.print(ec);
      lcd.print(" us/cm");
      lcd.setCursor(0, 1);             
      lcd.print("N   :  ");
      lcd.print(nitrogen);
      lcd.print(" mg/kg");
      lcd.setCursor(0, 2);             
      lcd.print("P   :  ");
      lcd.print(phospor);
      lcd.print(" mg/kg");
      lcd.setCursor(0, 3);             
      lcd.print("K   :  ");
      lcd.print(kalium);
      lcd.print(" mg/kg");
    }

    else if (lcdPrint == 3){
      lcdPrint = 0;
      lcd.clear();    
      lcd.print("pH  :  ");            
      lcd.print(ph);
    }
    
    
    // update hasil pembacaan sensor dan output fuzzy di serial monitor
    // hanya untuk monitoring
    Serial.println("Hasil Pembacaan Sensor :");
    Serial.print("Suhu Udara       : ");
    Serial.println(temp);
    Serial.print("Hujan            : ");
    Serial.println(rainIntensity);
    Serial.print("Ketinggian Air   : ");
    Serial.println(dist);
    Serial.print("Kelembapan Tanah : ");
    Serial.println(moisture);
    Serial.print("pH               : ");
    Serial.println(ph);
    Serial.print("EC               : ");
    Serial.println(ec);
    Serial.print("Nitrogen         : ");
    Serial.println(nitrogen);
    Serial.print("Fosfor           : ");
    Serial.println(phospor); 
    Serial.print("Kalium           : ");
    Serial.println(kalium);
    Serial.println("----------------------");
    Serial.print("HST : ");
    Serial.println(HST);
    Serial.print("Output Fuzzy : ");
    Serial.println(output);
    Serial.println();
  }
  
  //kirim data ke Thinger.IO
  thing.handle();
  pson data = output*100;
  thing.call_device("aktuator1", "aktuator", data);
}
