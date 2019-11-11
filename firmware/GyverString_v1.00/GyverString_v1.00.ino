// Скетч к проекту "Бегущая строка"
// Гайд по постройке матрицы: https://alexgyver.ru/matrix_guide/
// Страница базового проекта (схемы, описания): https://alexgyver.ru/GyverMatrixBT/
// Страница проекта на GitHub: https://github.com/vvip-68/GyverMatrixWiFi
// Автор: AlexGyver Technologies, 2018
// Дальнейшее развитие: vvip, 2019
// https://AlexGyver.ru/

#define FIRMWARE_VER F("\n\nGyverString-WiFi v.1.00.2019.1111")

#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ALLOW_INTERRUPTS 0

// Подключение используемых библиотек
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include "FastLED.h"
#include "timerMinim.h"
#include "fonts.h"
#include "GyverButton.h"

// ************************ МАТРИЦА *************************

#define BRIGHTNESS 255        // стандартная маскимальная яркость (0-255)
#define CURRENT_LIMIT 5000    // лимит по току в миллиамперах, автоматически управляет яркостью (пожалей свой блок питания!) 0 - выключить лимит

#define WIDTH 16              // ширина матрицы
#define HEIGHT 16             // высота матрицы
#define SEGMENTS 1            // диодов в одном "пикселе" (для создания матрицы из кусков ленты)

#define COLOR_ORDER GRB       // порядок цветов на ленте. Если цвет отображается некорректно - меняйте. Начать можно с RGB

#define MATRIX_TYPE 0         // тип матрицы: 0 - зигзаг, 1 - параллельная
#define CONNECTION_ANGLE 0    // угол подключения: 0 - левый нижний, 1 - левый верхний, 2 - правый верхний, 3 - правый нижний
#define STRIP_DIRECTION 0     // направление ленты из угла: 0 - вправо, 1 - вверх, 2 - влево, 3 - вниз
// при неправильной настрйоке матрицы вы получите предупреждение "Wrong matrix parameters! Set to default"
// шпаргалка по настройке матрицы здесь! https://alexgyver.ru/matrix_guide/

// ******************* ПОДКЛЮЧЕНИЕ К СЕТИ *******************

                                             // Внимание!!! Если вы меняете эти значения ПОСЛЕ того, как прошивка уже хотя бы раз была загружена в плату и выполнялась,
                                             // чтобы изменения вступили в силу нужно также изменить значение константы EEPROM_OK в первой строке в файле eeprom.ino 
                                             // 
#define NETWORK_SSID ""                      // Имя WiFi сети - пропишите здесь или задайте из программы на смартфоне
#define NETWORK_PASS ""                      // Пароль для подключения к WiFi сети - пропишите здесь или задайте из программы на смартфоне
#define DEFAULT_AP_NAME "StringAP"           // Имя точки доступа по умолчанию 
#define DEFAULT_AP_PASS "12341234"           // Пароль точки доступа по умолчанию
#define localPort 2390                       // Порт матрицы для подключения из программы на смартфоне
byte IP_STA[] = {192, 168, 0, 106};          // Статический адрес матрицы в локальной сети WiFi

#define DEFAULT_NTP_SERVER "ru.pool.ntp.org" // NTP сервер по умолчанию "time.nist.gov"

// ****************** ПИНЫ ПОДКЛЮЧЕНИЯ *******************

// пины подписаны согласно pinout платы, а не надписям на пинах!
// esp8266 - плату выбирал "Node MCU v3 (SP-12E Module)"
#define PIN_BTN D6   // кнопка подключена сюда (PIN --- КНОПКА --- GND)

