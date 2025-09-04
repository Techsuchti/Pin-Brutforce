/*
 *  Pin-Bruteforce – komplett & stabil
 *  ESP32-C3-MINI-1 | Web-Konfig | Dropdown-Menü | Debug-Level | Timing
 *  ------------------------------------------------------------
 *  1. Erststart: AP „StromBruteforce“ + Passwort → nur WLAN eintragen
 *  2. Danach **automatisch** mit gespeichertem WLAN verbinden
 *  3. Dropdown-Menü (oben rechts) für alle Einstellungen
 */

#include <WiFiManager.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

/* ---------- Defaults ---------- */
#define  DEFAULT_LED    2
#define  DEFAULT_RX     18
#define  DEFAULT_TX     19
#define  DEFAULT_BAUD   9600
#define  DEFAULT_FROM   0
#define  DEFAULT_TO     9999
#define  DEFAULT_ZAEHLER "Iskra MT 691 (SML)"
#define  DEFAULT_DEBUG   2          // 1=Error … 4=Verbose

/* ---------- Zählertyp-Datenbank ---------- */
const char* zaehlerRaw[] = {
"Apator 12EC3,+1,3,o,0,300,Strom,1,30,2F3F210D0A,063030300D0A",
"Apator 12EC3G,+1,3,o,0,300,Strom,1,30,2F3F210D0A,063030300D0A",
"Apator APOX+ (SML),+1,3,s,0,9600,SML",
"Apator Lepus 3.060 (SML),+1,3,s,1,9600,SML",
"Apator Norax 1D+ (SML),+1,3,s,1,9600,SML",
"Apator Picus eHZ.060.D/J (SML),+1,3,s,0,9600,PICUS",
"Digimeto GS303 (SML),+1,3,s,0,9600,GS303",
"DZG DWZE12.2.G2 (SML),+1,3,s,16,9600,DWZE12",
"DZG DWS7410.2V.G2 (SML),+1,3,s,16,9600,DWS7410",
"DZG DWS7412.1.G2 (SML),+1,3,s,16,9600,DWS7412",
"DZG DWS76 (SML),+1,3,s,16,9600,DWS7612",
"DZG DWSB12.2 (SML),+1,3,s,16,9600,DWSB122",
"EasyMeter Q3A / Apator APOX+,+1,3,s,0,9600,SML",
"eBZ MD3 (SML),+1,3,s,0,9600,Smartmeter",
"EFR SGM-DD-4A92T (SML),+1,3,s,0,9600,sml",
"EFR Alternative,+1,3,s,16,9600,ENERGY",
"EMH eBZD (SML),+1,3,s,0,9600,Main",
"EMH ED300L (SML),+1,13,s,0,9600,Haus",
"EMH ED300S (SML),+1,3,s,0,9600,Main",
"EMH mMe4.0 (SML),+1,3,s,0,9600,Main,1,10",
"EMH LZQJ-XC (OBIS),+1,3,o,0,9600,OBIS",
"EMH LZQJ-XC (SML),+1,3,s,0,9600,SML",
"Iskra MT 174 (OBIS),+1,3,o,0,300,STROM,1,100,2F3F210D0A",
"Iskra MT 175 (SML),+1,3,s,16,9600,MT175",
"Iskra MT 631 (SML),+1,3,s,0,9600,MT631",
"Iskra MT 681 (SML),+1,3,s,0,9600,MT681",
"Iskra MT 691 (SML),+1,3,s,0,9600,MT691",
"Itron (SML V1.04),+1,12,s,0,9600,ELZ",
"Itron Alternative,+1,3,s,0,9600,Power",
"Kamstrup K382Lx7 (SML),+1,5,k,0,9600,K382Lx7",
"KSMC403,+1,3,kN2,0,1200,KSMC403",
"Landis+Gyr E320 (SML),+1,3,s,20,9600,E320",
"Landis+Gyr E350 (OBIS),+1,3,o,0,300,STROM,1,600,2F3F210D0A",
"Logarex LK13BE (OBIS),+1,3,o,0,9600,LK13BE,13,30,2F3F210D0A",
"Logarex H37 (OBIS),+1,3,o,0,300,H37,1,600,2F3F210D0A",
"PAFAL 20EC3gr,+1,3,o,0,300,PAFAL,1,30,2F3F210D0A",
"Siemens TD-3511,+1,3,o,0,300,STROM,1,600,2F3F210D0A",
"ZPA GH302 (SML),+1,3,s,0,9600,Strom",
"ZPA GH305 (SML),+1,3,s,0,9600,SML"
};

