void buildFootballPages() {
  totalFootballPages = 0;
  if (matchCount == 0) return;

  int pageIdx = 0;
  footballPages[pageIdx].rowCount = 0;

  String currentComp = "";

  for (int i = 0; i < matchCount; i++) {
    bool needsHeader = (matches[i].competition != currentComp);
    int rowsNeeded = needsHeader ? 2 : 1;  // Serve 1 riga per header + 1 riga per partita, oppure solo 1 riga partita

    // Se non c'è spazio sufficiente in questa pagina, passa alla successiva
    if (footballPages[pageIdx].rowCount + rowsNeeded > MAX_ROWS_PER_PAGE) {
      pageIdx++;
      if (pageIdx >= MAX_PAGES) break;
      footballPages[pageIdx].rowCount = 0;
    }

    // Aggiungi Header se necessario
    if (needsHeader) {
      currentComp = matches[i].competition;
      int r = footballPages[pageIdx].rowCount;
      footballPages[pageIdx].rows[r].isHeader = true;
      footballPages[pageIdx].rows[r].compName = currentComp;
      footballPages[pageIdx].rowCount++;
    }

    // Aggiungi Partita
    int r = footballPages[pageIdx].rowCount;
    footballPages[pageIdx].rows[r].isHeader = false;
    footballPages[pageIdx].rows[r].match = matches[i];
    footballPages[pageIdx].rowCount++;
  }

  totalFootballPages = pageIdx + 1;
  currentFootballPage = 0;
}

void fetchFootballData(int nTry) {
  if (WiFi.status() != WL_CONNECTED) {
    footballStatusMsg = "WiFi disconnesso";
    return;
  }

  nTry++;
  url = "https://api.football-data.org/v4/matches";

  WiFiClientSecure client;
  client.setInsecure();

  if (http.begin(client, url)) {
    http.setTimeout(10000);  // 10 secondi timeout
    http.addHeader("User-Agent", "ESP32-M5Stack/1.0");
    http.addHeader("X-Auth-Token", FOOTBALL_API_KEY);

    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
      String payload = http.getString();

      error = deserializeJson(doc, payload);

      if (!error) {
        matchCount = 0;
        JsonArray matchesArray = doc["matches"].as<JsonArray>();

        for (JsonObject match : matchesArray) {
          if (matchCount >= MAX_MATCHES) break;

          matches[matchCount].competition = match["competition"]["name"] | "Altre";

          String home = match["homeTeam"]["shortName"] | match["homeTeam"]["name"].as<const char*>();
          String away = match["awayTeam"]["shortName"] | match["awayTeam"]["name"].as<const char*>();

          if (home.length() > 11) home = home.substring(0, 10) + ".";
          if (away.length() > 11) away = away.substring(0, 10) + ".";

          matches[matchCount].homeTeam = home;
          matches[matchCount].awayTeam = away;

          matches[matchCount].homeScore = match["score"]["fullTime"]["home"].isNull() ? -1 : match["score"]["fullTime"]["home"].as<int>();
          matches[matchCount].awayScore = match["score"]["fullTime"]["away"].isNull() ? -1 : match["score"]["fullTime"]["away"].as<int>();

          matches[matchCount].status = match["status"] | "TIMED";

          // Data/ora ISO per ordinamento (es: "2026-09-21T20:45:00Z")
          String utcDate = match["utcDate"] | "";
          matches[matchCount].fullUtcDate = utcDate;

          if (utcDate.length() >= 16) {
            matches[matchCount].matchTime = utcDate.substring(11, 16);  // "HH:MM"
          } else {
            matches[matchCount].matchTime = "--:--";
          }

          matchCount++;
        }

        // Ordinamento DECRESCENTE per data/ora di inizio
        if (matchCount > 1) {
          std::sort(matches, matches + matchCount, [](const MatchItem& a, const MatchItem& b) {
            return a.fullUtcDate > b.fullUtcDate;
          });
        }

        // Costruisce le pagine bilanciando righe header e righe partite
        buildFootballPages();

        footballStatusMsg = "Partite aggiornate";

        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          char timeBuffer[25];
          strftime(timeBuffer, sizeof(timeBuffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
          lastUpdDateTime = String(timeBuffer);
        }
      } else {
        footballStatusMsg = "Errore JSON";
      }

    } else {
      footballStatusMsg = "Errore HTTP " + String(httpResponseCode);
    }
    http.end();
  } else {
    footballStatusMsg = "Errore Connessione";
  }

  lastFootballCheck = millis();
}


void updateDisplayMatches() {
  auto& display = M5StackChan.Display();
  display.fillScreen(TFT_BLACK);

  drawTitleBar(TFT_DARKGREEN, "PARTITE DI OGGI");
  colorList.assign(nLedNumber, TFT_DARKGREEN);

  if (matchCount == 0 || totalFootballPages == 0) {
    display.setTextSize(2);
    display.setTextColor(TFT_YELLOW);
    display.setTextDatum(textdatum_t::middle_center);
    display.drawString(footballStatusMsg, centerX, 120);
    drawFooterBar("Ultimo agg: " + lastUpdDateTime);
    return;
  }

  // Assicura che l'indice di pagina sia entro i limiti
  if (currentFootballPage >= totalFootballPages) {
    currentFootballPage = 0;
  }

  int startY = 32;
  int rowHeight = 20;
  int currentY = startY;

  Page& activePage = footballPages[currentFootballPage];

  // Renderizza esattamente gli elementi pre-calcolati per la pagina attiva
  for (int r = 0; r < activePage.rowCount; r++) {
    DisplayRow& row = activePage.rows[r];

    if (row.isHeader) {
      // Intestazione Competizione
      display.fillRect(0, currentY, w, rowHeight, 0x39C4);  // Sfondo intestazione
      display.setTextSize(1);
      display.setTextColor(TFT_YELLOW);
      display.setTextDatum(textdatum_t::middle_center);
      display.drawString("-- " + row.compName + " --", centerX, currentY + (rowHeight / 2));
    } else {
      // Riga Partita (sfondo alternato)
      if (r % 2 == 0) {
        display.fillRect(0, currentY, w, rowHeight, 0x1082);
      }

      display.setTextSize(1);

      // Casa
      display.setTextColor(TFT_WHITE);
      display.setTextDatum(textdatum_t::middle_left);
      display.drawString(row.match.homeTeam, 10, currentY + (rowHeight / 2));

      // Risultato / Ora
      String scoreOrTime = "";
      uint16_t statusColor = TFT_LIGHTGREY;

      if (row.match.status == "IN_PLAY" || row.match.status == "PAUSED") {
        scoreOrTime = String(row.match.homeScore) + " - " + String(row.match.awayScore);
        statusColor = TFT_GREEN;
      } else if (row.match.status == "FINISHED") {
        scoreOrTime = String(row.match.homeScore) + " - " + String(row.match.awayScore);
        statusColor = TFT_WHITE;
      } else {
        scoreOrTime = row.match.matchTime;
        statusColor = TFT_CYAN;
      }

      display.setTextColor(statusColor);
      display.setTextDatum(textdatum_t::middle_center);
      display.drawString(scoreOrTime, centerX, currentY + (rowHeight / 2));

      // Ospite
      display.setTextColor(TFT_WHITE);
      display.setTextDatum(textdatum_t::middle_right);
      display.drawString(row.match.awayTeam, w - 10, currentY + (rowHeight / 2));
    }

    currentY += rowHeight;
  }

  drawFooterBar("Pag " + String(currentFootballPage + 1) + "/" + String(totalFootballPages) + " - " + lastUpdDateTime);
}