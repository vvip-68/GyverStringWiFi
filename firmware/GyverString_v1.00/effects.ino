
byte hue;

// Эффекты наложения цвета на изображение, имеющееся на матрице
void effects() {
  // Наложение эффекта изменения цвета / яркости на матрицу
  switch (effect) {
    case EFFECT_COLOR: colorsRoutine(); break;              // Цвет
    case EFFECT_RAINBOW_PIX: rainbowColorsRoutine(); break; // Радуга пикс.
  }

  // Изменение параметра цвета
  if (effectTimer.isReady()) {
    switch (effect) {
      case EFFECT_BREATH: brightnessRoutine(); break;       // Дыхание
      case EFFECT_COLOR: hue += 4; break;                   // Цвет
      case EFFECT_RAINBOW_PIX: hue++; break;                // Радуга пикс.
    }
  }
}

// *********** "дыхание" яркостью ***********
boolean brightnessDirection;
void brightnessRoutine() {
  if (brightnessDirection) {
    breathBrightness += 2;
    if (breathBrightness > globalBrightness - 2) {
      breathBrightness = globalBrightness;
      brightnessDirection = false;
    }
  } else {
    breathBrightness -= 2;
    if (breathBrightness < 2) {
      breathBrightness = 2;
      brightnessDirection = true;
    }
  }
  FastLED.setBrightness(breathBrightness);
}

// *********** смена цвета активных светодиодов (рисунка) ***********
void colorsRoutine() {
  for (int i = 0; i < NUM_LEDS; i++) {
    if (getPixColor(i) > 0) leds[i] = CHSV(hue, 255, 255);
  }
}


// *********** радуга активных светодиодов (рисунка) ***********
void rainbowColorsRoutine() {
  for (byte i = 0; i < WIDTH; i++) {
    CRGB thisColor = CHSV((byte)(hue + i * float(255 / WIDTH)), 255, 255);
    for (byte j = 0; j < HEIGHT; j++)
      if (getPixColor(getPixelNumber(i, j)) > 0) drawPixelXY(i, j, thisColor);
  }
}
