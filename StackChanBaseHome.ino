#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <M5StackChan.h>
#include <config.h>
#include <Arduino.h>
#include <algorithm>
#include <WiFiClientSecure.h>
#include <esp_system.h>


void setup() {
  M5StackChan.begin();
  Serial.printf("Reset reason: %d, heap libero: %u\n", esp_reset_reason(), ESP.getFreeHeap());

  auto& display = M5StackChan.Display();
  centerX = display.width() / 2;
  centerY = display.height() / 2;
  bodyCenterY = centerY + 10;
  w = display.width();
  h = display.height();
  hBody = h - hTitleBar;

  M5StackChan.Display().setTextDatum(textdatum_t::middle_center);
  M5StackChan.Display().setTextSize(2);

  updateDisplay("Connessione WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int dots = 0;
  const unsigned long wifiTimeout = 15000;
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < wifiTimeout) {
    delay(500);
    String connectingMsg = "Connessione WiFi";
    for (int i = 0; i <= (dots % 3); i++) {
      connectingMsg += ".";
    }
    updateDisplay(connectingMsg.c_str());
    dots++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    currentLocalIP = WiFi.localIP().toString();
    updateDisplay("WiFi Connesso!");
    delay(500);
  } else {
    // L'avvio prosegue anche senza rete: le funzioni di stato ritenteranno in seguito.
    currentLocalIP = "N/D";
    updateDisplay("WiFi non connesso");
  }

  // Avvia SNTP anche offline: si sincronizzerà automaticamente a una successiva connessione WiFi.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();
  if (WiFi.status() == WL_CONNECTED) {
    updateDisplay("Sincronizzazione ora...");
    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 30) {
      delay(500);
      retry++;
    }
  }

  currentState = STATE_METEO;

  showCurrentStateInfo();
}


