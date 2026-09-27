void fetchNewsData() {
  if (WiFi.status() != WL_CONNECTED) {
    newsHeadlines[0] = "WiFi disconnesso.";
    return;
  }

  url = "https://www.ansa.it/sito/ansait_rss.xml";
  http.begin(url);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String payload = http.getString();
    int count = 0;
    int pos = 0;

    // Scansiona il feed XML cercando i tag <item> e isolando i <title>
    while (count < MAX_NEWS) {
      int itemPos = payload.indexOf("<item>", pos);
      if (itemPos == -1) break;

      int startTag = payload.indexOf("<title>", itemPos);
      if (startTag == -1) break;
      startTag += 7;  // Lunghezza di "<title>"

      int endTag = payload.indexOf("</title>", startTag);
      if (endTag == -1) break;

      String title = payload.substring(startTag, endTag);

      // Pulizia dei caratteri speciali HTML più comuni
      title.replace("&amp;", "&");
      title.replace("&quot;", "\"");
      title.replace("&#39;", "'");
      title.replace("&lt;", "<");
      title.replace("&gt;", ">");
      title.replace("<![CDATA[", "");
      title.replace("]]>", "");
      title.trim();

      newsHeadlines[count] = title;
      count++;

      pos = endTag + 8;
    }
  } else {
    newsHeadlines[0] = "Errore download notizie.";
  }

  http.end();
  lastNewsCheck = millis();
}

void updateDisplayNews() {
  auto& display = M5StackChan.Display();

  display.fillScreen(TFT_BLACK);

  // BARRA DEL TITOLO IN ALTO
  drawTitleBar(TFT_BLACK, "ANSA - ULTIME");

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

  int textStartX = 10;
  int textStartY = hTitleBar + 10;
  int maxWidth = w - textStartX;  // Larghezza massima disponibile in pixel
  int yOffset = 0;
  int lineHeight = 22;  // Spaziatura verticale tra una riga e l'altra

  String newsText = newsHeadlines[currentNewsIndex];
  String currentLine = "";

  // Analizziamo il testo parola per parola per fare il wrap corretto
  int startIdx = 0;
  while (startIdx < newsText.length()) {
    int spaceIdx = newsText.indexOf(' ', startIdx);
    String word = "";

    if (spaceIdx == -1) {
      word = newsText.substring(startIdx);
      startIdx = newsText.length();
    } else {
      word = newsText.substring(startIdx, spaceIdx + 1);  // mantiene lo spazio
      startIdx = spaceIdx + 1;
    }

    // Verifichiamo se aggiungendo questa parola superiamo la larghezza del box
    String testLine = currentLine + word;
    if (display.textWidth(testLine) > maxWidth && currentLine.length() > 0) {
      // Stampiamo la riga corrente
      display.setCursor(textStartX, textStartY + yOffset);
      display.print(currentLine);

      yOffset += lineHeight;
      currentLine = word;  // Inizia la nuova riga con la parola corrente
    } else {
      currentLine = testLine;
    }
  }

  drawFooterBar("Notizia " + String(currentNewsIndex + 1) + "/" + String(MAX_NEWS) + " (Tocca per prox)");
}