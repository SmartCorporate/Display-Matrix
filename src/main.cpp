#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <time.h>

#define MATRIX_PIN 23
#define CHART_WIDTH 48

const char* PRICE_HOST = "api.coingecko.com";
const char* DEVICE_NAME = "ESP32WiFiDisplay";
const char* HOSTNAME = "ESP32WiFiDisplay";

WiFiClientSecure secureClient;
HttpClient httpClient(secureClient, PRICE_HOST, 443);
WebServer server(80);
Preferences preferences;
Adafruit_NeoMatrix matrix(160, 8, MATRIX_PIN,
  NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

struct Settings {
  String message = "";
  String mode = "all";
  uint8_t brightness = 25;
  uint16_t speed = 12;
} settings;

float btc = 0, eth = 0, eurUsd = 0, previousBtc = 0, previousEth = 0, previousEurUsd = 0;
float btcChange24h = 0, ethChange24h = 0;
float knownBtcAth = 0, knownEthAth = 0;
float btcHistory[24] = {0};
float ethHistory[24] = {0};
uint8_t btcHistoryCount = 0;
uint8_t ethHistoryCount = 0;
uint16_t lastPriceColor;
bool lastPriceUp = true;
uint16_t lastForexColor;
bool lastForexUp = true;
unsigned long lastApiCall = 0;
unsigned long bootTime = 0;
unsigned long lastSuccessfulPriceUpdate = 0;
String currentActivity = "Starting";
String lastError = "None";
int priceHttpStatus = 0;
int priceHistoryHttpStatus = 0;
bool btcAthPending = false;
bool ethAthPending = false;

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en-US"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Matrix Display WiFi</title><style>
:root{--bg:#f4f7f9;--panel:#ffffff;--line:#cbd3d9;--text:#172027;--muted:#5f6d76;--green:#16875d;--orange:#f28b24;--red:#c7353c}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 system-ui,sans-serif}header{border-bottom:1px solid var(--line);padding:20px}main{width:min(1040px,calc(100% - 32px));margin:18px auto 50px}h1{font-size:22px;margin:0 0 4px}.status,.hint{color:var(--muted)}.dot{display:inline-block;width:9px;height:9px;border-radius:50%;background:var(--green);margin-right:7px}.dot.offline{background:var(--red)}section{padding:22px 0;border-bottom:1px solid var(--line)}h2{font-size:16px;margin:0 0 14px}label{display:block;color:var(--muted);margin:13px 0 6px}textarea,select,input[type=number]{width:100%;border:1px solid var(--line);background:var(--panel);color:var(--text);padding:11px;border-radius:6px;font:inherit}textarea{min-height:92px;resize:vertical}.row{display:grid;grid-template-columns:1fr 1fr;gap:16px}.range{display:flex;align-items:center;gap:12px}.range input{width:100%}button{border:0;border-radius:6px;padding:10px 15px;background:var(--orange);color:#17120d;font-weight:700;cursor:pointer}button.secondary{background:#30363b;color:#fff}button.danger{background:#e5484d;color:#fff}.actions{display:flex;gap:10px;margin-top:18px;flex-wrap:wrap}.hint{font-size:13px}.ok{color:var(--green);min-height:22px;margin-top:10px}.preview-section{padding-top:10px}.preview{width:100%;height:auto;background:#020303;border:1px solid #293238;display:block}.debug-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:1px;background:var(--line);border:1px solid var(--line)}.debug-item{background:var(--panel);padding:12px}.debug-item span{display:block;color:var(--muted);font-size:12px}.debug-item strong{display:block;margin-top:3px;overflow-wrap:anywhere}.problem{margin-top:12px;padding:12px;border-left:4px solid var(--green);background:var(--panel)}.problem.error{border-color:var(--red);color:var(--red)}code{background:var(--panel);padding:2px 5px;border-radius:4px}@media(max-width:560px){.row,.debug-grid{grid-template-columns:1fr}}
</style></head><body><header><h1>Matrix Display WiFi</h1><div class="status"><span class="dot"></span><span id="device">Connecting...</span></div></header><main>
<section class="preview-section"><h2>Live Display Preview</h2><canvas id="preview" class="preview" width="1280" height="64"></canvas></section>
<section><h2>Display content</h2><label for="message">Extra message</label><textarea id="message" maxlength="240" placeholder="Leave empty to skip it"></textarea><div class="row"><div><label for="mode">Display mode</label><select id="mode"><option value="all">Prices, extra message, and clock</option><option value="message">Extra message only</option><option value="prices">Prices only</option><option value="clock">Date and time only</option></select></div><div><label for="brightness">Brightness: <span id="brightnessValue"></span></label><div class="range"><input id="brightness" type="range" min="1" max="255"></div></div></div><label for="speed">Scroll speed (milliseconds)</label><input id="speed" type="number" min="1" max="100"><div class="actions"><button onclick="save()">Save to display</button><button class="danger" onclick="restart()">Force reboot</button></div><div id="result" class="ok"></div></section>
<section><h2>Device Status &amp; Debug</h2><div class="debug-grid"><div class="debug-item"><span>Current activity</span><strong id="activity">Loading...</strong></div><div class="debug-item"><span>Wi-Fi signal</span><strong id="wifi">Loading...</strong></div><div class="debug-item"><span>Uptime</span><strong id="uptime">Loading...</strong></div><div class="debug-item"><span>Free memory</span><strong id="memory">Loading...</strong></div><div class="debug-item"><span>Last price update</span><strong id="priceUpdate">Loading...</strong></div><div class="debug-item"><span>API responses</span><strong id="apis">Loading...</strong></div></div><div id="problem" class="problem">No problems reported.</div></section>
<section><h2>Wi-Fi Setup</h2><p class="hint">Use this when moving the display to a different Wi-Fi network. The ESP32 will restart and open a setup network named <code>ESP32WiFiDisplay</code>.</p><div class="actions"><button class="secondary" onclick="changeWifi()">Change Wi-Fi network</button></div><div id="wifiResult" class="ok"></div></section>
<section><h2>Request a Codex change</h2><p class="hint">Describe a firmware change, then paste the prepared request into this project's Codex task.</p><textarea id="codex" placeholder="Example: add a color picker for the custom message"></textarea><div class="actions"><button class="secondary" onclick="copyRequest()">Copy request</button></div><div id="copied" class="ok"></div></section>
<section><h2>Updates</h2><p class="hint">OTA is active at <code>ESP32WiFiDisplay.local</code>. Future firmware updates can be sent over Wi-Fi.</p></section></main><script>
const $=id=>document.getElementById(id);let latest={btc:0,eth:0,eurUsd:0,btcChange24h:0,ethChange24h:0};let previewX=160,lastPreview=0;const previewBuffer=document.createElement('canvas');previewBuffer.width=160;previewBuffer.height=8;$('brightness').oninput=()=>{$('brightnessValue').textContent=$('brightness').value};
async function load(){const s=await(await fetch('/api/settings')).json();for(const k of ['message','mode','brightness','speed'])$(k).value=s[k];$('brightnessValue').textContent=s.brightness;$('device').textContent=s.hostname+' - '+s.ip}
function duration(seconds){if(seconds<60)return seconds+' sec';if(seconds<3600)return Math.floor(seconds/60)+' min '+seconds%60+' sec';return Math.floor(seconds/3600)+' hr '+Math.floor(seconds%3600/60)+' min'}
async function loadStatus(){try{const s=await(await fetch('/api/status',{cache:'no-store'})).json();latest=s;document.querySelector('.dot').classList.toggle('offline',!s.wifiConnected);$('device').textContent=s.hostname+' - '+s.ip;$('activity').textContent=s.activity;$('wifi').textContent=s.wifiConnected?s.rssi+' dBm':'Disconnected';$('uptime').textContent=duration(s.uptimeSeconds);$('memory').textContent=Math.round(s.freeHeap/1024)+' KB';$('priceUpdate').textContent=s.priceAgeSeconds<0?'Not completed yet':duration(s.priceAgeSeconds)+' ago';$('apis').textContent='Prices '+s.priceApi+' | BTC/ETH charts '+s.historyApi;const problem=$('problem');problem.textContent=s.lastError==='None'?'No problems reported.':s.lastError;problem.classList.toggle('error',s.lastError!=='None')}catch(e){document.querySelector('.dot').classList.add('offline');$('device').textContent='ESP32 is unavailable';$('activity').textContent='Connection lost';$('problem').textContent='The web page cannot reach the ESP32.';$('problem').classList.add('error')}}
function signed(v){return(v>=0?'+':'')+Number(v||0).toFixed(1)+'% 24h'}
function previewRuns(){const mode=$('mode').value,msg=$('message').value.trim(),btcColor=latest.btcChange24h>=0?'#00ff38':'#ff3030',ethColor=latest.ethChange24h>=0?'#00ff38':'#ff3030',clock=new Date().toLocaleTimeString('en-US',{hour:'numeric',minute:'2-digit'});if(mode==='message')return[[msg||'EXTRA MESSAGE EMPTY','#ffffff']];if(mode==='clock')return[[clock+'   ','#ff3030']];const runs=[['Bitcoin BTC ','#ff8c00'],['$'+Math.round(latest.btc||0).toLocaleString('en-US')+' '+signed(latest.btcChange24h)+'   ',btcColor],['Ethereum ETH ','#008cff'],['$'+Math.round(latest.eth||0).toLocaleString('en-US')+' '+signed(latest.ethChange24h)+'   ',ethColor],['EUR/USD ','#ffffff'],[Number(latest.eurUsd||0).toFixed(4)+'   ','#00ff38']];if(mode==='all'&&msg)runs.push([msg+'   ','#ffffff']);if(mode==='all')runs.push([clock+'   ','#ff3030']);return runs}
function drawPreview(now){const c=$('preview'),x=c.getContext('2d'),b=previewBuffer.getContext('2d'),runs=previewRuns(),speed=Math.max(1,+$('speed').value||12);b.clearRect(0,0,160,8);b.font='bold 8px monospace';b.textBaseline='top';let cursor=previewX;for(const run of runs){b.fillStyle=run[1];b.fillText(run[0],cursor,-1);cursor+=b.measureText(run[0]).width}x.fillStyle='#020303';x.fillRect(0,0,1280,64);const pixels=b.getImageData(0,0,160,8).data,level=.3+.7*(+$('brightness').value||25)/255;for(let py=0;py<8;py++)for(let px=0;px<160;px++){const i=(py*160+px)*4,lit=pixels[i+3]>72;x.beginPath();x.arc(px*8+4,py*8+4,lit?2.8:1,0,Math.PI*2);x.fillStyle=lit?'rgba('+pixels[i]+','+pixels[i+1]+','+pixels[i+2]+','+level+')':'#101516';x.fill()}if(now-lastPreview>=speed){previewX--;lastPreview=now}if(cursor<0)previewX=160;requestAnimationFrame(drawPreview)}
async function save(){const b={message:$('message').value,mode:$('mode').value,brightness:+$('brightness').value,speed:+$('speed').value};$('result').textContent='Saving...';try{const r=await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});const reply=await r.json();if(!r.ok||!reply.ok)throw new Error();$('result').textContent=b.message.length?'Extra message loaded successfully.':'Extra message cleared successfully.'}catch(e){$('result').textContent='Extra message could not be loaded. Please try again.'}}
async function restart(){if(confirm('Force ESP32WiFiDisplay to reboot now?')){await fetch('/api/restart',{method:'POST'});$('result').textContent='ESP32 is rebooting now...'}}
async function changeWifi(){if(confirm('Clear the saved Wi-Fi network and start Wi-Fi setup?')){const r=await fetch('/api/wifi/reset',{method:'POST'});if(r.ok){$('wifiResult').textContent='Wi-Fi setup is starting. Connect to ESP32WiFiDisplay and follow the setup page.'}else $('wifiResult').textContent='Could not start Wi-Fi setup.'}}
async function copyRequest(){const t='In the Display Matrix LED project, update the ESP32 firmware. Device: ESP32WiFiDisplay at ESP32WiFiDisplay.local. Request: '+$('codex').value;await navigator.clipboard.writeText(t);$('copied').textContent='Request copied to the clipboard.'}load().catch(()=>$('device').textContent='ESP32 is unavailable');loadStatus();setInterval(loadStatus,2000);requestAnimationFrame(drawPreview);
</script></body></html>)HTML";