// Внимание!!! При использовании платы микроконтроллера Wemos D1 (xxxx) и выбранной в менеджере плат платы "Wemos D1 (xxxx)"
// прошивка скорее всего нормально работать не будет. 
// Наблюдались следующие сбои у разных пользователей:
// - нестабильная работа WiFi (постоянные отваливания и пропадание сети) - попробуйте варианты с разным значением параметров компиляции IwIP в Arduino IDE
// - прекращение вывода контрольной информации в Serial.print() - в монитор порта не выводится
// - настройки в EEPROM не сохраняются
// Думаю все эти проблемы из за некорректной работы ядра ESP8266 для платы (варианта компиляции) Wemos D1 (xxxx) в версии ядра ESP8266 2.5.2
// Диод на ногу питания Wemos как нарисовано в схеме от Alex Gyver можно не ставить, конденсатор по питанию - желателен

// Используйте данное определение, если у вас МК NodeMCU, в менеджере плат выбрано NodeMCU v1.0 (ESP-12E), лента физически подключена к пину D2 на плате 
#define LED_PIN 2    // пин ленты
// Используйте данное определение, если у вас МК Wemos D1, в менеджере плат выбрано NodeMCU v1.0 (ESP-12E), лента физически подключена к пину D4 на плате 
//#define LED_PIN 4  // пин ленты

// ******************************** ДЛЯ РАЗРАБОТЧИКОВ ********************************

#define D_TEXT_SPEED 100      // скорость бегущего текста по умолчанию (мс)
#define D_TEXT_SPEED_MIN 10
#define D_TEXT_SPEED_MAX 255

#define D_EFFECT_SPEED 80     // скорость эффектов по умолчанию (мс)
#define D_EFFECT_SPEED_MIN 0
#define D_EFFECT_SPEED_MAX 255

#define DEBUG 0
#define NUM_LEDS WIDTH * HEIGHT * SEGMENTS
#define HOLD_TIMEOUT 2000            // Время удержания кнопки перед выполнением действия ( + debounce time) суммарно - около 3 сек

CRGB leds[NUM_LEDS];

// Сквозная нумерация (ID) эффектов в группе эффектов, накладываемых на бегущую строку
#define EFFECT_NONE                 0   // Нет эффекта
#define EFFECT_BREATH               1   // Дыхание яркостью
#define EFFECT_COLOR                2   // Смена цвета
#define EFFECT_RAINBOW_PIX          3   // Радужный текст
#define EFFECT_COLOR_LETTERS_1      4   // Цветные буквы (вариант 1)
#define EFFECT_COLOR_LETTERS_2      5   // Цветные буквы (вариант 2)

// ---- Подключение к сети

WiFiUDP udp;

bool useSoftAP = false;              // использовать режим точки доступа
bool wifi_connected = false;         // true - подключение к wifi сети выполнена
bool ap_connected = false;           // true - работаем в режиме точки доступа;
bool wifi_print_ip = false;          // В качестве бегущей строки отображается текущий IP WiFi соединения
String wifi_current_ip = "";

// Буфер для загрузки имен/пароля из EEPROM текущих имени/пароля подключения к сети / точке доступа
// Внимание: к длине +1 байт на \0 - терминальный символ.
char apName[11] = "";                // Имя сети в режиме точки доступа
char apPass[17]  = "";               // Пароль подключения к точке доступа
char ssid[25] = "";                  // SSID (имя) вашего роутера (конфигурируется подключением через точку доступа и сохранением в EEPROM)
char pass[17] = "";                  // пароль роутера

// ---- Синхронизация часов с сервером NTP

IPAddress timeServerIP;
#define NTP_PACKET_SIZE 48           // NTP время в первых 48 байтах сообщения
uint16_t SYNC_TIME_PERIOD = 60;      // Период синхронизации в минутах
byte packetBuffer[NTP_PACKET_SIZE];  // буффер для хранения входящих и исходящих пакетов

int8_t timeZoneOffset = 7;           // смещение часового пояса от UTC
long ntp_t = 0;                      // Время, прошедшее с запроса данных с NTP-сервера (таймаут)
byte ntp_cnt = 0;                    // Счетчик попыток получить данные от сервера
bool init_time = false;              // Флаг false - время не инициализировано; true - время инициализировано
bool refresh_time = true;            // Флаг true - пришло время выполнить синхронизацию часов с сервером NTP
bool useNtp = true;                  // Использовать синхронизацию времени с NTP-сервером
char ntpServerName[31] = "";         // Используемый сервер NTP

