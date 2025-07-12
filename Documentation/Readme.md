# ESP32-EC200U-LTE-Dashboard Function Documentation

Hey! This is the detailed rundown of the functions I put together for the **ESP32-EC200U-LTE-Dashboard**. I built this after messing around with an ESP32 and a Quectel EC200U LTE module, and I’ve tried to lay it out clearly for anyone who wants to dive into the code. It’s released under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html), so feel free to modify it—just keep it open source! Below, I’ve grouped the functions by what they do, listed their parameters, and explained how they work, with code snippets to show you how to use them.

## Function Listings

### Initialization Functions

#### `setup()`
- **Parameters**:
  - None
- **Working**: This is the kickoff! I start the Serial monitor and UART2 (GPIO16 RX, GPIO17 TX) for the EC200U, set GPIO4 as an output for the reset pin, and connect to Wi-Fi with my credentials. Then, I set up mDNS with `esp32-ec200u` and initialize the web server with routes. Here’s the actual setup:
  ```cpp
  void setup() {
    Serial.begin(115200);
    ecSerial.begin(115200, SERIAL_8N1, 16, 17);
    delay(1000);
    pinMode(RST_PIN, OUTPUT);
    digitalWrite(RST_PIN, LOW);
    delay(100);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nConnected. IP: " + WiFi.localIP().toString());
    if (MDNS.begin(mdnsHostname)) {
      Serial.println("mDNS started: http://" + String(mdnsHostname) + ".local");
    }
    initEC200U();
    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/reset-sim", HTTP_GET, handleResetSIM);
    server.begin();
    Serial.println("Async web server started");
  }
  ```
  It’s the foundation that gets everything running!

#### `initEC200U()`
- **Parameters**:
  - None
- **Working**: This preps the EC200U module by sending initial AT commands to check status, configure the network, and set up HTTP. I added a 5-second delay after activating the network to let it settle. Here’s the code:
  ```cpp
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
    sendCommand("AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\",1");
    sendCommand("AT+QIACT=1");
    delay(5000);
    sendCommand("AT+QHTTPCFG=\"contextid\",1");
    sendCommand("AT+QHTTPCFG=\"responseheader\",1");
  }
  ```
  It’s like a startup checklist I came up with!

#### `resetEC200U()`
- **Parameters**:
  - None
- **Working**: This resets the EC200U by driving GPIO4 high for 500ms. I use it when the module hangs, and it logs the action. Here’s the implementation:
  ```cpp
  void resetEC200U() {
    Serial.println("Resetting EC200U via RST pin");
    digitalWrite(RST_PIN, HIGH);
    delay(500);
    digitalWrite(RST_PIN, LOW);
    Serial.println("EC200U reset completed");
  }
  ```
  It’s a quick fix I figured out during testing!

### Communication Functions

#### `sendCommand(String cmd, int timeout = 800)`
- **Parameters**:
  - `cmd`: The AT command string to send to the EC200U
  - `timeout`: The maximum time to wait for a response (defaults to 800ms)
- **Working**: This handles communication with the EC200U by flushing the serial buffer, sending the command, and collecting the response within the timeout. It logs everything for debugging. Here’s how I use it:
  ```cpp
  String response = sendCommand("AT+CSQ", 1000); // Example with custom timeout
  Serial.println("Response: " + response);
  ```
  It took some fiddling with the timing, but it’s solid now! The actual code is:
  ```cpp
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
    if (response != "") Serial.println("Response: " + response);
    else Serial.println("No response received");
    return response;
  }
  ```

### Data Parsing Functions

#### `parseCCID(String response)`
- **Parameters**:
  - `response`: The raw string returned by the EC200U
- **Working**: I extract the SIM CCID from the AT+CCID response by finding "+CCID: " and taking the next 20 characters. Here’s the code:
  ```cpp
  String parseCCID(String response) {
    int idx = response.indexOf("+CCID: ");
    return (idx >= 0) ? response.substring(idx + 7, idx + 27) : "Unknown";
  }
  ```
  It was a straightforward fix after some testing!

#### `parseSIMStatus(String response)`
- **Parameters**:
  - `response`: The raw string returned by the EC200U
- **Working**: This checks the SIM status from AT+CPIN? by looking for specific responses. Here’s the logic:
  ```cpp
  String parseSIMStatus(String response) {
    if (response.indexOf("+CPIN: READY") >= 0) return "SIM Detected";
    if (response.indexOf("+CPIN: NOT INSERTED") >= 0) return "No SIM";
    if (response.indexOf("+CPIN: SIM PIN") >= 0) return "SIM PIN Required";
    return "Unknown";
  }
  ```
  It’s my first debug step when the SIM misbehaves!