struct ZaehlerEntry { String name; String proto; int baud; };
std::vector<ZaehlerEntry> zaehlerList;

void buildZaehlerList() {
  zaehlerList.clear();
  for (const char* line : zaehlerRaw) {
    String s = line;
    int i1 = s.indexOf(',');
    String name = s.substring(0, i1);
    int i2 = s.lastIndexOf(',');
    int i3 = s.lastIndexOf(',', i2-1);
    int baud = s.substring(i3+1, i2).toInt();
    zaehlerList.push_back({name, s, baud});
  }
}

/* ---------- Config ---------- */
struct Config {
  int  led   = DEFAULT_LED;
  int  rx    = DEFAULT_RX;
  int  tx    = DEFAULT_TX;
  int  baud  = DEFAULT_BAUD;
  int  from  = DEFAULT_FROM;
  int  to    = DEFAULT_TO;
  String zaehler = DEFAULT_ZAEHLER;
} cfg;

struct TimingCfg {
  int prePinDelay  = 1000;
  int bitHigh      = 250;
  int bitLow       = 750;
  int bitPause     = 300;
  int digitPause   = 800;
  int endSequence  = 1200;
} tcfg;

uint8_t debugLevel = DEFAULT_DEBUG;

bool        shouldSaveConfig = false;
bool        attackRunning    = false;
size_t      tested           = 0;
size_t      found            = 0;
String      currentPin       = "0000";
String      termBuffer;
std::vector<String> validPins;

WebServer server(80);
HardwareSerial* meter = nullptr;

const char* CONFIG_FILE = "/config.json";
const char* FOUND_FILE  = "/found.txt";

/* ---------- Debug ---------- */
void dPrint(uint8_t level, const String& msg){
  if(level <= debugLevel){
    Serial.println("[D" + String(level) + "] " + msg);
  }
}

/* ---------- SPIFFS ---------- */
bool loadConfig() {
  if (!SPIFFS.begin(true)) return false;
  File f = SPIFFS.open(CONFIG_FILE, "r");
  if (!f) return false;
  StaticJsonDocument<1024> doc;
  deserializeJson(doc, f);
  cfg.led   = doc["led"]   | DEFAULT_LED;
  cfg.rx    = doc["rx"]    | DEFAULT_RX;
  cfg.tx    = doc["tx"]    | DEFAULT_TX;
  cfg.baud  = doc["baud"]  | DEFAULT_BAUD;
  cfg.from  = doc["from"]  | DEFAULT_FROM;
  cfg.to    = doc["to"]    | DEFAULT_TO;
  cfg.zaehler = doc["zaehler"].as<String>();
  if (cfg.zaehler.isEmpty()) cfg.zaehler = DEFAULT_ZAEHLER;

  tcfg.prePinDelay = doc["prePinDelay"] | 1000;
  tcfg.bitHigh     = doc["bitHigh"]     | 250;
  tcfg.bitLow      = doc["bitLow"]      | 750;
  tcfg.bitPause    = doc["bitPause"]    | 300;
  tcfg.digitPause  = doc["digitPause"]  | 800;
  tcfg.endSequence = doc["endSequence"] | 1200;

  debugLevel = doc["debugLevel"] | DEFAULT_DEBUG;
  f.close();
  return true;
}
void saveConfig() {
  StaticJsonDocument<1024> doc;
  doc["led"]    = cfg.led;
  doc["rx"]     = cfg.rx;
  doc["tx"]     = cfg.tx;
  doc["baud"]   = cfg.baud;
  doc["from"]   = cfg.from;
  doc["to"]     = cfg.to;
  doc["zaehler"]= cfg.zaehler;

  doc["prePinDelay"] = tcfg.prePinDelay;
  doc["bitHigh"]     = tcfg.bitHigh;
  doc["bitLow"]      = tcfg.bitLow;
  doc["bitPause"]    = tcfg.bitPause;
  doc["digitPause"]  = tcfg.digitPause;
  doc["endSequence"] = tcfg.endSequence;

  doc["debugLevel"] = debugLevel;

  File f = SPIFFS.open(CONFIG_FILE, "w");
  serializeJson(doc, f);
  f.close();
}
void loadFound() {
  File f = SPIFFS.open(FOUND_FILE, "r");
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 4) validPins.push_back(line);
  }
  f.close();
  found = validPins.size();
}
void appendFound(const String& pin) {
  File f = SPIFFS.open(FOUND_FILE, "a");
  if (f) { f.println(pin); f.close(); }
}

