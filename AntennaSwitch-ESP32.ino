//
//    .----------------------------------------------------------------------------------------------------------------------------------------------------------------.
//    |                                                                 "AntSwitch Remote" with ESP32                                                                  |
//    '----------------------------------------------------------------------------------------------------------------------------------------------------------------'
//     by Daniel Wendt-Fröhlich, DL2AB (daniel.w-froehlich@aetherfoton.de, dl2ab@darc.de) - AI supported bei Google Gemini
//     License CC-by-SA 4.0 - Sep-Oct 2025 - Bremen(GER) -- https://creativecommons.org/licenses/by-sa/4.0/
//
//     Schematic, PCB and Code: https://github.com/danielwf/Remote-Antennaswitch    
// 
//     Used Libs:
//     - Install "esp32" by Espressif via Arduino-Board-Manager and select "Wemos D1 ESP32" (you have to scroll down the unsorted list)
//     - "WiFiManager" by tzapu https://github.com/tzapu/WiFiManager *
//     - "Adafruit_MCP23017" by adafruit https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library *
//     - "Adafruit ADS1X15" by adafruit https://github.com/adafruit/Adafruit_ADS1X15 *
//        * install via Arduino Library Manager
// 
//     IF YOU LOOSE ADMIN-CONFIG-PASSWORD OR NEED A "FACTORY-RESET" FOR ANOTHER REASON:
//         - Reupload Sketch with -> [Tools] -> "Erase All Flash Before Sketch Upload" -> "enabled"
//         - Disable it after reupload or you have to connect Wifi after every sketch update via Arduino IDE ;) 
//
//     ------------------------------------------------Usage // Serial Commands------------------------------------------------------
//     First Boot: Connect to "antswitch"-Wifi and use the Captive Portal for connecting to an existing Wifi-Network
//     After Wifi-Connect: IP is shown after bootup in serial console (or use the DHCP-Client list in your Wifi-Router)
//     Webbrowser: Use http://antswitch.local oder http://[IP] for switching, http://[IP]/config for config (default user: admin; default password: antswitch)
//     curl: http://[IP]/api/switch?input=1 (example to set input to 1) and http://[IP]/api/status for JSON-Status
//     Serial commands (115200 baud):
//       ____DESCRIPTION__________INPUT_________________________RESPONSE___________________________________
//       Switching
//       Switch Input (TRX)          SET:I<Index>                  ACK:I<CurrentInput>,O<CurrentOutput>
//                                                                ERR:BLOCKED_TX (if P_fwd > 5.0W)
//       Switch Output (Antenna)     SET:O<Index>                  ACK:I<CurrentInput>,O<CurrentOutput>
//                                                                ERR:BLOCKED_TX (if P_fwd > 5.0W)
//       ___________________________________________________________________________________________________
//       Configuration
//       Set Input Label             SET_LABEL I <Index> <Label>   ACK:LABEL_OK
//                                                                ERR:INVALID_LENGTH
//       Set Output Label            SET_LABEL O <Index> <Label>   ACK:LABEL_OK
//                                                                ERR:INVALID_LENGTH
//       Set Default Input           SET_DEFAULT I <Index>         ACK:DEFAULT_OK
//                                                                ERR:INVALID_INDEX
//       Set Default Output          SET_DEFAULT O <Index>         ACK:DEFAULT_OK
//                                                                ERR:INVALID_INDEX
//       Set Admin Password          SET_PASS <Password>           ACK:PASS_OK
//                                                                ERR:INVALID_LENGTH (min 8 chars)
//       ___________________________________________________________________________________________________
//       Status and Measurements
//       Get All Status & Meas.      GET:STATUS                    ACK:STATUS,PF=x.x,PP=x.x,SWR=x.xx
//       Get Forward Power (Formatted) GET:PF                        ACK:PF=x.x
//       Get Peak Power (Formatted)  GET:PP                        ACK:PP=x.x
//       Get SWR Value (Formatted)   GET:SWR                       ACK:SWR=x.xx
//       Get Forward Power (Numeric) NUM:PF                        [x.x] (Unformatted numeric value)
//       Get Peak Power (Numeric)    NUM:PP                        [x.x] (Unformatted numeric value)
//       Get SWR Value (Numeric)     NUM:SWR                       [x.xx] (Unformatted numeric value)
//       Get IP and Hostname         GET:NET                       ACK:NET,IP=x.x.x.x,HOST=<Hostname>
//       Get Input Label             GET_LABEL I <Index>           ACK:LABEL_I<Index>=<Label>
//       Get Output Label            GET_LABEL O <Index>           ACK:LABEL_O<Index>=<Label>
//       ___________________________________________________________________________________________________
//       Errors
//       Unknown Command                                       ERR:UNKNOWN_CMD
//       Invalid Index (e.g., I4 for 3 inputs)                 ERR:INVALID_INDEX
//       Invalid Format                                        ERR:FORMAT_PASS / ERR:FORMAT_LABEL / ...
//       ___________________________________________________________________________________________________//
//     ------------------------------------------------Connections----------------------------------------------------------
//       buttonInput: A0/IO36/SVP
//       buttonOutput: D0/IO26
//
//       ledOut5(led1): D5/IO18/GPIO18
//       ledOut4(led2): D6/IO19/GPIO19
//       ledOut3(led3): D7/IO23/GPOI23
//       ledOut2(led4): D8/IO05/GPIO05
//       ledOut1(led5): TCK/IO13/GPIO13
//       ledIn3(led6): SD3/IO10/GPIO10
//       ledIn2(led7): SD2/IO09/GPIO09
//       ledIn1(led8): TMS/IO14/GPIO14
//       
//       nSleep: D3/IO17/GPIO17
//
//       SDA: D2/IO21/SDA
//       SCL: D1/IO22/SCL
//       
//    .----------------------------------------------------------------------------------------------------------------------------------------------------------------.
//    |                                                          Libraries, Settings, Functions                                                                        |
//    '----------------------------------------------------------------------------------------------------------------------------------------------------------------'
//
//    .---------------------------------------------------------------------------------------.
//    |             WIFI-Settings (1/2), Configuration and Memory (1/2                        |
//    '---------------------------------------------------------------------------------------'

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Preferences.h>

