#pragma once
/* Группа физических кнопок, ведущая себя как одна кнопка EncButton:
   up   = джойстик вверх + влево + кнопка A
   down = джойстик вниз + вправо + кнопка B
   ok   = центр джойстика
   Поэтому читалки работают без изменений: up.click(), down.step(), ok.hold()... */
struct KeyGroup {
  Button* b[3];
  uint8_t n;
  KeyGroup(Button* a, Button* b2 = nullptr, Button* c = nullptr) {
    b[0] = a; b[1] = b2; b[2] = c;
    n = c ? 3 : (b2 ? 2 : 1);
  }
  void tick()  { for (uint8_t i = 0; i < n; i++) b[i]->tick(); }
  bool click() { bool r = false; for (uint8_t i = 0; i < n; i++) if (b[i]->click()) r = true; return r; }
  bool hold()  { bool r = false; for (uint8_t i = 0; i < n; i++) if (b[i]->hold())  r = true; return r; }
  bool step()  { bool r = false; for (uint8_t i = 0; i < n; i++) if (b[i]->step())  r = true; return r; }
};
