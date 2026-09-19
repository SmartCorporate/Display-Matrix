# ESP32 WiFi Matrix Display

ESP32 Wi-Fi firmware for a 160 x 8 RGB LED matrix made from five chained 32 x 8 modules. The display shows Bitcoin, Ethereum, EUR/USD, a clock, and an optional custom message. It includes a local web control panel, wireless firmware updates, diagnostics, and automatic Wi-Fi recovery.

## Main Features

- Device name and setup network: `ESP32WiFiDisplay`
- Local control panel: `http://ESP32WiFiDisplay.local`
- Bitcoin and Ethereum prices in US dollars
- Bitcoin and Ethereum 24-hour percentage change
- 24-hour price charts with 24 samples stretched across 48 columns
- White chart axes and green/red hourly chart segments
- EUR/USD value with direction arrow, without a chart
- One-time Bitcoin and Ethereum all-time-high alerts
- Optional scrolling extra message
- Date and time display in American English
- Adjustable brightness, scrolling speed, and display mode
- Animated 160 x 8 preview in the web control panel
- Device status and debug information in the web control panel
- Remote reboot button
- Wi-Fi network reconfiguration from the web control panel
- Blinking red `NO WIFI` warning with automatic reconnection
- Arduino OTA firmware updates over Wi-Fi
- Automatic restart every three hours

## Hardware

- DOIT ESP32 DEVKIT V1
- Silicon Labs CP210x USB-to-UART interface
- Five chained 32 x 8 WS2812/NeoPixel-compatible matrix modules
- A regulated 5 V, 20 A external power supply
- A dedicated power distribution/control board for the LED modules
- A common ground between the ESP32 and LED power supply

### Wiring

| ESP32 / supply | Matrix connection | Purpose |
| --- | --- | --- |
| `GPIO 23` | First module `DIN` | LED data |
| 20 A supply through the power board | Matrix `5V` | Distributed LED power |
| Power board `GND` | Matrix `GND` | Distributed LED power ground |
| ESP32 `GND` | External `GND` | Common signal reference |

The installed system powers the matrices from a dedicated 20 A supply and power board. Do not route the matrix load through the ESP32 board. The power board ground and ESP32 ground must remain connected to provide a common data-signal reference. A 3.3 V-to-5 V logic level shifter on the data line is recommended when wiring is long or the signal is unreliable. A resistor on the data input and suitable bulk capacitance across the matrix power distribution are also recommended for a permanent installation.

## Software Requirements

- PlatformIO Core or the PlatformIO extension for Visual Studio Code
- Espressif 32 PlatformIO platform
- A USB data cable for the first installation or recovery
- Windows CP210x driver for USB programming

The project dependencies are declared in [`platformio.ini`](platformio.ini):

- Adafruit GFX Library
- Adafruit NeoMatrix
- ArduinoJson
- ArduinoHttpClient
- WiFiManager
- ArduinoOTA, ESPmDNS, Preferences, WebServer, and WiFi from the ESP32 framework

## First Installation by USB

1. Connect the DOIT ESP32 DEVKIT V1 with a USB data cable.
2. Find its COM port with:

   ```powershell
   platformio device list
   ```

3. Build and upload the firmware, replacing `COM3` when necessary:

   ```powershell
   platformio run -e esp32doit-devkit-v1 --target upload --upload-port COM3
   ```

4. If no saved Wi-Fi network is available, connect a phone or computer to the temporary network named `ESP32WiFiDisplay`.
5. Select the home Wi-Fi network and enter its password in the WiFiManager setup page.
6. After connection, open `http://ESP32WiFiDisplay.local` in a browser.

The serial monitor runs at 115200 baud:

```powershell
platformio device monitor --port COM3 --baud 115200
```

## Startup Sequence

At power-on, the firmware:

1. Loads saved display settings and remembered ATH values from ESP32 nonvolatile storage.
2. Connects to the saved Wi-Fi network or opens the `ESP32WiFiDisplay` setup network.
3. Starts mDNS, the web server, and Arduino OTA.
4. Shows centered white `WELCOME` for 1.5 seconds.
5. Shows centered white `WIFI CONNECTED` for 1.5 seconds.
6. Downloads current prices and 24-hour chart data.
7. Starts the configured display rotation.

The firmware automatically restarts after three hours of operation to reduce the chance of a long-running lockup.

## Display Rotation

In the default `all` mode, the normal sequence is:

1. Bitcoin price, 24-hour percentage, direction arrow, and chart
2. Ethereum price, 24-hour percentage, direction arrow, and chart
3. EUR/USD value and direction arrow
4. Extra message, only when the field is not empty
5. Date and time

The entire sequence scrolls across the five chained modules as one 160 x 8 display.

### Price Charts

Bitcoin and Ethereum use the latest 24 hourly samples returned by CoinGecko. Each chart is 48 columns wide:

- The vertical and horizontal axes are white.
- The chart uses seven drawable vertical levels above the horizontal axis.
- Each hourly segment is green when its value is at least the previous hour's value.
- Each hourly segment is red when its value is below the previous hour's value.

EUR/USD intentionally has no chart because its small movements were not consistently readable at eight pixels high.

### 24-Hour Change

Bitcoin and Ethereum show CoinGecko's 24-hour percentage change. A positive value is green and uses an upward arrow. A negative value is red and uses a downward arrow.

### ATH Alerts

The firmware stores the most recently reported all-time high for Bitcoin and Ethereum in nonvolatile storage. The first valid API response establishes the baseline and does not produce an alert.

When CoinGecko later reports a higher ATH, the display shows one scrolling gold alert:

- `BITCOIN NEW ATH $...`
- `ETHEREUM NEW ATH $...`

