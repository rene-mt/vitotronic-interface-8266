// Import required libraries
#include <ESP8266WiFi.h>
#include <FS.h>
#include <ESP8266WebServer.h>

//GPIO pin triggering setup interrupt for re-configuring the server
#define SETUP_INTERRUPT_PIN 12

//SSID and password for the configuration WiFi network established bei the ESP in setup mode
#define SETUP_AP_SSID "vitotronic-interface"
#define SETUP_AP_PASSWORD "vitotronic"

//setup mode flag, 1 when in setup mode, 0 otherwise
uint8_t _setupMode = 0;

/*
   Config file layout - one entry per line, seven lines overall, terminated by '\n':
    <ssid>
    <password>
    <port>
    <static IP - or empty line>
    <dnsServer IP - or empty line>
    <gateway IP - or empty line>
    <subnet mask - or empty line>
*/
//path+filename of the WiFi configuration file in the ESP's internal file system.
const char* _configFile = "/config/config.txt";

#define FIELD_SSID "ssid"
#define FIELD_PASSWORD "password"
#define FIELD_PORT "port"
#define FIELD_IP "ip"
#define FIELD_DNS "dns"
#define FIELD_GATEWAY "gateway"
#define FIELD_SUBNET "subnet"

const char* _htmlConfigTemplate =
  "<html>" \
    "<head>" \
      "<title>Vitotronic WiFi Interface</title>" \
    "</head>" \
    "<body>" \
      "<h1>Vitotronic WiFi Interface</h1>" \
      "<h2>Setup</h2>" \
      "<form action=\"update\" id=\"update\" method=\"post\">" \
        "<p>Fields marked by (*) are mandatory.</p>" \
        "<h3>WiFi Network Configuration Data</h3>" \
        "<p>" \
          "<div>The following information is required to set up the WiFi connection of the server.</div>" \
          "<label for=\"" FIELD_SSID "\">SSID (*):</label><input type=\"text\" name=\"" FIELD_SSID "\" required  />" \
          "<label for=\"" FIELD_PASSWORD "\">Password:</label><input type=\"password\" name=\"" FIELD_PASSWORD "\" />" \
        "</p>" \
        "<h3>Static IP settings</h3>" \
        "<p>" \
          "<div>If you want to assing a static IP address fill out the following information. All addresses have to by given in IPv4 format (XXX.XXX.XXX.XXX)." \
          "Leave the fields empty to rely on DHCP to obtain an IP address.</div>" \
          "<label for=\"" FIELD_IP "\">Static IP:</label><input type=\"text\" name=\"" FIELD_IP "\" pattern=\"^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$\" /><br/>" \
          "<label for=\"" FIELD_DNS "\">DNS Server:</label><input type=\"text\" name=\"" FIELD_DNS "\" pattern=\"^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$\" /><br/>" \
          "<label for=\"" FIELD_GATEWAY "\">Gateway:</label><input type=\"text\" name=\"" FIELD_GATEWAY "\" pattern=\"^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$\" /><br/>" \
          "<label for=\"" FIELD_SUBNET "\">Subnet mask:</label><input type=\"text\" name=\"" FIELD_SUBNET "\" pattern=\"^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$\" /><br/>" \
        "</p>" \
        "<h3>Server Port</h3>" \
        "<p>" \
          "<div>The Vitotronic WiFi Interface will listen at the following port for incoming telnet connections:</div>" \
          "<label for=\"" FIELD_PORT "\">Port (*):</label><input type=\"number\" name=\"" FIELD_PORT "\" value=\"8888\" required />" \
        "</p>" \
        "<div>" \
          "<button type=\"reset\">Reset</button>&nbsp;" \
          "<button type=\"submit\">Submit</button>" \
        "</div>" \
      "</form>" \
    "</body>" \
  "</html>";

const char* _htmlSuccessTemplate =
  "<html>" \
    "<head>" \
      "<title>Vitotronic WiFi Interface</title>" \
    "</head>" \
    "<body>" \
      "<h1>Vitotronic WiFi Interface</h1>" \
      "<h2>Configuration Saved</h2>" \
      "<p>" \
        "The configuration has been successfully saved to the adapter:<br/><br/>" \
        "- SSID: %%ssid<br/>" \
        "- Password: %%password<br/>" \
        "- Port: %%port<br/><br/>" \
        "- Static IP: %%ip<br/>" \
        "- DNS server: %%dns<br/>" \
        "- Gateway: %%gateway<br/>" \
        "- Subnet mask: %%subnet<br/>" \
        "The adapter will reboot now and connect to the specified WiFi network. In case of a successful connection the <em>vitotronic-interface</em> network will be gone. " \
        "<strong>If no connection is possible, e.g. because the password is wrong or the network is not available, the adapter will return to setup mode again.</strong>" \
      "</p>" \
      "<p>" \
        "<strong><em>IMPORTANT NOTICE:</em></strong><br/>" \
        "Some of the ESP8266 mikrocontrollers, built into the adapter, need a hard reset to be able to connect to the new WiFi network. " \
        "Therefore it is recommended to interrupt the power supply for a short time within the next 10 seconds.</strong>" \
      "</p>" \
    "</body>" \
  "</html>";

