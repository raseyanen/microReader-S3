#pragma once
/* ============================ НАСТРОЙКИ microReader-S3 ============================
   ESP32-S3 Super Mini + e-paper WeAct 1.54" (200x200, SSD1681) + джойстик 5 позиций + 2 кнопки.
   ПИНЫ проверьте по распиновке СВОЕЙ платы: GPIO19/20 заняты USB, GPIO0/3/45/46 - strapping,
   GPIO26..32 - флеш/PSRAM. Кнопки и джойстик подключаются между пином и GND (внутренняя подтяжка). */

// --- e-paper (SPI) ---
#define EPD_SCK_PIN    8
#define EPD_MOSI_PIN   9
#define EPD_CS_PIN     11
#define EPD_DC_PIN      10
#define EPD_RST_PIN     13
#define EPD_BUSY_PIN    12

// --- джойстик 5 позиций ---
#define JOY_UP_PIN      1
#define JOY_DN_PIN      5
#define JOY_LF_PIN      3
#define JOY_RT_PIN      4
#define JOY_OK_PIN      2      // центр (нажатие). Должен быть RTC-пином (GPIO0..21): он будит из глубокого сна

// --- две кнопки (дублируют джойстик при чтении: A = назад, B = вперёд) ---
#define BTN_A_PIN      6
#define BTN_B_PIN      7

// --- экран ---
#define EPD_NATIVE    200      // физическое разрешение панели (200x200)
#define EPD_SCALE       2      // 2 = логический экран 100x100, каждая точка удваивается (текст читаемый)
                               // 1 = логический экран 200x200 (мелко, но больше помещается)
#define SCR_W (EPD_NATIVE / EPD_SCALE)
#define SCR_H (EPD_NATIVE / EPD_SCALE)
#define EPD_FULL_EVERY  8      // полное обновление (против остаточного изображения) раз в N частичных

// --- прочее ---
#define SLEEP_MS   120000UL    // бездействие до глубокого сна, мс (страница остаётся на экране)
#define MAX_FILES      48      // максимум файлов в меню
#define FILE_NAME_MAX  48      // максимум байт в имени файла
