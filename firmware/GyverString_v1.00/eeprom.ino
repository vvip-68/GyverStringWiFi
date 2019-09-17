#define EEPROM_OK 0x5F                     // Флаг, показывающий, что EEPROM инициализирована корректными данными 
#define EFFECT_EEPROM 120                  // начальная ячейка eeprom с параметром скорости эффектов 
#define TEXT_EEPROM 130                    // начальная ячейка eeprom сохраненного текста бегущей строки 
#define MAX_TEXT_LENGTH 360                // Максимальная длина строки текста

void loadSettings() {

  // Адреса в EEPROM:
  //    0 - если EEPROM_OK - EEPROM инициализировано, если другое значение - нет 
  //    1 - максимальная яркость ленты 1-255
  //    2 - скорость прокрутки текста
  //    3 - текущий эффект расцвечивания текста
  //    4 - использовать синхронизацию времени через NTP
  //  5,6 - период синхронизации NTP (int16_t - 2 байта)
  //    7 - time zone
  //    8 - Использовать режим точки доступа
  //    9 - globalColor R
  //   10 - globalColor G
  //   11 - globalColor B
  //   12 - Режим 1 по времени - часы
  //   13 - Режим 1 по времени - минуты
  //   14 - Режим 1 по времени - ID эффекта или -1 - выключено; 0 - выключить устройство; 1 - включить бкгущую строку
  //   15 - Режим 2 по времени - часы
  //   16 - Режим 2 по времени - минуты
  //   17 - Режим 2 по времени - ID эффекта или -1 - выключено; 0 - выключить устройство; 1 - включить бкгущую строку
  //   18 - Текущий режим, чтобы при включении устройства переключиться в него
  //   19 - зарезервировано
  //  20-29   - имя точки доступа    - 10 байт
  //  30-45   - пароль точки доступа - 16 байт
  //  46-69   - имя сети  WiFi       - 24 байта
  //  70-85   - пароль сети  WiFi    - 16 байт
  //  86-89   - Статический IP адрес лампы  
  //  90-119  - имя NTP сервера      - 30 байт
  // ....
  //  120 - скорость эффекта 0       - нет эффекта
  //  121 - скорость эффекта 1       - скорость Дыхание
  //  122 - скорость эффекта 2       - скорость Цвет
  //  123 - скорость эффекта 3       - скорость Радуга 
  //  124 - тип "цветные буквы"      - 1 - буквы раскрашиваются по мере продвижения; 2 - каждая буква свой цвет
   
  //  130 - начало сохранения текста бегущей строки

  // Сначала инициализируем имя сети/точки доступа, пароли и имя NTP-сервера значениями по умолчанию.
  // Ниже, если EEPROM уже инициализирован - из него будут загружены актуальные значения
  strcpy(apName, DEFAULT_AP_NAME);
  strcpy(apPass, DEFAULT_AP_PASS);
  strcpy(ssid, NETWORK_SSID);
  strcpy(pass, NETWORK_PASS);
  strcpy(ntpServerName, DEFAULT_NTP_SERVER);    

  // Инициализировано ли EEPROM
  bool isInitialized = EEPROMread(0) == EEPROM_OK;  
    
  if (isInitialized) {    
    globalBrightness = getMaxBrightness();
    effect = getTextEffect();
    scrollSpeed = getScrollSpeed();
    effectSpeed = getEffectSpeed(effect);
    useNtp = getUseNtp();
    SYNC_TIME_PERIOD = getNtpSyncTime();
    timeZoneOffset = getTimeZone();
    globalColor = getGlobalColor();
    runningText = getRunningText();

    useSoftAP = getUseSoftAP();
    getSoftAPName().toCharArray(apName, 10);        //  54-63   - имя точки доступа    ( 9 байт макс) + 1 байт '\0'
    getSoftAPPass().toCharArray(apPass, 17);        //  64-79   - пароль точки доступа (16 байт макс) + 1 байт '\0'
    getSsid().toCharArray(ssid, 25);                //  80-103  - имя сети  WiFi       (24 байта макс) + 1 байт '\0'
    getPass().toCharArray(pass, 17);                //  104-119 - пароль сети  WiFi    (16 байт макс) + 1 байт '\0'
    getNtpServer().toCharArray(ntpServerName, 31);  //  120-149 - имя NTP сервера      (30 байт макс) + 1 байт '\0'
    if (strlen(apName) == 0) strcpy(apName, DEFAULT_AP_NAME);
    if (strlen(apPass) == 0) strcpy(apPass, DEFAULT_AP_PASS);
    if (strlen(ntpServerName) == 0) strcpy(ntpServerName, DEFAULT_NTP_SERVER);

    AM1_hour = getAM1hour();
    AM1_minute = getAM1minute();
    AM1_effect_id = getAM1effect();
    AM2_hour = getAM2hour();
    AM2_minute = getAM2minute();
    AM2_effect_id = getAM2effect();

    loadStaticIP();
    
  } else {
    globalBrightness = BRIGHTNESS;
    effect = 0;
    scrollSpeed = D_TEXT_SPEED;
    effectSpeed = D_EFFECT_SPEED;
    useNtp = true;
    SYNC_TIME_PERIOD = 60;
    timeZoneOffset = 7;
    useSoftAP = false;
    globalColor = 0xFFFFFF;

    AM1_hour = 0;
    AM1_minute = 0;
    AM1_effect_id = -1;
    AM2_hour = 0;
    AM2_minute = 0;
    AM2_effect_id = -1;    
  }

  scrollTimer.setInterval(scrollSpeed);
  effectTimer.setInterval(effectSpeed);
  ntpSyncTimer.setInterval(1000 * 60 * SYNC_TIME_PERIOD);
  
  // После первой инициализации значений - сохранить их принудительно
  if (!isInitialized) {
    saveDefaults();
    saveSettings();
  }
}

