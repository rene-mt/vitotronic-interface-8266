# vitotronic-interface-8266

**Currently I am working on a way to allow dynamic configuration of the ESP server in the [develop](https://github.com/rene-mt/vitotronic-interface-8266/tree/develop) branch.**

Simple ESP8266 WiFi to serial interface, built to connect to a Viessmann Vitotronic heating control. The heating control is accessed by using an _Optokoppler_, according to the instructions in the [OpenV wiki](http://openv.wikispaces.com/Bauanleitung+RaspberryPi) and made available as TCP/telnet server in the WiFi network. The communication bewteen the ESP8266 and the heating (via the _Optokoppler_) runs at 4800bps, 8 bits, even parity, 2 stop bits.

The source is based on the [ESP8266 Arduino WiFi Telnet example](https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WiFi/examples/WiFiTelnetToSerial).

## How to set up
1. Install the Arduino IDE plus the 8266 package following the instructions  at https://github.com/esp8266/Arduino#installing-with-boards-manager
2. Open the _vitotronic-interface-8266.ino_ sketch and adapt the following information:
   * Set _MAX_SRV_CLIENTS_ to the number of WiFi clients that should be able to connect simultanously to the server. The default of _1_ should be fine, since multiple parallel requests would screw up the communication with the heating.
   * Set _SRV_PORT_ to the port you want the server to listen to (default 8888).
   * Set your WiFi credentials (SSID/password) in _ssid_ and _password_.
   * If you want to use static IP assignment:
     * set _ip_, _dns_, _gateway_, and _subnet_ according to your network/router settings
   * otherwise, if you are using DHCP:
     * comment/remove the line `WiFi.config(ip, dns, gateway, subnet);`
3. Compile the sketch and upload it to your ESP8266

## How to connect
* Connect the _Optokoppler_ to the heating, connect _IN/RX_ with the ESP8266 _RX_ line and _OUT/TX_ with the ESP8266 _TX_ line, as well as _VCC_ and _GND_ to 3.3V and Ground.
* Turn power on. The ESP should show up in your WiFi network. Afterwards you should be able to telnet into it at _<it's IP>:<the port you assigned>_.
* If you want to retrieve debugging output, connnect a serial port's _RX_ line to _GPIO2_ of the ESP8266 at 115200bps, 8N1. Don't forget to connect GND of the port as well to GND of the ESP8266.
