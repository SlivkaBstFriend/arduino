//МУЗЫКАЛЬНЫЙ ДВЕРНОЙ ЗАМОК

#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ============================================================================
// НАЗНАЧЕНИЕ ПИНОВ
// ============================================================================
#define SPEAKER_PIN_OUT 2    // Speaker output (D2)
#define DOORBELL_BUTTON 3    // Doorbell button (D3, INT1)
#define MENU_BUTTON     4    // Menu/OK button (D4)
#define LEFT_BUTTON     5    // Left navigation (D5)
#define RIGHT_BUTTON    6    // Right navigation (D6)
#define BACK_BUTTON     7    // Back button (D7)
#define EXTERNAL_LED    9    // External LED indicator (D9)

// ============================================================================
// НАСТРОЙКА ЖК-ДИСПЛЕЯ
// ============================================================================
LiquidCrystal_I2C lcd(0x3f, 20, 4);  

// ============================================================================
// РЕЖИМЫ СИСТЕМЫ
// ============================================================================
enum Mode {
  MODE_IDLE,      // Режим главного экрана
  MODE_MENU       // Режим меню настроек
};

enum MenuState {
  MENU_SONG,      // Меню песен
  MENU_MODE,      // Меню режимов
  MENU_GUESTS     // Гостевой счетчик
};

enum SubMenu {
  SUBMENU_NONE,           // Нет подменю
  SUBMENU_SONG_SELECT,    // Выбор песни
  SUBMENU_MODE_SELECT,    // Выбор режима
  SUBMENU_GUEST_COUNTER   // Гостевой счетчик
};

enum WorkMode {
  MODE_NORMAL = 0,  // Мелодия + LED
  MODE_SILENT = 1,  // LED
  MODE_NOBODY = 2   // Только счетчик
};

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
volatile bool Flg_Doorbell = false;  // Флаг прерывания от кнопки звонка
Mode currentMode = MODE_IDLE;        // Текущий режим системы
MenuState menuSelection = MENU_SONG; // Выбранный пункт меню
SubMenu currentSubmenu = SUBMENU_NONE; // Текущее подменю
unsigned long lastActivity = 0;      // Время последней активности пользователя
int guestCount = 0;                  // Счётчик нажатий на кнопку звонка
uint8_t currentSongIndex = 0;        // Индекс текущей мелодии (0–4)
WorkMode workMode = MODE_NORMAL;     // Текущий режим работы
uint8_t tempSongIndex = 0;           // Временный выбор мелодии в меню
WorkMode tempWorkMode = MODE_NORMAL; // Временный выбор режима в меню

// ============================================================================
// АДРЕСА В EEPROM
// ============================================================================
#define EEPROM_SONG_ADDR   0  // Адрес для сохранения индекса мелодии
#define EEPROM_MODE_ADDR   1  // Адрес для сохранения режима работы
#define EEPROM_GUEST_ADDR  2  // Адрес для сохранения счётчика гостей

// ============================================================================
// ДАННЫЕ МЕЛОДИЙ (только 5 мелодий)
// ============================================================================
const char title_0[] PROGMEM = "HAPPY BIRTHDAY";
const char title_1[] PROGMEM = "TWINKLE TWINKLE";
const char title_2[] PROGMEM = "FRERES JACQUES";
const char title_3[] PROGMEM = "AU CLAIR LUNE";
const char title_4[] PROGMEM = "BERCEUSE";

const char* const title_table[] PROGMEM = {title_0, title_1, title_2, title_3, title_4};

#define MIN_SONG 0
#define MAX_SONG 4