void saveDefaults() {

  EEPROMwrite(1, globalBrightness);

  EEPROMwrite(2, D_TEXT_SPEED);
  EEPROMwrite(3, effect);
  EEPROMwrite(4, useNtp ? 1 : 0);
  EEPROM_int_write(5, SYNC_TIME_PERIOD);
  EEPROMwrite(7, (byte)timeZoneOffset);
  EEPROMwrite(8, useSoftAP ? 1 : 0);

  // globalColor = 0xFFFFFF
  EEPROMwrite(9, 0xFF);
  EEPROMwrite(10, 0xFF);
  EEPROMwrite(11, 0xFF);

  EEPROMwrite(12, AM1_hour);            // Режим 1 по времени - часы
  EEPROMwrite(13, AM1_minute);          // Режим 1 по времени - минуты
  EEPROMwrite(14, (byte)AM1_effect_id); // Режим 1 по времени - действие: -1 - выключено; 0 - выключить матрицу (черный экран); 1 - бегущая строка
  EEPROMwrite(15, AM2_hour);            // Режим 2 по времени - часы
  EEPROMwrite(16, AM2_minute);          // Режим 2 по времени - минуты
  EEPROMwrite(17, (byte)AM2_effect_id); // Режим 2 по времени - действие: -1 - выключено; 0 - выключить матрицу (черный экран); 1 - бегущая строка
  
  EEPROMwrite(18, 0);                   // Текущий режим - устройство выключено
      
  strcpy(apName, DEFAULT_AP_NAME);
  strcpy(apPass, DEFAULT_AP_PASS);
  setSoftAPName(String(apName));
  setSoftAPPass(String(apPass));
  
  strcpy(ntpServerName, DEFAULT_NTP_SERVER);
  setNtpServer(String(ntpServerName));

  EEPROMwrite(86, IP_STA[0]);
  EEPROMwrite(87, IP_STA[1]);
  EEPROMwrite(88, IP_STA[2]);
  EEPROMwrite(89, IP_STA[3]);

  // Скорость эффектов по умолчанию
  EEPROMwrite(EFFECT_EEPROM+1, D_EFFECT_SPEED);   // Дыхание
  EEPROMwrite(EFFECT_EEPROM+2, D_EFFECT_SPEED);   // Цвет
  EEPROMwrite(EFFECT_EEPROM+3, D_EFFECT_SPEED);   // Радуга
  EEPROMwrite(EFFECT_EEPROM+4, 2);                // Цветные буквы - каждая буква свой цвет

  // Изначально бегущая строка пустая (символ /0)
  EEPROMwrite(TEXT_EEPROM, 0);

  eepromModified = true;
}

void saveSettings() {

  saveSettingsTimer.reset();

  if (!eepromModified) return;
  
  // Поставить отметку, что EEPROM инициализировано параметрами эффектов
  EEPROMwrite(0, EEPROM_OK);
  
  EEPROM.commit();
  Serial.println(F("Настройки сохранены в EEPROM"));

  eepromModified = false;
}

byte getMaxBrightness() {
  return EEPROMread(1);
}

void saveMaxBrightness(byte brightness) {
  if (brightness != getMaxBrightness()) {
    EEPROMwrite(1, brightness);
    eepromModified = true;
  }
}

