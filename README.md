# ESP32_OldTVBacklight01

**ESP32_OldTVBacklight01** is a Cyberpunk-inspired smart controller that resurrects a 30-year-old portable TV backlight. This project transforms "e-waste" into a functional piece of tech art, featuring a hacker-style UI and internet-connected capabilities.
![Cyberpunk Lamp Thumbnail](/thumbnail.jpg)

Video link: https://youtu.be/SuoXpwKDSis

## 🚀 Features
* **1,000V Power Control:** Safely harvesting and switching the original high-voltage CCFL inverter system using a MOSFET circuit.
* **Modern Brain:** Powered by an **ESP32-C3 SuperMini** with integrated Wi-Fi and Bluetooth.
* **Smart Interface:** 3.2-inch TFT touchscreen displaying real-time weather, temperature, humidity, and NTP-synced time.
* **Cyberpunk Aesthetic:** Custom hacker-style boot sequence and randomly displayed inspirational quotes.
* **Handcrafted Design:** Housed in a custom pine wood frame with dedicated cooling vents for hardware stability.

## 🛠️ Hardware Requirements
* **Microcontroller:** ESP32-C3 SuperMini.
* **Display:** 3.2" TFT LCD Touchscreen (driven via SPI).
* **Power:** 12V DC input, stepped down to 5V via a buck converter (AMS1117-5V) for the electronics.
* **Lighting:** Original CCFL backlight tube and inverter from a vintage portable TV.
* **Switching:** MOSFET circuit to allow the ESP32 to toggle the high-voltage lamp.

![schematic](/schematic.jpg)

## 💻 Software & Libraries
This project is built using the **Arduino IDE**. Ensure you have the following libraries installed:
* `Adafruit_ST7789` (Screen control).
* Touch and Wi-Fi support libraries.
* Time sync via NTP server.
* Weather data fetched via HTTP from `wttr.in`.

## ⚠️ Safety Warning
This project involves **High Voltage (approx. 1,000 Volts)**. 
* **Do not touch** the transformers or lamp leads while the device is powered on. 
* High voltage can arc through the air and is extremely dangerous. 
* Proceed with caution and ensure all high-voltage joints are secured with heavy-duty wiring.

## 📂 Installation
1.  Clone the repository: `git clone https://github.com/yourusername/esp32_oldtvbacklight01.git`
2.  Open the source code in Arduino IDE.
3.  Configure your Wi-Fi credentials in the `config.h` file.
4.  Select **ESP32-C3** as your board and upload.

---
*Created by **SparkWorks**. Old light, new soul.*
