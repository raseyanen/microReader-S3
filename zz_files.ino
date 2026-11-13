/* ====================== Файлы: список на диске FFat и чтение .txt ====================== */

static const char* basenameOf(const char* path) {
  const char* slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

static bool readableExt(const char* n) {
  size_t a = strlen(n);
  auto has = [&](const char* e) { size_t b = strlen(e); return a >= b && strcasecmp(n + a - b, e) == 0; };
  return has(".txt") || has(".md") || has(".tex");
}

void checkFileSystem(void) {
  fileCount = 0;
  File root = FFat.open("/");
  if (!root) return;
  while (true) {
    File f = root.openNextFile();
    if (!f) break;
    const char* n = basenameOf(f.name());
    if (!f.isDirectory() && n[0] != '.' && readableExt(n) && strlen(n) < FILE_NAME_MAX && fileCount < MAX_FILES)
      strcpy(fileList[fileCount++], n);
    f.close();
    yield();
  }
  for (int16_t i = 1; i < fileCount; i++) {                 // сортировка по имени (без учёта регистра)
    char t[FILE_NAME_MAX];
    strcpy(t, fileList[i]);
    int16_t j = i - 1;
    while (j >= 0 && strcasecmp(fileList[j], t) > 0) { strcpy(fileList[j + 1], fileList[j]); j--; }
    strcpy(fileList[j + 1], t);
  }
  if (cursor >= fileCount) cursor = fileCount > 0 ? fileCount - 1 : 0;
}

/* ---- Примеры на пустой диск ---- */
static const char DEMO_MD[] PROGMEM = R"MD(% Пример файла-шпаргалки (.md). Страницы секции: текст, формулы (tex:), таблица, графики (plot:).

## Дискриминант
Квадратное уравнение ax^2 + bx + c = 0 имеет вещественные корни, если D >= 0.
tex: D = b^2 - 4ac
tex: x_{1,2} = \frac{-b \pm \sqrt{D}}{2a}

## Степени двойки
| n | 2^n | n^2 |
|--:|----:|----:|
| 0 | 1 | 0 |
| 1 | 2 | 1 |
| 2 | 4 | 4 |
| 3 | 8 | 9 |
| 4 | 16 | 16 |
plot: table x=1 y=2,3

## Графики функций
plot: y = sin(x); y = cos(x); x = -2*pi..2*pi; y = -1.5..1.5
plot: y = exp(-x/6)*cos(2*x); x = 0..20
)MD";

void writeDemoFiles(void) {
  File f = FFat.open("/demo.md", "w");
  if (!f) return;
  f.print(FPSTR(DEMO_MD));
  f.close();
}

/* ---- Чтение .txt: перенос по словам, страницы вперёд/назад ---- */
#define TXT_COLS ((SCR_W - 2) / 6)       // символов в строке
#define TXT_ROWS ((SCR_H - 8) / 9)       // строк на странице
#define TXT_MAX_PAGES 1024

struct TxtRow { uint16_t off; uint8_t len; };
static uint8_t txtBuf[1024];
static TxtRow txtRows[TXT_ROWS];
static uint8_t txtN = 0;
static uint32_t txtStarts[TXT_MAX_PAGES];

// раскладывает страницу, начинающуюся с позиции start; возвращает позицию следующей страницы
static uint32_t txtLayout(File& f, uint32_t start) {
  f.seek(start);
  int n = f.read(txtBuf, sizeof(txtBuf));
  if (n < 0) n = 0;
  bool more = (n == (int)sizeof(txtBuf));
  txtN = 0;
  int i = 0;
  while (txtN < TXT_ROWS && i < n) {
    int rs = i, j = i, chars = 0, lastSp = -1;
    while (j < n && txtBuf[j] != '\n' && chars < TXT_COLS) {
      if (txtBuf[j] == ' ' || txtBuf[j] == '\t') lastSp = j;
      j++;
      while (j < n && (txtBuf[j] & 0xC0) == 0x80) j++;       // хвост UTF-8 символа
      chars++;
    }
    int cut, next;
    if (j >= n) { if (more) break; cut = j; next = j; }
    else if (txtBuf[j] == '\n') { cut = j; next = j + 1; }
    else if (txtBuf[j] == ' ' || txtBuf[j] == '\t') { cut = j; next = j + 1; }
    else if (lastSp > rs) { cut = lastSp; next = lastSp + 1; }
    else { cut = j; next = j; }                              // слово длиннее строки - режем жёстко
    int l = cut - rs;
    while (l > 0 && (txtBuf[rs + l - 1] == '\r' || txtBuf[rs + l - 1] == ' ' || txtBuf[rs + l - 1] == '\t')) l--;
    txtRows[txtN].off = rs;
    txtRows[txtN].len = l > 255 ? 255 : l;
    txtN++;
    i = next;
    if (next == cut + 1 && cut < n && txtBuf[cut] != '\n') while (i < n && txtBuf[i] == ' ') i++;
  }
  if (i == 0 && n > 0) i = 1;                                // защита от зацикливания
  return start + i;
}

static void txtDraw(uint32_t cur, uint32_t size) {
  oled.clear();
  texClipTop = 0;
  char tmp[260];
  for (uint8_t r = 0; r < txtN; r++) {
    uint16_t l = txtRows[r].len;
    memcpy(tmp, txtBuf + txtRows[r].off, l);
    tmp[l] = 0;
    const char* s = tmp;
    for (int16_t col = 0; *s && col < TXT_COLS; col++) texDrawCp(texU8Next(s), 1 + col * 6, 1 + r * 9);
  }
  char st[8];
  snprintf(st, sizeof(st), "%u%%", (unsigned)(size ? (uint64_t)cur * 100 / size : 0));
  for (uint8_t i = 0; st[i]; i++) texChar3(st[i], SCR_W - (int16_t)strlen(st) * 4 + i * 4, SCR_H - 6);
  oled.update();
}

void enterToReadTxtFile(void) {
  String fn = ("/" + selectedFile);
  File f = FFat.open(fn, "r");
  if (!f) {
    fileReadError();
    checkFileSystem();
    drawMainMenu();
    return;
  }
  uint32_t size = f.size();
  uint32_t cur = 0;
  uint16_t idx = 0;
  txtStarts[0] = 0;
  if (resumeVal) {                                           // продолжение после сна: восстанавливаем стек страниц
    while (cur < resumeVal) {
      uint32_t e = txtLayout(f, cur);
      if (e <= cur || e > resumeVal) break;
      if (idx + 1 < TXT_MAX_PAGES) txtStarts[++idx] = e;
      cur = e;
    }
    resumeVal = 0;
  }
  uint32_t nextOff = txtLayout(f, cur);
  txtDraw(cur, size);
  uiTimer = millis();

  while (1) {
    up.tick(); ok.tick(); down.tick();
    if (millis() - uiTimer > SLEEP_MS) sleepNow('T', cur);
    if (ok.hold()) {                                         // выход - удержание центра
      waitOkRelease();
      uiTimer = millis();
      f.close();
      drawMainMenu();
      return;
    }
    if (down.click() || down.step()) {                       // вперёд
      uiTimer = millis();
      if (nextOff < size && nextOff > cur) {
        if (idx + 1 < TXT_MAX_PAGES) txtStarts[++idx] = nextOff;
        cur = nextOff;
        nextOff = txtLayout(f, cur);
        txtDraw(cur, size);
      }
    } else if (up.click() || up.step()) {                    // назад
      uiTimer = millis();
      if (idx > 0) {
        cur = txtStarts[--idx];
        nextOff = txtLayout(f, cur);
        txtDraw(cur, size);
      }
    }
    yield();
  }
}
