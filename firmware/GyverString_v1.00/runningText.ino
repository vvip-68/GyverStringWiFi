// работа с бегущим текстом

// **************** НАСТРОЙКИ ****************
#define MIRR_V 0          // отразить текст по вертикали (0 / 1)
#define MIRR_H 0          // отразить текст по горизонтали (0 / 1)

#if (BIG_FONT == 0)
  #define LET_WIDTH 5                       // ширина буквы шрифта
  #define LET_HEIGHT 8                      // высота буквы шрифта
  #define SPACE 1                           // пробел между буквами
#elif (BIG_FONT == 1)
  #define LET_WIDTH 10                      // ширина буквы шрифта
  #define LET_HEIGHT 16                     // высота буквы шрифта
  #define SPACE 2                           // пробел между буквами
#else  
  #define LET_WIDTH 8                       // ширина буквы шрифта
  #define LET_HEIGHT 13                     // высота буквы шрифта
  #define SPACE 1                           // пробел между буквами
#endif

// --------------------- ДЛЯ РАЗРАБОТЧИКОВ ----------------------

int offset = WIDTH;

void fillString(String text, uint32_t color) {
  if (loadingFlag) {
    offset = WIDTH;   // перемотка в правый край
    loadingFlag = false;    
    fullTextFlag = false;
  }
  
  if (scrollTimer.isReady()) {
    FastLED.clear();
    byte i = 0, j = 0, modif = 0;
    
    while (text[i] != '\0') {
      if ((byte)text[i] > 191) {    // работаем с русскими буквами!
        modif = (byte)text[i];
        i++;
      } else {      
        drawLetter(j, text[i], modif, offset + j * (LET_WIDTH + SPACE), color);
        modif = 0;
        i++;
        j++;
      }
    }
    
    fullTextFlag = false;

    // Если бегущая строка включена - смещаем текст, иначе позицию не изменяем
    if (runningFlag) offset--;
    
    if (offset < -j * (LET_WIDTH + SPACE)) {    // строка убежала
      offset = WIDTH + 3;
      fullTextFlag = true;
    }

    // Наложить эффект раскраски текста
    effects();

    FastLED.show();
  }
}

byte getTextY() {
  int8_t LH = LET_HEIGHT;
  if (LH > HEIGHT) LH = HEIGHT;
  int8_t offset_y = (HEIGHT - LH) / 2;     // по центру матрицы по высоте
  return offset_y; 
}

void drawLetter(uint8_t index, uint8_t letter, uint8_t modif, int16_t offset, uint32_t color) {
  int8_t start_pos = 0, finish_pos = LET_WIDTH;
  int8_t LH = LET_HEIGHT;
  if (LH > HEIGHT) LH = HEIGHT;
  int8_t offset_y = (HEIGHT - LH) / 2;     // по центру матрицы по высоте
  
  CRGB letterColor;
  if (color == 1) letterColor = CHSV(byte(offset * 10), 255, 255);
  else if (color == 2) letterColor = CHSV(byte(index * 30), 255, 255);
  else letterColor = color;

  if (offset < -LET_WIDTH || offset > WIDTH) return;
  if (offset < 0) start_pos = -offset;
  if (offset > (WIDTH - LET_WIDTH)) finish_pos = WIDTH - offset;

  for (byte i = start_pos; i < finish_pos; i++) {
    uint16_t thisByte; // байт колонки i отображаемого символа шрифта
    uint16_t diasByte; // байт колонки i отображаемого диакритического символа
    int8_t   diasOffs; // смещение по Y отображения диакритического символа: diasOffs > 0 - позиция над основной буквой; diasOffs < 0 - позиция ниже основной буквы
    int16_t  pn;       // номер пикселя в массиве leds[]
    
    if (MIRR_V) {
      thisByte = getFont(letter, modif, LET_WIDTH - 1 - i);
      diasByte = getDiasByte(letter, modif, LET_WIDTH - 1 - i);
    } else {
      thisByte = getFont(letter, modif, i);
      diasByte = getDiasByte(letter, modif, i);
    }
    diasOffs = getDiasOffset(letter, modif);

    for (byte j = 0; j < LH; j++) {
      boolean thisBit;

      if (MIRR_H) thisBit = thisByte & (1 << j);
      else        thisBit = thisByte & (1 << (LH - 1 - j));

      // рисуем столбец буквы шрифта (i - горизонтальная позиция, j - вертикальная)      
      if (thisBit) { 
        int8_t y = offset_y + j;
        if (y >= 0 && y < HEIGHT) {
          pn = getPixelNumber(offset + i, offset_y + j);
          if (pn >= 0 && pn < NUM_LEDS) {
            leds[pn] = letterColor;
          }
        }
      }

      if (MIRR_H) thisBit = diasByte & (1 << j);
      else        thisBit = diasByte & (1 << (LH - 1 - j));

      // рисуем столбец диакритического символа (i - горизонтальная позиция, j - вертикальная)      
      if (thisBit) { 
        int8_t y = offset_y + j + diasOffs;
        if (y >= 0 && y < HEIGHT) {
          pn = getPixelNumber(offset + i, y);
          if (pn >= 0 && pn < NUM_LEDS) {
            leds[pn] = letterColor;
          }
        }
      }
    }
  }
}

