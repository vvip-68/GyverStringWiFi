#define UDP_PACKET_MAX_SIZE 1024
#define PARSE_AMOUNT 8          // максимальное количество значений в массиве, который хотим получить
#define header '$'              // стартовый символ
#define divider ' '             // разделительный символ
#define ending ';'              // завершающий символ
 
int16_t intData[PARSE_AMOUNT];  // массив численных значений после парсинга - для WiFi часы время синхр м.б отрицательным + 
                                // период синхронизации м.б больше 255 мин - нужен тип int16_t
boolean recievedFlag;
boolean parseStarted;
char replyBuffer[7];                           // ответ клиенту - подтверждения получения команды: "ack;/r/n/0"
char incomeBuffer[UDP_PACKET_MAX_SIZE];        // Буфер для приема строки команды из wifi udp сокета. Также буфер используется для отправки строк в смартфон
                                               // Строка со списком эффектов может быть длинной, плюс кириллица в UTF занимает 2 байта - буфер должен быть большим.
unsigned long ackCounter = 0;
String receiveText = "", s_tmp = "";

void process() {  
  
  parsing();                                    // принимаем данные

  // на время принятия данных матрицу не обновляем!
  if (!parseStarted) {                          

    if (wifi_connected && useNtp) {
      if (ntp_t > 0 && millis() - ntp_t > 5000) {
        Serial.println(F("Таймаут NTP запроса!"));
        ntp_t = 0;
        ntp_cnt++;
        if (init_time && ntp_cnt >= 10) {
          Serial.println(F("Не удалось установить соединение с NTP сервером."));  
          refresh_time = false;
        }
      }
      bool timeToSync = ntpSyncTimer.isReady();
      if (timeToSync) { ntp_cnt = 0; refresh_time = true; }
      if (timeToSync || (refresh_time && ntp_t == 0 && (ntp_cnt < 10 || !init_time))) {
        getNTP();
        if (ntp_cnt >= 10) {
          if (init_time) {
            udp.flush();
          } else {
            //ESP.restart();
            ntp_cnt = 0;
            connectToNetwork();
          }
        }        
      }
    }
        
    String txt = runningText;
    uint32_t txtColor = globalColor;
    if (effect == EFFECT_COLOR_LETTERS_1) txtColor = 1;
    if (effect == EFFECT_COLOR_LETTERS_2) txtColor = 2;
    if (wifi_print_ip && (wifi_current_ip.length() > 0)) {
      txt = wifi_current_ip;
      txtColor = 0xffffff;
    }
    if (txt.length() == 0) {
       txt = init_time
         ? clockCurrentText() + " " + dateCurrentTextLong()  // + dateCurrentTextShort()
         : getLocalIP();
    }      

    if (isTurnedOff) {
      if (effectTimer.isReady()) {
        FastLED.clear();
        FastLED.show();
      }
    } else {
      fillString(txt, txtColor);  // Вывести строку
    }
    
    butt.tick();  // обязательная функция отработки. Должна постоянно опрашиваться
    byte clicks = 0;

    // Один клик
    if (butt.isSingle()) clicks = 1;    
    // Двойной клик
    if (butt.isDouble()) clicks = 2;
    // Тройной клик
    if (butt.isTriple()) clicks = 3;
    // Четверной и более клик
    if (butt.hasClicks()) clicks = butt.getClicks();
        
    if (butt.isPress()) {
      // Состояние - кнопку нажали  
    }
    
    if (butt.isRelease()) {
      // Состояние - кнопку отпустили
      isButtonHold = false;
    }

    if (butt.isHolded()) {
      isButtonHold = true;
      if (!isTurnedOff) {
        if (globalBrightness == 255)
          brightDirection = false;
        else if (globalBrightness == 0)  
          brightDirection = true;
        else  
          brightDirection = !brightDirection;
      }
    }

    // Был одинарный клик
    if (clicks == 1) { 
        if (isTurnedOff)
          // Если выключен - включить бегущую строку
          setCurrentMode(1);
        else 
          // Если включен - выключить (включить черный экран)
          setCurrentMode(0);
    }

    else if (!isTurnedOff) {

      // Был двойной клик
      if (clicks == 2) {
        if (wifi_print_ip) {
          // Остановить показ текущего IP матрицы
          wifi_print_ip = false;
          wifi_current_ip = "";
        } else {
          // Показать текущий IP матрицы
          showCurrentIP();
        }        
      }

      // Тройное нажатие      
      else if (clicks == 3) {
      }

      // ... и т.д.

      else if (butt.isStep()) {
        if (brightDirection) {
          if (globalBrightness < 10) globalBrightness += 1;
          else if (globalBrightness < 250) globalBrightness += 5;
          else {
            globalBrightness = 255;
          }
        } else {
          if (globalBrightness > 15) globalBrightness -= 5;
          else if (globalBrightness > 1) globalBrightness -= 1;
          else {
            globalBrightness = 1;
          }
        }

        FastLED.setBrightness(globalBrightness);
        
        saveMaxBrightness(globalBrightness);
        saveSettingsTimer.reset();
      }
      
    }

    checkAutoMode1Time();
    checkAutoMode2Time();
    
    // Если есть несохраненные в EEPROM данные - сохранить их
    if (saveSettingsTimer.isReady()) {
      saveSettings();
    }
  }
}