void logHeap(const char* context) {
  Serial.printf("Heap %s - libero: %u, minimo: %u, blocco max: %u\n", context, ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
}


void loop() {
  M5StackChan.update();

  auto& ts = M5StackChan.TouchSensor;
  if (ts.wasClicked()) {
    // I sensori capacitivi esterni sono riservati al cambio di STATE.
    handleTouchTransition();
  }

  // Il touchscreen del display sfoglia news e pagine calcio.
  if (currentState == STATE_NEWS || currentState == STATE_FOOTBALL_SCORES) {
    auto touch = M5.Touch.getDetail();
    if (touch.wasClicked()) {
      // Metà destra = avanti, metà sinistra = indietro.
      handleContentNavigation(touch.x >= centerX);
    }
  }

  runCurrentState();
}


void handleContentNavigation(bool forward) {
  if (currentState == STATE_NEWS && newsCount > 1) {
    if (forward) {
      currentNewsIndex = (currentNewsIndex + 1) % newsCount;
    } else {
      currentNewsIndex = (currentNewsIndex + newsCount - 1) % newsCount;
    }
    updateDisplayNews();
  } else if (currentState == STATE_FOOTBALL_SCORES && totalFootballPages > 1) {
    if (forward) {
      currentFootballPage = (currentFootballPage + 1) % totalFootballPages;
    } else {
      currentFootballPage = (currentFootballPage + totalFootballPages - 1) % totalFootballPages;
    }
    updateDisplayMatches();
  }
}


void handleTouchTransition() {
  currentState = static_cast<State>((currentState + 1) % STATE_MAX);
  blankAllDevice();
  showCurrentStateInfo();
}


void showCurrentStateInfo() {
  switch (currentState) {
    case STATE_METEO:
      updateDisplayMeteo();
      break;
    case STATE_NEWS:
      updateDisplayNews();
      break;
    case STATE_FOOTBALL_SCORES:
      updateDisplayMatches();
      break;
    case STATE_IP:
      updateDisplayIP();
      break;
  }
}


void runCurrentState() {
  switch (currentState) {
    case STATE_METEO:
      if (millis() - lastMeteoCheck > meteoInterval || lastMeteoCheck == 0) {
        fetchMeteoData(0);
        updateDisplayMeteo();
      }
      drawLedColor(100, 50);
      break;
    case STATE_NEWS:
      {
        if (millis() - lastNewsCheck > NEWS_UPDATE_INTERVAL || lastNewsCheck == 0) {
          fetchNewsData();
          updateDisplayNews();
        }
        drawLedColor(100, 50);
        break;
      }
    case STATE_FOOTBALL_SCORES:
      {
        if (millis() - lastFootballCheck > footballInterval || lastFootballCheck == 0) {
          fetchFootballData(0);
          updateDisplayMatches();
        }
        drawLedColor(100, 50);
        break;
      }
    case STATE_IP:
      if (millis() - lastIPCheck > IP_UPDATE_INTERVAL || lastIPCheck == 0) {
        fetchPublicIP();
        updateDisplayIP();
      }
      drawLedColor(100, 50);
      break;
  }

  if (M5.BtnPWR.wasClicked()) {
    bLedOn = !bLedOn;
  }

  if (millis() - lastBatteryCheck > batteryInterval || lastBatteryCheck == 0) {
    // Stack-chan misura la batteria tramite il proprio INA226; M5.Power non è il driver corretto.
    float batteryVoltage = M5StackChan.getBatteryVoltage();
    if (batteryVoltage > 0.0f) {
      float batteryCurrent = M5StackChan.getBatteryCurrent();
      drawBattery(w - (wBattery + 8), 8, batteryVoltageToPercent(batteryVoltage), batteryCurrent < -0.01f);
    } else {
      lastBatteryCheck = millis();
    }
  }
}


void updateDisplay(const char* text) {
  auto& display = M5StackChan.Display();

  display.fillScreen(TFT_BLACK);

  // Imposta parametri grafici
  display.setTextSize(2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextDatum(textdatum_t::middle_center);

  // Gestione delle righe multiple se il testo contiene '\n'
  String str(text);
  int firstNewLine = str.indexOf('\n');

  if (firstNewLine == -1) {
    display.drawString(str, centerX, centerY);
  } else {
    int yOffset = -15;  // Spaziatura iniziale per centrare il blocco
    int startIdx = 0;

    while (true) {
      int nextNewLine = str.indexOf('\n', startIdx);
      String line = "";

      if (nextNewLine == -1) {
        line = str.substring(startIdx);
      } else {
        line = str.substring(startIdx, nextNewLine);
      }

      display.drawString(line, centerX, centerY + yOffset);
      yOffset += 25;  // Distanza verticale tra una riga e la successiva

      if (nextNewLine == -1) break;
      startIdx = nextNewLine + 1;
    }
  }
}


void blankAllDevice() {
  M5.Display.startWrite();
  M5.Display.clear(TFT_BLACK);
  M5.Display.endWrite();

  for (int i = 0; i < nLedNumber; i++) colorList[i] = TFT_BLACK;
}


void drawTitleBar(uint16_t color, const String& text) {
  auto& display = M5StackChan.Display();
  display.fillRect(0, 0, w, hTitleBar, color);
  display.drawFastHLine(0, hTitleBar, w, TFT_BLACK);  // Bordo inferiore della barra
  display.setTextColor(TFT_WHITE, color);
  display.setTextSize(2);
  display.setTextDatum(textdatum_t::middle_center);
  display.drawString(text, w / 2, 17);
}


void drawFooterBar(const String& text) {
  auto& display = M5StackChan.Display();
  display.fillRect(0, h - 15, w, 15, TFT_BLACK);
  display.setTextSize(1);
  display.setTextDatum(textdatum_t::middle_center);
  display.setTextColor(TFT_BLACK);
  display.drawString(text, centerX + 2, (h - 10) + 2);
  display.setTextSize(1);
  display.setTextDatum(textdatum_t::middle_center);
  display.setTextColor(TFT_WHITE);
  display.drawString(text, centerX, h - 10);
}


void drawLedColor(unsigned long intervalMs, uint8_t brightness) {
  if (!bLedOn) {
    for (int led = 0; led < nLedNumber; led++) {
      M5StackChan.setRgbColor(led, 0, 0, 0);
    }
    M5StackChan.refreshRgb();
    return;
  }

  unsigned long currentMillis = millis();

  // Controlla se è passato il tempo necessario
  if (currentMillis - lastLedCheck >= intervalMs) {
    lastLedCheck = currentMillis;  // Aggiorna il timer

    // Prende il colore attuale dalla lista
    uint16_t color = colorList[currentLedIndex];

    // Estrae i componenti R, G, B dal formato RGB565 e applica la luminosità (0 - 255)
    uint8_t r = (((color & 0xF800) >> 8) * brightness) / 255;
    uint8_t g = (((color & 0x07E0) >> 3) * brightness) / 255;
    uint8_t b = (((color & 0x001F) << 3) * brightness) / 255;

    for (int led = 0; led < nLedNumber; led++) {
      M5StackChan.setRgbColor(led, r, g, b);
    }
    M5StackChan.refreshRgb();

    currentLedIndex = (currentLedIndex + 1) % nLedNumber;
  }
}


uint8_t batteryVoltageToPercent(float voltage) {
  // Curva approssimata LiPo 1S: protegge anche da valori INA226 non validi.
  if (voltage <= 3.20f) return 0;
  if (voltage >= 4.20f) return 100;
  return static_cast<uint8_t>((voltage - 3.20f) * 100.0f);
}


void drawBattery(int xBattery, int yBattery, uint8_t percentage, bool isCharging) {
  auto& display = M5StackChan.Display();

  // Limita la percentuale tra 0 e 100 per sicurezza
  if (percentage > 100) percentage = 100;

  // 1. Disegna il bordo esterno del corpo (bianco)
  display.drawRect(xBattery, yBattery, wBattery, hBattery, TFT_WHITE);

  // 2. Disegna il "piolo" positivo a destra (rettangolo pieno bianco)
  display.fillRect(xBattery + wBattery, yBattery + 4, 3, hBattery - 8, TFT_WHITE);

  // 3. Calcola lo spazio interno e l'area di riempimento
  int innerMargin = 2;  // Margine tra bordo e riempimento
  int maxWidthFill = wBattery - (innerMargin * 2);
  int currentFillWidth = (maxWidthFill * percentage) / 100;

  // Colora lo sfondo interno di nero (per azzerare la carica precedente)
  display.fillRect(xBattery + innerMargin, yBattery + innerMargin, maxWidthFill, hBattery - (innerMargin * 2), TFT_BLACK);

  // 4. Determina il colore in base allo stato di carica e alla percentuale
  uint16_t fillColor = TFT_GREEN;
  if (isCharging) {
    fillColor = 0x03E0;  // Verde più scuro (Dark Green in RGB565)
  } else if (percentage < 5) {
    fillColor = TFT_RED;
  } else if (percentage < 20) {
    fillColor = 0xFD20;  // Arancione (Orange in RGB565)
  } else if (percentage < 50) {
    fillColor = TFT_YELLOW;
  }

  // 5. Disegna la parte colorata in base alla percentuale
  if (currentFillWidth > 0) {
    display.fillRect(xBattery + innerMargin, yBattery + innerMargin, currentFillWidth, hBattery - (innerMargin * 2), fillColor);
  }

  // 6. Scrive la percentuale al centro della batteria
  display.setTextDatum(textdatum_t::middle_center);
  display.setTextSize(1);
  display.setTextColor(TFT_BLUE);

  String battText = String(percentage) + "%";
  display.drawString(battText, xBattery + (wBattery / 2), yBattery + (hBattery / 2));

  lastBatteryCheck = millis();
}
