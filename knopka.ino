/* =========================================================
   СИСТЕМА ВИКТОРИНЫ (2 РЕЖИМА + IDLE + ШТРАФЫ)
   ========================================================= */

/* ===================== НАСТРОЙКИ ВРЕМЕНИ ===================== */
const unsigned long DEBOUNCE_DELAY       = 10;       // антидребезг RESET
const unsigned long MULTICLICK_TIMEOUT   = 1000;      // окно для 4 быстрых кликов
const unsigned long PENALTY_DURATION     = 3 * 1000;     // длительность штрафа
const unsigned long PENALTY_BLINK_INTERVAL = 80;    // скорость мигания штрафа
const unsigned long IDLE_TIMEOUT         = 1200 * 1000;   // 20 минут без активности
const unsigned long IDLE_DURATION        = 10 * 1000;    // длительность IDLE
const unsigned long IDLE_BLINK_INTERVAL  = 150;      // скорость мигания IDLE
const unsigned long FLASH_DURATION       = 50;       // вспышка всех светодиодов

/* ===================== ПИНЫ ===================== */
const int ledPins[]    = {27, 26, 25, 33};
const int buttonPins[] = {5, 18, 19, 21};
const int resetButtonPin = 23;

const int numLeds    = 4;
const int numButtons = 4;

/* ===================== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ===================== */
bool mode2 = false;

bool lastResetReading  = HIGH;
bool resetStableState  = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long firstClickTime   = 0;
int clickCount                 = 0;

int activeButton = -1;

bool penaltyActive[4] = {false};
bool penaltyArmed[4]  = {false};
unsigned long penaltyStartTime[4] = {0};
unsigned long blinkTimer[4]       = {0};
bool blinkState[4] = {false};

unsigned long lastActivityTime = 0;
bool idleMode                  = false;
unsigned long idleStartTime    = 0;
unsigned long idleBlinkTimer   = 0;

/* ===================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ===================== */
void flashAllLeds() {
  for (int i = 0; i < numLeds; i++)
    digitalWrite(ledPins[i], HIGH);
  delay(FLASH_DURATION);
  for (int i = 0; i < numLeds; i++)
    digitalWrite(ledPins[i], LOW);
}

void startupBlink() {
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], HIGH);
    delay(100);
    digitalWrite(ledPins[i], LOW);
  }
}

// регистрируем активность пользователя
void registerActivity() {
  lastActivityTime = millis();

  if (idleMode) {
    idleMode = false;

    // Принудительно гасим все светодиоды при выходе из IDLE
    for (int i = 0; i < numLeds; i++)
      digitalWrite(ledPins[i], LOW);
  }
}

/* ===================== SETUP ===================== */
void setup() {
  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  for (int i = 0; i < numButtons; i++)
    pinMode(buttonPins[i], INPUT_PULLUP);

  pinMode(resetButtonPin, INPUT_PULLUP);

  startupBlink();

  lastActivityTime = millis();
  randomSeed(analogRead(34));
}

/* ===================== LOOP ===================== */
void loop() {

  /* ===================== IDLE ===================== */
  if (!idleMode && millis() - lastActivityTime > IDLE_TIMEOUT) {
    idleMode = true;
    idleStartTime = millis();
  }

  if (idleMode) {

    if (millis() - idleStartTime > IDLE_DURATION) {
      idleMode = false;
      lastActivityTime = millis();

      // Принудительно гасим все светодиоды после завершения IDLE
      for (int i = 0; i < numLeds; i++)
        digitalWrite(ledPins[i], LOW);
    } 
    else {
      if (millis() - idleBlinkTimer > IDLE_BLINK_INTERVAL) {
        idleBlinkTimer = millis();

        // Выключаем все светодиоды перед новым миганием
        for (int i = 0; i < numLeds; i++)
          digitalWrite(ledPins[i], LOW);

        int led = random(numLeds);
        digitalWrite(ledPins[led], HIGH);
      }
    }

    // Любое нажатие прерывает IDLE
    for (int i = 0; i < numButtons; i++)
      if (digitalRead(buttonPins[i]) == LOW)
        registerActivity();

    if (digitalRead(resetButtonPin) == LOW)
      registerActivity();

    delay(5);
    return;
  }

  /* ===================== RESET + 4 КЛИКА ===================== */
  bool reading = digitalRead(resetButtonPin);

  if (reading != lastResetReading)
    lastDebounceTime = millis();

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {

    if (reading != resetStableState) {
      resetStableState = reading;

      // Нажатие RESET
      if (resetStableState == LOW) {
        registerActivity();
        flashAllLeds();

        if (clickCount == 0)
          firstClickTime = millis();

        clickCount++;

        if (millis() - firstClickTime > MULTICLICK_TIMEOUT) {
          clickCount = 1;
          firstClickTime = millis();
        }

        if (clickCount >= 4) {
          mode2 = !mode2;
          int cycles = mode2 ? 2 : 1;
          for (int c = 0; c < cycles; c++)
            startupBlink();
          clickCount = 0;
        }
      }

      // Отпускание RESET — запуск штрафов
      if (resetStableState == HIGH && mode2) {
        for (int i = 0; i < numButtons; i++) {
          if (penaltyArmed[i]) {
            penaltyArmed[i] = false;
            penaltyActive[i] = true;
            penaltyStartTime[i] = millis();
          }
        }
      }
    }
  }

  lastResetReading = reading;
  bool resetPressed = (resetStableState == LOW);

  /* ===================== РЕЖИМ 2: подготовка штрафов ===================== */
  if (mode2 && resetPressed) {
    for (int i = 0; i < numButtons; i++) {
      if (digitalRead(buttonPins[i]) == LOW && !penaltyActive[i]) {
        penaltyArmed[i] = true;
        registerActivity();
      }
    }
  }

  /* ===================== ОБНОВЛЕНИЕ ШТРАФОВ ===================== */
  for (int i = 0; i < numButtons; i++) {
    if (penaltyArmed[i] || penaltyActive[i]) {
      if (millis() - blinkTimer[i] > PENALTY_BLINK_INTERVAL) {
        blinkTimer[i] = millis();
        blinkState[i] = !blinkState[i];
        digitalWrite(ledPins[i], blinkState[i]);
      }
    }

    if (penaltyActive[i] &&
        millis() - penaltyStartTime[i] >= PENALTY_DURATION) {
      penaltyActive[i] = false;
      digitalWrite(ledPins[i], LOW);
    }
  }

  /* ===================== ОПРОС КНОПОК ===================== */
  if (activeButton == -1 && !resetPressed) {
    for (int i = 0; i < numButtons; i++) {
      if (!penaltyActive[i] && digitalRead(buttonPins[i]) == LOW) {
        activeButton = i;
        digitalWrite(ledPins[i], HIGH);
        registerActivity();
        break;
      }
    }
  }

  /* ===================== СБРОС ИГРЫ ===================== */
  if (activeButton != -1 && resetPressed) {
    digitalWrite(ledPins[activeButton], LOW);
    activeButton = -1;
    registerActivity();
  }

  delay(5);
}
