# vitotronic-interface-8266

ESP8266 WiFi to serial interface, built to connect to a Viessmann Vitotronic heating control. The heating control is accessed by using an _Optokoppler_, according to the instructions in the [OpenV wiki](http://openv.wikispaces.com/Bauanleitung+RaspberryPi) and made available as TCP/telnet server in the WiFi network. The communication bewteen the ESP8266 and the heating (via the _Optokoppler_) runs at 4800bps, 8 bits, even parity, 2 stop bits.

## Flashing the Firmware
1. Install the Arduino IDE plus the 8266 package following the instructions  at https://github.com/esp8266/Arduino#installing-with-boards-manager
2. Open, compile and upload the _vitotronic-interface-8266.ino_ sketch to your ESP8266.

## Setting Up the *Optokoppler* Hardware
* Connect the _Optokoppler_ to the heating, connect _IN/RX_ with the ESP8266 _RX_ line and _OUT/TX_ with the ESP8266 _TX_ line, as well as _VCC_ and _GND_ to 3.3V and Ground.
* Turn power on. The ESP should show up in your WiFi network. Afterwards you should be able to telnet into it at _<it's IP>:<the port you assigned>_.
* If you want to retrieve debugging output, connnect a serial port's _RX_ line to _GPIO2_ of the ESP8266 at 115200bps, 8N1. Don't forget to connect GND of the port as well to GND of the ESP8266.

## Configuring the Adapter
As long as the ESP of the adapter is not configured for connecting to a WiFi network it will provide an own WPA2-secured WiFi access point for configuration. To set up the adapter,
* Scan your WiFi environment for the SSID *"vitotronic-interface"*.
* Connect to this network, using the password *"vitotronic"*.
* In your web browser, visit http://192.168.4.1
* Provide the required configuration information:
  * **SSID** of the WiFi network to connect to (mandatory)
  * **Password** for the WiFi network
  * If you want to assign a static IP to the adapter, specify
    * **IP** address to be assigned to the adapter
    * **DNS** server address
    * **Gateway** address
    * **Subnet Mask**
  * The **Port** at which the adapter listens for an incoming connection (mandatory)
* Press "Submit" afterwards. The adapter will save the configuration, restart and connect to the given WiFi network. Afterwards the server will be reachable in the network at the IP (DHCP or static) and specified port. The server's IP is also pingable.
* To re-configure the adapter, connect _GPIO12_ to _GND_ for a short time (e.g. by a pushbutton). Thus, the existing configuration will be deleted and the adapter will enter setup mode again (see above).