// Компактный массив данных мелодий (только 5 мелодий)
const unsigned char ArrayMusic[] PROGMEM = {
  // 0: HAPPY BIRTHDAY
  0x18, 0x66,0x14,0x66,0x14,0x5b,0x2d,0x66,0x28, 0x4c,0x36,0x51,0x66,0x66,0x14,0x66,0x14, 0x5b,0x2d,0x66,0x28,0x44,0x3c,0x4c,0x6c,
  // 1: TWINKLE TWINKLE  
  0x1c, 0x9a,0x0e,0x9a,0x0e,0x66,0x15,0x66,0x15, 0x5b,0x18,0x5b,0x18,0x66,0x2b,0x73,0x13, 0x73,0x13,0x7a,0x12,0x7a,0x12,0x89,0x10, 0x89,0x10,0x9a,0x1d,
  // 2: FRERES JACQUES
  0x1c, 0x66,0x33,0x5b,0x39,0x51,0x40,0x66,0x33, 0x66,0x33,0x5b,0x39,0x51,0x40,0x66,0x33, 0x51,0x40,0x4c,0x44,0x44,0xa2,0x51,0x40, 0x4c,0x44,0x44,0xa2,
  // 3: AU CLAIR DE LA LUNE
  0x16, 0x4c,0x20,0x4c,0x20,0x4c,0x20,0x44,0x24, 0x3c,0x52,0x44,0x49,0x4c,0x20,0x3c,0x29, 0x44,0x24,0x44,0x24,0x4c,0x82,
  // 4: BERCEUSE - BRAHMS
  0x1a, 0x7a,0x1e,0x7a,0x0a,0x66,0x62,0x7a,0x1e, 0x7a,0x0a,0x66,0x62,0x7a,0x14,0x66,0x18, 0x4c,0x41,0x51,0x3d,0x5b,0x37,0x5b,0x37, 0x66,0x31,
  // End marker
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};

// Таблица соответствия нот и частот (значение ноты > частота в Гц)
int frequence[][2] = {
  {0x32,784}, {0x35,740}, {0x38,698}, {0x3C,659}, {0x40,622}, {0x44,587},
  {0x48,554}, {0x4C,523}, {0x51,494}, {0x56,466}, {0x5B,440}, {0x61,415},
  {0x66,392}, {0x6D,370}, {0x73,349}, {0x7A,330}, {0x82,311}, {0x89,294},
  {0x92,277}, {0x9A,262}, {0xA4,247}, {0xAE,233}, {0xB8,220}, {0xC3,208},
  {0xC7,196}, {0x00,  0}, {-1, -1}
};

// ============================================================================
// ОБРАБОТЧИК ПРЕРЫВАНИЯ
// ============================================================================
void Doorbell_ISR() {
  // Устанавливаем флаг при нажатии кнопки звонка (антидребезг обрабатывается в основном цикле)
  Flg_Doorbell = true;
}

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================

// Центрирование текста на строке ЖК-дисплея (20 символов)
void lcdPrintCenter(const char* text, uint8_t line) {
  int len = strlen(text);
  if (len > 20) len = 20;
  int pos = (20 - len) / 2;
  lcd.setCursor(pos, line);
  lcd.print(text);
}

// Чтение байта из памяти программ (данные мелодий)
unsigned char music(int off) {
  return pgm_read_byte_near(ArrayMusic + off);
}

// Получение частоты по значению ноты
int Get_Frequency(int val) {
  for(int i = 0; frequence[i][0] != -1; i++) {
    if(frequence[i][0] == val) return frequence[i][1];
  }
  return 0;
}

// Расчёт смещения к началу данных мелодии
int Walk_Music(char song) {
  int offset = 0;
  for(int fnd = 0; fnd < song; fnd++)
    offset = offset + music(offset) + 1;
  return ++offset;
}

// Получение количества нот в мелодии
int Get_Number_Of_Music_Note(char song) {
  int offset = Walk_Music(song);
  return music(--offset);
}

// Получение смещения к первой ноте мелодии
int Get_Offset_Of_Song_Start(char song) {
  int offset = 0;
  for(int fnd = 0; fnd < song; fnd++)
    offset = offset + music(offset) + 1;
  return ++offset;
}