#### `parseLTE(String response)`
- **Parameters**:
  - `response`: The raw string returned by the EC200U
- **Working**: I determine LTE status from AT+CEREG? by checking for "0,1" or "0,5". Here’s the code:
  ```cpp
  String parseLTE(String response) {
    if (response.indexOf("0,1") >= 0 || response.indexOf("0,5") >= 0) return "Connected";
    return "Not Connected";
  }
  ```
  It’s a fast way to check the network status!

#### `parseNetwork(String response)`
- **Parameters**:
  - `response`: The raw string returned by the EC200U
- **Working**: This gets network registration from AT+CREG? by looking for "0,1" or "0,5". Here’s how:
  ```cpp
  String parseNetwork(String response) {
    if (response.indexOf("0,1") >= 0) return "Registered (Home)";
    if (response.indexOf("0,5") >= 0) return "Registered (Roaming)";
    return "Not Registered";
  }
  ```
  It helped me sort out registration glitches!

#### `parseRSSI(String response)`
- **Parameters**:
  - `response`: The raw string returned by the EC200U
- **Working**: I pull the RSSI from AT+CSQ by parsing the value after "+CSQ: " and validating it. Here’s the code:
  ```cpp
  int parseRSSI(String response) {
    int idx = response.indexOf("+CSQ: ");
    if (idx == -1) return 99;
    int comma = response.indexOf(",", idx);
    String rssiStr = response.substring(idx + 6, comma);
    int rssiVal = rssiStr.toInt();
    return (rssiVal >= 0 && rssiVal <= 31 || rssiVal == 99) ? rssiVal : 99;
  }
  ```
  It’s crucial for signal strength, and I tweaked it for edge cases!

#### `parseHttpStatus(String response)`
- **Parameters**:
  - `response`: The raw string returned by the EC200U
- **Working**: This decodes HTTP status from AT+QHTTPPOST responses with specific error codes. Here’s the implementation:
  ```cpp
  String parseHttpStatus(String response) {
    if (response.indexOf("+QHTTPPOST: 0,200") >= 0) return "Success (200 OK)";
    if (response.indexOf("+QHTTPPOST: 0,400") >= 0) return "Bad Request (400)";
    if (response.indexOf("+QHTTPPOST: 0,401") >= 0) return "Unauthorized (401)";
    if (response.indexOf("+QHTTPPOST: 0,429") >= 0) return "Too Many Requests (429)";
    if (response.indexOf("+QHTTPPOST:") >= 0) return "Failed (Other Error)";
    return "No Response";
  }
  ```
  It took some debugging to make Firebase sends work!

#### `parseHttpResponseBody(String response)`
- **Parameters**:
  - `response`: The raw string returned by the EC200U
- **Working**: I extract the HTTP response body, looking for the "name" field after headers. Here’s the code:
  ```cpp
  String parseHttpResponseBody(String response) {
    int start = response.indexOf("\n\n");
    if (start == -1) start = response.indexOf("\r\n\r\n");
    if (start >= 0) {
      start += 2;
      int end = response.indexOf("\r\nOK", start);
      if (end == -1) end = response.length();
      String body = response.substring(start, end);
      if (body.length() > 0) {
        int nameStart = body.indexOf("\"name\":\"") + 8;
        if (nameStart >= 8) {
          int nameEnd = body.indexOf("\"", nameStart);
          if (nameEnd > nameStart) return sanitizeString(body.substring(nameStart, nameEnd));
        }
      }
    }
    return "None";
  }
  ```
  It was a puzzle, but it shows Firebase feedback!

#### `parseNetworkTime(String response)`
- **Parameters**:
  - `response`: The raw string returned by the EC200U
- **Working**: I pull the network time from AT+QLTS=2 by extracting the quoted value. Here’s how:
  ```cpp
  String parseNetworkTime(String response) {
    int start = response.indexOf("+QLTS: \"") + 8;
    int end = response.indexOf("\"", start);
    if (start >= 8 && end > start) return sanitizeString(response.substring(start, end));
    return "Unknown";
  }
  ```
  It’s neat to see the module’s time sync!

#### `parseNetworkOperator(String response)`
- **Parameters**:
  - `response`: The raw string returned by the EC200U
- **Working**: I get the operator name from AT+COPS? by taking text after "+COPS: 0,0,\"". Here’s the code:
  ```cpp
  String parseNetworkOperator(String response) {
    int start = response.indexOf("+COPS: 0,0,\"") + 12;
    if (start >= 12) {
      int end = response.indexOf("\"", start);
      if (end > start) return sanitizeString(response.substring(start, end));
    }
    return "Unknown";
  }
  ```
  It tells me who I’m connected to!