// ------------- СЛУЖЕБНЫЕ ФУНКЦИИ --------------

#if (BIG_FONT == 0)
 // Шрифт меньше/равно 8 точек - достаточно байта
 #define read_char pgm_read_byte
#else
 // Шрифт меньше/равно 16 точек - достаточно двух байт (word)
 #define read_char pgm_read_word
#endif


uint16_t getFont(uint8_t font, uint8_t modif, uint8_t row) {
  font = font - '0' + 16;   // перевод код символа из таблицы ASCII в номер согласно нумерации массива
  if (font <= 94) {
    return read_char(&(fontHEX[font][row]));   // для английских букв и символов
  } else if (modif == 209 && font == 116) {        // є
    return read_char(&(fontHEX[161][row])); 
  } else if (modif == 209 && font == 118) {        // і
      return read_char(&(fontHEX[73][row])); 
  } else if (modif == 209 && font == 119) {        // ї
      return read_char(&(fontHEX[162][row])); 
  } else if (modif == 209 && font == 113) {        // ё
      return read_char(&(fontHEX[132][row])); 
  } else if (modif == 208 && font == 100) {        // Є
      return read_char(&(fontHEX[160][row])); 
  } else if (modif == 208 && font == 102) {        // І
    return read_char(&(fontHEX[41][row])); 
  } else if (modif == 208 && font == 103) {        // Ї
    return read_char(&(fontHEX[41][row])); 
  } else if (modif == 208 && font == 97) {         // Ё
    return read_char(&(fontHEX[100][row]));     
  } else if ((modif == 208 || modif == 209) && font >= 112 && font <= 159) {         // и пизд*ц для русских
    return read_char(&(fontHEX[font - 17][row]));
  } else if ((modif == 208 || modif == 209) && font >= 96 && font <= 111) {
    return read_char(&(fontHEX[font + 47][row]));
  } else if (modif == 194 && font == 144) {                                          // Знак градуса '°'
    return read_char(&(fontHEX[159][row]));
  } else if (modif == 196 || modif == 197) {                                         // Буквы литовского алфавита  Ą Č Ę Ė Į Š Ų Ū Ž ą č ę ė į š ų ū ž
    switch (font) {
      case 100: return read_char(&(fontHEX[33][row])); //Ą 196   100  -     33
      case 108: return read_char(&(fontHEX[35][row])); //Č 196   108  -     35
      case 120: return read_char(&(fontHEX[37][row])); //Ę 196   120  -     37
      case 118: return read_char(&(fontHEX[37][row])); //Ė 196   118  -     37
      case 142: return read_char(&(fontHEX[41][row])); //Į 196   142  -     41
      case 128: return read_char(&(fontHEX[51][row])); //Š 197   128  -     51
      case 146: return read_char(&(fontHEX[53][row])); //Ų 197   146  -     53
      case 138: return read_char(&(fontHEX[53][row])); //Ū 197   138  -     53
      case 157: return read_char(&(fontHEX[58][row])); //Ž 197   157  -     58
      case 101: return read_char(&(fontHEX[65][row])); //ą 196   101  -     65
      case 109: return read_char(&(fontHEX[67][row])); //č 196   109  -     67  
      case 121: return read_char(&(fontHEX[69][row])); //ę 196   121  -     69
      case 119: return read_char(&(fontHEX[69][row])); //ė 196   119  -     69
      case 143: return read_char(&(fontHEX[73][row])); //į 196   143  -     73
      case 129: return read_char(&(fontHEX[83][row])); //š 197   129  -     83
      case 147: return read_char(&(fontHEX[85][row])); //ų 197   147  -     85
      case 139: return read_char(&(fontHEX[85][row])); //ū 197   139  -     85
      case 158: return read_char(&(fontHEX[90][row])); //ž 197   158  -     90
    }
  }
  return 0;
}