void drawCenteredMessage(const char* text, uint16_t color) {
  int16_t x1, y1;
  uint16_t width, height;
  matrix.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
  matrix.fillScreen(0);
  matrix.setCursor((matrix.width() - width) / 2, 0);
  matrix.setTextColor(color);
  matrix.print(text);
  matrix.show();
}

void waitForWiFiConnection() {
  if (WiFi.status() == WL_CONNECTED) return;

  currentActivity = "Waiting for Wi-Fi";
  lastError = "Wi-Fi connection lost. Reconnecting automatically.";
  bool messageVisible = false;
  unsigned long lastBlink = 0;
  unsigned long lastReconnectAttempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastBlink >= 500) {
      messageVisible = !messageVisible;
      matrix.fillScreen(0);
      if (messageVisible) drawCenteredMessage("NO WIFI", matrix.Color(255, 0, 0));
      else matrix.show();
      lastBlink = now;
    }
    if (now - lastReconnectAttempt >= 10000) {
      WiFi.reconnect();
      lastReconnectAttempt = now;
    }
    delay(20);
  }
  currentActivity = "Wi-Fi reconnected";
  lastError = "None";
  matrix.fillScreen(0);
  matrix.show();
}

void serviceNetwork() {
  waitForWiFiConnection();
  server.handleClient();
  ArduinoOTA.handle();
  delay(0);
}