// Проигрывание мелодии по индексу (0–4)
void Read_Song(char song) {
  if (song < 0 || song > MAX_SONG) return;

  int offset = Get_Offset_Of_Song_Start(song);
  int maxnote = Get_Number_Of_Music_Note(song);

  if (maxnote > 0) {
    do {
      int frq = Get_Frequency(music(offset));
      int lgt = music(offset + 1);
      offset += 2;
      if (frq > 0) {
        tone(SPEAKER_PIN_OUT, frq);
        delay(lgt * 3);  // Множитель темпа (регулирует скорость воспроизведения)
      } else {
        noTone(SPEAKER_PIN_OUT);
        delay(lgt * 3);
      }
      maxnote -= 2;
    } while (maxnote > 0);
  }
  noTone(SPEAKER_PIN_OUT);
}

// Предварительное прослушивание мелодии в меню (с индикацией LED)
void Preview_Song(uint8_t songIndex) {
  char titleBuf[21];
  strcpy_P(titleBuf, (char*)pgm_read_word(&(title_table[songIndex])));
  
  lcd.clear();
  lcdPrintCenter("Preview:", 0);
  lcdPrintCenter(titleBuf, 1);

  digitalWrite(EXTERNAL_LED, HIGH);
  Read_Song(songIndex);
  digitalWrite(EXTERNAL_LED, LOW);
}

// Функция антидребезга для кнопок (500 мс задержка)
bool isButtonPressed(uint8_t pin) {
  static unsigned long lastPress[20] = {0};
  if (digitalRead(pin) == LOW) {
    if (millis() - lastPress[pin] > 500) {  // Уменьшено с 250 мс для лучшей отзывчивости
      lastPress[pin] = millis();
      return true;
    }
  }
  return false;
}

// ============================================================================
// ФУНКЦИИ ОТОБРАЖЕНИЯ МЕНЮ
// ============================================================================

// Отображение главного меню настроек
void showMenu() {
  lcd.clear();
  lcdPrintCenter("== SETTINGS ==", 0);

  const char* items[] = {"Song Select", "Mode", "Guest Counter"};
  for (int i = 0; i < 3; i++) {
    lcd.setCursor(0, i + 1);
    if (i == (int)menuSelection) lcd.print("> ");
    else lcd.print("  ");
    lcd.print(items[i]);
  }
}

// Отображение подменю выбора мелодии
void showSongSelect() {
  lcd.clear();
  lcdPrintCenter("Choose melody:", 0);

  char titleBuf[21];
  strcpy_P(titleBuf, (char*)pgm_read_word(&(title_table[tempSongIndex])));
  lcdPrintCenter(titleBuf, 1);
}

// Отображение подменю выбора режима
void showModeSelect() {
  lcd.clear();
  lcdPrintCenter("Select mode:", 0);

  const char* modes[] = {"Normal", "Silent", "Nobody"};
  char modeStr[20];
  strcpy(modeStr, modes[(int)tempWorkMode]);
  lcdPrintCenter(modeStr, 1);
}

// Отображение счётчика гостей
void showGuestCounter() {
  lcd.clear();
  char buf[20];
  sprintf(buf, "Guests: %d", guestCount);
  lcdPrintCenter(buf, 0);
  lcdPrintCenter("Press OK to reset", 1);
}

// ============================================================================
// ФУНКЦИИ РАБОТЫ С EEPROM
// ============================================================================

// Сохранение текущего индекса мелодии в EEPROM
void Save_Current_Song() {
  EEPROM.write(EEPROM_SONG_ADDR, currentSongIndex);
}

// Загрузка индекса мелодии из EEPROM с проверкой на корректность
void Load_Current_Song() {
  currentSongIndex = EEPROM.read(EEPROM_SONG_ADDR);
  if (currentSongIndex > MAX_SONG) {
    currentSongIndex = 0;
    Save_Current_Song();
  }
}

// Сохранение текущего режима работы в EEPROM
void Save_Work_Mode() {
  EEPROM.write(EEPROM_MODE_ADDR, (uint8_t)workMode);
}

