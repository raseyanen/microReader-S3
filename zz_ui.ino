/* ====================== Интерфейс: главное меню и сообщения ====================== */

// строка UTF-8 шрифтом 5x7 (кириллица поддерживается), не более maxCols символов
static void uiText(const char* s, int16_t x, int16_t y, int16_t maxCols) {
  texClipTop = 0;
  for (int16_t col = 0; *s && col < maxCols; col++) texDrawCp(texU8Next(s), x + col * 6, y);
}

void drawMainMenu(void) {
  oled.clear();
  texClipTop = 0;
  uiText("MICROREADER", 0, 0, 16);
  char cnt[8];
  snprintf(cnt, sizeof(cnt), "%d", fileCount);
  uiText(cnt, SCR_W - (int16_t)strlen(cnt) * 6, 0, 8);
  texHLine(0, SCR_W - 1, 9);

  const int16_t rows = (SCR_H - 12 - 9) / 9;                 // строк списка
  if (fileCount == 0) {
    uiText("Нет файлов", 0, 16, 16);
    uiText("Удержите центр", 0, 34, 16);
    uiText("джойстика: USB", 0, 43, 16);
    uiText("накопитель", 0, 52, 16);
  } else {
    int16_t top = cursor - rows / 2;
    if (top > fileCount - rows) top = fileCount - rows;
    if (top < 0) top = 0;
    for (int16_t r = 0; r < rows && top + r < fileCount; r++) {
      int16_t idx = top + r, y = 12 + r * 9;
      if (idx == cursor) uiText(">", 0, y, 1);
      uiText(fileList[idx], 8, y, (SCR_W - 8) / 6);
    }
  }
  texHLine(0, SCR_W - 10, SCR_H - 10);
  uiText("удерж.центр: USB", 0, SCR_H - 8, 16);
  oled.update();
}

void uiMessage(const char* l1, const char* l2, const char* l3) {
  oled.clear();
  texClipTop = 0;
  const char* ls[3] = {l1, l2, l3};
  for (uint8_t i = 0; i < 3; i++) {
    if (!ls[i] || !*ls[i]) continue;
    int16_t w = texU8Len(ls[i]) * 6;
    if (w > SCR_W) w = SCR_W;
    uiText(ls[i], (SCR_W - w) / 2, 14 + i * 18, SCR_W / 6);
  }
  oled.update();
}

void fileReadError(void) {
  uiMessage("Ошибка чтения", "файла", "");
  delay(1500);
}