/* ---------- WLAN – nur beim ersten Mal Portal ---------- */
void startWiFiManager() {
  WiFiManager wm;
  const char* AP_SSID     = "StromBruteforce";
  const char* AP_PASSWORD = "12345678";   // <-- eigenes Passwort

  // Portal-Timeout: danach Neustart
  wm.setConfigPortalTimeout(30);
  wm.setConnectTimeout(10);

  // 1. Versuch: mit gespeicherten Daten verbinden
  bool res = wm.autoConnect(AP_SSID, AP_PASSWORD);
  if (!res) {
    // Keine Credentials oder Verbindung fehlgeschlagen → neu starten
    ESP.restart();
  }
}

/* ---------- Web-Seiten ---------- */
String pageHead(String title) {
  return R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>)rawliteral" + title + R"rawliteral(</title>
<style>
  body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;margin:0;background:#f5f7fa;color:#333}
  header{background:#0d47a1;color:#fff;padding:1.2rem 1rem;font-size:1.4rem;font-weight:600;position:relative}
  nav{position:absolute;right:1rem;top:1rem}
  .wrap{max-width:900px;margin:auto;padding:1rem}
  .card{background:#fff;border-radius:8px;padding:1rem;margin-bottom:1rem;box-shadow:0 2px 6px rgba(0,0,0,.1)}
  h2{margin-top:0}
  input[type=number],select{width:100%;padding:.5rem;margin:.25rem 0;border:1px solid #ccc;border-radius:4px}
  button{background:#1976d2;color:#fff;border:0;padding:.6rem 1.2rem;border-radius:4px;cursor:pointer;font-size:1rem}
  button:hover{background:#1565c0}
  pre{background:#263238;color:#aed581;padding:.8rem;border-radius:4px;overflow:auto}
</style></head><body>
<header>
  Stromzähler Bruteforce
  <nav>
    <select onchange="if(this.value)location.href=this.value">
      <option value="">☰ Menü</option>
      <option value="/">Dashboard</option>
      <option value="/meter">Stromzähler-Einstellungen</option>
      <option value="/config">Allgemeine Einstellungen</option>
    </select>
  </nav>
</header>
<div class="wrap">
)rawliteral";
}

void handleRoot() {
  String html = pageHead("Dashboard");
  html += R"rawliteral(
<div class="card">
  <h2>Steuerung</h2>
  <p>Zählertyp: <b>)rawliteral" + cfg.zaehler + R"rawliteral(</b></p>
  <p>PIN-Bereich: <b>)rawliteral" + String(cfg.from) + "-" + String(cfg.to) + R"rawliteral(</b></p>
  <button onclick="startAttack()">Attacke starten</button>
  <button onclick="stopAttack()">Stoppen</button>
</div>
<div class="card">
  <h2>Live-Status</h2>
  <pre id="status">Warte auf Start...</pre>
</div>
<div class="card">
  <h2>Terminal (Rohdaten)</h2>
  <pre id="term"></pre>
</div>
<script>
async function startAttack(){await fetch("/start");}
async function stopAttack() {await fetch("/stop");}
setInterval(async ()=>{
  const st = await (await fetch("/api/status")).json();
  document.getElementById("status").textContent=
    `Läuft: ${st.running}\nGetestet: ${st.tested}\nAktuell: ${st.current}\nGefunden: ${st.found}`;
  const term = await (await fetch("/api/term")).text();
  document.getElementById("term").textContent=term;
},500);
</script></body></html>)rawliteral";
  server.send(200, "text/html", html);
}

void handleConfig() {
  String html = pageHead("Allgemeine Einstellungen");
  html += R"rawliteral(
<div class="card">
  <form method="post" action="/saveConfig">
    <h2>GPIO & UART</h2>
    LED-Pin: <input type="number" name="led" value=")rawliteral" + String(cfg.led) + R"rawliteral(" min="0" max="20"><br>
    RX-Pin:  <input type="number" name="rx"  value=")rawliteral" + String(cfg.rx) + R"rawliteral(" min="0" max="20"><br>
    TX-Pin:  <input type="number" name="tx"  value=")rawliteral" + String(cfg.tx) + R"rawliteral(" min="0" max="20"><br>
    Baudrate: <select name="baud">
      <option value="300")rawliteral"   + String(cfg.baud == 300   ? " selected" : "") + R"rawliteral(>300</option>
      <option value="1200")rawliteral"  + String(cfg.baud == 1200  ? " selected" : "") + R"rawliteral(>1200</option>
      <option value="9600")rawliteral"  + String(cfg.baud == 9600  ? " selected" : "") + R"rawliteral(>9600</option>
      <option value="19200")rawliteral" + String(cfg.baud == 19200 ? " selected" : "") + R"rawliteral(>19200</option>
    </select><br>

    <h2>PIN-Bereich</h2>
    Von: <input type="number" name="from" value=")rawliteral" + String(cfg.from) + R"rawliteral(" maxlength="4"><br>
    Bis: <input type="number" name="to"   value=")rawliteral" + String(cfg.to) + R"rawliteral(" maxlength="4"><br>

    <h2>Debugging</h2>
    <select name="debugLevel">
      <option value="1")rawliteral"+String(debugLevel==1?" selected":"")+R"rawliteral(>Level 1 – Error</option>
      <option value="2")rawliteral"+String(debugLevel==2?" selected":"")+R"rawliteral(>Level 2 – Warn</option>
      <option value="3")rawliteral"+String(debugLevel==3?" selected":"")+R"rawliteral(>Level 3 – Info</option>
      <option value="4")rawliteral"+String(debugLevel==4?" selected":"")+R"rawliteral(>Level 4 – Verbose</option>
    </select><br><br>

    <button type="submit">Speichern & Neustart</button>
  </form>
</div>)rawliteral";
  server.send(200, "text/html", html);
}

void handleSaveConfig() {
  cfg.led  = server.arg("led").toInt();
  cfg.rx   = server.arg("rx").toInt();
  cfg.tx   = server.arg("tx").toInt();
  cfg.baud = server.arg("baud").toInt();
  cfg.from = server.arg("from").toInt();
  cfg.to   = server.arg("to").toInt();
  debugLevel = server.arg("debugLevel").toInt();
  saveConfig();
  server.send(200, "text/html", "<meta http-equiv='refresh' content='2;url=/'>gespeichert – starte neu...</body>");
  delay(1000);
  ESP.restart();
}

void handleMeterCfg() {
  buildZaehlerList();
  String html = pageHead("Stromzähler-Einstellungen");
  html += R"rawliteral(
<div class="card">
  <form method="post" action="/saveMeter">
    <h2>Zählertyp</h2>
    <select name="zaehler">
)rawliteral";
  for (const auto& kv : zaehlerList) {
    html += "<option value='" + kv.name + "'" +
            (kv.name == cfg.zaehler ? " selected" : "") +
            ">" + kv.name + "</option>";
  }
  html += R"rawliteral(
    </select><br>

    <h2>PIN-Bereich</h2>
    Von: <input type="number" name="from" value=")rawliteral" + String(cfg.from) + R"rawliteral(" min="0" max="9999"><br>
    Bis: <input type="number" name="to"   value=")rawliteral" + String(cfg.to) + R"rawliteral(" min="0" max="9999"><br>

    <h2>Timing (Morse-Code)</h2>
    Pre-PIN lang [ms]: <input type="number" name="prePinDelay"  value=")rawliteral" + String(tcfg.prePinDelay) + R"rawliteral(" min="100" max="5000"><br>
    1-Impuls [ms]:     <input type="number" name="bitHigh"      value=")rawliteral" + String(tcfg.bitHigh) + R"rawliteral(" min="50"  max="2000"><br>
    0-Impuls [ms]:     <input type="number" name="bitLow"       value=")rawliteral" + String(tcfg.bitLow) + R"rawliteral(" min="50"  max="2000"><br>
    Bit-Pause [ms]:    <input type="number" name="bitPause"     value=")rawliteral" + String(tcfg.bitPause) + R"rawliteral(" min="50"  max="2000"><br>
    Ziffern-Pause [ms]:<input type="number" name="digitPause"   value=")rawliteral" + String(tcfg.digitPause) + R"rawliteral(" min="100" max="3000"><br>
    Ende-Signal [ms]:  <input type="number" name="endSequence"  value=")rawliteral" + String(tcfg.endSequence) + R"rawliteral(" min="100" max="5000"><br><br>

    <button type="submit">Speichern & Neustart</button>
  </form>
</div>)rawliteral";
  server.send(200, "text/html", html);
}

void handleSaveMeter() {
  cfg.zaehler      = server.arg("zaehler");
  cfg.from         = server.arg("from").toInt();
  cfg.to           = server.arg("to").toInt();
  tcfg.prePinDelay = server.arg("prePinDelay").toInt();
  tcfg.bitHigh     = server.arg("bitHigh").toInt();
  tcfg.bitLow      = server.arg("bitLow").toInt();
  tcfg.bitPause    = server.arg("bitPause").toInt();
  tcfg.digitPause  = server.arg("digitPause").toInt();
  tcfg.endSequence = server.arg("endSequence").toInt();
  saveConfig();
  server.send(200,"text/html","<meta http-equiv='refresh' content='2;url=/'>gespeichert – Neustart...");
  delay(1000); ESP.restart();
}

/* ---------- API ---------- */
void apiStatus() {
  String json = "{";
  json += "\"running\":" + String(attackRunning ? "true" : "false") + ",";
  json += "\"tested\":"  + String(tested) + ",";
  json += "\"found\":"   + String(found) + ",";
  json += "\"current\":\""+ currentPin + "\"";
  json += "}";
  server.send(200, "application/json", json);
}
void apiTerm() {
  server.send(200, "text/plain", termBuffer);
}

/* ---------- Brute-Force ---------- */
void blinkCode(const String& pin) {
  digitalWrite(cfg.led, HIGH);
  delay(tcfg.prePinDelay);
  digitalWrite(cfg.led, LOW);
  delay(tcfg.bitPause);

  for (unsigned int d=0; d<pin.length(); d++) {
    uint8_t val = pin[d] - '0';
    for (int8_t bit=3; bit>=0; bit--) {
      bool isOne = (val >> bit) & 1;
      digitalWrite(cfg.led, HIGH);
      delay(isOne ? tcfg.bitHigh : tcfg.bitLow);
      digitalWrite(cfg.led, LOW);
      if (bit>0 || d<pin.length()-1) delay(tcfg.bitPause);
    }
    if (d<pin.length()-1) delay(tcfg.digitPause);
  }
  digitalWrite(cfg.led, HIGH);
  delay(tcfg.endSequence);
  digitalWrite(cfg.led, LOW);
}

void startAttack() {
  if (attackRunning) return;
  attackRunning = true;
  tested = 0;
  found = validPins.size();
  currentPin = String(cfg.from, DEC);
  termBuffer = "";
  String prot;
  for (const auto& kv : zaehlerList) if (kv.name == cfg.zaehler) { prot = kv.proto; break; }
  int i1=prot.indexOf(',');
  int i2=prot.indexOf(',',i1+1);
  int i3=prot.indexOf(',',i2+1);
  int i4=prot.indexOf(',',i3+1);
  int i5=prot.indexOf(',',i4+1);
  int newBaud = prot.substring(i4+1,i5).toInt();
  if (newBaud>0) cfg.baud=newBaud;
  meter->updateBaudRate(cfg.baud);
  dPrint(3,"Attacke gestartet, Baud=" + String(cfg.baud));
}
void stopAttack() {
  attackRunning = false;
  dPrint(3,"Attacke gestoppt");
}

void bruteLoop() {
  if (!attackRunning) return;

  while (meter->available()) {
    char c = meter->read();
    termBuffer += c;
    if (termBuffer.length() > 800) termBuffer = "";
    if (c == '\n') {
      if (termBuffer.length() > 10) {
        if (found < cfg.to - cfg.from + 1) {
          validPins.push_back(currentPin);
          appendFound(currentPin);
          found++;
          for (int i = 0; i < 6; i++) {
            digitalWrite(cfg.led, !digitalRead(cfg.led));
            delay(80);
          }
        }
        attackRunning = false;
        return;
      }
    }
  }

  static uint32_t t = 0;
  if (millis() - t < tcfg.prePinDelay + 2000) return;
  t = millis();
  blinkCode(currentPin);
  int cur = currentPin.toInt();
  cur++;
  if (cur <= cfg.to) {
    currentPin = String(cur, DEC);
    tested++;
  } else {
    attackRunning = false;
  }
}

/* ---------- Setup & Loop ---------- */
void setup() {
  Serial.begin(115200);
  loadConfig();
  startWiFiManager();      // nur 1. Mal Portal, danach auto-connect
  loadFound();
  meter = new HardwareSerial(1);
  meter->begin(cfg.baud, SERIAL_8N1, cfg.rx, cfg.tx);
  pinMode(cfg.led, OUTPUT);
  digitalWrite(cfg.led, LOW);

  server.on("/", handleRoot);
  server.on("/config", handleConfig);
  server.on("/saveConfig", HTTP_POST, handleSaveConfig);
  server.on("/meter", handleMeterCfg);
  server.on("/saveMeter", HTTP_POST, handleSaveMeter);
  server.on("/start", [](){ startAttack(); server.send(200,"application/json","{\"result\":\"started\"}"); });
  server.on("/stop",  [](){ stopAttack();  server.send(200,"application/json","{\"result\":\"stopped\"}"); });
  server.on("/api/status", apiStatus);
  server.on("/api/term", apiTerm);
  server.begin();
  Serial.print("HTTP-Server gestartet unter http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
  bruteLoop();
}
