// Code Written By Krrish Verma 
// Visit https://github.com/KrrishVerma for more details & Implementation
// Released under the [GNU General Public License v3.0]
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char* ssid = "Your_SSID";  // Replace with your SSID
const char* password = "Your_Password"; // Replace with your Password
 
// Firebase Realtime DB endpoint
const char* firebaseURL = "yourdatapoint.firebasedatabase.app/data.json"; // Replace with Your FireBase Data Point

// mDNS hostname
const char* mdnsHostname = "esp32-ec200u"; // You can change the hostname if needed

// Async web server and UART
AsyncWebServer server(80);
HardwareSerial ecSerial(2); // UART2 (GPIO16=RX, GPIO17=TX)  // Use ESP32'S UART2

// Pin definitions
const int RST_PIN = 4; // GPIO 4 connected to EC200U RST pin

// LTE status info
String simID = "Unknown";
String simStatus = "Unknown";
String lteStatus = "Not Connected";
String networkStatus = "Not Registered";
int rssi = 99;
String httpStatus = "Unknown";
String networkTime = "Unknown";
String httpResponseBody = "None";
String networkOperator = "Unknown";
String mobileNumber = "Unknown";

// RSSI history for graph
#define MAX_HISTORY 10
int rssiHistory[MAX_HISTORY] = {99};
String timeHistory[MAX_HISTORY] = {"Unknown"};
int historyIndex = 0;

// Timing for staggered updates
unsigned long lastFullUpdate = 0;
unsigned long lastFirebaseUpdate = 0;
const unsigned long updateInterval = 10000; // 10s for all updates
const unsigned long firebaseInterval = 15000; // 15s for Firebase

// Function Declarations
String sendCommand(String cmd, int timeout = 800);
void initEC200U();
void sendToFirebase(float temperature, float humidity);
String parseCCID(String response);
String parseSIMStatus(String response);
String parseLTE(String response);
String parseNetwork(String response);
int parseRSSI(String response);
String parseHttpStatus(String response);
String parseHttpResponseBody(String response);
String parseNetworkTime(String response);
String parseNetworkOperator(String response);
String parseMobileNumber(String response);
String sanitizeString(String str);
void handleRoot(AsyncWebServerRequest *request);
void handleStatus(AsyncWebServerRequest *request);
void handleResetSIM(AsyncWebServerRequest *request);
void resetEC200U();

void setup() {
  Serial.begin(115200);
  ecSerial.begin(115200, SERIAL_8N1, 16, 17);
  delay(1000);

  // Initialize RST pin as output
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW); // Default state, release reset
  delay(100);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected. IP: " + WiFi.localIP().toString());

  // Initialize mDNS
  if (MDNS.begin(mdnsHostname)) {
    Serial.println("mDNS started: http://" + String(mdnsHostname) + ".local");
  } else {
    Serial.println("mDNS failed to start");
  }

  initEC200U();

  // Web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/reset-sim", HTTP_GET, handleResetSIM);
  server.begin();
  Serial.println("Async web server started");
}