void loadSettings() {
  preferences.begin("matrix", true);
  settings.message = preferences.getString("message", settings.message);
  settings.mode = preferences.getString("mode", settings.mode);
  settings.brightness = preferences.getUChar("brightness", settings.brightness);
  settings.speed = preferences.getUShort("speed", settings.speed);
  knownBtcAth = preferences.getFloat("btcAth", 0);
  knownEthAth = preferences.getFloat("ethAth", 0);
  preferences.end();
}

void saveKnownAth(const char* key, float value) {
  preferences.begin("matrix", false);
  preferences.putFloat(key, value);
  preferences.end();
}

void saveSettings() {
  preferences.begin("matrix", false);
  preferences.putString("message", settings.message);
  preferences.putString("mode", settings.mode);
  preferences.putUChar("brightness", settings.brightness);
  preferences.putUShort("speed", settings.speed);
  preferences.end();
}

String formatPrice(float price) {
  char buffer[32];
  int value = static_cast<int>(price);
  snprintf(buffer, sizeof(buffer), "$%d,%03d", value / 1000, value % 1000);
  return String(buffer);
}

void fetchPrices() {
  currentActivity = "Updating prices";
  httpClient.beginRequest();
  httpClient.get("/api/v3/simple/price?ids=bitcoin,ethereum&vs_currencies=usd,eur");
  httpClient.sendHeader("User-Agent", "ESP32WiFiDisplay/1.0");
  httpClient.endRequest();
  int status = httpClient.responseStatusCode();
  priceHttpStatus = status;
  String body = httpClient.responseBody();
  if (status != 200) {
    Serial.printf("Price service error: %d\n", status);
    lastError = "Price API request failed with HTTP " + String(status) + ".";
    httpClient.stop();
    return;
  }
  DynamicJsonDocument json(512);
  DeserializationError jsonError = deserializeJson(json, body);
  if (!jsonError) {
    previousBtc = btc;
    previousEth = eth;
    btc = json["bitcoin"]["usd"].as<float>();
    eth = json["ethereum"]["usd"].as<float>();
    float btcEur = json["bitcoin"]["eur"].as<float>();
    if (btcEur > 0) {
      previousEurUsd = eurUsd;
      eurUsd = btc / btcEur;
    }
    lastSuccessfulPriceUpdate = millis();
    lastError = "None";
  } else {
    lastError = "Could not read the price API response.";
  }
  httpClient.stop();
}