#### `parseMobileNumber(String response)`
- **Parameters**:
  - `response`: The raw string returned by the EC200U
- **Working**: I extract the mobile number from AT+CNUM by finding it between quotes. Here’s the logic:
  ```cpp
  String parseMobileNumber(String response) {
    int firstQuote = response.indexOf("\"");
    if (firstQuote >= 0) {
      int secondQuote = response.indexOf("\"", firstQuote + 1);
      if (secondQuote >= 0) {
        int thirdQuote = response.indexOf("\"", secondQuote + 1);
        int fourthQuote = response.indexOf("\"", thirdQuote + 1);
        if (thirdQuote >= 0 && fourthQuote > thirdQuote) return response.substring(thirdQuote + 1, fourthQuote);
      }
    }
    return "Unknown";
  }
  ```
  It took some head-scratching, but it’s solid!

#### `sanitizeString(String str)`
- **Parameters**:
  - `str`: The input string to clean up
- **Working**: I clean the string by removing control characters and escaping quotes. Here’s the code:
  ```cpp
  String sanitizeString(String str) {
    String result = str;
    result.replace("\r", "").replace("\n", "").replace("\t", "").replace("\b", "").replace("\f", "").replace("\"", "\\\"");
    return result;
  }
  ```
  It’s a little fix to keep data tidy!

### Data Sending Functions

#### `sendToFirebase(float temperature, float humidity)`
- **Parameters**:
  - `temperature`: The temperature value to send
  - `humidity`: The humidity value to send
- **Working**: I send a JSON payload to Firebase via HTTP POST, handling the URL and response. Here’s the implementation:
  ```cpp
  void sendToFirebase(float temperature, float humidity) {
    String payload = "{\"temperature\":" + String(temperature) + ",\"humidity\":" + String(humidity) + "}";
    sendCommand("AT+QHTTPURL=" + String(strlen(firebaseURL)) + ",30", 800);
    delay(300);
    ecSerial.println(firebaseURL);
    delay(1000);
    sendCommand("AT+QHTTPPOST=" + String(payload.length()) + ",30,30", 800);
    delay(300);
    ecSerial.println(payload);
    delay(1500);
    String httpResp = sendCommand("AT+QHTTPREAD=80", 2000);
    httpStatus = parseHttpStatus(httpResp);
    httpResponseBody = parseHttpResponseBody(httpResp);
    Serial.print("HTTP Response: ");
    Serial.println(httpResp);
  }
  ```
  It was a process to get the timing right, but it logs data well!

### Web Handling Functions

#### `handleRoot(AsyncWebServerRequest *request)`
- **Parameters**:
  - `request`: Pointer to the web server request object
- **Working**: This serves the dashboard HTML with live data and styling. Here’s a snippet of the HTML generation:
  ```cpp
  void handleRoot(AsyncWebServerRequest *request) {
    String html = "<!DOCTYPE html><html><head><title>EC200U LTE Dashboard</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    // ... (styles and scripts omitted for brevity)
    html += "<body><div class='container'><h1>Quectel EC200U Dashboard</h1>";
    html += "<div class='status'><span class='label'>SIM ID:</span><span class='value' id='simID'>" + simID + "</span></div>";
    // ... (more status divs)
    html += "</div></body></html>";
    request->send(200, "text/html", html);
  }
  ```
  I had a good time styling it to look decent!

#### `handleStatus(AsyncWebServerRequest *request)`
- **Parameters**:
  - `request`: Pointer to the web server request object
- **Working**: I return a JSON status update for the dashboard. Here’s the code:
  ```cpp
  void handleStatus(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(1024);
    doc["simID"] = simID;
    doc["simStatus"] = simStatus;
    // ... (other fields)
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  }
  ```
  It keeps the dashboard updated—I tweaked the JSON for smoothness!

#### `handleResetSIM(AsyncWebServerRequest *request)`
- **Parameters**:
  - `request`: Pointer to the web server request object
- **Working**: This triggers a reset and returns a JSON response. Here’s the logic:
  ```cpp
  void handleResetSIM(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(128);
    doc["success"] = false;
    doc["message"] = "Reset failed";
    resetEC200U();
    delay(2000);
    simStatus = parseSIMStatus(sendCommand("AT+CPIN?", 800));
    if (simStatus != "Unknown") {
      doc["success"] = true;
      doc["message"] = "Reset successful";
    }
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  }
  ```
  It’s my go-to fix for SIM hiccups!

## Notes

This code is a work in progress—I learned a lot while building it, and I’m excited to share it under the GPL-3.0 license. If you spot a bug or have an idea to make it better, let me know! I’d love to see how you put it to use or improve it.
- Thankyou !
