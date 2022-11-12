//----------------------------------------------------------------------
//    Sistem Irigasi Cerdas dan Monitoring Sawah 4.0 IPB University
//                       (Node Sensor : Slave)
//
//    Author        : Karyadi Mochtar & Muhammad Hafiduddin
//    Version       : 1.0 (2022)
//    Supervised by : Dr. Karlisa Priandana, S.T., M.Eng.
//                    Dr. Ir. Sri Wahjuni, M.T.
//                    Auriza Rahmad Akbar, S.Komp., M.Kom.
//
//    c 2022 Computer Science IPB University
//----------------------------------------------------------------------

// node sensor bagian slave bertugas membaca data dari sensor hujan, 
// membaca data sensor tanah dengan protokol modbus, menerima input
// dari keypad, dan mengirimkan data sensor melalui komunikasi serial UART
// ke master

#include <SoftwareSerial.h>
#include <SensorModbusMaster.h>
#include <Keypad.h>
#include <Wire.h>

// konfigurasi parameter modbus RS485
#define DERE  10          // pin DE dan RE pada modul MAX485
#define SLAVEID 0x01      // id sensor modbus secara default adalah 0x01
#define HOLDINGREG 0x03   // function code untuk membaca holding register adalah 0x03

// konfigurasi pin sensor hujan
#define RAINPIN A0        // pin analog data sensor hujan
#define RAINENABLE 13     // pin power sensor hujan

#define DELAY_TIME 5000   // delay pengambilan data sensor

// address register untuk setiap variabel di sensor tanah
// {addr pH, addr Soil Moisture, addr EC, addr nitrogen, addr phospor, addr kalium}
const byte registerAddr[] = {0x06, 0x12, 0x15, 0x1e, 0x1f, 0x20};

// konfigurasi baris dan kolom keypad
const byte ROWS = 4;      // four rows
const byte COLS = 4;      // four columns

// konfigurasi nilai setiap tombol keypad
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','R'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'S','0','O','D'}
};


byte rowPins[ROWS] = {2, 3, 4, 5}; // pinout baris keypad
byte colPins[COLS] = {6, 7, 8, 9}; // pinout kolom keypad

String bytes = "";                 // variabel string yang menampung hasil input keypad 
bool loopStat = 0;                 // status loop, untuk mencegah loop berulang pada setiap millis

SoftwareSerial mod(11, 12);  // (RX, TX) software serial untuk sensor tanah via MAX485
Keypad keypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);
modbusMaster modbus;

// ---------------------------- MAIN FUNCTION --------------------------------

void setup() {
  Serial.begin(9600);   // hardware serial untuk komunikasi ke master
  mod.begin(9600);      // software serial untuk komunikasi ke sensor tanah via MAX485
  
  char key;
  pinMode(DERE, OUTPUT);

  pinMode(RAINPIN, INPUT);      // analog pin sensor hujan
  pinMode(RAINENABLE, OUTPUT);  // vcc pin sensor hujan

  modbus.begin(SLAVEID, mod, DERE); // (ID sensor, port serial, pin DE RE)

  // looping sampai dapat flag request dari master
  for(;;){  
    String request;
    
    while(Serial.available()>0){
      Serial.print("check request");
      request = Serial.readString();
    }
    request.trim();
    if(request == "Ya"){
      kirimdata();
      break;
    }
    request = "";
  }
}

void loop() {
  
  // get key yang ditekan di keypad
  char key = keypad.getKey();
  char resKey;

  // jika key = 'D' maka kirim flag "CAL" ke master
  // untuk menjalankan fungsi kalibrasi sensor ultrasonik  
  if (key == 'D'){
    Serial.print("CAL");
  }

  // jika key = 'S' maka kirim flag "SET" ke master
  // untuk menjalankan fungsi set tanggal penanaman
  while (key == 'S'){
    Serial.print("SET");

    // looping untuk mendapatkan 3 jenis masukan tanggal yaitu
    // tanggal, bulan, dan tahun dengan format DD/MM/YY 
    for (int i = 0; i < 3; ){
      for (int j = 0; j < 2; ){
        resKey = keypad.getKey();
        // cek masukan, hanya menerima masukan karakter angka
        if(isDigit(resKey)){
          bytes += resKey;
          j++;
        }
      }
      // cek data hasil masukan
      // i = 0 untuk pengecekan data tanggal (day)
      if (i == 0){
        if (bytes.toInt()<=31){
          Serial.print(bytes);
          i++;
          bytes = "";
        } 
        // jika tanggal > 31 (tidak sesuai), flag "XX" akan dikirim 
        else {
          Serial.print("XX");   
          bytes = "";
        }
      // i = 1 untuk pengecekan data bulan (month)
      } else if (i == 1){
        if (bytes.toInt()<=12){
          Serial.print(bytes);
          i++;
          bytes = "";
        } 
        // jika bulan > 12 (tidak sesuai), flag "XX" akan dikirim
        else { 
          Serial.print("XX");
          bytes = "";
        }
      } else if (i == 2){
        Serial.print(bytes);
        i++;
        bytes = "";
      }
    }

    // setelah tiga data tanggal didapat, tunggu user menginput 'O'
    // sebagai tanda tanggal sudah sesuai
    for(int i = 0; i < 1; ){
      resKey = keypad.getKey();
      if (resKey == 'O'){
        Serial.print(resKey);
        i++;
        key = NULL;
      }
    }    
  }

  // lakukan pengambilan data dari sensor dan pengiriman data ke master
  // setiap interval DELAY_TIME, pengecekan delay dilakukan dengan millis
  if (millis()%DELAY_TIME == 0 && loopStat == 0){
    loopStat = 1;
    kirimdata();
  } else{
    loopStat = 0;
  }
  
}

void kirimdata(){
  
  uint16_t nitro, phos, kal, ec, rain;
  float moist, ph;

  // baca data dari sensor tanah
  // format : modbus.uint16FromRegister(modbusFunctionCode, holdingRegisterAddressPerVariable, endianness);
  ph = modbus.uint16FromRegister(HOLDINGREG, registerAddr[0], bigEndian)/100.0f;        
  // hasil pembacaan nilai pH dibagi 100 karena nilai pH sesungguhnya adalah 1/100 
  // dari nilai yang dikembalikan (datasheet sensor tanah) 
    
  moist = modbus.uint16FromRegister(HOLDINGREG, registerAddr[1], bigEndian)/10.0f;
  // hasil pembacaan nilai kelembapan tanah dibagi 10 karena nilai kelembapan tanah
  // sesungguhnya adalah 1/10 dari nilai yang dikembalikan (datasheet sensor tanah) 
  
  ec = modbus.uint16FromRegister(HOLDINGREG, registerAddr[2], bigEndian);
  nitro = modbus.uint16FromRegister(HOLDINGREG, registerAddr[3], bigEndian);
  phos = modbus.uint16FromRegister(HOLDINGREG, registerAddr[4], bigEndian);
  kal = modbus.uint16FromRegister(HOLDINGREG, registerAddr[5], bigEndian);

  // baca nilai sensor hujan  
  digitalWrite(RAINENABLE, HIGH);
  delay(250);
  rain = analogRead(RAINPIN);
  delay(250);
  digitalWrite(RAINENABLE, LOW);
  delay(250);

  // gabungkan data sensor dalam satu string kemudian kirim data ke serial
  String senddata = String(ph) + "#" + String(moist) + "#" + String(ec) + "#" + String(nitro) + "#" + String(phos) + "#" + String(kal) + "#" + String(rain);
  Serial.println(senddata);
}