void copyLatestHistory(JsonArray prices, float* destination, uint8_t& count) {
  size_t total = prices.size();
  size_t start = total > 24 ? total - 24 : 0;
  count = 0;
  for (size_t index = start; index < total && count < 24; ++index) {
    destination[count++] = prices[index].as<float>();
  }
}

void fetchPriceHistory() {
  currentActivity = "Updating BTC and ETH charts";
  httpClient.beginRequest();
  httpClient.get("/api/v3/coins/markets?vs_currency=usd&ids=bitcoin%2Cethereum&sparkline=true");
  httpClient.sendHeader("User-Agent", "ESP32WiFiDisplay/1.0");
  httpClient.endRequest();
  int status = httpClient.responseStatusCode();
  priceHistoryHttpStatus = status;
  String body = httpClient.responseBody();
  if (status != 200) {
    Serial.printf("Price history error: %d\n", status);
    lastError = "Chart API request failed with HTTP " + String(status) + ".";
    httpClient.stop();
    return;
  }

  DynamicJsonDocument json(24576);
  DeserializationError jsonError = deserializeJson(json, body);
  if (!jsonError) {
    for (JsonObject coin : json.as<JsonArray>()) {
      String id = coin["id"].as<String>();
      JsonArray history = coin["sparkline_in_7d"]["price"].as<JsonArray>();
      float reportedAth = coin["ath"].as<float>();
      float change24h = coin["price_change_percentage_24h"].as<float>();
      if (id == "bitcoin") {
        copyLatestHistory(history, btcHistory, btcHistoryCount);
        btcChange24h = change24h;
        if (knownBtcAth <= 0) {
          knownBtcAth = reportedAth;
          saveKnownAth("btcAth", knownBtcAth);
        } else if (reportedAth > knownBtcAth) {
          knownBtcAth = reportedAth;
          btcAthPending = true;
          saveKnownAth("btcAth", knownBtcAth);
        }
      } else if (id == "ethereum") {
        copyLatestHistory(history, ethHistory, ethHistoryCount);
        ethChange24h = change24h;
        if (knownEthAth <= 0) {
          knownEthAth = reportedAth;
          saveKnownAth("ethAth", knownEthAth);
        } else if (reportedAth > knownEthAth) {
          knownEthAth = reportedAth;
          ethAthPending = true;
          saveKnownAth("ethAth", knownEthAth);
        }
      }
    }
  } else {
    lastError = "Could not read the BTC and ETH chart response.";
  }
  httpClient.stop();
}

