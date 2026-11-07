void fetchPublicIP() {
  if (WiFi.status() != WL_CONNECTED) {
    currentPublicIP = "WiFi disconnesso.";
    return;
  }

  // Usiamo il client standard in chiaro sulla porta 80 (niente Secure, niente crash)
  WiFiClient client;
  // Timeout di sicurezza per non bloccare il codice se il server non risponde
  client.setTimeout(5000);

  if (client.connect("api.ipify.org", 80)) {
    Serial.println("Connesso a ipify (porta 80)");

    // Inviamo la richiesta HTTP in chiaro
    client.println("GET /?format=json HTTP/1.1");
    client.println("Host: api.ipify.org");
    client.println("Connection: close");
    client.println();  // Riga vuota obbligatoria per chiudere gli headers

    // Leggiamo la risposta del server
    String payload = "";
    bool headersEnded = false;

    while (client.connected() || client.available()) {
      String line = client.readStringUntil('\n');
      // Quando troviamo una riga vuota "\r", finiscono gli header HTTP e inizia il corpo (il JSON)
      if (line == "\r") {
        headersEnded = true;
        continue;
      }
      if (headersEnded) {
        payload += line + "\n";
      }
    }
    client.stop();

    // Parsing del JSON ricevuto
    error = deserializeJson(doc, payload);

    if (!error) {
      currentPublicIP = doc["ip"].as<String>();
    } else {
      currentPublicIP = "Errore parsing JSON";
    }

  } else {
    currentPublicIP = "Errore connessione 80";
  }

  lastIPCheck = millis();

  // Aggiornamento ora
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