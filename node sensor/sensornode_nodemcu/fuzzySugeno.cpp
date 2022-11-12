#include "arduino.h"
#include "fuzzySugeno.h"

int n_rule = 0;         //jumlah rules fuzzy yang berlaku
float ifRules[17];      //variabel untuk rules anteseden
float thenRules[17];    //variabel untuk rules konsekuen

float moist;
float Tc;
float rain;

// INISIALISASI MEMBERSHIP FUNCTION
// fungsi keanggotaan anteseden
float moist_dry[] = {0, 60, 70};
float moist_humid[] = {60, 70, 80};
float moist_wet[] = {70, 80, 100};

float Tc_cold[] = {0, 24, 27};
float Tc_warm[] = {24, 27, 32};
float Tc_hot[] = {27, 32, 50};

float rain_heavy[] = {85, 100, 100};
float rain_medium[] = {70, 80, 90};
float rain_light[] = {0, 65, 75};

// fungsi keanggotaan konsekuen
float close = 0;
float half = 0.5;
float open = 1;

// FUZZIFIKASI
// fuzzifikasi untuk variabel soil moisture (kelembapan tanah)
// variabel linguistik kering
float dry_mf() {
  if (moist < moist_dry[1]) return 1;
  else if (moist >= moist_dry[1] && moist <= moist_dry[2]){
    return (moist_dry[2]-moist)/(moist_dry[2]-moist_dry[1]);
  }
  else return 0;
}

// variabel linguistik lembap
float humid_mf() {
  if (moist < moist_humid[0]) return 0;
  else if (moist >= moist_humid[0] && moist < moist_humid[1]){
    return (moist - moist_humid[0])/(moist_humid[1]-moist_humid[0]);
  }
  else if (moist >= moist_humid[1] && moist <= moist_humid[2]){
    return (moist_humid[2]-moist)/(moist_humid[2]-moist_humid[1]);
  }
  else return 0;
}

// variabel linguistik basah
float wet_mf() {
  if (moist < moist_wet[0]) return 0;
  else if (moist >= moist_wet[0] && moist <= moist_wet[1]){
    return (moist - moist_wet[0])/(moist_wet[1]-moist_wet[0]);
  }
  else return 1;
}

// fuzzifikasi untuk variabel suhu udara (air temperature)
// variabel linguistik dingin
float cold_mf() {
  if (Tc < Tc_cold[1]) return 1;
  else if (Tc >= Tc_cold[1] && Tc <= Tc_cold[2]){
    return (Tc_cold[2]-Tc)/(Tc_cold[2]-Tc_cold[1]);
  }
  else return 0;
}

// variabel linguistik hangat
float warm_mf() {
  if (Tc < Tc_warm[0]) return 0;
  else if (Tc >= Tc_warm[0] && Tc < Tc_warm[1]){
    return (Tc - Tc_warm[0])/(Tc_warm[1]-Tc_warm[0]);
  }
  else if (Tc >= Tc_warm[1] && Tc <= Tc_warm[2]){
    return (Tc_warm[2]-Tc)/(Tc_warm[2]-Tc_warm[1]);
  }
  else return 0;
}

// variabel linguistik panas
float hot_mf() {
  if (Tc < Tc_hot[0]) return 0;
  else if (Tc >= Tc_hot[0] && Tc <= Tc_hot[1]){
    return (Tc - Tc_hot[0])/(Tc_hot[1]-Tc_hot[0]);
  }
  else return 1;
}

// fuzzifikasi untuk variabel hujan
// variabel linguistik ringan
float light_mf() {
  if (rain < rain_light[1]) return 1;
  else if (rain >= rain_light[1] && rain <= rain_light[2]){
    return (rain_light[2]-rain)/(rain_light[2]-rain_light[1]);
  }
  else return 0;
}

// variabel linguistik sedang
float medium_mf() {
  if (rain < rain_medium[0]) return 0;
  else if (rain >= rain_medium[0] && rain < rain_medium[1]){
    return (rain - rain_medium[0])/(rain_medium[1]-rain_medium[0]);
  }
  else if (rain >= rain_medium[1] && rain <= rain_medium[2]){
    return (rain_medium[2]-rain)/(rain_medium[2]-rain_medium[1]);
  }
  else return 0;
}

// variabel linguistik lebat
float heavy_mf() {
  if (rain < rain_heavy[0]) return 0;
  else if (rain >= rain_heavy[0] && rain <= rain_heavy[1]){
    return (rain - rain_heavy[0])/(rain_heavy[1]-rain_heavy[0]);
  }
  else return 1;
}