const byte MAX_LABEL_LENGTH = 16;
const byte MAX_PASSWORD_LENGTH = 32;
struct Config {                            // initiale Konfiguration, kann alles später auf der Config-Seite geändert werden
  int version = 1;
  char hostname[40] = "antswitch";
  char configPassword[MAX_PASSWORD_LENGTH] = "antswitch"; // Standardpasswort
  char inputLabels[3][MAX_LABEL_LENGTH] = {"TRX 1", "TRX 2", "TRX 3"}; // Input-Labels
  char outputLabels[5][MAX_LABEL_LENGTH] = {"Antenna 1", "Antenna 2", "Antenna 3", "Antenna 4", "Antenna 5"}; // Output-Labels
  bool defaultInputEnabled[3] = {true, false, false}; // Standardzustand-Checkboxen
  bool defaultOutputEnabled[5] = {true, false, false, false, false}; // Standardzustand-Checkboxen
};
Config appConfig;
Preferences appPrefs;
WiFiManager wm; 
WiFiManagerParameter custom_hostname("hostname", "Hostname", appConfig.hostname, 40);
const char* apName = "AntSwitch-Config";
const char* apPassword = "antennaswitch"; 

//    .---------------------------------------------------------------------------------------.
//    |             PWR/SWR-Meter                                                             |
//    '---------------------------------------------------------------------------------------'
#include <Adafruit_ADS1X15.h> // supports ADS1115
Adafruit_ADS1115 ads;
int16_t rawForward = 0; // Gemessen an AIN0
int16_t rawReflected = 0; // Gemessen an AIN1
const float ADS_FULLSCALE = 4.096;
const float ADS_MAX_VALUE = 32767.0; // Max. Rohwert (15-Bit, da +/-)
const float MAX_CAL_VOLTAGE = 3.3;  // 3.3V am ADS1115 entspricht maximaler Leistung
const float MAX_CAL_POWER = 150.0;      // NEU: Maximale Leistung in Watt (durch Poti eingestellt)
const float VOLTS_SQ_TO_WATTS_FACTOR = MAX_CAL_POWER / (MAX_CAL_VOLTAGE * MAX_CAL_VOLTAGE); // Berechnung: 150.0 / (3.3 * 3.3) ≈ 13.77 W/V²

void setupADS() {
  ads.begin(0x48);
  ads.setGain(GAIN_ONE); //max Signal can be 3.3V... 2.048V is too low, so we have to set the gain to max 4.096V. 
  ads.setDataRate(RATE_ADS1115_475SPS); //setzt die Datenrate auf 475SPS, um Busgeschwindigkeit zu erhöhen.
}

float voltageF = 0.0;
float voltageR = 0.0;
float powerF = 0.0;       // current Power
float powerPeak = 0.0;    // MaxPower for current/last TX
float swr = 1.0;          // SWR
void readSWR(){
  rawForward = ads.readADC_SingleEnded(0);
  rawReflected = ads.readADC_SingleEnded(1);
  voltageF = ((float)rawForward / ADS_MAX_VALUE) * ADS_FULLSCALE;
  if (voltageF < 0.0) voltageF = 0.0;
  voltageR = ((float)rawReflected / ADS_MAX_VALUE) * ADS_FULLSCALE;
  if (voltageR < 0.0) voltageR = 0.0;
  powerF = (voltageF * voltageF) * VOLTS_SQ_TO_WATTS_FACTOR;
  if (powerF >= powerPeak) powerPeak = powerF;
  if (voltageF <= 0.005) { 
    swr = 1.0;
  } else {
    float rho = voltageR / voltageF;     // Reflexionskoeffizient ρ = U_R / U_F
    if (rho >= 1.0) {                    // Wenn U_R >= U_F (d.h. ρ >= 1.0), ist SWR unendlich oder sehr hoch
        swr = 9.9;                       // Setze auf maximalen Anzeigewert
    } else {
        swr = (1.0 + rho) / (1.0 - rho); 
    }
  }
  if (swr > 9.9) swr = 9.9;
}

#define TX_HOLD_TIME 5000       // 5000 ms = 5 Sekunden Haltezeit nach letztem TX
#define SWR_INTERVAL_TX 50      // 50 ms Leserate im aktiven TX-Betrieb
#define SWR_INTERVAL_IDLE 500   // 500 ms Leserate im Leerlauf (IDLE)
unsigned long lastSWRRead = 0;             // Zeitpunkt der letzten SWR-Messung
unsigned long lastTxTime = 0;              // Zeitpunkt des letzten Power > 0 Ereignisses
volatile bool isSwitching = false;         // Zustand Relaisschaltung. Hintergrund: Das Auslesen des SWR-Meters "blockiert" Zeit auf dem I2C-Bus, die beim Umschalten nicht nötig ist. 
void handleSWRReading() {
    unsigned long currentTime = millis();
    unsigned long currentInterval;
    if (isSwitching) return;               // keine Messung, wenn Relais geschaltet werden. 
    if ( (currentTime - lastTxTime) < TX_HOLD_TIME) { // Bestimmt das Leseintervall basierend auf der letzten TX-Aktivität
        currentInterval = SWR_INTERVAL_TX;         // Zustand: TX-Modus (oder Haltezeit läuft noch)
    } else {
        currentInterval = SWR_INTERVAL_IDLE; // Zustand: Leerlauf (länger als 10s kein TX)
        powerPeak = 0.0; // powerPeak zurücksetzen, da wir in den IDLE-Modus gewechselt sind.
    }

    if (currentTime - lastSWRRead >= currentInterval) {     // prüft, ob Intervall abgelaufen ist
        readSWR();                                          // Messung durchführen
        lastSWRRead = currentTime;
        if (powerF > 0.0) lastTxTime = currentTime;         // prüft, ob gesendet wird und aktualisiert den TX-Zeitstempel
    }
}

//    .---------------------------------------------------------------------------------------.
//    |             Relais                                                                    |
//    '---------------------------------------------------------------------------------------'
#define nSleep 17 //Sleep für DRV8833
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;
const int NUM_INPUTS = 3;
const int NUM_OUTPUTS = 5;
int currentInput = 1; // Standardmäßig Transceiver 1
int currentOutput = 1; // Standardmäßig Antenne 1
const float MAX_SWITCHING_POWER_W = 5.0; // Max. erlaubte Leistung in Watt für Umschaltvorgänge

#define mcpInput1  0b0000001100000000
#define mcpInput2  0b0000110000000000
#define mcpInput3  0b0011000000000000
#define mcpOutput1 0b0000000000000011
#define mcpOutput2 0b0000000000001100
#define mcpOutput3 0b0000000000110000
#define mcpOutput4 0b0000000011000000
#define mcpOutput5 0b1100000000000000
#define mcpRelOn   0b0101011001010101 //bits for Input1 reversed, see error in schematic U2/AOUT1+AOUT2
#define mcpRelOff  0b1010100110101010 
#define mcpRelNull 0x0000
const uint16_t mcpInputRelais[NUM_INPUTS] = {mcpInput1, mcpInput2, mcpInput3};
const uint16_t mcpOutputRelais[NUM_OUTPUTS] = {mcpOutput1, mcpOutput2, mcpOutput3, mcpOutput4, mcpOutput5};
uint16_t mcpValue = 0x0000;