void loop() {
  // Check Wi-Fi status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected, reconnecting...");
    WiFi.reconnect();
    delay(500);
    return;
  }

  unsigned long currentMillis = millis();
  static unsigned long lastLoop = 0;
  if (currentMillis - lastLoop >= updateInterval) {
    Serial.println("Loop start at " + String(currentMillis) + "ms");

    // Update all critical info every 10s
    simStatus = parseSIMStatus(sendCommand("AT+CPIN?", 800));
    if (simStatus == "SIM Detected") {
      rssi = parseRSSI(sendCommand("AT+CSQ", 800));
      String newTime = parseNetworkTime(sendCommand("AT+QLTS=2", 800));
      rssiHistory[historyIndex] = rssi;
      timeHistory[historyIndex] = newTime;
      historyIndex = (historyIndex + 1) % MAX_HISTORY;
      networkTime = newTime;
      simID = parseCCID(sendCommand("AT+CCID", 800));
      lteStatus = parseLTE(sendCommand("AT+CEREG?", 800));
      networkStatus = parseNetwork(sendCommand("AT+CREG?", 800));
      networkOperator = parseNetworkOperator(sendCommand("AT+COPS?", 1000));
      mobileNumber = parseMobileNumber(sendCommand("AT+CNUM", 800));
    } else {
      rssi = 99;
      for (int i = 0; i < MAX_HISTORY; i++) {
        rssiHistory[i] = 99;
        timeHistory[i] = "No SIM";
      }
      historyIndex = 0;
      simID = "Unknown";
      lteStatus = "Not Connected";
      networkStatus = "Not Registered";
      httpStatus = "Network Not Available";
      networkOperator = "Unknown";
      mobileNumber = "Unknown";
    }

    // Debug status
    Serial.print("RSSI History: [");
    for (int i = 0; i < MAX_HISTORY; i++) {
      Serial.print(rssiHistory[(historyIndex + i) % MAX_HISTORY]);
      if (i < MAX_HISTORY - 1) Serial.print(", ");
    }
    Serial.println("]");
    Serial.print("Time History: [");
    for (int i = 0; i < MAX_HISTORY; i++) {
      Serial.print(timeHistory[(historyIndex + i) % MAX_HISTORY]);
      if (i < MAX_HISTORY - 1) Serial.print(", ");
    }
    Serial.println("]");
    Serial.println("SIM Status: " + simStatus);
    Serial.println("LTE Status: " + lteStatus);
    Serial.println("Network Status: " + networkStatus);
    Serial.println("Network Operator: " + networkOperator);
    Serial.println("Mobile Number: " + mobileNumber);

    // Send data to Firebase every 15s if network is available
    if (currentMillis - lastFirebaseUpdate >= firebaseInterval && lteStatus == "Connected" && (networkStatus == "Registered (Home)" || networkStatus == "Registered (Roaming)")) {
      sendToFirebase(25.0, 65.0);
      lastFirebaseUpdate = currentMillis;
    } else if (currentMillis - lastFirebaseUpdate >= firebaseInterval) {
      httpStatus = "Network Not Available";
      httpResponseBody = "None";
      Serial.println("Firebase skipped: Network not available");
    }

    Serial.println("Loop end at " + String(millis()) + "ms");
    lastLoop = currentMillis;
  }

  delay(10);
}

String sendCommand(String cmd, int timeout) {
  ecSerial.flush();
  ecSerial.println(cmd);
  Serial.println("Sent: " + cmd);
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    while (ecSerial.available()) {
      response += ecSerial.readString();
    }
    delay(1);
  }
  if (response != "") {
    Serial.println("Response: " + response);
  } else {
    Serial.println("No response received");
  }
  return response;
}

void initEC200U() {
  sendCommand("AT");
  sendCommand("ATE0");
  sendCommand("AT+CPIN?");
  sendCommand("AT+CCID");
  sendCommand("AT+CREG?");
  sendCommand("AT+CEREG?");
  sendCommand("AT+CSQ");
  sendCommand("AT+QLTS=2");
  sendCommand("AT+COPS?");
  sendCommand("AT+CNUM");

  sendCommand("AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\",1"); // Note:  Replace airtelgprs.com with your APN if using other sim
  sendCommand("AT+QIACT=1");
  delay(5000); // Wait for network activation
  sendCommand("AT+QHTTPCFG=\"contextid\",1");
  sendCommand("AT+QHTTPCFG=\"responseheader\",1");
}

void sendToFirebase(float temperature, float humidity) {
  String payload = "{\"temperature\":" + String(temperature) + ",\"humidity\":" + String(humidity) + "}";   // This codes sends sampl;e Hardcoded Data to Firebase Replace According to your needs

  // Set URL
  sendCommand("AT+QHTTPURL=" + String(strlen(firebaseURL)) + ",30", 800);
  delay(300);
  ecSerial.println(firebaseURL);
  delay(1000);

  // Send POST request
  sendCommand("AT+QHTTPPOST=" + String(payload.length()) + ",30,30", 800);
  delay(300);
  ecSerial.println(payload);
  delay(1500);

  // Read response
  String httpResp = sendCommand("AT+QHTTPREAD=80", 2000);
  httpStatus = parseHttpStatus(httpResp);
  httpResponseBody = parseHttpResponseBody(httpResp);

  // Debug output
  Serial.print("HTTP Response: ");
  Serial.println(httpResp);
  Serial.print("HTTP Status: ");
  Serial.println(httpStatus);
  Serial.print("Firebase Response Body: ");
  Serial.println(httpResponseBody);

  if (httpStatus != "Success (200 OK)") {
    Serial.println("Firebase POST failed. Check network or payload.");
  }
}