enum eModes {NORMAL, COLOR, TEXT} parseMode;

byte parse_index;
String string_convert = "";

bool haveIncomeData = false;
char incomingByte;

int16_t  bufIdx = 0;
int16_t  packetSize = 0;

// ********************* ПРИНИМАЕМ ДАННЫЕ **********************
void parsing() {
// ****************** ОБРАБОТКА *****************
  String str, str1, str2;
  byte b_tmp;
  int8_t tmp_eff, idx;

  /*
    Протокол связи, посылка начинается с режима. Режимы:
    0 - отправка цвета текста $0 colorHEX;
    4 - яркость - 
        $4 0 value;   установить текущий уровень яркости
    6 - текст $6 N|some text, где N - назначение текста;
        0 - текст бегущей строки
        1 - имя сервера NTP
        2 - SSID сети подключения
        3 - пароль для подключения к сети 
        4 - имя точки доступа
        5 - пароль к точке доступа
    7 - управление текстом: 
        $7 1 пуск; 
        $7 0 стоп; 
    8 - включить эффект
      - $8 0 номер эффекта;
    14 - быстрая установка ручных режимов с пред-настройками
       - $14 0; Черный экран (выкл);  
       - $14 1; Бегущая строка вкл.;  
    15 - скорость $15 скорость таймер; 0 - таймер эффектов, 1 - таймер скроллинга текста
    18 - Запрос текущих параметров программой: $18 page;  page: 0 - проверка связи; 1 - настройки; 2 - текст; 3 - эффекты; 4-часы; 5-настройки подключения; 6 - вкл/выкл по времени
    19 - работа с настройками часов
    21 - настройки подключения к сети . точке доступа
    22 - настройки включения режимов матрицы в указанное время
       - $22 X HH MM NN
           X  - номер режима 1 или 2
           HH - час срабатывания
           MM - минуты срабатывания
           NN - эффект: -2 - выключено; -1 - выключить матрицу; 0 - включить матрицу
  */  
  if (recievedFlag) {      // если получены данные
    recievedFlag = false;
    
    switch (intData[0]) {
      case 0:
        sendAcknowledge();
        break;
      case 4:
        // $4 0 value   установить текущий уровень яркости
        if (intData[1] == 0) {
          globalBrightness = intData[2];
          FastLED.setBrightness(globalBrightness);
          FastLED.show();
        }
        saveSettingsTimer.reset();
        sendAcknowledge();
        break;
      case 6:
        loadingFlag = true;
        // строка принимается в переменную receiveText, формат строки N|text, где N:
        // 0 - текст для бегущей строки
        // 1 - имя сервера NTP
        // 2 - имя сети (SSID)
        // 3 - пароль к сети
        // 4 - имя точки доступа
        // 5 - пароль точки доступа
        tmp_eff = receiveText.indexOf("|");
        if (tmp_eff > 0) {
          b_tmp = receiveText.substring(0, tmp_eff).toInt();
          str = receiveText.substring(tmp_eff+1, receiveText.length()+1);
           switch(b_tmp) {
            case 0:
              runningText = str;
              saveRunningText(str);
              break;
            case 1:
              str.toCharArray(ntpServerName, 30);
              setNtpServer(str);
              if (wifi_connected) {
                refresh_time = true; ntp_t = 0; ntp_cnt = 0;
              }
              break;
            case 2:
              str.toCharArray(ssid, 24);
              setSsid(str);
              break;
            case 3:
              str.toCharArray(pass, 16);
              setPass(str);
              break;
            case 4:
              str.toCharArray(apName, 10);
              setSoftAPName(str);
              break;
            case 5:
              str.toCharArray(apPass, 16);
              setSoftAPPass(str);
              // Передается в одном пакете - использовать SoftAP, имя точки и пароль
              // После получения пароля - перезапустить создание точки доступа
              if (useSoftAP) startSoftAP();
              break;
           }
        }
        saveSettingsTimer.reset();
        sendAcknowledge();
        break;
      case 7:
        if (intData[1] == 0 || intData[1] == 1) {
          if (intData[1] == 1) runningFlag = true;
          if (intData[1] == 0) runningFlag = false;          
        }
        sendAcknowledge();
        break;
      case 8:      
        effect = intData[2];        
        // intData[1] : дейстие -> 0 - выбор эффекта;
        // intData[2] : номер эффекта
        if (intData[1] == 0) {
          setEffect(effect);
        }
        saveSettingsTimer.reset();                
        sendPageParams(2);
        break;
      case 14:
        setCurrentMode(intData[1]);
        sendPageParams(1);
        break;
      case 15: 
        if (intData[2] == 0) {
          effectSpeed = intData[1]; 
          saveEffectSpeed(effect, effectSpeed);
          effectTimer.setInterval(effectSpeed);
        } else if (intData[2] == 1) {
          scrollSpeed = intData[1]; 
          scrollTimer.setInterval(scrollSpeed);
          saveScrollSpeed(scrollSpeed);
        }
        saveSettingsTimer.reset();
        sendAcknowledge();
        break;
      case 18: 
        if (intData[1] == 0) { // ping
          sendAcknowledge();
        } else {               // запрос параметров страницы приложения
          sendPageParams(intData[1]);
        }
        break;
      case 19: 
         switch (intData[1]) {
           case 2:               // $19 2 X; - Использовать синхронизацию часов NTP  X: 0 - нет, 1 - да
             useNtp = intData[2] == 1;
             saveUseNtp(useNtp);
             if (wifi_connected) {
               refresh_time = true; ntp_t = 0; ntp_cnt = 0;
             }
             break;
           case 3:               // $19 3 N Z; - Период синхронизации часов NTP и Часовой пояс
             SYNC_TIME_PERIOD = intData[2];
             timeZoneOffset = (int8_t)intData[3];
             saveTimeZone(timeZoneOffset);
             saveNtpSyncTime(SYNC_TIME_PERIOD);
             saveTimeZone(timeZoneOffset);
             ntpSyncTimer.setInterval(1000 * 60 * SYNC_TIME_PERIOD);
             if (wifi_connected) {
               refresh_time = true; ntp_t = 0; ntp_cnt = 0;
             }
             break;
           case 8:               // $19 8 YYYY MM DD HH MM; - Установить текущее время YYYY.MM.DD HH:MM
             setTime(intData[5],intData[6],0,intData[4],intData[3],intData[2]);
             init_time = true; refresh_time = false; ntp_cnt = 0;
             break;
        }
        saveSettingsTimer.reset();
        sendAcknowledge();
        break;
      case 21:
        // Настройки подключения к сети
        switch (intData[1]) { 
          // $21 0 0 - не использовать точку доступа $21 0 1 - использовать точку доступа
          case 0:  
            useSoftAP = intData[2] == 1;
            setUseSoftAP(useSoftAP);
            if (useSoftAP && !ap_connected) 
              startSoftAP();
            else if (!useSoftAP && ap_connected) {
              if (wifi_connected) { 
                ap_connected = false;              
                WiFi.softAPdisconnect(true);
                Serial.println(F("Точка доступа отключена."));
              }
            }      
            saveSettingsTimer.reset();
            break;
          case 1:  
            // $21 1 IP1 IP2 IP3 IP4 - установить статический IP адрес подключения к локальной WiFi сети, пример: $21 1 192 168 0 106
            // Локальная сеть - 10.х.х.х или 172.16.х.х - 172.31.х.х или 192.168.х.х
            // Если задан адрес не локальной сети - сбросить его в 0.0.0.0, что означает получение динамического адреса 
            if (!(intData[2] == 10 || intData[2] == 172 && intData[3] >= 16 && intData[3] <= 31 || intData[2] == 192 && intData[3] == 168)) {
              intData[2] = 0;
              intData[3] = 0;
              intData[4] = 0;
              intData[5] = 0;
            }
            saveStaticIP(intData[2], intData[3], intData[4], intData[5]);
            saveSettingsTimer.reset();
            break;
          case 2:  
            // $21 2; Выполнить переподключение к сети WiFi
            FastLED.clear();
            FastLED.show();
            saveSettings();
            startWiFi();
            showCurrentIP();
            break;
        }
        if (intData[1] == 0 || intData[1] == 1) {
          sendAcknowledge();
        } else {
          sendPageParams(9);
        }
        break;
      case 22:
      /*  22 - настройки включения режимов матрицы в указанное время
       - $22 HH1 MM1 NN1 HH2 MM2 NN2
           HHn - час срабатывания
           MMn - минуты срабатывания
           NNn - эффект: -1 - выключено; 0 - выключить матрицу;  1 бегущая строка
      */    
        AM1_hour = intData[1];
        AM1_minute = intData[2];
        AM1_effect_id = intData[3];
        if (AM1_hour < 0) AM1_hour = 0;
        if (AM1_hour > 23) AM1_hour = 23;
        if (AM1_minute < 0) AM1_minute = 0;
        if (AM1_minute > 59) AM1_minute = 59;
        if (AM1_effect_id < -1) AM1_effect_id = -1;
        setAM1params(AM1_hour, AM1_minute, AM1_effect_id);
        AM2_hour = intData[4];
        AM2_minute = intData[5];
        AM2_effect_id = intData[6];
        if (AM2_hour < 0) AM2_hour = 0;
        if (AM2_hour > 23) AM2_hour = 23;
        if (AM2_minute < 0) AM2_minute = 0;
        if (AM2_minute > 59) AM2_minute = 59;
        if (AM2_effect_id < -1) AM2_effect_id = -1;
        setAM2params(AM2_hour, AM2_minute, AM2_effect_id);

        saveSettings();
        sendPageParams(10);
        break;
    }
  }

  // ****************** ПАРСИНГ *****************
  
  // Если предыдущий буфер еще не разобран - новых данных из сокета не читаем, продолжаем разбор уже считанного буфера
  haveIncomeData = bufIdx > 0 && bufIdx < packetSize; 
  if (!haveIncomeData) {
    packetSize = udp.parsePacket();      
    haveIncomeData = packetSize > 0;      
  
    if (haveIncomeData) {                
      // read the packet into packetBuffer
      memset(incomeBuffer,'\0',UDP_PACKET_MAX_SIZE);
      int len = udp.read(incomeBuffer, UDP_PACKET_MAX_SIZE);
      if (len > 0) {          
        incomeBuffer[len] = 0;
      }
      bufIdx = 0;
      
      delay(0);            // ESP8266 при вызове delay отпрабатывает стек IP протокола, дадим ему поработать        

      Serial.print(F("UDP пакeт размером "));
      Serial.print(packetSize);
      Serial.print(F(" от "));
      IPAddress remote = udp.remoteIP();
      for (int i = 0; i < 4; i++) {
        Serial.print(remote[i], DEC);
        if (i < 3) {
          Serial.print(F("."));
        }
      }
      Serial.print(F(", порт "));
      Serial.println(udp.remotePort());
      if (udp.remotePort() == localPort) {
        Serial.print(F("Содержимое: "));
        Serial.println(incomeBuffer);
      }
    }

    // NTP packet from time server
    if (haveIncomeData && udp.remotePort() == 123) {
      parseNTP();
      haveIncomeData = false;
      bufIdx = 0;
    }
  }

  if (haveIncomeData) {         
    
    // Из за ошибки в компоненте UdpSender в Thunkable - теряются половина отправленных 
    // символов, если их кодировка - двухбайтовый UTF8, т.к. оно вычисляет длину строки без учета двухбайтовости
    // Чтобы символы не терялись - при отправке строки из андроид-программы, она добивается с конца пробелами
    // Здесь эти конечные пробелы нужно предварительно удалить
    while (packetSize > 0 && incomeBuffer[packetSize-1] == ' ') packetSize--;
    incomeBuffer[packetSize] = 0;

    if (parseMode == TEXT) {                         // если нужно принять строку - принимаем всю

        // Оставшийся буфер преобразуем с строку
        if (intData[0] == 6) {  // текст
          receiveText = String(&incomeBuffer[bufIdx]);
          receiveText.trim();
        }
                  
        incomingByte = ending;                       // сразу завершаем парс
        parseMode = NORMAL;
        bufIdx = 0; 
        packetSize = 0;                              // все байты из входящего пакета обработаны
      } else { 
        incomingByte = incomeBuffer[bufIdx++];       // обязательно ЧИТАЕМ входящий символ
      } 
   }       
  
  if (haveIncomeData) {

    if (parseStarted) {                                             // если приняли начальный символ (парсинг разрешён)
      if (incomingByte != divider && incomingByte != ending) {      // если это не пробел И не конец
        string_convert += incomingByte;                             // складываем в строку
      } else {                                                      // если это пробел или ; конец пакета
        if (parse_index == 0) {
          byte cmdMode = string_convert.toInt();
          intData[0] = cmdMode;
          if (cmdMode == 0) parseMode = COLOR;                      // передача цвета (в отдельную переменную)
          else if (cmdMode == 6) {
            parseMode = TEXT;
          }
          else parseMode = NORMAL;
        }

        if (parse_index == 1) {       // для второго (с нуля) символа в посылке
          if (parseMode == NORMAL) intData[parse_index] = string_convert.toInt();             // преобразуем строку в int и кладём в массив}
          if (parseMode == COLOR) {                                                           // преобразуем строку HEX в цифру
            setGlobalColor((uint32_t)HEXtoInt(string_convert));
            if (intData[0] == 0) {
              incomingByte = ending;
              parseStarted = false;
            } else {
              parseMode = NORMAL;
            }
          }
        } else {
          intData[parse_index] = string_convert.toInt();  // преобразуем строку в int и кладём в массив
        }
        string_convert = "";                        // очищаем строку
        parse_index++;                              // переходим к парсингу следующего элемента массива
      }
    }

    if (incomingByte == header) {                   // если это $
      parseStarted = true;                          // поднимаем флаг, что можно парсить
      parse_index = 0;                              // сбрасываем индекс
      string_convert = "";                          // очищаем строку
    }

    if (incomingByte == ending) {                   // если таки приняли ; - конец парсинга
      parseMode == NORMAL;
      parseStarted = false;                         // сброс
      recievedFlag = true;                          // флаг на принятие
      bufIdx = 0;
    }

    if (bufIdx >= packetSize) {                     // Весь буфер разобран 
      bufIdx = 0;
      packetSize = 0;
    }
  }
}

