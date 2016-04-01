// Import required libraries
#include "ESP8266WiFi.h"

//on which port the server should listen
#define SRV_PORT 8888

//WiFi credentials
const char* ssid = "xxx";
const char* password = "xxx";

//Parameters for static IP configuration
IPAddress ip(192, 168, 178, 210);    //IP address of the ESP8266
IPAddress dns(192, 168,178, 1);      //DNS server
IPAddress gateway(192, 168, 178, 1); //Gateway
IPAddress subnet(255, 255, 255, 0);  //Subnet mask

WiFiServer server(SRV_PORT);
WiFiClient serverClient;

void setup() {
  Serial1.begin(115200); // User Serial1 (GPIO2) as debug output (TX), with 115200,N,1
  //Serial1.setDebugOutput(true);

  Serial.begin(4800, SERIAL_8E2); // Vitotronic connection runs at 4800,E,2
  
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  //if you are using DHCP please remove/comment the following line
  WiFi.config(ip, dns, gateway, subnet); //Set static IP address. 
  WiFi.begin(ssid, password);

  Serial1.println("Vitotronic WiFi Interface"); Serial1.println();
  Serial1.print("\nConnecting to "); Serial1.println(ssid);
  uint8_t i = 0;

  while (WiFi.status() != WL_CONNECTED && i++ < 20) delay(500);
  if (i == 21) {
    Serial1.print("Could not connect to"); Serial1.println(ssid);
    while (1) delay(500);
  }
  Serial1.print("Ready! Server available at ");
  Serial1.print(WiFi.localIP());
  Serial1.print(": "); Serial1.print(SRV_PORT);Serial1.println();

  server.begin();
}

void loop() {
  uint8_t i;
  //check if there is a new client trying to connect
  if (server.hasClient()) {
    //check, if no client is already connected
    if (!serverClient || !serverClient.connected()) {
      if (serverClient) serverClient.stop();
      serverClient = server.available();
      Serial1.print("New client connected."); Serial1.println(); Serial1.println();
    }
    else {
      //reject additional connection requests.
      WiFiClient serverClient = server.available();
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
        yield();
      }

      // Debug output received Serial data to Serial1
      Serial1.println();
      Serial1.print("Serial: ");
      for (uint8_t n = 0; n < len; n++) {
        Serial1.print(sbuf[n], HEX);
      }
      Serial1.println();
    }
}