String parseCCID(String response) {
  int idx = response.indexOf("+CCID: ");
  return (idx >= 0) ? response.substring(idx + 7, idx + 27) : "Unknown";
}

String parseSIMStatus(String response) {
  if (response.indexOf("+CPIN: READY") >= 0) return "SIM Detected";
  if (response.indexOf("+CPIN: NOT INSERTED") >= 0) return "No SIM";
  if (response.indexOf("+CPIN: SIM PIN") >= 0) return "SIM PIN Required";
  return "Unknown";
}

String parseLTE(String response) {
  if (response.indexOf("0,1") >= 0 || response.indexOf("0,5") >= 0) return "Connected";
  return "Not Connected";
}

String parseNetwork(String response) {
  if (response.indexOf("0,1") >= 0) return "Registered (Home)";
  if (response.indexOf("0,5") >= 0) return "Registered (Roaming)";
  return "Not Registered";
}

int parseRSSI(String response) {
  int idx = response.indexOf("+CSQ: ");
  if (idx == -1) return 99;
  int comma = response.indexOf(",", idx);
  String rssiStr = response.substring(idx + 6, comma);
  int rssiVal = rssiStr.toInt();
  return (rssiVal >= 0 && rssiVal <= 31 || rssiVal == 99) ? rssiVal : 99;
}

String parseHttpStatus(String response) {
  if (response.indexOf("+QHTTPPOST: 0,200") >= 0) return "Success (200 OK)";
  if (response.indexOf("+QHTTPPOST: 0,400") >= 0) return "Bad Request (400)";
  if (response.indexOf("+QHTTPPOST: 0,401") >= 0) return "Unauthorized (401)";
  if (response.indexOf("+QHTTPPOST: 0,429") >= 0) return "Too Many Requests (429)";
  if (response.indexOf("+QHTTPPOST:") >= 0) return "Failed (Other Error)";
  return "No Response";
}

String parseHttpResponseBody(String response) {
  int start = response.indexOf("\n\n"); // Find the end of headers (double newline)
  if (start == -1) start = response.indexOf("\r\n\r\n"); // Handle different newline formats
  if (start >= 0) {
    start += 2; // Move past the double newline
    int end = response.indexOf("\r\nOK", start);
    if (end == -1) end = response.length();
    String body = response.substring(start, end);
    if (body.length() > 0) {
      int nameStart = body.indexOf("\"name\":\"") + 8;
      if (nameStart >= 8) {
        int nameEnd = body.indexOf("\"", nameStart);
        if (nameEnd > nameStart) {
          return sanitizeString(body.substring(nameStart, nameEnd));
        }
      }
    }
    return "None";
  }
  return "None";
}

String parseNetworkTime(String response) {
  int start = response.indexOf("+QLTS: \"") + 8;
  int end = response.indexOf("\"", start);
  if (start >= 8 && end > start) {
    return sanitizeString(response.substring(start, end));
  }
  return "Unknown";
}

String parseNetworkOperator(String response) {
  int start = response.indexOf("+COPS: 0,0,\"") + 12; // Move past "+COPS: 0,0,\"" to the operator name
  if (start >= 12) {
    int end = response.indexOf("\"", start); // Find the closing quote
    if (end > start) {
      return sanitizeString(response.substring(start, end)); // Extract "IND airtel"
    }
  }
  return "Unknown";
}

String parseMobileNumber(String response) {
  // Expected format: +CNUM: "", "919876543210", 129
  int firstQuote = response.indexOf("\"");
  if (firstQuote >= 0) {
    int secondQuote = response.indexOf("\"", firstQuote + 1); // Skip first empty string ("")
    if (secondQuote >= 0) {
      int thirdQuote = response.indexOf("\"", secondQuote + 1);
      int fourthQuote = response.indexOf("\"", thirdQuote + 1);
      if (thirdQuote >= 0 && fourthQuote > thirdQuote) {
        return response.substring(thirdQuote + 1, fourthQuote);
      }
    }
  }
  return "Unknown";
}

String sanitizeString(String str) {
  String result = str;
  result.replace("\r", "");
  result.replace("\n", "");
  result.replace("\t", "");
  result.replace("\b", "");
  result.replace("\f", "");
  result.replace("\"", "\\\"");
  return result;
}

