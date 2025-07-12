# ESP32-EC200U-LTE-Dashboard

Hey there! I built this **ESP32-EC200U-LTE-Dashboard** after messing around with an ESP32 and a Quectel EC200U LTE module one weekend. It’s a fun web dashboard I whipped up to keep an eye on LTE stuff like SIM status, signal strength, and more, all while tossing some data to Firebase. It’s been a blast to create, and I hope you enjoy tinkering with it too!

## What’s Cool About It?

- **Live Updates**: See your SIM ID, status, LTE connection, network details, mobile number, RSSI, and time as they happen!
- **Cool Visuals**: Check out signal bars and a chart tracking RSSI over time.
- **Firebase Support**: Send temperature and humidity readings to Firebase Realtime Database.
- **SIM Reset**: Hit a button to reset the EC200U with a hardware pin—super handy!
- **Easy Access**: Just type `http://esp32-ec200u.local` in your browser.

## Why I Made This?

I got curious about hooking up an LTE module to my ESP32 and thought, “Why not make it useful?” This project is great for:
- Learning IoT and LTE hands-on.
- Setting up a quick telemetry system for remote gadgets.
- Playing with Firebase to store data.
- Building a project that looks good and works!

## What You’ll Need

- **ESP32 Board**: I used an ESP32-WROOM-32—works like a charm!
- **Quectel EC200U LTE Module**: The star of the show for connectivity.
- **UART Setup**: Connect EC200U TX to ESP32 GPIO16 (RX2) and RX to GPIO17 (TX2).
- **RST Pin**: Link it to ESP32 GPIO4 for resets.
- **Wi-Fi**: A solid network connection.

## What to Install

- **Arduino IDE**: Download it from [Arduino](https://www.arduino.cc/en/software) and add ESP32 support via Board Manager (search "esp32" by Espressif).
- **Libraries**: Install these via Library Manager:
  - `WiFi.h`
  - `ESPmDNS.h`
  - `ESPAsyncWebServer.h`
  - `HardwareSerial.h`
  - `ArduinoJson.h`

## How to Set It Up

### 1. Get Arduino IDE Ready
- Download the [Arduino IDE](https://www.arduino.cc/en/software) -> Install it -> Add ESP32 boards in **Board Manager**.

### 2. Add the Libraries
- Open up the Arduino IDE -> **Sketch > Include Library > Manage Libraries** -> Search and install those libraries.

### 3. Tweak the Code
- Open `sketch.ino` and update:
  - `ssid` and `password` with your Wi-Fi details.
  - `firebaseURL` with your Firebase URL.
  - Change the APN in `AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\",1` if your network needs it.
- **Pro Tip**: Double-check your APN with your provider—it saved me a headache!
- **Note**: I used "airtelgprs" because that’s the APN for my Airtel SIM. If you live in India, be aware that Jio SIMs won’t work with this setup—they don’t play nice with the EC200U!

### 4. Wire It Up
- Hook up EC200U TX -> ESP32 GPIO16 (RX2) and RX -> GPIO17 (TX2).
- Connect EC200U RST -> ESP32 GPIO4.
- Pop in a SIM card and power it on!

### 5. Upload the Sketch
- Load `sketch.ino` in Arduino IDE -> Pick your ESP32 board and port (**Tools > Board** and **Tools > Port**) -> Click **Upload**.

### 6. Check Out the Dashboard
- Open your browser -> Go to `http://esp32-ec200u.local` or the IP from Serial Monitor (115200 baud).
- Enjoy the live action!

## Why Reset SIM Might Be Useful

The SIM reset feature is a lifesaver in a few scenarios! Here’s why I added it:
- **Fix Connectivity Issues**: If your LTE drops or the module freezes, a reset can kick it back to life.
- **SIM Swap Trouble**: Swapped a new SIM? A reset helps the module recognize it without a full reboot.
- **Debugging Aid**: Stuck on a weird status? Resetting clears the slate for a fresh start.
- I’ve used it myself when my signal went wonky during testing—worked like a charm!

## How to Use It

- **Auto-Refresh**: It updates every 5 seconds—watch it change!
- **Reset SIM**: Click **"Reset SIM"** to reboot the EC200U.
- **Debug Mode**: Open Serial Monitor (115200 baud) for the nitty-gritty details.
- **Real-World Example**: I moved my setup around to see RSSI dips -> Adjusted my antenna and boom, better signal!

## If Things Go Wrong

- **Dashboard Won’t Load?** -> Make sure Wi-Fi’s up and try `ping esp32-ec200u.local`.
- **Mobile Number Missing?** -> Check if your SIM supports `AT+CNUM` and peek at the Serial output.
- **Firebase Acting Up?** -> Verify `firebaseURL` and network; I had to tweak `AT+QIACT` timing once.
- **No Signal?** -> Ensure the antenna’s tight and run `AT+CREG?` to check registration.

## Let’s Work Together

Love this? Help me improve it!
- Fork the repo -> Share issues or pull requests.
- Keep commit messages clear (e.g., “Fixed RSSI chart bug”).
- Drop an issue before big changes -> Let’s chat!

## License

This project is released under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html). Feel free to use, modify, and share it, but please keep it open source! Add a `LICENSE` file with the GPL-3.0 text if you want (check the link for the full version).

## Shoutouts

- Big thanks to the IoT community and Quectel’s docs for guiding me.
- Shoutout to Arduino and Espressif for their awesome tools!