void sendPageParams(int page) {
  // OF:число    статус вкл/выкл, где Х = 0 - выкл; 1 - вкл
  // BR:число    яркость
  // CL:HHHHHH   текущий цвет текста, HEX
  // TX:[текст]  текст, ограничители [] обязательны
  // TS:Х        состояние бегущей строки, где Х = 0 - выкл; 1 - вкл
  // ST:число    скорость прокрутки текста
  // EF:число    текущий эффект
  // SE:число    скорость эффектов
  // NP:Х        использовать NTP, где Х = 0 - выкл; 1 - вкл
  // NT:число    период синхронизации NTP в минутах
  // NZ:число    часовой пояс -12..+12
  // NS:[текст]  сервер NTP, ограничители [] обязательны
  // LE:[список] список эффектов, разделенный запятыми, ограничители [] обязательны        
  // NW:[текст]  SSID сети подключения
  // NA:[текст]  пароль подключения к сети
  // AU:X        создавать точку доступа 0-нет, 1-да
  // AN:[текст]  имя точки доступа
  // AA:[текст]  парольточки доступа
  // IP:xx.xx.xx.xx Текущий IP адрес WiFi соединения в сети
  // AM1H:HH     час включения режима 1     00..23
  // AM1M:MM     минуты включения режима 1  00..59
  // AM1E:NN     номер эффекта режима 1:   -1 - не используется; 0 - выключить матрицу; 1 - включить бегущую строку
  // AM2H:HH     час включения режима 2     00..23
  // AM2M:MM     минуты включения режима 2  00..59
  // AM2E:NN     номер эффекта режима 1:   -1 - не используется; 0 - выключить матрицу; 1 - включить бегущую строку
  
  String str = "", color, text;
  boolean allowed;
  byte b_tmp;
  // page: 1 - настройки; 2 - текст; 3 - эффекты; 4-часы; 5-настройки подключения; 6 - вкл/выкл по времени
  switch (page) { 
    case 1:  // Подключение; Бегущая строка. Вернуть: Ширина/Высота матрицы; Яркость;
      text = runningText;
      text.replace(";","~");
      str="$18 BR:"+String(globalBrightness) + "|ST:" + String(constrain(map(scrollSpeed, D_TEXT_SPEED_MIN,D_TEXT_SPEED_MAX, 0, 255), 0,255));
      str+="|OF:"; if (isTurnedOff)  str+="0"; else str+="1";
      str+="|EF:"+String(effect+1) + "|SE:" + String(constrain(map(effectSpeed, D_EFFECT_SPEED_MIN,D_EFFECT_SPEED_MAX, 0, 255), 0,255))+"|TS:";
      if (runningFlag)  str+="1|TX:["; else str+="0|TX:[";
      str += text + "]";
      str+=";";
      break;
    case 2:  // Настройки часов. Вернуть: Настройки синхронизации
      str="$18 NP:"; 
      if (useNtp)  str+="1|NT:"; else str+="0|NT:";
      str+=String(SYNC_TIME_PERIOD) + "|NZ:" + String(timeZoneOffset) + "|NS:["+String(ntpServerName)+"]";
      str+="|OF:"; if (isTurnedOff)  str+="0"; else str+="1";
      str+=";";
      break;
    case 3:  // Настройки подключения
      str="$18 AU:"; 
      if (useSoftAP) str+="1|AN:["; else str+="0|AN:[";
      str+=String(apName) + "]|AA:[";
      str+=String(apPass) + "]|NW:[";
      str+=String(ssid) + "]|NA:[";
      str+=String(pass) + "]|OF:";
      if (isTurnedOff)  str+="0"; else str+="1";
      str+="|IP:";
      if (wifi_connected) str += WiFi.localIP().toString(); 
      else                str += String(F("нет подключения"));
      str+=";";
      break;
    case 4:  // Настройки режимов автовключения по времени
      str="$18 AM1T:"+String(AM1_hour)+" "+String(AM1_minute)+"|AM1A:"+String(AM1_effect_id)+
             "|AM2T:"+String(AM2_hour)+" "+String(AM2_minute)+"|AM2A:"+String(AM2_effect_id); 
      str+="|OF:"; if (isTurnedOff)  str+="0"; else str+="1";
      str+=";";
      break;
  }
  
  if (str.length() > 0) {
    // Отправить клиенту запрошенные параметры страницы / режимов
    str.toCharArray(incomeBuffer, str.length()+1);    
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(incomeBuffer, str.length()+1);
    udp.endPacket();
    delay(0);
    Serial.println(String(F("Ответ на ")) + udp.remoteIP().toString() + ":" + String(udp.remotePort()) + " >> " + String(incomeBuffer));
  } else {
    sendAcknowledge();
  }
}