void setupMcp() {
  mcp.begin_I2C(0x20); // 0x20 ist die Adresse des MCP23017
  for (int i = 0; i < 16; i++) {
    mcp.pinMode(i, OUTPUT);  // Setze alle 16 Pins als OUTPUT
    mcp.digitalWrite(i, LOW); // Alle Spulen auf inaktiv setzen
  }
}

void initInputRelais(){
  digitalWrite(nSleep, HIGH);
  delay(10);
  for (byte i=0; i < NUM_INPUTS; i++){
    mcpValue = mcpInputRelais[i] & mcpRelOn;
    mcp.writeGPIOAB(mcpValue);
    delay(10); //<-------------------Kurzer(!) Impuls setzt die Relaispule!
    mcp.writeGPIOAB(mcpRelNull);
    delay(10);
  }
  digitalWrite(nSleep, LOW);
}

void initOutputRelais(){
  digitalWrite(nSleep, HIGH);
  delay(10);
  for (byte i=0; i < NUM_OUTPUTS; i++){
    mcpValue = mcpOutputRelais[i] & mcpRelOn;
    mcp.writeGPIOAB(mcpValue);
    delay(10); //<-------------------Kurzer(!) Impuls setzt die Relaispule!
    mcp.writeGPIOAB(mcpRelNull);
    delay(10);
  }
  digitalWrite(nSleep, LOW);
}

bool switchOutput(byte outputRelais){
  if (powerF > MAX_SWITCHING_POWER_W) {
    return false; // Blockiert Relaisumschaltung, wenn gesendet wird und gibt 0-Wert zurück
  }
  isSwitching = true;           // sperrt SWR-Messung
  initOutputRelais();
  digitalWrite(nSleep, HIGH);
  mcpValue = mcpOutputRelais[(outputRelais-1)] & mcpRelOn;
  mcp.writeGPIOAB(mcpValue); 
  delay(100); //<-------------------langer Impuls setzt Relaisspule zurück
  mcp.writeGPIOAB(mcpRelNull);
  for (byte i=0; i < NUM_OUTPUTS; i++){
    if ( i!= (outputRelais-1) ) {
      mcpValue = mcpOutputRelais[i] & mcpRelOff;
      mcp.writeGPIOAB(mcpValue);
      delay(10); //<-------------------Kurzer(!) Impuls setzt die Relaispule!
      mcp.writeGPIOAB(mcpRelNull);
      delay(10);
    }
  }
  digitalWrite(nSleep, LOW);
  isSwitching = false;          // gibt SWR-Messung wieder frei
  setOutputLed(outputRelais);
  currentOutput = outputRelais;
  return true;
}

bool switchInput(byte inputRelais){
  if (powerF > MAX_SWITCHING_POWER_W) {
    return false; // Blockiert Relaisumschaltung, wenn gesendet wird und gibt 0-Wert zurück
  }
  isSwitching = true;           // sperrt SWR-Messung
  initInputRelais();
  digitalWrite(nSleep, HIGH);
  mcpValue = mcpInputRelais[(inputRelais-1)] & mcpRelOn;
  mcp.writeGPIOAB(mcpValue); 
  delay(100);
  mcp.writeGPIOAB(mcpRelNull);
  for (byte i=0; i < NUM_INPUTS; i++){
    if ( i!= (inputRelais-1) ) {
      mcpValue = mcpInputRelais[i] & mcpRelOff;
      mcp.writeGPIOAB(mcpValue);
      delay(10); //<-------------------Kurzer(!) Impuls setzt die Relaispule!
      mcp.writeGPIOAB(mcpRelNull);
      delay(10);
    }
  }
  digitalWrite(nSleep, LOW);
  isSwitching = false;          // gibt SWR-Messung wieder frei
  setInputLed(inputRelais);
  currentInput = inputRelais;
  return true;
}

void applyDefaultState() {
    bool inputSet = false;
    bool outputSet = false;
    for (int i = 0; i < NUM_INPUTS; i++) {          // STANDARD-EINGANG ANWENDEN
        if (appConfig.defaultInputEnabled[i]) {
            switchInput(i + 1); 
            inputSet = true;
            break; 
        }
    }
    if (!inputSet) {
        switchInput(currentInput); 
    }
    for (int i = 0; i < NUM_OUTPUTS; i++) {         // STANDARD-AUSGANG ANWENDEN
        if (appConfig.defaultOutputEnabled[i]) {
            switchOutput(i + 1); 
            outputSet = true;
            break; 
        }
    }
    if (!outputSet) {
        switchOutput(currentOutput);
    }
}

//    .---------------------------------------------------------------------------------------.
//    |             LEDs                                                                      |
//    '---------------------------------------------------------------------------------------'
#define ledIn1 14
#define ledIn2 9
#define ledIn3 10
#define ledOut1 13
#define ledOut2 5
#define ledOut3 23
#define ledOut4 19
#define ledOut5 18
const int INPUT_LED_PINS[] = {ledIn1, ledIn2, ledIn3};
const int OUTPUT_LED_PINS[] = {ledOut1, ledOut2, ledOut3, ledOut4, ledOut5};

void setupGpios() {
    for (int i = 0; i < NUM_INPUTS; i++) {
        pinMode(INPUT_LED_PINS[i], OUTPUT);
        digitalWrite(INPUT_LED_PINS[i], LOW);
    } 
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        pinMode(OUTPUT_LED_PINS[i], OUTPUT);
        digitalWrite(OUTPUT_LED_PINS[i], LOW);
    }
    pinMode(nSleep, OUTPUT);
    digitalWrite(nSleep, LOW); 
}

void blinkCheck() {
    delay(200); 
    for (int i = 0; i < NUM_INPUTS; i++) {
        digitalWrite(INPUT_LED_PINS[i], HIGH);
        delay(100);
        digitalWrite(INPUT_LED_PINS[i], LOW);
    }
        for (int i = 0; i < NUM_OUTPUTS; i++) {
        digitalWrite(OUTPUT_LED_PINS[i], HIGH);
        delay(100);
        digitalWrite(OUTPUT_LED_PINS[i], LOW);
    }
}

void setInputLed(int inputIndex) {
  for (int i = 0; i < NUM_INPUTS; i++) {
    digitalWrite(INPUT_LED_PINS[i], LOW);
  }
  digitalWrite(INPUT_LED_PINS[inputIndex - 1], HIGH);      
}
void setOutputLed(int outputIndex) {
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    digitalWrite(OUTPUT_LED_PINS[i], LOW);
  }
  digitalWrite(OUTPUT_LED_PINS[outputIndex - 1], HIGH);
}