// Загрузка режима работы из EEPROM с проверкой на корректность
void Load_Work_Mode() {
  uint8_t mode = EEPROM.read(EEPROM_MODE_ADDR);
  if (mode <= (uint8_t)MODE_NOBODY) {
    workMode = (WorkMode)mode;
  } else {
    workMode = MODE_NORMAL;
    Save_Work_Mode();
  }
}

// Сохранение счётчика гостей в EEPROM
void Save_Guest_Count() {
  EEPROM.write(EEPROM_GUEST_ADDR, min(guestCount, 255));
}

// Загрузка счётчика гостей из EEPROM с защитой от мусора
void Load_Guest_Count() {
  guestCount = EEPROM.read(EEPROM_GUEST_ADDR);
  if (guestCount > 200) guestCount = 0;
}

// ============================================================================
// ФУНКЦИЯ ОТОБРАЖЕНИЯ ГЛАВНОГО ЭКРАНА
// ============================================================================

void showMainScreen() {
  lcd.clear();
  lcdPrintCenter("DoorBell Ready", 0);
}

// ============================================================================
// ФУНКЦИЯ ИНИЦИАЛИЗАЦИИ
// ============================================================================

void setup() {
  // Мигание LED при запуске (индикация старта)
  pinMode(9, OUTPUT);
  for(int i=0; i<3; i++) {
    digitalWrite(9, HIGH);
    delay(200);
    digitalWrite(9, LOW);
    delay(200);
  }

  // Настройка всех пинов
  pinMode(EXTERNAL_LED, OUTPUT);
  digitalWrite(EXTERNAL_LED, LOW);

  pinMode(DOORBELL_BUTTON, INPUT_PULLUP);
  pinMode(MENU_BUTTON, INPUT_PULLUP);
  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  pinMode(BACK_BUTTON, INPUT_PULLUP);

  // Подключение внешнего прерывания на кнопку звонка (по спаду)
  attachInterrupt(digitalPinToInterrupt(DOORBELL_BUTTON), Doorbell_ISR, FALLING);

  // Загрузка сохранённых настроек из EEPROM
  Load_Current_Song();
  Load_Work_Mode();
  Load_Guest_Count();

  // Инициализация ЖК-дисплея
  lcd.init();
  lcd.backlight();
  showMainScreen();

  lastActivity = millis();
}

// ============================================================================
// ОСНОВНОЙ ЦИКЛ ПРОГРАММЫ
// ============================================================================

