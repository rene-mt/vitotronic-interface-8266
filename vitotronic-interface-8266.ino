// Import required libraries
#include <ESP8266WiFi.h>
#include <FS.h>

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
 
WiFiServer* server = NULL;
WiFiClient serverClient;

void setup() {
  Serial1.begin(115200); // User Serial1 (GPIO2) as debug output (TX), with 115200,N,1
  //Serial1.setDebugOutput(true);
  Serial1.println("Vitotronic WiFi Interface"); Serial1.println();

  pinMode(12, INPUT_PULLUP); // Set GPIO12 as an input w/ pull-up

  SPIFFS.begin();
  File configFile = SPIFFS.open(_configFile, "r");
  if (configFile){
    Serial1.println("Using existing WiFi config to connect");

    String ssid = configFile.readStringUntil('\n');
    String password = configFile.readStringUntil('\n');
    
    String sPort = configFile.readStringUntil('\n');
    
    if (!ssid || ssid.length() == 0 
      || !password || password.length() == 0
      || !sPort || sPort.length() == 0)
    {
      //reset & return to setup mode if minium configuration data is missing
      Serial1.println("Minimum configuration data is missing (ssid, password, port) - resetting to setup mode.");
      SPIFFS.remove(_configFile);
      ESP.reset();
      yield();
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
    Serial1.print(": "); Serial1.println(sPort);

    Serial.begin(4800, SERIAL_8E2); // Vitotronic connection runs at 4800,E,2
    Serial1.println("Serial port to Vitotronic opened at 4800bps, 8E2");
    
    server = new WiFiServer(sPort.toInt());
    server->begin();
  }
  else {
    Serial1.println("No WiFi config exists, switching to setup mode.");
    WiFi.mode(WIFI_AP);
    WiFi.begin("", "");
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

void setupLoop(){
  SPIFFS.begin();
  File configFile = SPIFFS.open(_configFile, "w");

  configFile.println("SSID");   //ssid
  configFile.println("PASSWORD"); //password
  configFile.println("8888");               //port
  configFile.println("");                   //static IP
  configFile.println("");                   //dns
  configFile.println("");                   //subnet

  configFile.close();
  
  yield();
  _setupMode = 0;
}

void loop() {
  if (digitalRead(12) == LOW) {
    //delete WiFi config file and reset the system to enter setup mode
    SPIFFS.remove(_configFile);
    ESP.reset();
    yield();
    return;
  }
  
  if (!_setupMode)
    wifiSerialLoop();
  else
    setupLoop();
}