void handleRoot(AsyncWebServerRequest *request) {
  Serial.println("Web request: / at " + String(millis()) + "ms");
  String html = "<!DOCTYPE html><html><head><title>EC200U LTE Dashboard</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<link href='https://fonts.googleapis.com/css2?family=Roboto:wght@400;700&display=swap' rel='stylesheet'>";
  html += "<script src='https://cdn.jsdelivr.net/npm/chart.js@3.9.1/dist/chart.min.js'></script>";
  html += "<script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.9.1/chart.min.js' onerror='document.getElementById(\"chartError\").style.display=\"block\"'></script>";
  html += "<style>";
  html += "body { font-family: 'Roboto', Arial, sans-serif; background: #f4f6f9; margin: 0; padding: 20px; color: #333; }";
  html += ".container { max-width: 800px; margin: auto; position: relative; }";
  html += "h1 { text-align: center; color: #1a73e8; }";
  html += ".card { background: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); padding: 20px; margin: 10px 0; transition: transform 0.2s; }";
  html += ".card:hover { transform: translateY(-2px); }";
  html += ".status { display: flex; justify-content: space-between; align-items: center; margin: 10px 0; font-size: 1.1em; }";
  html += ".label { font-weight: bold; color: #555; }";
  html += ".value { color: #333; }";
  html += ".signal-bars { display: flex; gap: 4px; width: 150px; height: 20px; }";
  html += ".bar { flex: 1; background: #ddd; border-radius: 2px; transition: background 0.3s; }";
  html += ".bar.filled { background: linear-gradient(90deg, #4CAF50, #81C784); }";
  html += ".http-status { font-weight: bold; }";
  html += ".alert { background: #f44336; color: white; padding: 10px; border-radius: 5px; margin: 10px 0; display: none; }";
  html += "#chartError { background: #ff9800; color: white; padding: 10px; border-radius: 5px; margin: 10px 0; display: none; }";
  html += ".chart-container { margin: 20px 0; width: 100%; height: 300px; border: 1px solid #ccc; }";
  html += "#rssiChart { width: 100%; height: 100%; border: 1px solid #000; }";
  html += ".button { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 0; }";
  html += ".button:hover { background-color: #45a049; }";
  html += ".loading-overlay { display: none; position: absolute; top: 0; left: 0; width: 100%; height: 100%; background: rgba(255, 255, 255, 0.8); backdrop-filter: blur(5px); z-index: 1000; justify-content: center; align-items: center; }";
  html += ".loading-overlay.active { display: flex; }";
  html += ".progress-bar { width: 300px; height: 20px; background: #e0e0e0; border-radius: 10px; overflow: hidden; }";
  html += ".progress { width: 0; height: 100%; background: #4CAF50; animation: progressAnimation 2s infinite; }";
  html += "@keyframes progressAnimation { 0% { width: 0; } 50% { width: 50%; } 100% { width: 100%; } }";
  html += ".loading-text { margin-top: 10px; color: #333; font-weight: bold; }";
  html += "@media (max-width: 600px) { .status { flex-direction: column; align-items: flex-start; } .signal-bars { width: 100px; } .chart-container { width: 100%; } .button { width: 100%; } .loading-overlay { width: 100%; } .progress-bar { width: 200px; } }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>Quectel EC200U Dashboard</h1>";
  html += "<div id='alert' class='alert'></div>";
  html += "<div id='chartError' class='alert'>Failed to load Chart.js or status data. Please check your internet connection or refresh the page.</div>";
  html += "<div class='card'>";
  html += "<div class='status'><span class='label'>SIM ID:</span><span class='value' id='simID'>" + simID + "</span></div>";
  html += "<div class='status'><span class='label'>SIM Status:</span><span class='value' id='simStatus'>" + simStatus + "</span></div>";
  html += "<div class='status'><span class='label'>LTE Status:</span><span class='value' id='lteStatus'>" + lteStatus + "</span></div>";
  html += "<div class='status'><span class='label'>Network Status:</span><span class='value' id='networkStatus'>" + networkStatus + "</span></div>";
  html += "<div class='status'><span class='label'>Network Operator:</span><span class='value' id='networkOperator'>" + networkOperator + "</span></div>";
  html += "<div class='status'><span class='label'>Mobile Number:</span><span class='value' id='mobileNumber'>" + mobileNumber + "</span></div>";
  html += "<div class='status'><span class='label'>Signal Strength (RSSI):</span><span class='value' id='rssi'>" + String(rssi) + "</span></div>";
  html += "<div class='status'><span class='label'>Signal Bars:</span><div class='signal-bars' id='signalBars'>";
  int signalBars = (rssi == 99) ? 0 : (rssi / 5) + 1;
  for (int i = 0; i < 6; i++) {
    html += "<div class='bar" + String(i < signalBars ? " filled" : "") + "'></div>";
  }
  html += "</div></div>";
  html += "<div class='status'><span class='label'>Network Time:</span><span class='value' id='networkTime'>" + networkTime + "</span></div>";
  html += "<div class='status'><span class='label'>HTTP Status:</span><span class='value http-status' id='httpStatus'>" + httpStatus + "</span></div>";
  html += "<div class='status'><span class='label'>Firebase Response:</span><span class='value' id='httpResponseBody'>" + httpResponseBody + "</span></div>";
  html += "</div>";
  html += "<button class='button' id='resetSimButton' onclick='resetSIM()'>Reset SIM</button>";
  html += "<div class='loading-overlay' id='loadingOverlay'>";
  html += "<div class='progress-bar'><div class='progress'></div></div>";
  html += "<div class='loading-text'>Resetting SIM... Please wait.</div>";
  html += "</div>";
  html += "<div class='card chart-container'>";
  html += "<canvas id='rssiChart'></canvas>";
  html += "</div>";
  html += "<script>";
  html += "function resetSIM() {";
  html += "  document.getElementById('loadingOverlay').classList.add('active');";
  html += "  document.getElementById('resetSimButton').disabled = true;";
  html += "  document.getElementById('simID').textContent = 'Unknown';";
  html += "  document.getElementById('simStatus').textContent = 'Unknown';";
  html += "  document.getElementById('lteStatus').textContent = 'Unknown';";
  html += "  document.getElementById('networkStatus').textContent = 'Unknown';";
  html += "  document.getElementById('networkOperator').textContent = 'Unknown';";
  html += "  document.getElementById('mobileNumber').textContent = 'Unknown';";
  html += "  document.getElementById('rssi').textContent = 'Unknown';";
  html += "  document.getElementById('signalBars').innerHTML = '';";
  html += "  for (let i = 0; i < 6; i++) {";
  html += "    let bar = document.createElement('div');";
  html += "    bar.className = 'bar';";
  html += "    document.getElementById('signalBars').appendChild(bar);";
  html += "  }";
  html += "  document.getElementById('networkTime').textContent = 'Unknown';";
  html += "  document.getElementById('httpStatus').textContent = 'Unknown';";
  html += "  document.getElementById('httpResponseBody').textContent = 'Unknown';";
  html += "  document.getElementById('alert').style.display = 'none';";
  html += "  if (window.rssiChart && typeof window.rssiChart.destroy === 'function') {";
  html += "    window.rssiChart.destroy();";
  html += "  }";
  html += "  document.getElementById('chartError').style.display = 'block';";
  html += "  document.getElementById('chartError').textContent = 'Chart disabled during reset.';";
  html += "  fetch('/reset-sim').then(response => response.json()).then(data => {";
  html += "    if (data.success) {";
  html += "      let interval = setInterval(() => {";
  html += "        fetch('/status').then(resp => resp.json()).then(status => {";
  html += "          if (status.simStatus !== 'Unknown') {";
  html += "            clearInterval(interval);";
  html += "            updateStatus();";
  html += "            document.getElementById('loadingOverlay').classList.remove('active');";
  html += "            document.getElementById('resetSimButton').disabled = false;";
  html += "          }";
  html += "        }).catch(err => console.error('Status fetch error:', err));";
  html += "      }, 1000);";
  html += "    } else {";
  html += "      console.error('Reset failed:', data.message);";
  html += "      document.getElementById('loadingOverlay').classList.remove('active');";
  html += "      document.getElementById('resetSimButton').disabled = false;";
  html += "    }";
  html += "  }).catch(err => {";
  html += "    console.error('Reset fetch error:', err);";
  html += "    document.getElementById('loadingOverlay').classList.remove('active');";
  html += "    document.getElementById('resetSimButton').disabled = false;";
  html += "  });";
  html += "}";
  html += "function updateStatus() {";
  html += "  console.log('Checking Chart.js availability:', typeof Chart);";
  html += "  fetch('/status').then(response => response.json()).then(data => {";
  html += "    console.log('Status data:', data);";
  html += "    document.getElementById('simID').textContent = data.simID;";
  html += "    document.getElementById('simStatus').textContent = data.simStatus;";
  html += "    document.getElementById('lteStatus').textContent = data.lteStatus;";
  html += "    document.getElementById('networkStatus').textContent = data.networkStatus;";
  html += "    document.getElementById('networkOperator').textContent = data.networkOperator;";
  html += "    document.getElementById('mobileNumber').textContent = data.mobileNumber;";
  html += "    document.getElementById('rssi').textContent = data.rssi;";
  html += "    document.getElementById('networkTime').textContent = data.networkTime;";
  html += "    document.getElementById('httpStatus').textContent = data.httpStatus;";
  html += "    document.getElementById('httpResponseBody').textContent = data.httpResponseBody;";
  html += "    document.getElementById('httpStatus').style.color = data.httpStatus.startsWith('Success') ? 'green' : (data.httpStatus.startsWith('Bad') || data.httpStatus.startsWith('Unauthorized') || data.httpStatus.startsWith('Failed') ? 'red' : 'gray');";
  html += "    let signalBars = document.getElementById('signalBars');";
  html += "    signalBars.innerHTML = '';";
  html += "    let bars = data.rssi == 99 ? 0 : Math.floor(data.rssi / 5) + 1;";
  html += "    for (let i = 0; i < 6; i++) {";
  html += "      let bar = document.createElement('div');";
  html += "      bar.className = 'bar' + (i < bars ? ' filled' : '');";
  html += "      signalBars.appendChild(bar);";
  html += "    }";
  html += "    let alert = document.getElementById('alert');";
  html += "    let alertMsg = '';";
  html += "    if (data.rssi >= 10 && data.rssi <= 31) alertMsg += '';";
  html += "    else if (data.rssi < 10 && data.rssi != 99) alertMsg += 'Low Signal Strength (RSSI: ' + data.rssi + '). ';";
  html += "    else if (data.rssi == 99) alertMsg += 'No Signal Detected. ';";
  html += "    if (data.lteStatus != 'Connected') alertMsg += 'LTE Not Connected. ';";
  html += "    if (data.networkStatus != 'Registered (Home)' && data.networkStatus != 'Registered (Roaming)') alertMsg += 'Network Not Registered. ';";
  html += "    if (!data.httpStatus.startsWith('Success')) alertMsg += 'HTTP Error: ' + data.httpStatus + '. ';";
  html += "    if (data.simStatus == 'No SIM' || data.simStatus == 'SIM PIN Required') alertMsg += 'SIM Issue: ' + data.simStatus + '. ';";
  html += "    alert.style.display = alertMsg ? 'block' : 'none';";
  html += "    alert.textContent = alertMsg || '';";
  html += "    if (typeof Chart !== 'undefined' && data.simStatus == 'SIM Detected') {";
  html += "      let ctx = document.getElementById('rssiChart').getContext('2d');";
  html += "      if (window.rssiChart && typeof window.rssiChart.destroy === 'function') {";
  html += "        console.log('Destroying previous chart');";
  html += "        window.rssiChart.destroy();";
  html += "      }";
  html += "      let validData = data.rssiHistory.filter(r => r >= 0 && r <= 31 || r == 99);";
  html += "      let validLabels = data.timeHistory.map(t => t === 'No SIM' ? 'No SIM' : (t === 'Unknown' ? 'No Time' : t));";
  html += "      if (validData.length === 0) {";
  html += "        validData = [0];";
  html += "        validLabels = ['No Data'];";
  html += "      }";
  html += "      try {";
  html += "        window.rssiChart = new Chart(ctx, {";
  html += "          type: 'line',";
  html += "          data: {";
  html += "            labels: validLabels,";
  html += "            datasets: [{";
  html += "              label: 'RSSI',";
  html += "              data: validData,";
  html += "              borderColor: '#4CAF50',";
  html += "              backgroundColor: 'rgba(76, 175, 80, 0.1)',";
  html += "              fill: true,";
  html += "              tension: 0.1";
  html += "            }]";
  html += "          },";
  html += "          options: {";
  html += "            responsive: true,";
  html += "            maintainAspectRatio: false,";
  html += "            scales: {";
  html += "              y: { beginAtZero: true, max: 31, title: { display: true, text: 'RSSI' } },";
  html += "              x: { title: { display: true, text: 'Time' } }";
  html += "            },";
  html += "            plugins: { legend: { display: true } }";
  html += "          }";
  html += "        });";
  html += "        let canvas = document.getElementById('rssiChart');";
  html += "        console.log('Canvas size:', canvas.width, 'x', canvas.height, 'Chart size:', window.rssiChart.width, 'x', window.rssiChart.height);";
  html += "        console.log('Chart rendered with RSSI:', validData, 'Time:', validLabels);";
  html += "        document.getElementById('chartError').style.display = 'none';";
  html += "      } catch (e) {";
  html += "        console.error('Chart rendering error:', e);";
  html += "        document.getElementById('chartError').style.display = 'block';";
  html += "        document.getElementById('chartError').textContent = 'Chart rendering error: ' + e.message;";
  html += "      }";
  html += "    } else if (data.simStatus != 'SIM Detected') {";
  html += "      if (window.rssiChart && typeof window.rssiChart.destroy === 'function') {";
  html += "        window.rssiChart.destroy();";
  html += "      }";
  html += "      document.getElementById('chartError').style.display = 'block';";
  html += "      document.getElementById('chartError').textContent = 'Chart disabled: No SIM detected.';";
  html += "    }";
  html += "  }).catch(err => {";
  html += "    console.error('Fetch error:', err);";
  html += "    document.getElementById('chartError').style.display = 'block';";
  html += "    document.getElementById('chartError').textContent = 'Error fetching status: ' + err.message;";
  html += "  });";
  html += "}";
  html += "setInterval(updateStatus, 5000);";
  html += "updateStatus();";
  html += "</script></body></html>";
  request->send(200, "text/html", html);
}