//    .---------------------------------------------------------------------------------------.
//    |             WIFI-Settings (2/2), Config-AP                                            |
//    '---------------------------------------------------------------------------------------'
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.print("Betrete den Konfigurationsmodus (Captive Portal). Verbinde mit dem AP: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
}
void saveConfigCallback() {
  // WICHTIG: Diese Funktion bleibt leer, da sie nur der Callback des WiFiManagers ist.
}

void WLANbegin(){
  wm.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  wm.setDebugOutput(false);
  wm.setAPCallback(configModeCallback);
  wm.setSaveConfigCallback(saveConfigCallback);
  if (!wm.autoConnect(apName, apPassword)) {
  } else {
    WiFi.setHostname(appConfig.hostname);
    MDNS.begin(appConfig.hostname);
    Serial.print("Erfolgreich mit WLAN verbunden! http://");
    Serial.print(WiFi.localIP());
    Serial.println(" | http://" + String(appConfig.hostname) + ".local");
    startWebServer(); // Registriert Handler und ruft server.begin()
    // --- Task auf Core 0 starten, um die Web-Anfragen zu bearbeiten ---
    xTaskCreatePinnedToCore(
            webTask,       // Funktion, die den Webserver in der Schleife handled
            "WebServer",   
            4096,          
            NULL,          
            1,             
            NULL,          
            0              // Core 0
        );
  }
}

//    .---------------------------------------------------------------------------------------.
//    |             Configuration and Memory (2/2)                                            |
//    '---------------------------------------------------------------------------------------'
void saveConfig() {
  appPrefs.begin("antswitch", false); 
  appPrefs.putInt("version", appConfig.version);                 // Speichern der Einzelwerte (Version, Hostname, Passwort)
  appPrefs.putString("hostname", (const char*)appConfig.hostname); 
  appPrefs.putString("password", (const char*)appConfig.configPassword);
  for (int i = 0; i < NUM_INPUTS; i++) {                         //Speichern der Labels und Defaults (Arrays)
    String iLabelKey = "iLabel" + String(i + 1);
    String iDefKey = "iDef" + String(i + 1);
    appPrefs.putString(iLabelKey.c_str(), (const char*)appConfig.inputLabels[i]); // Explizite Konvertierung des char[] zu const char* für stabile Speicherung
    appPrefs.putBool(iDefKey.c_str(), appConfig.defaultInputEnabled[i]);
  }
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    String oLabelKey = "oLabel" + String(i + 1);
    String oDefKey = "oDef" + String(i + 1);
    appPrefs.putString(oLabelKey.c_str(), (const char*)appConfig.outputLabels[i]);     // Explizite Konvertierung des char[] zu const char*
    appPrefs.putBool(oDefKey.c_str(), appConfig.defaultOutputEnabled[i]);
  }
  appPrefs.end(); // Schreibt alle Änderungen auf den Flash
} 

void loadConfig() {
  // 1. Lesezugriff für den Namespace starten
  if (!appPrefs.begin("antswitch", true)) { 
    Serial.println("FEHLER: Konnte Namespace nicht öffnen! Fehler beim Mounten.");
  }
  int loadedVersion = appPrefs.getInt("version", 0);   // Prüfen, ob die Version existiert und wie hoch sie ist. 0 ist der Standardwert, wenn der Key fehlt.
  appPrefs.end();  // Lesezugriff sofort beenden, für den Fall dass Version nicht stimmt (wird sonst gleich für die eigentlichen Werte wieder gestartet)
  if (loadedVersion != appConfig.version) {
    saveConfig();  // Rufe die Speicherfunktion auf, um die aktuellen RAM-Standardwerte zu sichern.
    return;  // Die Konfiguration ist jetzt gespeichert; die RAM-Struktur behält die Standardwerte. loadConfig kann beendet werden.
  }
  appPrefs.begin("antswitch", true); // Neuen, stabilen Lesezugriff starten
  appPrefs.getString("hostname", appConfig.hostname, sizeof(appConfig.hostname));       // Hostname und Passwort laden
  appPrefs.getString("password", appConfig.configPassword, sizeof(appConfig.configPassword));
  for (int i = 0; i < NUM_INPUTS; i++) {                     // Input Labels und Defaults
    String iLabelKey = "iLabel" + String(i + 1);
    String iDefKey = "iDef" + String(i + 1);
    appPrefs.getString(iLabelKey.c_str(), appConfig.inputLabels[i], MAX_LABEL_LENGTH);
    appConfig.defaultInputEnabled[i] = appPrefs.getBool(iDefKey.c_str(), appConfig.defaultInputEnabled[i]);
  }
  for (int i = 0; i < NUM_OUTPUTS; i++) {                    // Output Labels und Defaults
    String oLabelKey = "oLabel" + String(i + 1);
    String oDefKey = "oDef" + String(i + 1);
    appPrefs.getString(oLabelKey.c_str(), appConfig.outputLabels[i], MAX_LABEL_LENGTH);
    appConfig.defaultOutputEnabled[i] = appPrefs.getBool(oDefKey.c_str(), appConfig.defaultOutputEnabled[i]);
  }   
  appPrefs.end();
}

//    .---------------------------------------------------------------------------------------.
//    |             Webserver                                                                 |
//    '---------------------------------------------------------------------------------------'
WebServer server(80);
bool checkConfigAuth() {
  if (server.authenticate("admin", appConfig.configPassword)) {
    return true;
  }
  // Wenn die Authentifizierung fehlschlägt, den Browser zur Eingabe auffordern
  server.requestAuthentication(); 
  return false;
}