int8_t hrs = 0, mins = 0, secs = 0, aday = 1, amnth = 1;
int16_t ayear = 1970;

bool isTurnedOff = false;            // Включен черный экран (т.е всё выключено)
bool prevTurnedOff = false;          // предыдущее состояние

bool isButtonHold = false;           // Кнопка нажата и удерживается
GButton butt(PIN_BTN);               // Физическая кнопка

// ---- Режимы, включаемые в заданное время

bool AM1_running = false;            // Режим 1 по времени - работает
byte AM1_hour = 0;                   // Режим 1 по времени - часы
byte AM1_minute = 0;                 // Режим 1 по времени - минуты
int8_t AM1_effect_id = -1;           // Режим 1 по времени - ID эффекта или -1 - выключено; 0 - выключить матрицу (черный экран); 1 - бегущая строка
bool AM2_running = false;            // Режим 2 по времени - работает
byte AM2_hour = 0;                   // Режим 2 по времени - часы
byte AM2_minute = 0;                 // Режим 2 по времени - минуты
int8_t AM2_effect_id = -1;           // Режим 2 по времени - ID эффекта или -1 - выключено; 0 - выключить матрицу (черный экран); 1 - бегущая строка

// ---- Таймеры

timerMinim effectTimer(D_EFFECT_SPEED);                 // Таймер скорости эффекта (шага выполнения эффекта)
timerMinim scrollTimer(D_TEXT_SPEED);                   // Таймер прокрутки текста эффекта бегущей строки
timerMinim saveSettingsTimer(5000);                     // Таймер отложенного сохранения настроек
timerMinim ntpSyncTimer(1000 * 60 * SYNC_TIME_PERIOD);  // Таймер синхронизации времени с NTP-сервером

// ---- Прочие переменные

byte globalBrightness = BRIGHTNESS;// Текущая яркость
byte breathBrightness;             // Яркость эффекта "Дыхание"
uint32_t globalColor = 0xffffff;   // Цвет рисования при запуске белый
bool brightDirection = false;      // true - увеличение яркости; false - уменьшение яркости

int scrollSpeed = D_TEXT_SPEED;    // скорость прокрутки текста бегущей строки
int effectSpeed = D_EFFECT_SPEED;  // скрость изменения эффекта

boolean loadingFlag = true;        // флаг: выполняется инициализация параметров режима
boolean runningFlag;               // флаг: текущий режим ручном режиме - бегущая строка
boolean fullTextFlag = false;      // флаг: текст бегущей строки показан полностью (строка убежала)
boolean eepromModified = false;    // флаг: данные, сохраняемые в EEPROM были изменены и требуют сохранения

byte current_mode = 0;             // 0 - выключено; 1 - включено
byte effect;                       // индекс текущего эффекта, накладываемого на бегущую строку
String runningText = "";           // Текущий текст бегущей строки, задаваемый пользователем со смартфона

void setup() {

  // Watcdog Timer - 8 секунд
  ESP.wdtEnable(WDTO_8S);

  // Инициализация последовательного порта
  Serial.begin(115200);
  delay(10);

  Serial.println(FIRMWARE_VER);

  // Инициализация EEPROM и загрузка сохраненных параметров
  EEPROM.begin(512);
  loadSettings();

  // WiFi всегда включен
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Выполнить подключение к сети / создание точки доступа
  connectToNetwork();

  // UDP-клиент на указанном порту
  udp.begin(localPort);

  butt.setStepTimeout(100);
  butt.setClickTimeout(500);

  // Настройки ленты
  FastLED.addLeds<WS2812, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(globalBrightness);
  if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  FastLED.clear();
  FastLED.show();

  // Если был задан спец.режим во время предыдущего сеанса работы матрицы - включить его
  // Номер спец-режима запоминается при его включении и сбрасывается при включении обычного режима или игры
  // Это позволяет в случае внезапной перезагрузки матрицы (например по wdt), когда был включен спец-режим (например ночные часы или выкл. матрицы)
  // снова включить его, а не отображать случайный обычный после включения матрицы
  int8_t current_mode = getCurrentMode();
  if (current_mode < 0 || current_mode > 1) current_mode = 0;     // 0 - выключено; 1 - включено
  setCurrentMode(current_mode);
}