void loop() {
  unsigned long now = millis();

  // Обработка события нажатия кнопки звонка
  if (Flg_Doorbell) {
    Flg_Doorbell = false;
    
    // Краткое мигание LED для отладки
    digitalWrite(9, HIGH);
    delay(100);
    digitalWrite(9, LOW);

    // Увеличение счётчика гостей и сохранение
    guestCount++;
    Save_Guest_Count();

    // Реакция в зависимости от текущего режима работы
    switch (workMode) {
      case MODE_NORMAL: {
        // Проигрывание выбранной мелодии с отображением на дисплее
        char titleBuf[21];
        strcpy_P(titleBuf, (char*)pgm_read_word(&(title_table[currentSongIndex])));
        lcd.clear();
        lcdPrintCenter("Now playing:", 0);
        lcdPrintCenter(titleBuf, 1);
        digitalWrite(EXTERNAL_LED, HIGH);
        Read_Song(currentSongIndex);
        digitalWrite(EXTERNAL_LED, LOW);
        showMainScreen();
        break;
      }
      case MODE_SILENT:
        // Мигание внешнего LED 3 раза без звука
        for (int i = 0; i < 3; i++) {
          digitalWrite(EXTERNAL_LED, HIGH);
          delay(150);
          digitalWrite(EXTERNAL_LED, LOW);
          delay(150);
        }
        showMainScreen();
        break;
      case MODE_NOBODY:
        // Только увеличение счётчика, без визуальной или звуковой реакции
        showMainScreen();
        break;
    }
    lastActivity = now;
    return;
  }

  // Обработка режима ожидания (главный экран)
  if (currentMode == MODE_IDLE) {
    if (isButtonPressed(MENU_BUTTON)) {
      // Переход в меню настроек
      currentMode = MODE_MENU;
      menuSelection = MENU_SONG;
      currentSubmenu = SUBMENU_NONE;
      showMenu();
      lastActivity = now;
    }
    return;
  }

  // Обработка режима меню
  if (currentMode == MODE_MENU) {
    // Обработка кнопки "Назад"
    if (isButtonPressed(BACK_BUTTON)) {
      if (currentSubmenu != SUBMENU_NONE) {
        // Возврат в главное меню
        currentSubmenu = SUBMENU_NONE;
        showMenu();
      } else {
        // Возврат на главный экран
        currentMode = MODE_IDLE;
        showMainScreen();
      }
      lastActivity = now;
      return;
    }

    // Обработка кнопки "Влево"
    if (isButtonPressed(LEFT_BUTTON)) {
      if (currentSubmenu == SUBMENU_SONG_SELECT) {
        tempSongIndex = (tempSongIndex + 4) % 5;
        Preview_Song(tempSongIndex);
        showSongSelect();
      } else if (currentSubmenu == SUBMENU_MODE_SELECT) {
        tempWorkMode = (WorkMode)(((int)tempWorkMode + 2) % 3);
        showModeSelect();
      } else {
        menuSelection = (MenuState)((menuSelection + 2) % 3);
        showMenu();
      }
      lastActivity = now;
    }

    // Обработка кнопки "Вправо"
    if (isButtonPressed(RIGHT_BUTTON)) {
      if (currentSubmenu == SUBMENU_SONG_SELECT) {
        tempSongIndex = (tempSongIndex + 1) % 5;
        Preview_Song(tempSongIndex);
        showSongSelect();
      } else if (currentSubmenu == SUBMENU_MODE_SELECT) {
        tempWorkMode = (WorkMode)(((int)tempWorkMode + 1) % 3);
        showModeSelect();
      } else {
        menuSelection = (MenuState)((menuSelection + 1) % 3);
        showMenu();
      }
      lastActivity = now;
    }

    // Обработка кнопки "Меню/OK"
    if (isButtonPressed(MENU_BUTTON)) {
      if (currentSubmenu == SUBMENU_NONE) {
        // Вход в выбранное подменю
        if (menuSelection == MENU_SONG) {
          currentSubmenu = SUBMENU_SONG_SELECT;
          tempSongIndex = currentSongIndex;
          showSongSelect();
        } else if (menuSelection == MENU_MODE) {
          currentSubmenu = SUBMENU_MODE_SELECT;
          tempWorkMode = workMode;
          showModeSelect();
        } else if (menuSelection == MENU_GUESTS) {
          currentSubmenu = SUBMENU_GUEST_COUNTER;
          showGuestCounter();
        }
      } else if (currentSubmenu == SUBMENU_SONG_SELECT) {
        // Подтверждение выбора мелодии
        currentSongIndex = tempSongIndex;
        Save_Current_Song();
        currentSubmenu = SUBMENU_NONE;
        showMenu();
      } else if (currentSubmenu == SUBMENU_MODE_SELECT) {
        // Подтверждение выбора режима
        workMode = tempWorkMode;
        Save_Work_Mode();
        currentSubmenu = SUBMENU_NONE;
        showMenu();
      } else if (currentSubmenu == SUBMENU_GUEST_COUNTER) {
        // Сброс счётчика гостей
        guestCount = 0;
        Save_Guest_Count();
        showGuestCounter();
      }
      lastActivity = now;
    }

    // Автоматический выход в режим ожидания после 15 секунд бездействия
    if (now - lastActivity > 15000) {
      currentMode = MODE_IDLE;
      showMainScreen();
    }
  }
}
