void fetchMeteoData(int nTry) {
  if (WiFi.status() != WL_CONNECTED) {
    weatherDesc = "WiFi disconnesso";
    lastMeteoCheck = millis();
    return;
  }

  const String requestUrl = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE, 4) + "&longitude=" + String(LONGITUDE, 4) + "&current=temperature_2m,weather_code" + "&daily=temperature_2m_max,temperature_2m_min" + "&hourly=temperature_2m" + "&forecast_days=1" + "&timezone=auto";
  bool updated = false;
  logHeap("meteo prima");

  // Retry iterativo: la versione ricorsiva riutilizzava HTTPClient prima di http.end().
  for (uint8_t attempt = nTry; attempt < nMaxHTTPRetry && !updated; attempt++) {
    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    // Open-Meteo può usare Transfer-Encoding: chunked; HTTP/1.0 fornisce uno stream JSON continuo.
    http.useHTTP10(true);
    if (!http.begin(requestUrl)) continue;

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      doc.clear();
      DeserializationError error = deserializeJson(doc, http.getStream());
      if (!error) {
        currentTemp = doc["current"]["temperature_2m"];
        int wcode = doc["current"]["weather_code"];
        weatherDesc = getWeatherDescription(wcode);

        tempMax = doc["daily"]["temperature_2m_max"][0];
        tempMin = doc["daily"]["temperature_2m_min"][0];

        JsonArray hourlyArray = doc["hourly"]["temperature_2m"];
        int index = 0;
        for (JsonVariant v : hourlyArray) {
          if (index < 24) hourlyTemps[index++] = v.as<float>();
        }
        updated = true;
      } else {
        weatherDesc = "Errore dati meteo";
        Serial.printf("Meteo JSON error: %s\n", error.c_str());
      }
    } else {
      weatherDesc = "Errore HTTP";
      Serial.printf("Meteo HTTP error: %d\n", httpResponseCode);
    }
    http.end();
  }

  lastMeteoCheck = millis();
  logHeap("meteo dopo");

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeBuffer[25];
    // Formatta la data e l'ora esattamente come richiesto: DD/MM/YYYY HH:MM:SS
    strftime(timeBuffer, sizeof(timeBuffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
    lastUpdDateTime = String(timeBuffer);
  }
}


String getWeatherDescription(int code) {
  switch (code) {
    case 0: return "Cielo sereno";
    case 1: return "Prevalentemente sereno";
    case 2: return "Parzialmente nuvoloso";
    case 3: return "Coperto";
    case 45: return "Nebbia";
    case 48: return "Nebbia con brina";
    case 51: return "Pioviggine leggera";
    case 53: return "Pioviggine moderata";
    case 55: return "Pioviggine densa";
    case 56: return "Pioviggine gelata leggera";
    case 57: return "Pioviggine gelata densa";
    case 61: return "Pioggia leggera";
    case 63: return "Pioggia moderata";
    case 65: return "Pioggia forte";
    case 66: return "Pioggia gelata leggera";
    case 67: return "Pioggia gelata forte";
    case 71: return "Nevicata leggera";
    case 73: return "Nevicata moderata";
    case 75: return "Nevicata forte";
    case 77: return "Granelli di neve";
    case 80: return "Rovesci di pioggia leggeri";
    case 81: return "Rovesci di pioggia moderati";
    case 82: return "Rovesci di pioggia violenti";
    case 85: return "Rovesci di neve leggeri";
    case 86: return "Rovesci di neve forti";
    case 95: return "Temporale";
    case 96: return "Temporale con grandine leggera";
    case 97: return "Temporale forte";
    case 99: return "Temporale con grandine forte";
    default: return "Codice meteo sconosciuto";
  }
}