void handleStatus(AsyncWebServerRequest *request) {
  Serial.println("Web request: /status at " + String(millis()) + "ms");
  DynamicJsonDocument doc(1024);
  doc["simID"] = simID;
  doc["simStatus"] = simStatus;
  doc["lteStatus"] = lteStatus;
  doc["networkStatus"] = networkStatus;
  doc["networkOperator"] = networkOperator;
  doc["mobileNumber"] = mobileNumber;
  doc["rssi"] = rssi;
  doc["networkTime"] = networkTime;
  doc["httpStatus"] = httpStatus;
  doc["httpResponseBody"] = httpResponseBody;
  JsonArray rssiHist = doc.createNestedArray("rssiHistory");
  for (int i = 0; i < MAX_HISTORY; i++) {
    rssiHist.add(rssiHistory[(historyIndex + i) % MAX_HISTORY]);
  }
  JsonArray timeHist = doc.createNestedArray("timeHistory");
  for (int i = 0; i < MAX_HISTORY; i++) {
    timeHist.add(timeHistory[(historyIndex + i) % MAX_HISTORY]);
  }
  String json;
  serializeJson(doc, json);
  Serial.println("Status JSON: " + json);
  request->send(200, "application/json", json);
}

void handleResetSIM(AsyncWebServerRequest *request) {
  Serial.println("SIM reset requested at " + String(millis()) + "ms");
  DynamicJsonDocument doc(128);
  doc["success"] = false;
  doc["message"] = "Reset failed";

  resetEC200U(); // Perform hardware reset

  // Wait for module to stabilize
  delay(2000);

  simStatus = parseSIMStatus(sendCommand("AT+CPIN?", 800));
  Serial.println("SIM Status after reset: " + simStatus);
  if (simStatus != "Unknown") {
    doc["success"] = true;
    doc["message"] = "Reset successful";
  }

  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json);
}

void resetEC200U() {
  Serial.println("Resetting EC200U via RST pin");
  digitalWrite(RST_PIN, HIGH);  // Drive RST high to initiate reset
  delay(500);                  // Hold for at least 500 ms
  digitalWrite(RST_PIN, LOW); // Release reset
  Serial.println("EC200U reset completed");
}
