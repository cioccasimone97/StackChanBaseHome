#include <secrets.h>

#define TFT_DARKGREEN 0x03E0


// --- WI-FI CONFIG ---
const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASS;


// --- WEATHER CONFIG ---
const float LATITUDE = SECRET_LATITUDE;
const float LONGITUDE = SECRET_LONGITUDE;
const String locName = SECRET_LOCNAME;
float hourlyTemps[24];
float tempMin = 0.0;
float tempMax = 0.0;
float currentTemp = 0.0;
String weatherDesc = "Caricamento...";
unsigned long lastMeteoCheck = 0;
const unsigned long meteoInterval = 1UL * 60UL * 1000UL;  // Aggiorna ogni 1 minuto


// --- WEB CONFIG ---
HTTPClient http;
JsonDocument doc;
DeserializationError error;
int nMaxHTTPRetry = 3;


// --- STATE MACHINE DEFINITION ---
enum State {
  STATE_METEO,
  STATE_NEWS,
  STATE_FOOTBALL_SCORES,
  STATE_MAX  // Usato per calcolare quanti stati ci sono
};
State currentState;


// --- GENERIC VARIABLE ---
String lastUpdDateTime = "--/--/---- --:--:--";
String url;
int centerX;
int centerY;
int bodyCenterY;
int w;
int h;
int hTitleBar = 35;
int hBody;
const int nLedNumber = 12;
int currentLedIndex = 0;
unsigned long lastLedCheck = 0;
unsigned long lastBatteryCheck = 0;
std::vector<uint16_t> colorList;
bool bLedOn = true;
int wBattery = 38;                                   // Larghezza del corpo della batteria
int hBattery = 16;                                   // Altezza del corpo della batteria
const unsigned long batteryInterval = 5UL * 1000UL;  // Aggiorna ogni 5 secondi


// --- NEWS CONFIG ---
const int MAX_NEWS = 10;
String newsHeadlines[MAX_NEWS];
int currentNewsIndex = 0;
unsigned long lastNewsCheck = 0;
unsigned long lastFootballCheck = 0;
const unsigned long NEWS_UPDATE_INTERVAL = 1UL * 60UL * 1000UL;  // Aggiorna ogni 1 minuto


// --- FOOTBALL SCORES CONFIG ---
const unsigned long footballInterval = 1UL * 60UL * 1000UL;  // Aggiorna ogni 1 minuto
const char* FOOTBALL_API_KEY = SECRET_FOOTBALL_API_KEY;
const int MAX_MATCHES = 150;
const int MAX_PAGES = 20;
int matchCount = 0;
int totalFootballPages = 0;
int currentFootballPage = 0;
const int MAX_ROWS_PER_PAGE = 8;
struct MatchItem {
  String competition;
  String homeTeam;
  String awayTeam;
  int homeScore;
  int awayScore;
  String status;
  String matchTime;
  String fullUtcDate;
};
MatchItem matches[MAX_MATCHES];
struct DisplayRow {
  bool isHeader;
  String compName;
  MatchItem match;
};
struct Page {
  DisplayRow rows[MAX_ROWS_PER_PAGE];
  int rowCount = 0;
};
Page footballPages[MAX_PAGES];
String footballStatusMsg = "Caricamento...";