void saveScrollSpeed(int speed) {
  if (speed != getScrollSpeed()) {
    EEPROMwrite(2, constrain(map(speed, D_TEXT_SPEED_MIN, D_TEXT_SPEED_MAX, 0, 255), 0, 255));
    eepromModified = true;
  }
}

int getScrollSpeed() {
  return map(EEPROMread(2),0,255,D_TEXT_SPEED_MIN,D_TEXT_SPEED_MAX);
}

byte getTextEffect() {
  return EEPROMread(3);
}

void saveTextEffect(byte eff) {
  if (eff != getTextEffect()) {
    EEPROMwrite(3, eff);
    eepromModified = true;
  }
}

void saveEffectSpeed(byte effect, int speed) {
  if (speed != getEffectSpeed(effect)) {
    const int addr = EFFECT_EEPROM;  
    EEPROMwrite(addr + effect, constrain(map(speed, D_EFFECT_SPEED_MIN, D_EFFECT_SPEED_MAX, 0, 255), 0, 255));        // Скорость эффекта
    eepromModified = true;
  }
}

int getEffectSpeed(byte effect) {
  const int addr = EFFECT_EEPROM;
  return map(EEPROMread(addr + effect),0,255,D_EFFECT_SPEED_MIN,D_EFFECT_SPEED_MAX);
}

void saveUseNtp(boolean value) {
  if (value != getUseNtp()) {
    EEPROMwrite(4, value);
    eepromModified = true;
  }
}

bool getUseNtp() {
  return EEPROMread(4) == 1;
}

void saveNtpSyncTime(uint16_t value) {
  if (value != getNtpSyncTime()) {
    EEPROM_int_write(5, SYNC_TIME_PERIOD);
    eepromModified = true;
  }
}

uint16_t getNtpSyncTime() {
  uint16_t time = EEPROM_int_read(5);  
  if (time == 0) time = 60;
  return time;
}

void saveTimeZone(int8_t value) {
  if (value != getTimeZone()) {
    EEPROMwrite(7, (byte)value);
    eepromModified = true;
  }
}

int8_t getTimeZone() {
  return (int8_t)EEPROMread(7);
}

bool getUseSoftAP() {
  return EEPROMread(8) == 1;
}

void setUseSoftAP(boolean use) {  
  if (use != getUseSoftAP()) {
    EEPROMwrite(8, use ? 1 : 0);
    eepromModified = true;
  }
}

String getSoftAPName() {
  return EEPROM_string_read(20, 10);
}

void setSoftAPName(String SoftAPName) {
  if (SoftAPName != getSoftAPName()) {
    EEPROM_string_write(20, SoftAPName, 10);
    eepromModified = true;
  }
}

String getSoftAPPass() {
  return EEPROM_string_read(30, 16);
}

void setSoftAPPass(String SoftAPPass) {
  if (SoftAPPass != getSoftAPPass()) {
    EEPROM_string_write(30, SoftAPPass, 16);
    eepromModified = true;
  }
}

String getSsid() {
  return EEPROM_string_read(46, 24);
}

void setSsid(String Ssid) {
  if (Ssid != getSsid()) {
    EEPROM_string_write(46, Ssid, 24);
    eepromModified = true;
  }
}

String getPass() {
  return EEPROM_string_read(70, 16);
}

void setPass(String Pass) {
  if (Pass != getPass()) {
    EEPROM_string_write(70, Pass, 16);
    eepromModified = true;
  }
}

String getNtpServer() {
  return EEPROM_string_read(90, 30);
}

void setNtpServer(String server) {
  if (server != getNtpServer()) {
    EEPROM_string_write(90, server, 30);
    eepromModified = true;
  }
}

void setAM1params(byte hour, byte minute, int8_t effect) { 
  setAM1hour(hour);
  setAM1minute(minute);
  setAM1effect(effect);
}

byte getAM1hour() { 
  byte hour = EEPROMread(12);
  if (hour>23) hour = 0;
  return hour;
}

void setAM1hour(byte hour) {
  if (hour != getAM1hour()) {
    EEPROMwrite(12, hour);
    eepromModified = true;
  }
}

byte getAM1minute() {
  byte minute = EEPROMread(13);
  if (minute > 59) minute = 0;
  return minute;
}

void setAM1minute(byte minute) {
  if (minute != getAM1minute()) {
    EEPROMwrite(13, minute);
    eepromModified = true;
  }
}

int8_t getAM1effect() {
  return (int8_t)EEPROMread(14);
}

void setAM1effect(int8_t effect) {
  if (effect != getAM1effect()) {
    EEPROMwrite(14, (byte)effect);
    eepromModified = true;
  }
}

void setAM2params(byte hour, byte minute, int8_t effect) { 
  setAM2hour(hour);
  setAM2minute(minute);
  setAM2effect(effect);
}