void scrollText(const String& text, uint16_t color, const char* activity = "Showing extra message") {
  currentActivity = activity;
  int width = text.length() * 6;
  for (int x = matrix.width(); x >= -width; --x) {
    matrix.fillScreen(0);
    matrix.setCursor(x, 0);
    matrix.setTextColor(color);
    matrix.print(text);
    matrix.show();
    unsigned long started = millis();
    while (millis() - started < settings.speed) serviceNetwork();
  }
}

void drawTriangle(int x, bool up, uint16_t color) {
  if (up) matrix.fillTriangle(x - 3, 7, x + 3, 7, x, 1, color);
  else matrix.fillTriangle(x - 3, 1, x + 3, 1, x, 7, color);
}

void scrollPrice(const char* label, float price, float previous, uint16_t labelColor) {
  String value = formatPrice(price);
  bool up = price >= previous;
  uint16_t priceColor = price == previous ? lastPriceColor : matrix.Color(up ? 0 : 255, up ? 255 : 0, 0);
  if (price != previous) { lastPriceColor = priceColor; lastPriceUp = up; }
  else up = lastPriceUp;
  int width = 6 * (strlen(label) + value.length() + 2) + 10;
  for (int x = matrix.width(); x >= -width; --x) {
    matrix.fillScreen(0);
    matrix.setCursor(x, 0);
    matrix.setTextColor(labelColor);
    matrix.print(label);
    matrix.print(" ");
    matrix.setTextColor(priceColor);
    matrix.print(value);
    matrix.print(" ");
    drawTriangle(matrix.getCursorX(), up, priceColor);
    matrix.show();
    unsigned long started = millis();
    while (millis() - started < settings.speed) serviceNetwork();
  }
}

void drawHistoryChart(const float* history, uint8_t count, int startX) {
  uint16_t axisColor = matrix.Color(255, 255, 255);
  matrix.drawLine(startX, 0, startX, 7, axisColor);
  matrix.drawLine(startX, 7, startX + CHART_WIDTH - 1, 7, axisColor);
  if (count < 2) return;

  float minimum = history[0];
  float maximum = history[0];
  for (uint8_t index = 1; index < count; ++index) {
    minimum = min(minimum, history[index]);
    maximum = max(maximum, history[index]);
  }
  float range = maximum - minimum;
  if (range < 0.0000001f) range = 1.0f;
  int previousX = startX + 1;
  int previousY = 6 - round((history[0] - minimum) * 6.0f / range);
  uint16_t firstColor = history[1] >= history[0]
      ? matrix.Color(0, 255, 0)
      : matrix.Color(255, 0, 0);
  matrix.drawPixel(previousX, previousY, firstColor);
  for (uint8_t index = 1; index < count; ++index) {
    int x = startX + 1 + round(index * (CHART_WIDTH - 2.0f) / (count - 1));
    int y = 6 - round((history[index] - minimum) * 6.0f / range);
    uint16_t segmentColor = history[index] >= history[index - 1]
        ? matrix.Color(0, 255, 0)
        : matrix.Color(255, 0, 0);
    matrix.drawLine(previousX, previousY, x, y, segmentColor);
    previousX = x;
    previousY = y;
  }
}

