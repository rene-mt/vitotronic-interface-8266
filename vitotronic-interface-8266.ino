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
 * Config file layout - one entry per line, seven lines overall, terminated by '\n':
 *  <ssid>
 *  <password>
 *  <port>
 *  <static IP - or empty line>
 *  <dnsServer IP - or empty line>
 *  <gateway IP - or empty line>
 *  <subnet mask - or empty line>
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

const char* _htmlTemplate =
  "<html>" \
    "<head>" \
      "<title>Vitotronic WiFi Interface</title>" \
    "</head>" \
    "<body>" \
      "<h1>Vitotronic WiFi Interface</h1>" \
      "<form action=\"update\" id=\"update\" method=\"post\">" \
        "<p>Fields marked by (*) are mandatory.</p>" \
        "<h2>WiFi Network Configuration Data</h2>" \
        "<p>" \
          "<div>The following information is required to set up the WiFi connection of the server.</div>" \
          "<label for=\"" FIELD_SSID "\">SSID (*):</label><input type=\"text\" id=\"" FIELD_SSID "\" required  />" \
          "<label for=\"" FIELD_PASSWORD "\">Password:</label><input type=\"password\" id=\"" FIELD_PASSWORD "\" />" \
        "</p>" \
        "<h2>Static IP settings</h2>" \
        "<p>" \
          "<div>If you want to assing a static IP address fill out the following information. All addresses have to by given in IPv4 format (XXX.XXX.XXX.XXX)." \
                "Leave the fields empty to rely on DHCP to obtain an IP address.</div>" \
          "<label for=\"" FIELD_IP "\">Static IP:</label><input type=\"text\" id=\"" FIELD_IP "\" pattern=\"^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$\" />" \
          "<label for=\"" FIELD_DNS "\">DNS Server:</label><input type=\"text\" id=\"" FIELD_DNS "\" pattern=\"^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$\" />" \
          "<label for=\"" FIELD_GATEWAY "\">Gateway:</label><input type=\"text\" id=\"" FIELD_GATEWAY "\" pattern=\"^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$\" />" \
          "<label for=\"" FIELD_SUBNET "\">Subnet mask:</label><input type=\"text\" id=\"" FIELD_SUBNET "\" pattern=\"^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$\" />" \
        "</p>" \        
        "<h2>Server Port</h2>" \
        "<p>" \
          "<div>The Vitotronic WiFi Interface will listen at the following port for incoming telnet connections:</div>" \
          "<label for=\"" FIELD_PORT "\">Port (*):</label><input type=\"number\" id=\"" FIELD_PORT "\" value=\"8888\" required />" \
        "</p>" \
        "<button type=\"reset\">Reset</button>" \
        "<button type=\"submit\">Submit</button>" \
      "</form>" \
    "</body>" \
  "</html>";

ESP8266WebServer* _setupServer = NULL;   
WiFiServer* server = NULL;
WiFiClient serverClient;