void handleConfig() {
  if (!checkConfigAuth()) return; // Schutz prüfen

  String html = "<!DOCTYPE html><html><head><title>Antenna Switch Konfiguration</title>";
  html += "<style>";
  html += "input[type=text], input[type=password] { width: 40em; padding: 10px; margin: 10px; display: inline-block; border: 1px solid #ccc; background-color: #222; color: #eee; border-radius: 4px; box-sizing: border-box; font-size: 1.2em; font-weight: bold;}";
  html += "input[type=submit] { background-color: #4CAF50; color: white; padding: 14px 20px; margin: 8px 0; border: none; border-radius: 4px; cursor: pointer; font-size: 1.2em; font-weight: bold;}";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #222; color: #eee; margin: 0; padding: 20px; text-align: left; }";
  html += ".header-bar { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; padding-bottom: 10px; border-bottom: 1px solid #555; }";
  html += ".logo-title { display: flex; align-items: center; gap: 10px; }";
  html += ".logo-icon { color: #008cba; } /* Farbe des SVG-Icons anpassen */";
  html += "h1 { font-size: 1.8em; margin: 0; color: #008cba; }"; // Blau/Cyan für den Titel
  html += "a[href] {color: #bbb;}";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='header-bar'><div class='logo-title'><h1>AntSwitch Remote <font color=#eee> - Configuration</font></h1></div></div>";  html += "<form method='POST' action='/saveconfig'>";
  html += "<a href=\"./\">Return to Main Page</a><br></br>";
  
  // --- Input Labels und Standardzustand ---
  html += "<h2>Input Labels (Transceivers)</h2>";
  for (int i = 0; i < NUM_INPUTS; i++) {
    html += "<label><b>Input " + String(i + 1) + " Label:</b></label>";
    html += "<input type='text' name='iLabel" + String(i + 1) + "' value='" + String(appConfig.inputLabels[i]) + "'>";
    html += "<label><input type='radio' name='defaultInput' value='" + String(i + 1) + "'" + (appConfig.defaultInputEnabled[i] ? " checked" : "") + ">default input</label><p>";
  }
  
  // --- Output Labels und Standardzustand ---
  html += "<h2>Output Labels (Antennas)</h2>";
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    html += "<label><b>Output " + String(i + 1) + " Label:</b></label>";
    html += "<input type='text' name='oLabel" + String(i + 1) + "' value='" + String(appConfig.outputLabels[i]) + "'>";
    html += "<label><input type='radio' name='defaultOutput' value='" + String(i + 1) + "'" + (appConfig.defaultOutputEnabled[i] ? " checked" : "") + ">default output</label><p>";
  }

  html += "<h2>Password and Hostname</h2>";
  // --- Passwort ---
  html += "<label for='password'>New Passwort (Admin):</label>";
  html += "<input type='password' id='password' name='password' value=''><br><br>";
    // --- Hostname ---
  html += "<label for='hostname'><b>Hostname:</b></label>";
  html += "<input type='text' id='hostname' name='hostname' value='" + String(appConfig.hostname) + "'>(reset required after change)<br><br>";

  html += "<input type='submit' value='Save'>";
  html += "</form>";
  html += "<form method='POST' action='/restart'>";
  // ACHTUNG: Der Neustart-Button ist rot, um Verwechslungen zu vermeiden
  html += "<input type='submit' value='Reset AntSwitch' style='background-color: #f44336;'>"; 
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleRestart() {
    if (!checkConfigAuth()) return;      // Stellt sicher, dass nur authentifizierte Benutzer neu starten können
    server.send(200, "text/html", "ESP reset - <a href=\"./\">AntSwitch Main Page</a> - <a href=\"./config\">Config</a>");     // Zuerst eine Antwort an den Browser senden, damit der Browser nicht endlos auf eine Antwort vom ESP wartet.
    delay(500);     // Eine kurze Verzögerung einfügen, um sicherzustellen, dass die Antwort gesendet wurde
    ESP.restart();     // Gerät neu starten
}

void handleSaveConfig() {
    if (!checkConfigAuth()) return; // Sicherheitsprüfung

    // 1. Alle Formularwerte in die RAM-Struktur (appConfig) kopieren
    
    // a) HOSTNAME
    if (server.hasArg("hostname")) {
        String newHostname = server.arg("hostname");
        if (newHostname.length() >= 1 && newHostname.length() <= 31) {
            newHostname.toCharArray(appConfig.hostname, sizeof(appConfig.hostname));
        } else {
            server.send(400, "text/plain", "Fehler: Hostname muss 1 bis 31 Zeichen lang sein."); 
            return;
        }
    }

    // b) PASSWORT
    if (server.hasArg("password") && server.arg("password").length() >= 8) {
        server.arg("password").toCharArray(appConfig.configPassword, MAX_PASSWORD_LENGTH);
    }
    
    // c) LABELS und DEFAULTS
    int selectedInput = server.hasArg("defaultInput") ? server.arg("defaultInput").toInt() : -1;
    int selectedOutput = server.hasArg("defaultOutput") ? server.arg("defaultOutput").toInt() : -1;
    
    for (int i = 0; i < NUM_INPUTS; i++) {
        if (server.hasArg("iLabel" + String(i + 1))) {
            server.arg("iLabel" + String(i + 1)).toCharArray(appConfig.inputLabels[i], MAX_LABEL_LENGTH); 
        }
        appConfig.defaultInputEnabled[i] = (i + 1) == selectedInput;
    }
    
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        if (server.hasArg("oLabel" + String(i + 1))) {
            server.arg("oLabel" + String(i + 1)).toCharArray(appConfig.outputLabels[i], MAX_LABEL_LENGTH); 
        }
        appConfig.defaultOutputEnabled[i] = (i + 1) == selectedOutput;
    }

    // 2. Speicherfunktion aufrufen
    saveConfig(); 

    // 3. Bestätigung senden
    server.sendHeader("Location", "/config");
    server.send(302, "text/plain", "Konfiguration gespeichert.");
}

String buildStatusJson() {
    String json = "{";
    
    // Allgemeine Statusinformationen
    json += "\"status\": \"ok\",";
    json += "\"hostname\": \"" + String(appConfig.hostname) + "\",";
    json += "\"ip\": \"" + WiFi.localIP().toString() + "\",";
    
    // Aktuelle Zustände
    json += "\"input\": " + String(currentInput) + ",";
    json += "\"output\": " + String(currentOutput) + ",";
    
    // Messwerte
    json += "\"power_f\": " + String(powerF, 1) + ",";
    json += "\"power_peak\": " + String(powerPeak, 1) + ",";
    json += "\"swr\": " + String(swr, 2) + ",";

    // Labels (Input)
    for (int i = 0; i < NUM_INPUTS; i++) {
        json += "\"iLabel" + String(i + 1) + "\": \"" + String(appConfig.inputLabels[i]) + "\",";
    }

    // Labels (Output) - Letzte Elemente (ohne Komma nach dem letzten)
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        json += "\"oLabel" + String(i + 1) + "\": \"" + String(appConfig.outputLabels[i]) + "\"";
        if (i < NUM_OUTPUTS - 1) {
            json += ",";
        }
    }
    
    json += "}";
    return json;
}

