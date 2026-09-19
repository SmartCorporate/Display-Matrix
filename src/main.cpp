#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <time.h>  

#define PIN 23

const char* ssid = "XFINITY200MZ-EXT2G";
const char* password = "Xfinity8596!";
const char* serverAddress = "pro-api.coinmarketcap.com"; // Indirizzo del server aggiornato
int port = 443;

const char* apiKey = "aed53cee-04ab-488f-830b-1759dc04ba86"; // La tua chiave API

WiFiClientSecure wifiClient;
HttpClient httpClient = HttpClient(wifiClient, serverAddress, port);

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(160, 8, PIN,
  NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

float bitcoinPrice = 0;
float ethereumPrice = 0;
float previousBitcoinPrice = 0;
float previousEthereumPrice = 0;
uint16_t lastPriceColor = matrix.Color(0, 255, 0);
bool lastIsPriceUp = true;

unsigned long startTime;
unsigned long lastAPICall;

String formatPrice(float price) {
  char buf[50];
  int intPrice = (int)price;
  sprintf(buf, "$%d,%03d", intPrice / 1000, intPrice % 1000);
  return String(buf);
}

void fetchPrice(const char* crypto, float& price, float& previousPrice) {
  Serial.print("Fetching ");
  Serial.print(crypto);
  Serial.println(" price...");

  httpClient.beginRequest();
  httpClient.get(String("/v1/cryptocurrency/quotes/latest?symbol=") + crypto);
  httpClient.sendHeader("X-CMC_PRO_API_KEY", apiKey);
  httpClient.endRequest();

  int statusCode = httpClient.responseStatusCode();
  String response = httpClient.responseBody();

  if (statusCode == 200) {
    Serial.println("Data fetched successfully");
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, response);
    previousPrice = price;
    JsonObject data = doc["data"][crypto];
    price = data["quote"]["USD"]["price"].as<float>();
    Serial.print("Price of ");
    Serial.print(crypto);
    Serial.print(": ");
    Serial.println(price, 2);
  } else {
    Serial.print("Error code: ");
    Serial.println(statusCode);
    Serial.println("Failed to fetch data");
  }
}

void setup() {
  Serial.begin(115200);
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(25);

  configTime(0, 0, "pool.ntp.org");  
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);  
  tzset();

  startTime = millis();
  lastAPICall = 0;

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }

  wifiClient.setInsecure();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  matrix.fillScreen(0);
  matrix.setCursor((matrix.width() - 6 * 12) / 2, 0);
  matrix.setTextColor(matrix.Color(255, 255, 255));
  matrix.print("WiFi ");
  matrix.show();
  delay(500);
   matrix.print(">");
  matrix.show();
  delay(200); 
   matrix.print(">");
  matrix.show();
  delay(200); 
   matrix.print(">");
  matrix.show();
  delay(200); 
   matrix.print(" Connected !");
  matrix.show();
  delay(600);  // Display WiFi connected message for 2 seconds

  // First API calls to fetch prices
  fetchPrice("BTC", bitcoinPrice, previousBitcoinPrice);
  fetchPrice("ETH", ethereumPrice, previousEthereumPrice);
  lastAPICall = millis();
}

void drawTriangle(int x, int y, bool isUp, uint16_t color) {
  if (isUp) {
    matrix.fillTriangle(x - 3, y + 3, x + 3, y + 3, x, y - 3, color);
  } else {
    matrix.fillTriangle(x - 3, y - 3, x + 3, y - 3, x, y + 3, color);
  }
}

void scrollText(const char* crypto, float price, float previousPrice, uint16_t cryptoColor) {
  String priceStr = formatPrice(price);

  uint16_t priceColor;
  bool isPriceUp;
  if (price > previousPrice) {
    priceColor = matrix.Color(0, 255, 0);
    isPriceUp = true;
  } else if (price < previousPrice) {
    priceColor = matrix.Color(255, 0, 0);
    isPriceUp = false;
  } else {
    priceColor = lastPriceColor;
    isPriceUp = lastIsPriceUp;
  }

  lastPriceColor = priceColor;
  lastIsPriceUp = isPriceUp;

  int textLength = 6 * (3 + priceStr.length() + strlen(crypto)) + 10;
  for (int x = matrix.width(); x >= -textLength; --x) {
    matrix.fillScreen(0);
    matrix.setCursor(x + 10, 0);
    matrix.setTextColor(cryptoColor);
    matrix.print(crypto);
    matrix.setTextColor(priceColor);
    matrix.print(" " + priceStr + " ");
    drawTriangle(matrix.getCursorX(), 4, isPriceUp, priceColor);
    matrix.show();
    if (x == (matrix.width() - textLength) / 2) {
      delay(10); // Tempo pausa prezzo sul display
    }
    delay(1);
  }
}

void displayDateTime() {
  char buf[50];
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    matrix.fillScreen(0);
    matrix.setCursor((matrix.width() - strlen("Time Sync Error") * 6) / 2, 0);
    matrix.setTextColor(matrix.Color(255, 0, 0));
    matrix.print("Time Sync Error");
    matrix.show();
    delay(1000);
    return;
  }

  strftime(buf, sizeof(buf), "%I:%M%p   %d %b %Y", &timeinfo);
  matrix.fillScreen(0);
  matrix.setCursor((matrix.width() - strlen(buf) * 6) / 2, 0);
  matrix.setTextColor(matrix.Color(255, 0, 0));
  matrix.print(buf);
  matrix.show();
  delay(3000); // Tempo di visualizzazione dell'ora
  matrix.print(" >");
  matrix.show();
  delay(300); 
  
}

void loop() {
  unsigned long currentTime = millis();

  // Verifica se sono trascorsi 10 minuti dall'ultima chiamata API
  if (currentTime - lastAPICall >= 1000000) { // 1000000 ms = 16.6 minuti
    fetchPrice("BTC", bitcoinPrice, previousBitcoinPrice);
    fetchPrice("ETH", ethereumPrice, previousEthereumPrice);
    lastAPICall = currentTime;
  }

  // Visualizza alternativamente il prezzo e l'ora
  scrollText("Bitcoin BTC", bitcoinPrice, previousBitcoinPrice, matrix.Color(255, 140, 0));
  delay(300); // Attendi 0.3 secondi prima di cambiare visualizzazione
  scrollText("Ethereum ETH", ethereumPrice, previousEthereumPrice, matrix.Color(0, 127, 255));
  delay(300); // Attendi 0.3 secondi prima di cambiare visualizzazione
  displayDateTime();
  delay(200); // Attendi 0.2 secondo prima di ripetere il ciclo

  // Riavvia il dispositivo ogni 2 ore per evitare problemi di stabilità
  if (currentTime - startTime > 7200000) { // 7200000 ms = 2 ore
    esp_restart();
  }
}