ESP8266WebServer* _setupServer = NULL;
WiFiServer* server = NULL;
WiFiClient serverClient;

//helper function to remove trailing CR (0x0d) from strings read from config file
String removeTrailingCR(String input){
  if (!input)
    return String();
  if (input.charAt(input.length()-1) == 0x0d){
    input.remove(input.length()-1);
  }
  return input;
}

void setup() {
  Serial1.begin(115200); // User Serial1 (GPIO2) as debug output (TX), with 115200,N,1
  //Serial1.setDebugOutput(true);
  Serial1.println("\nVitotronic WiFi Interface\n");
  yield();

  //try to read config file from internal file system
  SPIFFS.begin();
  File configFile = SPIFFS.open(_configFile, "r");
  if (configFile) {
    Serial1.println("Using existing WiFi config to connect");

    String ssid = removeTrailingCR(configFile.readStringUntil('\n'));
    String password = removeTrailingCR(configFile.readStringUntil('\n'));

    String port = removeTrailingCR(configFile.readStringUntil('\n'));

    if (!ssid || ssid.length() == 0
        || !port || port.length() == 0)
    {
      //reset & return to setup mode if minium configuration data is missing
      Serial1.println("Minimum configuration data is missing (ssid, port) - resetting to setup mode.");
      SPIFFS.remove(_configFile);

      yield();
      ESP.reset();
      return;
    }

    String ip = removeTrailingCR(configFile.readStringUntil('\n'));
    String dns = removeTrailingCR(configFile.readStringUntil('\n'));
    String gateway = removeTrailingCR(configFile.readStringUntil('\n'));
    String subnet = removeTrailingCR(configFile.readStringUntil('\n'));

    configFile.close();

    // Initialize WiFi
    WiFi.disconnect();
    yield();
    WiFi.mode(WIFI_STA);
    yield();
    
    if (ip && ip.length() > 0
        && dns && dns.length() > 0
        && gateway && gateway.length() > 0
        && subnet && subnet.length() > 0)
    {
      //set static IP configuration, if available
      Serial1.println("Static IP configuration available:");
      Serial1.printf("IP: '%s', DNS: '%s', gateway: '%s', subnet mask: '%s'\n", ip.c_str(), dns.c_str(), gateway.c_str(), subnet.c_str());
      WiFi.config(IPAddress().fromString(ip), IPAddress().fromString(dns), IPAddress().fromString(gateway), IPAddress().fromString(subnet));
      yield();
    }
    
    Serial1.printf("\nConnecting to WiFi network '%s' password: '", ssid.c_str());
    for (int i=0; i<password.length(); i++){
      Serial1.print("*");
    }
    Serial1.print("'");

    uint8_t wifiAttempts = 0;
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED && wifiAttempts++ < 20) {
      Serial1.print('.');
      delay(1000);
    }
    if (wifiAttempts == 21) {
      Serial1.printf("\n\nCould not connect to WiFi network '%s'.\n", ssid.c_str());
      Serial1.println("Deleting configuration and resetting ESP to return to configuration mode...");
      SPIFFS.remove(_configFile);
      yield();
      ESP.reset();
    }

    Serial1.printf("\n\nReady! Server available at %s:%s\n", WiFi.localIP().toString().c_str(), port.c_str());

    Serial.begin(4800, SERIAL_8E2); // Vitotronic connection runs at 4800,E,2
    Serial1.println("Serial port to Vitotronic opened at 4800bps, 8E2");

    server = new WiFiServer(port.toInt());
    server->begin();
  }
  else {
    //start ESP in access point mode and provide a HTTP server at port 80 to handle the configuration page.
    Serial1.println("No WiFi config exists, switching to setup mode.");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASSWORD);

    _setupServer = new ESP8266WebServer(80);
    _setupServer->on("/", handleRoot);
    _setupServer->on("/update", handleUpdate);
    _setupServer->onNotFound(handleRoot);
    _setupServer->begin();

    Serial1.println("WiFi access point with SSID \"" SETUP_AP_SSID "\" opened.");
    Serial1.printf("Configuration web server started at %s:80\n\n", WiFi.softAPIP().toString().c_str());

    _setupMode = 1;
  }

  //configure GPIO for setup interrupt by push button
  pinMode(SETUP_INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SETUP_INTERRUPT_PIN), setupInterrupt, FALLING);
  Serial1.printf("Interrupt for re-entering setup mode attached to GPIO%d\n\n", SETUP_INTERRUPT_PIN); 
}

