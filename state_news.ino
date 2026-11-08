void fetchNewsData() {
  if (WiFi.status() != WL_CONNECTED) {
    newsHeadlines[0] = "WiFi disconnesso.";
    newsCount = 1;
    lastNewsCheck = millis();
    return;
  }

  logHeap("news prima");
  http.setConnectTimeout(5000);
  http.setTimeout(8000);
  http.useHTTP10(true);
  http.begin("https://www.ansa.it/sito/ansait_rss.xml");
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    int count = 0;
    bool inItem = false;
    bool inTag = false;
    bool collectingTitle = false;
    String tag;
    String title;
    tag.reserve(32);
    title.reserve(200);
    auto& stream = http.getStream();
    const unsigned long responseStart = millis();

    // Parser XML incrementale: non alloca l'intero RSS in una String temporanea.
    while ((stream.connected() || stream.available()) && millis() - responseStart < 10000 && count < MAX_NEWS) {
      if (!stream.available()) {
        delay(1);
        continue;
      }

      int value = stream.read();
      if (value < 0) continue;
      char character = static_cast<char>(value);

      if (character == '<') {
        inTag = true;
        tag = "";
      } else if (character == '>' && inTag) {
        inTag = false;
        tag.trim();
        if (tag == "item") {
          inItem = true;
        } else if (tag == "/item") {
          inItem = false;
          collectingTitle = false;
        } else if (inItem && tag == "title") {
          collectingTitle = true;
          title = "";
        } else if (inItem && tag == "/title" && collectingTitle) {
          collectingTitle = false;
          title.replace("&amp;", "&");
          title.replace("&quot;", "\"");
          title.replace("&#39;", "'");
          title.replace("&lt;", "<");
          title.replace("&gt;", ">");
          title.replace("]]>", "");
          title.trim();
          if (title.length() > 0) newsHeadlines[count++] = title;
        }
      } else if (inTag) {
        if (tag.length() < 31) tag += character;
      } else if (collectingTitle && title.length() < 200) {
        title += character;
      }
    }
    newsCount = count;
    if (newsCount == 0) {
      newsHeadlines[0] = "Nessuna notizia disponibile.";
      newsCount = 1;
    }
  } else {
    newsHeadlines[0] = "Errore download notizie.";
    newsCount = 1;
  }

  http.end();
  logHeap("news dopo");
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
  for (int i = 0; i < nLedNumber; i++) colorList[i] = COLOR_BOX;

  // Parametri di rendering del testo all'interno del box
  display.setTextSize(2);
  display.setTextColor(TFT_BLACK);
  display.setTextDatum(textdatum_t::top_left);

  int textStartX = 10;
  int textStartY = hTitleBar + 10;
  int maxWidth = w - (textStartX * 2);  // Larghezza massima disponibile in pixel
  int yOffset = 0;
  int lineHeight = 22;  // Spaziatura verticale tra una riga e l'altra

  if (newsCount <= 0) newsCount = 1;
  if (currentNewsIndex >= newsCount) currentNewsIndex = 0;

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
      if (textStartY + yOffset >= h - 15) break;
      display.setCursor(textStartX, textStartY + yOffset);
      display.print(currentLine);

      yOffset += lineHeight;
      currentLine = word;  // Inizia la nuova riga con la parola corrente
    } else {
      currentLine = testLine;
    }
  }

  // L'ultima riga non incontra un wrap e va quindi stampata fuori dal ciclo.
  if (currentLine.length() > 0 && textStartY + yOffset < h - 15) {
    display.setCursor(textStartX, textStartY + yOffset);
    display.print(currentLine);
  }

  drawFooterBar("Tocca display: notizia " + String(currentNewsIndex + 1) + "/" + String(newsCount));
}