// Funzione avanzata per disegnare la schermata meteo con uno sfondo a composizione grafica
void updateDisplayMeteo() {
  Serial.println("updateDisplayMeteo");
  auto& display = M5StackChan.Display();

  display.fillScreen(TFT_BLACK);

  // 1. COMPOSIZIONE DELLO SFONDO (Gradiente a bande verticali/orizzontali o pattern)
  uint16_t topColor = 0x2104;
  uint16_t bottomColor = 0x1082;
  uint16_t headerColor = TFT_DARKGREY;

  if (weatherDesc.indexOf("sereno") >= 0 || weatherDesc.indexOf("Sereno") >= 0) {
    // Sfondo cielo: gradiente da azzurro luminoso in alto a blu profondo in basso
    topColor = 0x039F;     // Azzurro cielo
    bottomColor = 0x0115;  // Blu notte/profondo
    headerColor = TFT_BLUE;
  } else if (weatherDesc.indexOf("Pioggia") >= 0 || weatherDesc.indexOf("Pioviggine") >= 0 || weatherDesc.indexOf("Rovesci di pioggia") >= 0) {
    // Sfondo pioggia: toni cupi metallici / grigio-blu con fasce
    topColor = 0x2964;     // Grigio-azzurro cupo
    bottomColor = 0x10A2;  // Blu tempestoso scuro
    headerColor = 0x2492;
  } else if (weatherDesc.indexOf("Temporale") >= 0) {
    // Sfondo temporale: viola elettrico e cupo
    topColor = 0x4810;     // Viola scuro
    bottomColor = 0x1803;  // Nero/Viola profondo
    headerColor = 0x600C;
  } else if (weatherDesc.indexOf("Nevicata") >= 0 || weatherDesc.indexOf("Neve") >= 0) {
    // Sfondo neve: toni ghiaccio / argento
    topColor = 0xDEFB;     // Grigio-azzurro molto chiaro (ghiaccio)
    bottomColor = 0x7BEF;  // Grigio freddo
    headerColor = 0x5AEB;
  } else if (weatherDesc.indexOf("Nuvoloso") >= 0 || weatherDesc.indexOf("Coperto") >= 0) {
    // Sfondo nuvoloso: grigi stratificati
    topColor = 0x528A;     // Grigio nuvola
    bottomColor = 0x2104;  // Grigio scuro
    headerColor = 0x39C4;
  }

  // Carico i colori nell'array
  for (int i = 0; i < nLedNumber; i++) colorList[i] = topColor;


  // Disegno di un gradiente a bande orizzontali fluide (ottimizzato per microcontroller)
  int steps = 25;
  int stripHeight = h / steps;
  for (int i = 0; i < steps; i++) {
    // Calcoliamo il colore intermedio (interpolazione lineare grossolana)
    uint16_t r = (((topColor >> 11) * (steps - i) + (bottomColor >> 11) * i) / steps);
    uint16_t g = ((((topColor >> 5) & 0x3F) * (steps - i) + ((bottomColor >> 5) & 0x3F) * i) / steps);
    uint16_t b = (((topColor & 0x1F) * (steps - i) + (bottomColor & 0x1F) * i) / steps);
    uint16_t blendedColor = (r << 11) | (g << 5) | b;

    display.fillRect(0, i * stripHeight, w, stripHeight + 1, blendedColor);
  }

  // 2. ELEMENTI GRAFICI AGGIUNTIVI (Composizione visiva a tema)
  if (weatherDesc.indexOf("sereno") >= 0 || weatherDesc.indexOf("Sereno") >= 0) {
    // Disegna un cerchio stilizzato "Sole" stilizzato sullo sfondo in alto a destra
    display.fillCircle(w - 30, 70, 22, 0xFFE0);  // Giallo sole
    display.fillCircle(w - 30, 70, 16, 0xFEA0);  // Interno più acceso
  } else if (weatherDesc.indexOf("Pioggia") >= 0) {
    // Aggiungiamo delle "linee di pioggia" diagonali decorative sullo sfondo
    display.setTextColor(0xAD55, topColor);
    display.setTextSize(1);
    for (int x = 20; x < w; x += 40) {
      display.drawString("/ / /", x, 50);
      display.drawString("/ / /", x + 15, 120);
    }
  }

  // 3. BARRA DEL TITOLO IN ALTO
  drawTitleBar(headerColor, "PREVISIONI METEO");

  //3.5 Località
  display.setTextSize(3);
  display.setTextColor(TFT_BLACK);
  display.drawString(locName, centerX + 2, (bodyCenterY - 75) + 2);
  display.setTextColor(TFT_WHITE);
  display.drawString(locName, centerX, bodyCenterY - 75);

  // 4. BOX CENTRALE DATI (Temperatura con effetto ombra per leggibilità)
  String tempStr = String(currentTemp, 1) + " C";

  // Ombra della temperatura
  display.setTextSize(3);
  display.setTextColor(TFT_BLACK);
  display.drawString(tempStr, centerX + 2, (bodyCenterY - 20) + 2);
  // Testo principale della temperatura
  display.setTextColor(TFT_WHITE);
  display.drawString(tempStr, centerX, bodyCenterY - 20);

  // 5. DESCRIZIONE TESTUALE IN BASSO
  display.setTextSize(2);
  display.setTextColor(TFT_YELLOW);
  display.drawString(weatherDesc, centerX, bodyCenterY + 10);

  // 6. DISEGNO DEL GRAFICO ORARIO NELLA PARTE BASSA
  int graphX = 20;
  int graphY = 160;
  int graphW = 280;
  int graphH = 50;

  // Sfondo scuro per il box del grafico
  display.fillRoundRect(graphX - 5, graphY - 5, graphW + 10, graphH + 15, 6, 0x1082);
  display.drawRect(graphX - 5, graphY - 5, graphW + 10, graphH + 15, TFT_DARKGREY);

  // Trova min, max e i rispettivi indici (ore) nelle 24 ore
  float minH = hourlyTemps[0];
  float maxH = hourlyTemps[0];
  int minIndex = 0;
  int maxIndex = 0;

  for (int i = 1; i < 24; i++) {
    if (hourlyTemps[i] < minH) {
      minH = hourlyTemps[i];
      minIndex = i;
    }
    if (hourlyTemps[i] > maxH) {
      maxH = hourlyTemps[i];
      maxIndex = i;
    }
  }
  if (maxH == minH) maxH += 1.0;  // Evita divisioni per zero

  int stepX = graphW / 23;  // Spaziatura tra le 24 ore
  int prevX = -1, prevY = -1;

  for (int i = 0; i < 24; i++) {
    int x = graphX + (i * stepX);
    int hBar = map((int)(hourlyTemps[i] * 10), (int)(minH * 10), (int)(maxH * 10), 5, graphH - 5);
    int y = graphY + graphH - hBar;

    // Disegna la mini barretta verticale
    display.drawFastVLine(x, y, hBar, 0x39C4);

    // Disegna la linea che collega i punti
    if (prevX != -1) {
      display.drawLine(prevX, prevY, x, y, TFT_YELLOW);
    }

    // Gestione colori e dimensioni dei pallini (Evidenziamo Max e Min)
    uint16_t dotColor = TFT_WHITE;
    int dotSize = 2;

    if (i == maxIndex) {
      dotColor = TFT_RED;  // Massimo in Rosso
      dotSize = 3;
    } else if (i == minIndex) {
      dotColor = TFT_BLUE;  // Minimo in Blu
      dotSize = 3;
    }

    // Pallino sul punto
    display.fillCircle(x, y, dotSize, dotColor);

    // Scrivi il valore numerico sopra il massimo e sotto il minimo
    if (i == maxIndex) {
      display.setTextSize(1);
      display.setTextColor(TFT_RED);
      display.setTextDatum(textdatum_t::bottom_center);
      display.drawString(String(hourlyTemps[i], 1), x, y - 5);
    } else if (i == minIndex) {
      display.setTextSize(1);
      display.setTextColor(TFT_BLUE);
      display.setTextDatum(textdatum_t::top_center);
      display.drawString(String(hourlyTemps[i], 1), x, y + 5);
    }

    prevX = x;
    prevY = y;

    // Etichetta delle ore principali (ogni 6 ore)
    if (i % 6 == 0 && i != 0) {
      display.setTextSize(1);
      display.setTextColor(TFT_DARKGREY);
      display.setTextDatum(textdatum_t::top_center);
      String hourStr = String(i) + "h";
      display.drawString(hourStr, x, graphY + graphH + 2);
    }
  }

  // 99. ULTIMO AGGIONRAMENTO
  // display.setTextSize(1);
  // display.setTextColor(TFT_BLACK);
  // display.drawString("Ultimo aggiornamento: " + lastUpdDateTime, centerX + 2, (h - 10) + 2);
  // display.setTextSize(1);
  // display.setTextColor(TFT_WHITE);
  // display.drawString("Ultimo aggiornamento: " + lastUpdDateTime, centerX, h - 10);

  drawFooterBar("Ultimo aggiornamento: " + lastUpdDateTime);
}