void showPriceWithChart(const char* symbol, float price, float previous,
                        float change24h, const float* history, uint8_t historyCount,
                        uint16_t labelColor) {
  currentActivity = "Showing " + String(symbol);
  bool isUp = change24h >= 0;
  uint16_t priceColor = matrix.Color(isUp ? 0 : 255, isUp ? 255 : 0, 0);
  lastPriceColor = priceColor;
  lastPriceUp = isUp;

  String priceText = formatPrice(price);
  String changeText = String(change24h >= 0 ? "+" : "") + String(change24h, 1) + "% 24h";
  int contentWidth = 6 * (strlen(symbol) + 1 + priceText.length() + 1 + changeText.length() + 1) + 10 + CHART_WIDTH;
  for (int x = matrix.width(); x >= -contentWidth; --x) {
    matrix.fillScreen(0);
    matrix.setCursor(x, 0);
    matrix.setTextColor(labelColor);
    matrix.print(symbol);
    matrix.print(" ");
    matrix.setTextColor(priceColor);
    matrix.print(priceText);
    matrix.print(" ");
    matrix.print(changeText);
    matrix.print(" ");
    int triangleX = matrix.getCursorX() + 3;
    drawTriangle(triangleX, isUp, priceColor);
    drawHistoryChart(history, historyCount, triangleX + 7);
    matrix.show();
    unsigned long started = millis();
    while (millis() - started < settings.speed) serviceNetwork();
  }
}

void showClock() {
  currentActivity = "Showing date and time";
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo)) return;
  char buffer[48];
  strftime(buffer, sizeof(buffer), "%I:%M%p   %d %b %Y", &timeInfo);
  scrollText(buffer, matrix.Color(255, 40, 40));
}

void showExchangeRate() {
  if (eurUsd <= 0) return;
  currentActivity = "Showing EUR/USD";
  bool isUp = eurUsd >= previousEurUsd;
  uint16_t valueColor = eurUsd == previousEurUsd
      ? lastForexColor
      : matrix.Color(isUp ? 0 : 255, isUp ? 255 : 0, 0);
  if (eurUsd != previousEurUsd) {
    lastForexColor = valueColor;
    lastForexUp = isUp;
  } else {
    isUp = lastForexUp;
  }

  String valueText = String(eurUsd, 4);
  int contentWidth = 6 * (strlen("EUR/USD") + 1 + valueText.length() + 1) + 10;
  for (int x = matrix.width(); x >= -contentWidth; --x) {
    matrix.fillScreen(0);
    matrix.setCursor(x, 0);
    matrix.setTextColor(matrix.Color(255, 255, 255));
    matrix.print("EUR/USD ");
    matrix.setTextColor(valueColor);
    matrix.print(valueText);
    matrix.print(" ");
    int triangleX = matrix.getCursorX() + 3;
    drawTriangle(triangleX, isUp, valueColor);
    matrix.show();
    unsigned long started = millis();
    while (millis() - started < settings.speed) serviceNetwork();
  }
}

void showCenteredMessage(const char* text, uint16_t color, unsigned long durationMs) {
  drawCenteredMessage(text, color);
  delay(durationMs);
}

