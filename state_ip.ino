void fetchPublicIP() {
  if (WiFi.status() != WL_CONNECTED) {
    currentPublicIP = "WiFi disconnesso.";
    return;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  // Usiamo un buffer locale sicuro
  if (http.begin(secureClient, "https://api.ipify.org?format=json")) {
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String payload = http.getString();
      error = deserializeJson(doc, payload);

      if (!error) {
        currentPublicIP = doc["ip"].as<String>();
      } else {
        currentPublicIP = "Errore parsing JSON";
      }
    } else {
      currentPublicIP = "Errore HTTP (" + String(httpResponseCode) + ")";
    }
    http.end();
  } else {
    currentPublicIP = "Errore connessione";
  }

  lastIPCheck = millis();
}

void updateDisplayIP() {
  auto& display = M5StackChan.Display();

  display.fillScreen(TFT_BLACK);

  // BARRA DEL TITOLO IN ALTO
  drawTitleBar(TFT_BLACK, "IP");

  uint16_t COLOR_BOX = display.color565(140, 140, 140);
  uint16_t COLOR_BORDER = display.color565(180, 180, 180);

  display.fillRoundRect(0, hTitleBar, w, h - hTitleBar, 8, COLOR_BOX);
  display.drawRoundRect(0, hTitleBar, w, h - hTitleBar, 8, COLOR_BORDER);

  display.setTextSize(2);
  display.setTextDatum(textdatum_t::top_center);  // Centrato orizzontalmente in alto per ogni riga

  int startY = bodyCenterY - 50;  // Sposta in alto per fare spazio a 4-5 righe
  int stepY = 20;                 // Spaziatura tra una riga e la successiva

  // ---- RIGA 1: IP pubblico (Titolo) ----
  display.setTextColor(TFT_BLACK);  // Ombra
  display.drawString("IP pubblico:", centerX + 1, startY + 1);
  display.setTextColor(TFT_YELLOW);
  display.drawString("IP pubblico:", centerX, startY);

  // ---- RIGA 2: Valore IP pubblico ----
  display.setTextColor(TFT_BLACK);
  display.drawString(currentPublicIP, centerX + 1, startY + stepY + 1);
  display.setTextColor(TFT_WHITE);
  display.drawString(currentPublicIP, centerX, startY + stepY);

  // ---- RIGA 3: IP locale (Titolo) ----
  display.setTextColor(TFT_BLACK);
  display.drawString("IP locale:", centerX + 1, startY + (stepY * 2.5) + 1);
  display.setTextColor(TFT_YELLOW);
  display.drawString("IP locale:", centerX, startY + (stepY * 2.5));

  // ---- RIGA 4: Valore IP locale ----
  display.setTextColor(TFT_BLACK);
  display.drawString(currentLocalIP, centerX + 1, startY + (stepY * 3.5) + 1);
  display.setTextColor(TFT_WHITE);
  display.drawString(currentLocalIP, centerX, startY + (stepY * 3.5));

  drawFooterBar("Ultimo aggiornamento: " + lastUpdDateTime);
}