void handleApiSwitch() {
    int requestedInput = currentInput;
    int requestedOutput = currentOutput; 
    bool switched = false;
    bool blocked = false;

    // --- Validierung und Zuweisung der angeforderten Werte ---
    if (server.hasArg("input")) {
        int tempInput = server.arg("input").toInt();
        if (tempInput >= 1 && tempInput <= NUM_INPUTS) {
            requestedInput = tempInput;
            switched = true;
        }
    }
    
    if (server.hasArg("output")) {
        int tempOutput = server.arg("output").toInt();
        if (tempOutput >= 1 && tempOutput <= NUM_OUTPUTS) {
            requestedOutput = tempOutput;
            switched = true;
        }
    }
    
    // --- Schaltung und JSON-Antwort ---
    if (switched) {
        bool inputSuccess = true;
        bool outputSuccess = true; 
        if (requestedInput != currentInput) inputSuccess = switchInput(requestedInput);
        if (requestedOutput != currentOutput) outputSuccess = switchOutput(requestedOutput);
        if (!inputSuccess || !outputSuccess) { blocked = true; }
        
        // --- JSON-Antwort erstellen ---
        String status_str = blocked ? "blocked" : "ok";
        String message_str = blocked ? "Switching blocked due to active transmission." : "Switched successfully.";
        int statusCode = blocked ? 409 : 200; 
        
        String json = buildStatusJson(); // Wir müssen die "status: ok" und das abschließende "}" durch Fehlercode und -Meldung ersetzen, wenn z.B. die Umschaltung durch TX blockiert ist. 
        json.replace("\"status\": \"ok\",", "");          // A. Ersetze die allgemeine Statusmeldung durch unsere spezifischen Felder
        json.replace("{", "{\"status\": \"" + status_str + "\",\"message\": \"" + message_str + "\","); // B. Füge unsere spezifischen Felder an den Anfang ein und behalte den Rest des JSON-Strings // Wir ersetzen { durch unseren Status und die Message
        server.send(statusCode, "application/json", json);         // C. Sende die Antwort
        return;
    }
    
    // Wenn keine gültigen Parameter übergeben wurden: Fehler 400 Bad Request
    server.send(400, "application/json", "{\"status\": \"error\", \"message\": \"Invalid or missing parameters.\" }");
}

void handleApiStatus() {
  // Die Statusmeldung ist immer 200 OK
  server.send(200, "application/json", buildStatusJson());
}

// Hauptseiten-Handler inkl. Javascript für Buttons, Status und PWR/SWR-Meter
void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>AntSwitch Remote</title>";
    // Kein Meta-Refresh mehr! Wir nutzen JavaScript.
    html += "<style>";
    html += "button { padding: 10px; margin: 5px; font-size: 16px; cursor: pointer; }";
    html += "#swr-display {font-size: 20px; font-weight: bold; color: #000;padding: 10px; border-radius: 5px; display: inline-block; min-width: 80px; text-align: center;}";
    html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #222; color: #eee; margin: 0; padding: 20px; text-align: center; }";
    html += ".header-bar { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; padding-bottom: 10px; border-bottom: 1px solid #555; }";
    html += ".logo-title { display: flex; align-items: center; gap: 10px; }";
    html += ".logo-icon { color: #008cba; } /* Farbe des SVG-Icons anpassen */";
    html += "h1 { font-size: 1.8em; margin: 0; color: #008cba; }"; // Blau/Cyan für den Titel
    html += "a[href] {color: #bbb;}";
    html += ".hostname-display { font-size: 0.9em; color: #bbb; }";
    html += "</style>";
    html += "</head><body>";
    html += "<div class='header-bar'><div class='logo-title'><h1>AntSwitch Remote</h1></div></div>";
    // --- STATUS-ANZEIGE (Wird später über JS befüllt) ---
    html += "<div id='status-display'><p>Status wird geladen...</p></div>"; // Platzhalter für Buttonstatus
    html += "<p><h2>TX-Power: <strong id='power-f'>0.0</strong> W</h2></p>";
    html += "<p><h3>Power(peak): <strong id='power-peak'>0.0</strong> W</h3></p>";
    html += "<p><h2>SWR: <span id='swr-display' style='background-color: lightgreen;'>1.00</span></h2></p>"; // NEU: Bereich für SWR mit Farbe
    
    // --- UMSCHALT-BUTTONS ---
    html += "<h2>Steuerung</h2>";
    
    // Buttons für Eingänge
    html += "<div id='input-buttons'>";
    for (int i = 1; i <= NUM_INPUTS; i++) {
        String buttonText = appConfig.inputLabels[i - 1];
        // Die Funktion 'doSwitch' wird über JavaScript aufgerufen
        html += "<button onclick=\"doSwitch('input', " + String(i) + ")\" id=\"btn-input-" + String(i) + "\">" + buttonText + "</button> ";
    }
    html += "</div><br>";

    // Buttons für Ausgänge
    html += "<div id='output-buttons'>";
    for (int i = 1; i <= NUM_OUTPUTS; i++) {
        String buttonText = appConfig.outputLabels[i - 1];
        html += "<button onclick=\"doSwitch('output', " + String(i) + ")\" id=\"btn-output-" + String(i) + "\">" + buttonText + "</button> ";
    }
    html += "</div>";
    html += "<hr><div class='hostname-display'><p>Hostname: <strong>" + String(appConfig.hostname) + ".local</strong> | IP: " + WiFi.localIP().toString() + " | <a href=\"./config\" target=_blank>Config</a></p></div>";

    // --- JavaScript Logik ---
    html += "<script>";
    
    // Funktion zur Bestimmung der SWR-Anzeigefarbe
    html += "function getSWRColor(swr) {";
    html += "  if (swr <= 1.5) return 'lightgreen';"; // Gut
    html += "  if (swr <= 2.5) return 'yellow';";     // Akzeptabel
    html += "  return '#ff6666';";                    // Schlecht (Rot)
    html += "}";
    
    // Hauptfunktion: Aktualisiert alle UI-Elemente basierend auf JSON-Daten
    html += "function updateStatus(data) {";
    
    // --- Status & Button-Update (Eingang/Ausgang) ---
    // Aktualisiert den Status-Block
    html += "  document.getElementById('status-display').innerHTML = '<p>Input (TRX): <strong>' + data.input + '</strong> | Output (Antenna): <strong>' + data.output + '</strong></p>';";
    
    // Setzt die Hintergrundfarbe des aktiven Buttons (Input)
    html += "  document.querySelectorAll('#input-buttons button').forEach(btn => btn.style.backgroundColor = '');";
    html += "  document.getElementById('btn-input-' + data.input).style.backgroundColor = '#00ff00';";
    // Setzt die Hintergrundfarbe des aktiven Buttons (Output)
    html += "  document.querySelectorAll('#output-buttons button').forEach(btn => btn.style.backgroundColor = '');";
    html += "  document.getElementById('btn-output-' + data.output).style.backgroundColor = '#00ff00';";
    
    // --- PWR/SWR-Messwerte ---
    html += "  document.getElementById('power-f').innerHTML = data.power_f;";
    html += "  document.getElementById('power-peak').innerHTML = data.power_peak;";
    html += "  var swrDisplay = document.getElementById('swr-display');";
    html += "  swrDisplay.innerHTML = data.swr;";
    html += "  swrDisplay.style.backgroundColor = getSWRColor(data.swr);"; // <-- Farbliche Hervorhebung
    
    html += "}"; // Ende von updateStatus

    // Funktion: Ruft den aktuellen Status vom Server ab (für window.onload und setInterval)
    html += "function fetchStatus() {";
    html += "  fetch('/api/status').then(response => response.json()).then(data => updateStatus(data)).catch(error => console.error('Error fetching status:', error));";
    html += "}";

    // Funktion: Sendet den Umschaltbefehl (wird bei Button-Klick aufgerufen)
    html += "function doSwitch(type, index) {";
    
    // Fetch-Aufruf, der auch mit 409 Conflict umgehen kann (Korrektur für Stabilität)
    html += "  fetch('/api/switch?' + type + '=' + index).then(response => {";
    
    // Prüfe auf HTTP-Fehler. response.ok ist true für 200-299, aber 409 ist ein Konfliktfehler.
    // Wir werfen einen Fehler, wenn der Status > 409 ist, um schwerwiegende Fehler abzufangen.
    // Wir lesen das JSON in jedem Fall, da der Server auch bei 409 eine JSON-Antwort sendet.
    
    html += "    if (!response.ok && response.status !== 409) {";
    html += "      throw new Error('Server returned HTTP status: ' + response.status);";
    html += "    }";
    html += "    return response.json();"; 
    
    html += "  }).then(data => {";
    
    // Statusanzeige aktualisieren (egal ob ok oder blockiert)
    html += "    updateStatus(data);"; 
    
    // Visuelle Rückmeldung bei Blockade (Korrektur für Benutzerfreundlichkeit)
    html += "    if (data.status === 'blocked') {";
    html += "      alert('Umschaltung blockiert: ' + data.message);";
    html += "    }";
    html += "  }).catch(error => console.error('Error switching:', error));";
    html += "}";

    // Initialisierung: Ruft den Status beim Laden ab und startet den Polling-Intervall
    html += "window.onload = function() { fetchStatus(); setInterval(fetchStatus, 5000); };"; 
    
    html += "</script>";
    html += "</body></html>";

    server.send(200, "text/html", html);
}