void loop() {
  process();
  ESP.wdtFeed();   // пнуть собаку
  yield();
}

// -----------------------------------------

void startWiFi() {

  WiFi.disconnect(true);
  wifi_connected = false;
  WiFi.mode(WIFI_STA);

  // Пытаемся соединиться с роутером в сети
  if (strlen(ssid) > 0) {
    Serial.print(F("\nПодключение к "));
    Serial.print(ssid);

    if (IP_STA[0] + IP_STA[1] + IP_STA[2] + IP_STA[3] > 0) {
      WiFi.config(IPAddress(IP_STA[0], IP_STA[1], IP_STA[2], IP_STA[3]),  // 192.168.0.106
                  IPAddress(IP_STA[0], IP_STA[1], IP_STA[2], 1),          // 192.168.0.1
                  IPAddress(255, 255, 255, 0));
    }
    WiFi.begin(ssid, pass);

    // Проверка соединения (таймаут 10 секунд)
    for (int j = 0; j < 10; j++ ) {
      wifi_connected = WiFi.status() == WL_CONNECTED;
      if (wifi_connected) {
        // Подключение установлено
        Serial.println();
        Serial.print(F("WiFi подключен. IP адрес: "));
        Serial.println(WiFi.localIP());
        break;
      }
      ESP.wdtFeed();
      delay(1000);
      Serial.print(".");
    }
    Serial.println();

    if (!wifi_connected)
      Serial.println(F("Не удалось подключиться к сети WiFi."));
  }
}

void startSoftAP() {
  WiFi.softAPdisconnect(true);
  ap_connected = false;

  Serial.print(F("Создание точки доступа "));
  Serial.print(apName);

  ap_connected = WiFi.softAP(apName, apPass);

  for (int j = 0; j < 10; j++ ) {
    if (ap_connected) {
      Serial.println();
      Serial.print(F("Точка доступа создана. Сеть: '"));
      Serial.print(apName);
      // Если пароль совпадает с паролем по умолчанию - печатать для информации,
      // если был изменен пользователем - не печатать
      if (strcmp(apPass, "12341234") == 0) {
        Serial.print(F("'. Пароль: '"));
        Serial.print(apPass);
      }
      Serial.println(F("'."));
      Serial.print(F("IP адрес: "));
      Serial.println(WiFi.softAPIP());
      break;
    }

    WiFi.enableAP(false);
    WiFi.softAPdisconnect(true);
    ESP.wdtFeed();
    delay(1000);

    Serial.print(".");
    ap_connected = WiFi.softAP(apName, apPass);
  }
  Serial.println();

  if (!ap_connected)
    Serial.println(F("Не удалось создать WiFi точку доступа."));
}

void printNtpServerName() {
  Serial.print(F("NTP-сервер "));
  Serial.print(ntpServerName);
  Serial.print(F(" -> "));
  Serial.println(timeServerIP);
}

void connectToNetwork() {
  // Подключиться к WiFi сети
  startWiFi();

  // Если режим точки тоступане используется и к WiFi сети подключиться не удалось - создать точку доступа
  if (!wifi_connected) {
    WiFi.mode(WIFI_AP);
    startSoftAP();
  }

  if (useSoftAP && !ap_connected) startSoftAP();

  // Сообщить UDP порт, на который ожидаются подключения
  if (wifi_connected || ap_connected) {
    Serial.print(F("UDP-сервер на порту "));
    Serial.println(localPort);
  }
}