void configureServer() {
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server.send_P(200, "text/html", PAGE);
  });
  server.on("/api/settings", HTTP_GET, [] {
    DynamicJsonDocument json(512);
    json["message"] = settings.message;
    json["mode"] = settings.mode;
    json["brightness"] = settings.brightness;
    json["speed"] = settings.speed;
    json["hostname"] = DEVICE_NAME;
    json["ip"] = WiFi.localIP().toString();
    String body;
    serializeJson(json, body);
    server.send(200, "application/json", body);
  });
  server.on("/api/settings", HTTP_POST, [] {
    DynamicJsonDocument json(768);
    if (deserializeJson(json, server.arg("plain"))) {
      server.send(400, "application/json", "{\"error\":\"invalid json\"}");
      return;
    }
    settings.message = json["message"] | settings.message;
    settings.mode = json["mode"] | settings.mode;
    settings.brightness = constrain(json["brightness"] | settings.brightness, 1, 255);
    settings.speed = constrain(json["speed"] | settings.speed, 1, 100);
    matrix.setBrightness(settings.brightness);
    saveSettings();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/status", HTTP_GET, [] {
    DynamicJsonDocument json(1024);
    bool connected = WiFi.status() == WL_CONNECTED;
    json["hostname"] = DEVICE_NAME;
    json["wifiConnected"] = connected;
    json["ip"] = connected ? WiFi.localIP().toString() : "No IP";
    json["rssi"] = connected ? WiFi.RSSI() : 0;
    json["uptimeSeconds"] = millis() / 1000;
    json["freeHeap"] = ESP.getFreeHeap();
    json["activity"] = currentActivity;
    json["lastError"] = lastError;
    json["priceAgeSeconds"] = lastSuccessfulPriceUpdate
        ? static_cast<long>((millis() - lastSuccessfulPriceUpdate) / 1000)
        : -1;
    json["priceApi"] = priceHttpStatus;
    json["historyApi"] = priceHistoryHttpStatus;
    json["btc"] = btc;
    json["eth"] = eth;
    json["eurUsd"] = eurUsd;
    json["btcChange24h"] = btcChange24h;
    json["ethChange24h"] = ethChange24h;
    String body;
    serializeJson(json, body);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", body);
  });
  server.on("/api/restart", HTTP_POST, [] {
    server.send(200, "application/json", "{\"ok\":true}");
    showCenteredMessage("REBOOT PRESSED", matrix.Color(255, 0, 0), 2000);
    ESP.restart();
  });
  server.on("/api/wifi/reset", HTTP_POST, [] {
    server.send(200, "application/json", "{\"ok\":true}");
    delay(250);
    showCenteredMessage("WIFI SETUP", matrix.Color(255, 255, 255), 1500);
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    ESP.restart();
  });
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void setup() {
  Serial.begin(115200);
  bootTime = millis();
  currentActivity = "Starting hardware";
  loadSettings();
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(settings.brightness);
  lastPriceColor = matrix.Color(0, 255, 0);
  lastForexColor = matrix.Color(0, 255, 0);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  currentActivity = "Connecting to Wi-Fi";
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);
  if (!wifiManager.autoConnect(DEVICE_NAME)) ESP.restart();

  secureClient.setInsecure();
  httpClient.setHttpResponseTimeout(5000);
  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
  tzset();
  MDNS.begin(HOSTNAME);
  MDNS.addService("http", "tcp", 80);
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.begin();
  configureServer();
  currentActivity = "Starting display";

  Serial.printf("Control panel: http://%s.local or http://%s\n", HOSTNAME, WiFi.localIP().toString().c_str());
  showCenteredMessage("WELCOME", matrix.Color(255, 255, 255), 1500);
  showCenteredMessage("WIFI CONNECTED", matrix.Color(255, 255, 255), 1500);
  lastError = "None";
  fetchPrices();
  fetchPriceHistory();
  lastApiCall = millis();
}

void loop() {
  serviceNetwork();
  if (millis() - lastApiCall >= 600000) {
    lastError = "None";
    fetchPrices();
    fetchPriceHistory();
    lastApiCall = millis();
  }
  if (settings.mode == "all" || settings.mode == "prices") {
    if (btcAthPending) {
      btcAthPending = false;
      scrollText("BITCOIN NEW ATH " + formatPrice(knownBtcAth), matrix.Color(255, 215, 0), "Showing Bitcoin ATH alert");
    }
    if (ethAthPending) {
      ethAthPending = false;
      scrollText("ETHEREUM NEW ATH " + formatPrice(knownEthAth), matrix.Color(255, 215, 0), "Showing Ethereum ATH alert");
    }
    showPriceWithChart("Bitcoin BTC", btc, previousBtc, btcChange24h, btcHistory, btcHistoryCount, matrix.Color(255, 140, 0));
    showPriceWithChart("Ethereum ETH", eth, previousEth, ethChange24h, ethHistory, ethHistoryCount, matrix.Color(0, 127, 255));
    showExchangeRate();
  }
  if ((settings.mode == "all" || settings.mode == "message") && settings.message.length())
    scrollText(settings.message, matrix.Color(255, 255, 255));
  if (settings.mode == "all" || settings.mode == "clock") showClock();
  if (millis() - bootTime >= 10800000UL) ESP.restart();
}