uint16_t getDiasByte(uint8_t font, uint8_t modif, uint8_t row) {
  font = font - '0' + 16;   // перевод код символа из таблицы ASCII в номер согласно нумерации массива
  if ((modif == 208) && font == 97) {              // Ё
    return read_char(&(diasHEX[0][row])); 
  } else if ((modif == 209) && font == 113) {      // ё
    return read_char(&(diasHEX[0][row])); 
  } else if ((modif == 208) && font == 103) {      // Ї
    return read_char(&(diasHEX[0][row])); 
  } else if (modif == 196 || modif == 197) {                                           // Буквы литовского алфавита  Ą Č Ę Ė Į Š Ų Ū Ž ą č ę ė į š ų ū ž
    // 0 - Č - перевернутая крышечка над заглавной буквой Č Ž č ž
    // 1 - Ė - точка над заглавной буквой Ė ė
    // 2 - Ū - надстрочная черта над заглавной буквой Ū ū
    // 3 - Ą - хвостик снизу букв Ą ą Ę ę ų - смещение к правому краю буквы
    // 4 - Į - хвостик снизу букв Į į Ų     - по центру буквы    
    switch (font) {
      case 100: return read_char(&(diasHEX[4][row])); //Ą 196   100  -     33
      case 108: return read_char(&(diasHEX[1][row])); //Č 196   108  -     35
      case 120: return read_char(&(diasHEX[4][row])); //Ę 196   120  -     37
      case 118: return read_char(&(diasHEX[2][row])); //Ė 196   118  -     37
      case 142: return read_char(&(diasHEX[5][row])); //Į 196   142  -     41
      case 128: return read_char(&(diasHEX[1][row])); //Š 197   128  -     51
      case 146: return read_char(&(diasHEX[5][row])); //Ų 197   146  -     53
      case 138: return read_char(&(diasHEX[3][row])); //Ū 197   138  -     53
      case 157: return read_char(&(diasHEX[1][row])); //Ž 197   157  -     58
      case 101: return read_char(&(diasHEX[4][row])); //ą 196   101  -     65
      case 109: return read_char(&(diasHEX[1][row])); //č 196   109  -     67  
      case 121: return read_char(&(diasHEX[4][row])); //ę 196   121  -     69
      case 119: return read_char(&(diasHEX[2][row])); //ė 196   119  -     69
      case 143: return read_char(&(diasHEX[5][row])); //į 196   143  -     73
      case 129: return read_char(&(diasHEX[1][row])); //š 197   129  -     83
      case 147: return read_char(&(diasHEX[4][row])); //ų 197   147  -     85
      case 139: return read_char(&(diasHEX[3][row])); //ū 197   139  -     85
      case 158: return read_char(&(diasHEX[1][row])); //ž 197   158  -     90
    }
  }
  return 0;
}

int8_t getDiasOffset(uint8_t font, uint8_t modif) {
  font = font - '0' + 16;   // перевод код символа из таблицы ASCII в номер согласно нумерации массива
  if ((modif == 208) && font == 97) {              // Ё
    return 3; 
  } else if ((modif == 209) && font == 113) {      // ё
    #if (BIG_FONT == 0)
      return 1; 
    #else
      return 0; 
    #endif  
  } else if ((modif == 208) && font == 103) {      // Ї
    return 3; 
  } else if (modif == 196 || modif == 197) {       // Буквы литовского алфавита  Ą Č Ę Ė Į Š Ų Ū Ž ą č ę ė į š ų ū ž
    // Смещение надстрочных заглавных - 3
    // Смещение надстрочных маленьких букв - 0 или 1
    // Смещение подстрочного символа -1
    #if (BIG_FONT == 0)
      switch (font) {
        case 100: return -1; //Ą 196   100
        case 108: return  2; //Č 196   108
        case 120: return -1; //Ę 196   120
        case 118: return  3; //Ė 196   118
        case 142: return -1; //Į 196   142
        case 128: return  2; //Š 197   128
        case 146: return -1; //Ų 197   146
        case 138: return  3; //Ū 197   138
        case 157: return  2; //Ž 197   157
        case 101: return -1; //ą 196   101
        case 109: return  0; //č 196   109
        case 121: return -1; //ę 196   121
        case 119: return  1; //ė 196   119
        case 143: return -1; //į 196   143
        case 129: return  0; //š 197   129
        case 147: return -1; //ų 197   147
        case 139: return  1; //ū 197   139
        case 158: return  0; //ž 197   158
      }
    #else
      switch (font) {
        case 100: return -1; //Ą 196   100
        case 108: return  3; //Č 196   108
        case 120: return -1; //Ę 196   120
        case 118: return  3; //Ė 196   118
        case 142: return -1; //Į 196   142
        case 128: return  3; //Š 197   128
        case 146: return -1; //Ų 197   146
        case 138: return  3; //Ū 197   138
        case 157: return  3; //Ž 197   157
        case 101: return -1; //ą 196   101
        case 109: return  0; //č 196   109
        case 121: return -1; //ę 196   121
        case 119: return  0; //ė 196   119
        case 143: return -1; //į 196   143
        case 129: return  0; //š 197   129
        case 147: return -1; //ų 197   147
        case 139: return  0; //ū 197   139
        case 158: return  0; //ž 197   158
      }
    #endif
  }
  return 0;
}