The new ATH is saved immediately, so the same alert is not repeated after a normal restart or the next data refresh.

## Web Control Panel

Open either:

- `http://ESP32WiFiDisplay.local`
- The IP address printed in the serial monitor or shown by the router

The page uses American English and is served directly by the ESP32.

### Display Content

- **Extra message:** optional text displayed after the market information
- **Display mode:** all content, message only, prices only, or clock only
- **Brightness:** matrix brightness from 1 to 255
- **Scroll speed:** delay between animation steps from 1 to 100 milliseconds
- **Save to display:** stores settings in nonvolatile memory
- **Force reboot:** shows centered red `REBOOT PRESSED` for two seconds and restarts

After saving, the page reports whether the extra message was loaded, cleared, or could not be saved.

### Live Display Preview

The 160 x 8 canvas previews the selected mode, text, market values, brightness, and scrolling speed. It is an approximate browser rendering of the physical LED output and updates while the controls are edited.

### Device Status and Debug

The status section refreshes every two seconds and reports:

- Current firmware activity
- Wi-Fi connection and RSSI signal level
- Uptime
- Free heap memory
- Time since the last successful price update
- HTTP response codes for price and chart services
- The latest detected problem

An RSSI closer to zero is stronger. Values below approximately `-80 dBm` indicate a weak connection and may cause slower updates or disconnections.

## Changing the Wi-Fi Network

Use **Change Wi-Fi network** in the web control panel before moving the display to a different router:

1. Confirm the reset in the browser.
2. The display shows `WIFI SETUP` and restarts.
3. The old Wi-Fi credentials are removed.
4. Connect a phone or computer to the `ESP32WiFiDisplay` setup network.
5. Select the new Wi-Fi network and enter its password.

Display settings and remembered ATH values are not erased by this operation.

## Wi-Fi Loss and Recovery

If an established Wi-Fi connection is lost during normal operation, the matrix stops the current rotation and flashes centered red `NO WIFI`. The ESP32 attempts to reconnect every ten seconds. The warning disappears automatically and normal display operation resumes after reconnection.

## Market Data

Market data comes from the public CoinGecko API over HTTPS. The firmware requests:

- Current BTC and ETH values in USD and EUR
- BTC and ETH market data with sparkline history
- 24-hour percentage changes
- Reported all-time-high values

EUR/USD is derived from the Bitcoin USD and EUR values returned in the same response. Prices and charts refresh every ten minutes. HTTPS certificate validation is currently disabled with `setInsecure()`, so the connection is encrypted but the remote certificate is not authenticated.

## Wireless OTA Updates

After the first USB installation, normal firmware updates can be sent over Wi-Fi:

```powershell
platformio run -e esp32doit-devkit-v1-ota --target upload --upload-port ESP32WiFiDisplay.local
```

If mDNS is unavailable, use the current IP address:

```powershell
platformio run -e esp32doit-devkit-v1-ota --target upload --upload-port 10.0.0.105
```

The computer and ESP32 must be on the same local network. OTA uses TCP port `3232`. Keep USB available as a recovery method if an incomplete or faulty firmware prevents Wi-Fi startup.

## Local HTTP API

The control panel uses these endpoints:

| Method | Endpoint | Function |
| --- | --- | --- |
| `GET` | `/` | Web control panel |
| `GET` | `/api/settings` | Read saved display settings |
| `POST` | `/api/settings` | Save display settings |
| `GET` | `/api/status` | Read live diagnostics and market values |
| `POST` | `/api/restart` | Show the reboot message and restart |
| `POST` | `/api/wifi/reset` | Remove Wi-Fi credentials and start setup mode |

The HTTP API is intended for a trusted home network and does not currently require authentication.

## Project Structure

```text
.
|-- platformio.ini              PlatformIO environments and dependencies
|-- src/
|   `-- main.cpp                Firmware, web page, API, display, and OTA logic
|-- MatrixDisplayWiFi-v2.ico    Current Windows desktop shortcut icon
|-- MatrixDisplayWiFi-v2.png    Source image for the current icon
|-- MatrixDisplayWiFi.ico       Original icon version
`-- MatrixDisplayWiFi.png       Original icon source image
```

## Troubleshooting

### The control panel does not open

- Try the ESP32 IP address instead of the `.local` address.
- Confirm the computer and ESP32 are on the same network.
- Check the router client list for `ESP32WiFiDisplay`.
- Read the serial monitor at 115200 baud.

### The display shows `NO WIFI`

- Wait for automatic reconnection.
- Move the display closer to the router if RSSI is weak.
- Restart the router or ESP32 if the network remains unavailable.
- Use the Wi-Fi setup flow if the router name or password changed.

### OTA upload fails

- Confirm that the web control panel is reachable first.
- Use the numeric IP address instead of mDNS.
- Allow PlatformIO and TCP port `3232` through the local firewall.
- Reconnect USB and upload with the USB environment when OTA recovery is impossible.

### The LEDs flicker or show incorrect colors

- Confirm that the ESP32 and matrix power supply share ground.
- Check the dedicated 20 A supply and power distribution board connections.
- Shorten the data wire or add a logic level shifter.
- Add power injection between modules for long chains.

## Current Resource Usage

The current build uses approximately:

- 1,099,989 of 1,310,720 firmware bytes: 83.9%
- 52,316 of 327,680 static RAM bytes: 16.0%
- About 215-221 KB of free heap during normal operation

The firmware partition has roughly 210 KB available for additional compiled code. Runtime values can vary depending on network activity and JSON processing.

## Security Notes

- The local web API has no password protection.
- OTA authentication is not configured.
- HTTPS server certificate validation for market data is disabled.
- Use the device only on a trusted private network until authentication and certificate validation are added.