// Startet den Webserver und die Handler
void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/switch", HTTP_GET, handleApiSwitch); 
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/saveconfig", HTTP_POST, handleSaveConfig); 
  server.on("/restart", HTTP_POST, handleRestart);
  server.onNotFound(handleRoot); 
  server.begin();
}

// Task-Funktion, die auf Core 0 läuft und den Webserver handled
void webTask(void *pvParameters) {
  // Die Task läuft in einer Endlosschleife
  for (;;) {
    server.handleClient();
    // Kurze Verzögerung, um anderen Aufgaben auf Core 0 Zeit zu geben
    vTaskDelay(1 / portTICK_PERIOD_MS); 
  }
}

//    .---------------------------------------------------------------------------------------.
//    |             Serial Interface                                                          |
//    '---------------------------------------------------------------------------------------'
void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim(); 
    String response = "";
    
    // Tokens für robustes Parsen
    String tokens[4];
    int tokenCount = 0;
    
    // Zerlege den String in Tokens (nach Leerzeichen)
    int lastIndex = 0;
    for (int i = 0; i < command.length() && tokenCount < 4; i++) {
      if (command.charAt(i) == ' ' || i == command.length() - 1) {
        String token = command.substring(lastIndex, i + (i == command.length() - 1 ? 1 : 0));
        token.trim();
        if (token.length() > 0) {
          tokens[tokenCount++] = token;
        }
        lastIndex = i + 1;
      }
    }
    
    String cmd = tokens[0];
    
    // --- SCHALTEN (SET:I / SET:O) ---
    if (cmd.startsWith("SET:I") || cmd.startsWith("SET:O")) {
      bool isInput = cmd.charAt(4) == 'I';
      int index = cmd.substring(5).toInt();
      int maxIndex = isInput ? NUM_INPUTS : NUM_OUTPUTS;
      bool success = false;
      
      if (index >= 1 && index <= maxIndex) {
        if (isInput) {
          success = switchInput(index);
        } else {
          success = switchOutput(index);
        }
        
        if (success) {
          response = "ACK:I" + String(currentInput) + ",O" + String(currentOutput);
        } else {
          response = "ERR:BLOCKED_TX"; 
        }
      } else {
        response = "ERR:INVALID_INDEX";
      }
    } 
    
    // --- KONFIGURATION SETZEN: SET_LABEL ---
    else if (cmd == "SET_LABEL") {
      // WICHTIG: Es müssen MINDESTENS vier Tokens vorhanden sein (SET_LABEL, Typ, Index, Label-Anfang)
      if (tokenCount < 4) { 
        response = "ERR:FORMAT_LABEL";
      } else {
        String type = tokens[1];
        int index = tokens[2].toInt();
        
        // NEU: Den Label-String aus dem Rest der Eingabe zusammensetzen
        String newLabel = command.substring(
          command.indexOf(tokens[3]) // Finde den Start des ersten Label-Tokens
        );
        newLabel.trim(); // Leerzeichen am Anfang/Ende entfernen

        bool isInput = type == "I";
        int maxIndex = isInput ? NUM_INPUTS : NUM_OUTPUTS;

        if (index >= 1 && index <= maxIndex) {
          if (newLabel.length() < 1 || newLabel.length() >= MAX_LABEL_LENGTH) {
            response = "ERR:INVALID_LENGTH";
          } else {
            // RAM-Struktur (appConfig) aktualisieren
            if (isInput) {
              newLabel.toCharArray(appConfig.inputLabels[index - 1], MAX_LABEL_LENGTH);
            } else {
              newLabel.toCharArray(appConfig.outputLabels[index - 1], MAX_LABEL_LENGTH);
            }
            response = "ACK:LABEL_OK";
            saveConfig(); // Bei Erfolg: Speichern
          }
        } else {
          response = "ERR:INVALID_INDEX";
        }
      }
    }
    
    // --- KONFIGURATION SETZEN: SET_PASS ---
    else if (cmd == "SET_PASS") {
      if (tokenCount != 2) {
        response = "ERR:FORMAT_PASS";
      } else {
        String newPassword = tokens[1];
        if (newPassword.length() >= 8 && newPassword.length() <= MAX_PASSWORD_LENGTH) {
          newPassword.toCharArray(appConfig.configPassword, MAX_PASSWORD_LENGTH);
          saveConfig(); // Speichern der kritischen Konfiguration
          response = "ACK:PASS_OK";
        } else {
          response = "ERR:INVALID_LENGTH"; // Passwort zu kurz (mind. 8 Zeichen)
        }
      }
    }
    
    // --- KONFIGURATION SETZEN: SET_DEFAULT ---
    else if (cmd == "SET_DEFAULT") {
      if (tokenCount != 3) {
        response = "ERR:FORMAT_DEFAULT";
      } else {
        String type = tokens[1];
        int index = tokens[2].toInt();
        bool isInput = type == "I";
        int maxIndex = isInput ? NUM_INPUTS : NUM_OUTPUTS;

        if ((type == "I" || type == "O") && index >= 1 && index <= maxIndex) {
          // Setze alle Defaults auf FALSE, außer den ausgewählten Index
          
          if (isInput) {
            // RAM-Struktur (appConfig) aktualisieren
            for (int i = 0; i < NUM_INPUTS; i++) {
              appConfig.defaultInputEnabled[i] = (i + 1 == index);
            }
          } else {
            // RAM-Struktur (appConfig) aktualisieren
            for (int i = 0; i < NUM_OUTPUTS; i++) {
              appConfig.defaultOutputEnabled[i] = (i + 1 == index);
            }
          }
          response = "ACK:DEFAULT_OK";
          saveConfig(); // Speichern der geänderten Defaults
        } else {
          response = "ERR:INVALID_INDEX";
        }
      }
    }
    // --- 4. STATUS/MESSWERTE AUSLESEN: GET:...) ---
    
    else if (cmd.startsWith("GET:")) {
      String subCmd = cmd.substring(4);

      // MESSWERTE EINZELN ODER GESAMT AUSLESEN
      if (subCmd == "STATUS") {
        readSWR(); // Sicherstellen, dass die Werte aktuell sind
        response = "ACK:STATUS";
        response += ",PF=" + String(powerF, 1);
        response += ",PP=" + String(powerPeak, 1);
        response += ",SWR=" + String(swr, 2);
      } else if (subCmd == "PF") {
        readSWR();
        response = "ACK:PF=" + String(powerF, 1);
      } else if (subCmd == "PP") {
        readSWR();
        response = "ACK:PP=" + String(powerPeak, 1);
      } else if (subCmd == "SWR") {
        readSWR();
        response = "ACK:SWR=" + String(swr, 2);
      }
      
      // NETZWERK INFOS
      else if (subCmd == "NET") {
        response = "ACK:NET";
        response += ",IP=" + WiFi.localIP().toString();
        response += ",HOST=" + String(appConfig.hostname);
      }
      
      // C. FEHLER
      else {
        response = "ERR:UNKNOWN_CMD";
      }
    }

    // --- LABEL AUSLESEN: GET_LABEL ---
    else if (cmd == "GET_LABEL") {
      if (tokenCount != 3) {
        response = "ERR:FORMAT_LABEL_GET";
      } else {
        String type = tokens[1];
        int index = tokens[2].toInt();
        bool isInput = type == "I";
        int maxIndex = isInput ? NUM_INPUTS : NUM_OUTPUTS;
        
        // 1. Index- und Typ-Validierung
        if ((type == "I" || type == "O") && index >= 1 && index <= maxIndex) {
          String label = isInput ? appConfig.inputLabels[index - 1] : appConfig.outputLabels[index - 1];
          response = "ACK:LABEL_" + type + String(index) + "=" + label;
        } else {
          response = "ERR:INVALID_INDEX";
        }
      }
    }

    // --- NUMERISCHE AUSGABE (NUM:PF / NUM:PP / NUM:SWR) ---
    else if (cmd.startsWith("NUM:")) {
      String subCmd = cmd.substring(4);
      readSWR(); // Sicherstellen, dass die Werte aktuell sind

      if (subCmd == "PF") {
        // Unformatiert: 1 Nachkommastelle, keine Präfixe
        Serial.println(String(powerF, 1)); 
      } 
      else if (subCmd == "PP") {
        // Unformatiert: 1 Nachkommastelle, keine Präfixe
        Serial.println(String(powerPeak, 1));
      } 
      else if (subCmd == "SWR") {
        // Unformatiert: 2 Nachkommastellen, keine Präfixe
        Serial.println(String(swr, 2));
      } 
      else {
        response = "ERR:UNKNOWN_NUM_CMD"; // Neue Fehlermeldung
      }
      
      // Wichtig: Wenn ein numerischer Wert gesendet wurde, darf die Standard-Antwort nicht mehr gesendet werden.
      // Da Serial.println() direkt im if-Block aufgerufen wurde, beenden wir hier.
      if (response == "") return; 
    }

    // --- UNBEKANNTER BEFEHL ---
    else {
      response = "ERR:UNKNOWN_CMD";
    }

    // --- ANTWORT SENDEN ---
    if (response.length() > 0) {
      Serial.println(response);
    }
  }
}