// BASIS ATURAN DAN IMPLIKASI
void rules_normal(){
  // if moist-dry and rain-light then open
  ifRules[0] = min(dry_mf(),light_mf());
  thenRules[0] = open;
  // if moist-dry and rain-medium then half
  ifRules[1] = min(dry_mf(),medium_mf());
  thenRules[1] = half;
  // if moist-dry and rain-heavy then half
  ifRules[2] = min(dry_mf(),heavy_mf());
  thenRules[2] = half;
  // if moist-humid and Tc-cold and rain-light then half
  ifRules[3] = min(min(humid_mf(),cold_mf()),light_mf());
  thenRules[3] = half;
  // if moist-humid and Tc-cold and rain-medium then close
  ifRules[4] = min(min(humid_mf(),cold_mf()),medium_mf());
  thenRules[4] = close;
  // if moist-humid and Tc-cold and rain-heavy then close
  ifRules[5] = min(min(humid_mf(),cold_mf()),heavy_mf());
  thenRules[5] = close;
  // if moist-humid and Tc-warm and rain-light then half
  ifRules[6] = min(min(humid_mf(),warm_mf()),light_mf());
  thenRules[6] = half;
  // if moist-humid and Tc-warm and rain-medium then half
  ifRules[7] = min(min(humid_mf(),warm_mf()),medium_mf());
  thenRules[7] = half;
  // if moist-humid and Tc-warm and rain-heavy then close
  ifRules[8] = min(min(humid_mf(),warm_mf()),heavy_mf());
  thenRules[8] = close;
  // if moist-humid and Tc-hot and rain-light then open
  ifRules[9] = min(min(humid_mf(),hot_mf()),light_mf());
  thenRules[9] = open;
  // if moist-humid and Tc-hot and rain-medium then half
  ifRules[10] = min(min(humid_mf(),hot_mf()),medium_mf());
  thenRules[10] = half;
  // if moist-humid and Tc-hot and rain-heavy then close
  ifRules[11] = min(min(humid_mf(),hot_mf()),heavy_mf());
  thenRules[11] = close;
  // if moist-wet then close 
  ifRules[12] = wet_mf();
  thenRules[12] = close;
}

void rules_ripening(){
  // if moist-dry and Tc-cold and rain-light then half
  ifRules[0] = min(min(dry_mf(),cold_mf()),light_mf());
  thenRules[0] = half;
  // if moist-dry and Tc-cold and rain-medium then close
  ifRules[1] = min(min(dry_mf(),cold_mf()),medium_mf());
  thenRules[1] = close;
  // if moist-dry and Tc-cold and rain-heavy then close
  ifRules[2] = min(min(dry_mf(),cold_mf()),heavy_mf());
  thenRules[2] = close;
  // if moist-dry and Tc-warm and rain-light then open
  ifRules[3] = min(min(dry_mf(),warm_mf()),light_mf());
  thenRules[3] = open;
  // if moist-dry and Tc-warm and rain-medium then close
  ifRules[4] = min(min(dry_mf(),warm_mf()),medium_mf());
  thenRules[4] = close;
  // if moist-dry and Tc-warm and rain-heavy then close
  ifRules[5] = min(min(dry_mf(),warm_mf()),heavy_mf());
  thenRules[5] = close;
  // if moist-dry and Tc-hot and rain-light then open
  ifRules[6] = min(min(dry_mf(),hot_mf()),light_mf());
  thenRules[6] = open;
  // if moist-dry and Tc-hot and rain-medium then half
  ifRules[7] = min(min(dry_mf(),hot_mf()),medium_mf());
  thenRules[7] = half;
  // if moist-dry and Tc-hot and rain-heavy then close
  ifRules[8] = min(min(dry_mf(),hot_mf()),heavy_mf());
  thenRules[8] = close;
  // if moist-humid and Tc-cold then close
  ifRules[9] = min(humid_mf(),cold_mf());
  thenRules[9] = close;
  // if moist-humid and Tc-warm and rain-light then half
  ifRules[10] = min(min(humid_mf(),warm_mf()),light_mf());
  thenRules[10] = half;
  // if moist-humid and Tc-warm and rain-medium then close
  ifRules[11] = min(min(humid_mf(),warm_mf()),medium_mf());
  thenRules[11] = close;
  // if moist-humid and Tc-warm and rain-heavy then close
  ifRules[12] = min(min(humid_mf(),warm_mf()),heavy_mf());
  thenRules[12] = close;
  // if moist-humid and Tc-hot and rain-light then half
  ifRules[13] = min(min(humid_mf(),hot_mf()),light_mf());
  thenRules[13] = half;
  // if moist-humid and Tc-hot and rain-medium then close
  ifRules[14] = min(min(humid_mf(),hot_mf()),medium_mf());
  thenRules[14] = close;
  // if moist-humid and Tc-hot and rain-heavy then close
  ifRules[15] = min(min(humid_mf(),hot_mf()),heavy_mf());
  thenRules[15] = close;
  // if moist-wet then close
  ifRules[16] = wet_mf();
  thenRules[16] = close;
}

// AGREGASI DAN DEFUZZIFIKASI
float defuzzification(String rule_base){
  // jika termasuk fase pematangan, maka aktifkan rules pematangan
  if (rule_base == "ripening"){
    rules_ripening();
    n_rule = 17;
  }
  // jika termasuk fase normal generatif, maka aktifkan rules normal
  else if (rule_base == "normal"){
    rules_normal();
    n_rule = 13;
  }

  // lakukan defuzzifikasi menggunakan metode weighted average (defuzzifikasi sugeno)
  float res = 0;
  float div = 0;
  for (int i = 0; i < n_rule; i++){
    res += ifRules[i]*thenRules[i];
    div += ifRules[i];
  }
  return res/div;
}

// fungsi yang dipanggil program utama
float fuzzyResult(float currentMoist, float currentTc, float currentRain, String rule_base) {
  moist = currentMoist;
  Tc = currentTc;
  rain = currentRain;
  return defuzzification(rule_base);
}