byte getAM2hour() { 
  byte hour = EEPROMread(15);
  if (hour>23) hour = 0;
  return hour;
}

void setAM2hour(byte hour) {
  if (hour != getAM2hour()) {
    EEPROMwrite(15, hour);
    eepromModified = true;
  }
}

byte getAM2minute() {
  byte minute = EEPROMread(16);
  if (minute > 59) minute = 0;
  return minute;
}

void setAM2minute(byte minute) {
  if (minute != getAM2minute()) {
    EEPROMwrite(16, minute);
    eepromModified = true;
  }
}

int8_t getAM2effect() {
  return (int8_t)EEPROMread(17);
}

void setAM2effect(int8_t effect) {
  if (effect != getAM2effect()) {
    EEPROMwrite(17, (byte)effect);
    eepromModified = true;
  }
}

int8_t getCurrentMode() {
  return (int8_t)EEPROMread(18);
}

void saveCurrentMode(int8_t mode) {
  if (mode != getCurrentMode()) {
    EEPROMwrite(18, (byte)mode);
    eepromModified = true;
  }
}

void loadStaticIP() {
  IP_STA[0] = EEPROMread(86);
  IP_STA[1] = EEPROMread(87);
  IP_STA[2] = EEPROMread(88);
  IP_STA[3] = EEPROMread(89);
}

void saveStaticIP(byte p1, byte p2, byte p3, byte p4) {
  if (IP_STA[0] != p1 || IP_STA[1] != p2 || IP_STA[2] != p3 || IP_STA[3] != p4) {
    IP_STA[0] = p1;
    IP_STA[1] = p2;
    IP_STA[2] = p3;
    IP_STA[3] = p4;
    EEPROMwrite(86, p1);
    EEPROMwrite(87, p2);
    EEPROMwrite(88, p3);
    EEPROMwrite(89, p4);
    eepromModified = true;  
  }
}

uint32_t getGlobalColor() {
  byte r,g,b;
  r = EEPROMread(9);
  g = EEPROMread(10);
  b = EEPROMread(11);
  return (uint32_t)r<<16 | (uint32_t)g<<8 | (uint32_t)b;
}

void setGlobalColor(uint32_t color) {
  globalColor = color;
  if (color != getGlobalColor()) {
    CRGB cl = CRGB(color);
    EEPROMwrite(9, cl.r); // R
    EEPROMwrite(10, cl.g); // G
    EEPROMwrite(11, cl.b); // B
    eepromModified = true;
  }
}

#define TEXT_EEPROM 130                    // начальная ячейка eeprom сохраненного текста бегущей строки 
#define MAX_TEXT_LENGTH 360                // Максимальная длина строки текста

String getRunningText() {
  return EEPROM_string_read(TEXT_EEPROM, MAX_TEXT_LENGTH);
}

void saveRunningText(String text){
  if (text != getRunningText()) {
    EEPROM_string_write(TEXT_EEPROM, text, MAX_TEXT_LENGTH);
    eepromModified = true;
  }
}

// ----------------------------------------------------------

byte EEPROMread(uint16_t addr) {    
  return EEPROM.read(addr);
}

void EEPROMwrite(uint16_t addr, byte value) {    
  EEPROM.write(addr, value);
}

// чтение uint16_t
uint16_t EEPROM_int_read(uint16_t addr) {    
  byte raw[2];
  for (byte i = 0; i < 2; i++) raw[i] = EEPROMread(addr+i);
  uint16_t &num = (uint16_t&)raw;
  return num;
}

// запись uint16_t
void EEPROM_int_write(uint16_t addr, uint16_t num) {
  byte raw[2];
  (uint16_t&)raw = num;
  for (byte i = 0; i < 2; i++) EEPROMwrite(addr+i, raw[i]);
}

String EEPROM_string_read(uint16_t addr, int16_t len) {
   char buffer[len+1];
   memset(buffer,'\0',len+1);
   int16_t i = 0;
   while (i < len) {
     byte c = EEPROMread(addr+i);
     if (c == 0) break;
     buffer[i++] = c;
     // if (c != 0 && (isAlphaNumeric(c) || isPunct(c) || isSpace(c)))
   }
   return String(buffer);
}

void EEPROM_string_write(uint16_t addr, String buffer, int16_t max_len) {
   uint16_t len = buffer.length();
   int16_t i = 0;
   if (len > max_len) len = max_len;
   while (i < len) {
     EEPROMwrite(addr+i, buffer[i++]);
   }
   if (i < max_len) EEPROMwrite(addr+i,0);
}

// ----------------------------------------------------------
