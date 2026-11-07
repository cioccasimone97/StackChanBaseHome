void fetchPublicIP() {
  if (WiFi.status() != WL_CONNECTED) {
    IPDesc = "WiFi disconnesso.";
    return;
  }

  url = "https://api.ipify.org?format=json";
  http.begin(url);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println("payload " + payload);
    /*error = deserializeJson(doc, payload);

    if (!error) {
      currentPublicIP = doc["ip"].as<String>();
    }*/
  } else {
    currentPublicIP = "Errore riconoscimento IP";
  }

  http.end();
  lastIPCheck = millis();

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeBuffer[25];
    strftime(timeBuffer, sizeof(timeBuffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
    lastUpdDateTime = String(timeBuffer);
  }
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

  // Carico i colori nell'array
  colorList.assign(nLedNumber, COLOR_BOX);

  // Parametri di rendering del testo all'interno del box
  display.setTextSize(2);
  display.setTextColor(TFT_BLACK);
  display.setTextDatum(textdatum_t::top_left);

  //display.drawString(IPDesc, centerX + 2, (bodyCenterY - 75) + 2);
  display.drawString("AA BB", centerX + 2, (bodyCenterY - 75) + 2);

  drawFooterBar("Ultimo aggiornamento: " + lastUpdDateTime);
}