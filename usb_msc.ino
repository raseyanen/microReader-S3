/* ====================== Режим USB-накопителя (TinyUSB MSC) ======================
   Раздел ffat отдаётся компьютеру как диск (FAT на сыром доступе к wear-levelling).
   На время режима файловая система в прошивке размонтируется (одновременный доступ испортил бы диск),
   по завершении устройство перезагружается и заново читает список файлов. */
#if !defined(ARDUINO_USB_MODE) || ARDUINO_USB_MODE != 0
#error "Выберите Tools -> USB Mode -> USB-OTG (TinyUSB), иначе режим накопителя недоступен"
#endif
#include "USB.h"
#include "USBMSC.h"
#include "wear_levelling.h"
#include "esp_partition.h"

static USBMSC msc;
static wl_handle_t mscWl = WL_INVALID_HANDLE;
static size_t mscSec = 4096;
static volatile bool mscEject = false;

static int32_t mscRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  return wl_read(mscWl, (size_t)lba * mscSec + offset, buffer, bufsize) == ESP_OK ? (int32_t)bufsize : -1;
}

static int32_t mscWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  size_t addr = (size_t)lba * mscSec;
  if (offset == 0 && wl_erase_range(mscWl, addr, mscSec) != ESP_OK) return -1;   // сектор стираем перед записью
  return wl_write(mscWl, addr + offset, buffer, bufsize) == ESP_OK ? (int32_t)bufsize : -1;
}

static bool mscStartStop(uint8_t power_condition, bool start, bool load_eject) {
  if (load_eject && !start) mscEject = true;                // ПК извлёк диск
  return true;
}

void usbMode(void) {
  uiMessage("USB-НАКОПИТЕЛЬ", "Подключите USB", "к компьютеру");
  FFat.end();                                               // освобождаем раздел
  const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "ffat");
  if (!part || wl_mount(part, &mscWl) != ESP_OK) {
    Serial.println(F("usb: не удалось открыть раздел ffat"));
    uiMessage("ОШИБКА USB", "раздел ffat", "не найден");
    delay(3000);
    ESP.restart();
  }
  mscSec = wl_sector_size(mscWl);
  uint32_t blocks = wl_size(mscWl) / mscSec;
  Serial.printf("usb: %u блоков по %u байт\n", (unsigned)blocks, (unsigned)mscSec);

  msc.vendorID("MicroRdr");
  msc.productID("Reader");
  msc.productRevision("1.0");
  msc.onRead(mscRead);
  msc.onWrite(mscWrite);
  msc.onStartStop(mscStartStop);
  msc.mediaPresent(true);
  msc.begin(blocks, mscSec);
  USB.begin();

  uiMessage("USB-НАКОПИТЕЛЬ", "Копируйте файлы", "ОК удерж: выход");
  mscEject = false;
  while (!mscEject) {
    ok.tick();
    if (ok.hold()) break;                                   // выход кнопкой
    delay(10);
  }
  msc.end();
  wl_unmount(mscWl);
  uiMessage("Перезагрузка...", "", "");
  delay(300);
  ESP.restart();
}