//    .----------------------------------------------------------------------------------------------------------------------------------------------------------------.
//    |                                                          SETUP                                                                                                 |
//    '----------------------------------------------------------------------------------------------------------------------------------------------------------------'

void setup() {
  Serial.begin(115200);  // serielles Interface zur USB-Steuerung (und zum Debugging^^)
  delay(1000);           // wartet 1s auf das Interface, wegen Debugging beim Starten
  loadConfig();          // lädt die Konfiguration aus dem NVS-Speicher
  setupGpios();          // stellt die GPIOS für LEDs und nSleep ein
  Wire.begin();          // I2C-Bus für MCP23017 (Relais) und ADS1115 (SWR-Meter)
  setupMcp();            // MCP23017, 16bit-Portexpander für Relaissteuerung
  setupADS();            // ADS1115, 16bit-ADC für PWR/SWR-Meter
  blinkCheck();          // nette Einschaltanimation
  applyDefaultState();   // setzt die als Default eingestellten Ein- und Ausgänge
  WLANbegin();           // startet WLAN-Interface und Webserver
}

//    .----------------------------------------------------------------------------------------------------------------------------------------------------------------.
//    |                                                          LOOP                                                                                                  |
//    '----------------------------------------------------------------------------------------------------------------------------------------------------------------'

void loop() {
  handleSWRReading(); // liest das SWR-Meter (Intervalle werde in der Funktion gesteuert)
  handleSerialCommands(); // <-- Platzhalter für die zukünftige Funktion
}