void sendAcknowledge() {
    // Если изменилось состояние вкл/выкл - отправить уведомление в программу
  if (isTurnedOff != prevTurnedOff) {
    prevTurnedOff = isTurnedOff;
    sendPageParams(1);    
  } else {
    // Отправить подтверждение, чтобы клиентский сокет прервал ожидание    
    String reply = "";
    bool isCmd = false; 
    reply += "ack" + String(ackCounter++) + ";";  
    reply.toCharArray(replyBuffer, reply.length()+1);    
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(replyBuffer, reply.length()+1);
    udp.endPacket();
    delay(0);
    if (isCmd) {
      Serial.println(String(F("Ответ на ")) + udp.remoteIP().toString() + ":" + String(udp.remotePort()) + " >> " + String(replyBuffer));
    }
  }
}

void setCurrentMode(int cMode) {
        
  runningFlag = false;
  loadingFlag = true;
  isTurnedOff = false;

  String str;
  byte tmp_eff = -1;

  switch(cMode) {
    case 0:  // Черный экран (выкл);
      isTurnedOff = true;
      break;
    case 1:  // Бегущая строка
      runningText = getRunningText();
      effect = getTextEffect();
      startRunningText();
      break;
  }

  setTimers();
  saveCurrentMode(cMode);
}

void setEffect(byte eff) {
  effect = eff;
  saveTextEffect(eff);
  setTimers();  
}

void startRunningText() {
  runningFlag = true;
  // Если при включении режима "Бегущая строка" цвет текста - черный -- включить белый, т.к черный на черном не видно
  if (globalColor == 0x000000) setGlobalColor(0xffffff);
  FastLED.setBrightness(globalBrightness);    
}

void showCurrentIP() {
  runningFlag = true;  
  wifi_print_ip = true;
  wifi_current_ip = getLocalIP();
}

String getLocalIP() {
  return wifi_connected ? WiFi.localIP().toString() : String(F("Нет подключения к сети WiFi"));
}

void setTimers() {
  scrollSpeed = getScrollSpeed();
  scrollTimer.setInterval(scrollSpeed);
  effectSpeed = getEffectSpeed(effect);
  effectTimer.setInterval(effectSpeed);
}