void wifiSerialLoop() {
  uint8_t i;
  //check if there is a new client trying to connect
  if (server && server->hasClient()) {
    //check, if no client is already connected
    if (!serverClient || !serverClient.connected()) {
      if (serverClient) serverClient.stop();
      serverClient = server->available();
      Serial1.println("New client connected.\n");
    }
    else {
      //reject additional connection requests.
      WiFiClient serverClient = server->available();
      serverClient.stop();
    }
  }

  yield();

  //check client for data
  if (serverClient && serverClient.connected()) {
    size_t len = serverClient.available();
    if (len) {
      uint8_t sbuf[len];
      serverClient.read(sbuf, len);

      // Write received WiFi data to serial
      Serial.write(sbuf, len);

      yield();

      // Debug output received WiFi data to Serial1
      Serial1.println();
      Serial1.print("WiFi: ");
      for (uint8_t n = 0; n < len; n++) {
        Serial1.print(sbuf[n], HEX);
      }
      Serial1.println();
    }
  }

  yield();

  //check UART for data
  if (Serial.available()) {
    size_t len = Serial.available();
    uint8_t sbuf[len];
    Serial.readBytes(sbuf, len);

    //push UART data to connected WiFi client
    if (serverClient && serverClient.connected()) {
      serverClient.write(sbuf, len);
    }

    yield();

    // Debug output received Serial data to Serial1
    Serial1.println();
    Serial1.print("Serial: ");
    for (uint8_t n = 0; n < len; n++) {
      Serial1.printf("%02x,", sbuf[n]);
    }
    Serial1.println();
  }
}

void handleRoot() {
  if (_setupServer) {
    _setupServer->send(200, "text/html", _htmlConfigTemplate);
  }
}

void handleUpdate() {
  Serial1.println("handleUpdate()");
  if (!_setupServer) {
    Serial1.println("_setupServer is NULL, exiting.");
    return;
  }
  if (_setupServer->uri() != "/update") {
    Serial1.printf("URI is '%s', which does not match expected URI '/update', exiting.\n", _setupServer->uri().c_str());
    return;
  }

  String ssid = _setupServer->arg(FIELD_SSID);
  String password = _setupServer->arg(FIELD_PASSWORD);
  String port = _setupServer->arg(FIELD_PORT);
  String ip = _setupServer->arg(FIELD_IP);
  String dns = _setupServer->arg(FIELD_DNS);
  String gateway = _setupServer->arg(FIELD_GATEWAY);
  String subnet = _setupServer->arg(FIELD_SUBNET);

  Serial1.println("Submitted parameters:");
  Serial1.printf("SSID: '%s'\n", ssid.c_str());
  Serial1.print("Password: '");
  for (int i=0; i < password.length(); i++) {
    Serial1.print('*');
  }
  Serial1.println("'");
  Serial1.printf("Port: '%s'\n", port.c_str());
  Serial1.printf("IP: '%s'\n", ip.c_str());
  Serial1.printf("DNS: '%s'\n", dns.c_str());
  Serial1.printf("Gateway: '%s'\n", gateway.c_str());
  Serial1.printf("Subnet: '%s'\n", subnet.c_str());

  // check if mandatory parameters have been set
  if (ssid.length() == 0 || port.length() == 0) {
    Serial1.println("Missing SSID or Port parameter!");
    handleRoot();
    return;
  }

  //for static IP configuration: check if all parameters have been set
  if (ip.length() > 0 || dns.length() > 0 || gateway.length() > 0 || subnet.length() > 0 &&
      (ip.length() == 0 || dns.length() == 0 || gateway.length() == 0 || subnet.length() == 0)) {
    Serial1.println("Missing static IP parameters!");
    handleRoot();
    return;
  }

  //write configuration data to file
  Serial1.printf("Writing config to file '%s'", _configFile);
  SPIFFS.begin();
  File configFile = SPIFFS.open(_configFile, "w");

  configFile.println(ssid);
  configFile.println(password);
  configFile.println(port);
  configFile.println(ip);
  configFile.println(dns);
  configFile.println(gateway);
  configFile.println(subnet);

  configFile.close();
  yield();
  
  String maskedPassword = "";
  for (int i=0; i<password.length(); i++){
    maskedPassword+="*";
  }

  String htmlSuccess(_htmlSuccessTemplate);
  htmlSuccess.replace("%%ssid", ssid);
  htmlSuccess.replace("%%password", maskedPassword);
  htmlSuccess.replace("%%port", port);
  htmlSuccess.replace("%%ip", ip);
  htmlSuccess.replace("%%dns", dns);
  htmlSuccess.replace("%%gateway", gateway);
  htmlSuccess.replace("%%subnet", subnet),
  
  //leave setup mode and restart with existing configuration
  _setupServer->send(200, "text/html", htmlSuccess);
  
  Serial1.println("Config saved, resetting ESP...");
  delay(10);
  ESP.reset();
}

void setupInterrupt() {
  //if the setup button has been pushed delete the existing configuration and reset the ESP to enter setup mode again
  Serial1.println("Reset button pressed, deleting existing configuration...");
  SPIFFS.begin();
  SPIFFS.remove(_configFile);
  yield();
  ESP.reset();
}

void loop() {
  if (!_setupMode)
    wifiSerialLoop();
  else if (_setupServer)
    _setupServer->handleClient();
}