void setup() {
  Serial1.begin(115200); // User Serial1 (GPIO2) as debug output (TX), with 115200,N,1
  //Serial1.setDebugOutput(true);
  Serial1.println("Vitotronic WiFi Interface"); Serial1.println();

  //configure GPIO for setup interrupt by push button
  pinMode(SETUP_INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SETUP_INTERRUPT_PIN), setupInterrupt, LOW);

  //try to read config file from internal file system
  SPIFFS.begin();
  File configFile = SPIFFS.open(_configFile, "r");
  if (configFile){
    Serial1.println("Using existing WiFi config to connect");

    String ssid = configFile.readStringUntil('\n');
    String password = configFile.readStringUntil('\n');
    
    String port = configFile.readStringUntil('\n');
    
    if (!ssid || ssid.length() == 0 
      || !port || port.length() == 0)
    {
      //reset & return to setup mode if minium configuration data is missing
      Serial1.println("Minimum configuration data is missing (ssid, port) - resetting to setup mode.");
      SPIFFS.remove(_configFile);
      ESP.reset();
      return;        
    }

    String ip = configFile.readStringUntil('\n');
    String dns = configFile.readStringUntil('\n');
    String gateway = configFile.readStringUntil('\n');
    String subnet = configFile.readStringUntil('\n');

    configFile.close();

    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    if (ip && ip.length() > 0
      && dns && dns.length() > 0
      && gateway && gateway.length() > 0
      && subnet && subnet.length() > 0)
    {
       //set static IP configuration, if available
      Serial1.println("Static IP configuration available.");
      WiFi.config(IPAddress().fromString(ip), IPAddress().fromString(dns), IPAddress().fromString(gateway), IPAddress().fromString(subnet)); 
    }
    WiFi.begin(ssid.c_str(), password.c_str());
  
    Serial1.print("\nConnecting to "); Serial1.println(ssid);
    uint8_t i = 0;
    while (WiFi.status() != WL_CONNECTED && i++ < 20) delay(500);
    if (i == 21) {
      Serial1.print("Could not connect to"); Serial1.println(ssid);
      while (1) delay(500);
    }
    
    Serial1.print("Ready! Server available at ");
    Serial1.print(WiFi.localIP());
    Serial1.print(":"); Serial1.println(port);

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
    _setupServer->on("/update", HTTP_POST, handleUpdate);
    _setupServer->onNotFound(handleRoot);
    _setupServer->begin();

    Serial1.println("WiFi access point with SSID \"" SETUP_AP_SSID "\" opened.");
    Serial1.print("Configuration web server started at ");
    Serial1.print(WiFi.softAPIP());
    Serial1.println(":80");
    
    _setupMode = 1;  
  }
}

void wifiSerialLoop() {
   uint8_t i;
  //check if there is a new client trying to connect
  if (server && server->hasClient()) {
    //check, if no client is already connected
    if (!serverClient || !serverClient.connected()) {
      if (serverClient) serverClient.stop();
      serverClient = server->available();
      Serial1.print("New client connected."); Serial1.println(); Serial1.println();
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
  if(Serial.available()){
    size_t len = Serial.available();
    uint8_t sbuf[len];
    Serial.readBytes(sbuf, len);
    
    //push UART data to connected WiFi client
    if (serverClient && serverClient.connected()){
      serverClient.write(sbuf, len);
    }

    yield();

    // Debug output received Serial data to Serial1
    Serial1.println();
    Serial1.print("Serial: ");
    for (uint8_t n = 0; n < len; n++) {
      Serial1.print(sbuf[n], HEX);
    }
    Serial1.println();
  } 
}

void handleRoot(){
  if (_setupServer){
    _setupServer->send(200, "text/html", _htmlTemplate);
  }
}

void handleUpdate(){
  if (!_setupServer || _setupServer->uri() != "/update")
    return;

  String ssid = _setupServer->header(FIELD_SSID);
  String password = _setupServer->header(FIELD_PASSWORD);
  String port = _setupServer->header(FIELD_PORT);
  String ip = _setupServer->header(FIELD_IP);
  String dns = _setupServer->header(FIELD_DNS);
  String gateway = _setupServer->header(FIELD_GATEWAY);
  String subnet = _setupServer->header(FIELD_SUBNET);

  // check if mandatory parameters have been set
  if (ssid.length() == 0 || port.length() == 0){
    handleRoot();
    return;
  }

  //for static IP configuration: check if all parameters have been set
  if(ip.length() > 0 || dns.length() > 0 || gateway.length() > 0 || subnet.length() > 0 &&
     (ip.length() == 0|| dns.length() == 0 || gateway.length() == 0 || subnet.length() == 0)){
    handleRoot();
    return;
  }

  //write configuration data to file
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
  
  //leave setup mode and restart with existing configuration
  ESP.reset();
}

void setupInterrupt(){
  //if the setup button has been pushed delete the existing configuration and reset the ESP to enter setup mode again
  SPIFFS.begin();
  SPIFFS.remove(_configFile);
  ESP.reset();
}

void loop() {
  if (!_setupMode)
    wifiSerialLoop();
  else if (_setupServer)
    _setupServer->handleClient();
}
