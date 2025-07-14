#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
extern "C" {
  #include <user_interface.h>
}

const char* ssid = "Free_WiFi"; // rogue AP’s name
const IPAddress apIP(192, 168, 4, 1); // AP’s IP
const IPAddress subnet(255, 255, 255, 0); // Subnet mask

// DNS server to fuck with all DNS requests
DNSServer dnsServer;
const byte DNS_PORT = 53;

// web server on port 80
ESP8266WebServer server(80);

// track clients
struct ClientInfo {
  uint8_t mac[6];
  IPAddress ip;
  bool active;
};

ClientInfo clients[10]; // limit up to 10 clients
int clientCount = 0;

// HTML
const char* htmlPage = R"(
<!DOCTYPE html>
<html>
<head>
  <title>Free WiFi - Get In!</title>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; padding: 20px; }
    h1 { color: #333; }
    p { font-size: 18px; }
    .button { background-color: #3ea24f; color: white; padding: 10px 20px; text-decoration: none; font-size: 16px; border-radius: 5px; }
  </style>
</head>
<body>
  <h1>Free WiFi!</h1>
  <p>Connect to this Wifi network for free!!!</p>
  <a href='/login' class='button'>Get Connected</a>
</body>
</html>
)";

// Handle root URL
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

// Handle login page (just for shits and giggles)
void handleLogin() {
  String message = "<!DOCTYPE html><html><head><title>Connected!</title><style>";
  message += "body { font-family: Arial, sans-serif; text-align: center; padding: 20px; }";
  message += "h1 { color: #333; } p { font-size: 18px; }";
  message += "</style></head><body><h1>You're Connected!</h1>";
  message += "<p>Enjoy the free wifi!</p></body></html>";
  server.send(200, "text/html", message);
}


void handleNotFound() {
  server.send(200, "text/html", htmlPage); // handle 404 (redirect all bs to the portal)
}

// snoop on connected clients
void checkClients() {
  struct station_info *stat_info = wifi_softap_get_station_info();
  struct station_info *current = stat_info;
  bool clientsUpdated = false;

  // marking all clients as inactive
  for (int i = 0; i < clientCount; i++) {
    clients[i].active = false;
  }

  // for checkiong who’s connected
  while (current != NULL) {
    bool isNew = true;
    for (int i = 0; i < clientCount; i++) {
      if (memcmp(clients[i].mac, current->bssid, 6) == 0) {
        clients[i].active = true;
        isNew = false;
        break;
      }
    }

    if (isNew && clientCount < 10) {
      memcpy(clients[clientCount].mac, current->bssid, 6);
      clients[clientCount].ip = IPAddress(current->ip.addr);
      clients[clientCount].active = true;
      Serial.print("New genius connected: MAC=");
      for (int i = 0; i < 6; i++) {
        Serial.print(clients[clientCount].mac[i], HEX);
        if (i < 5) Serial.print(":");
      }
      Serial.print(", IP=");
      Serial.println(clients[clientCount].ip);
      clientCount++;
      clientsUpdated = true;
    }
    current = STAILQ_NEXT(current, next);
  }

  // for disconnected clients
  for (int i = 0; i < clientCount; i++) {
    if (!clients[i].active) {
      Serial.print("Client bailed: MAC=");
      for (int j = 0; j < 6; j++) {
        Serial.print(clients[i].mac[j], HEX);
        if (j < 5) Serial.print(":");
      }
      Serial.print(", IP=");
      Serial.println(clients[i].ip);
      // shift array to remove disconnected client
      for (int j = i; j < clientCount - 1; j++) {
        clients[j] = clients[j + 1];
      }
      clientCount--;
      i--;
      clientsUpdated = true;
    }
  }

  wifi_softap_free_station_info();
  if (clientsUpdated) {
    Serial.print("Total connected suckers: ");
    Serial.println(clientCount);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n-----ESP8266 Rogue WiFi Access Point-----");

  // starting the AP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP(ssid);
  Serial.print("AP Running: ");
  Serial.println(ssid);
  Serial.print("IP Address: ");
  Serial.println(apIP);

  // starting DNS server to trap everyone
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println("DNS Server Fired Up");

  // setting up web server routes
  server.on("/", handleRoot);
  server.on("/login", handleLogin);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web Server Ready");
}

void loop() {
  // keeping the DNS and HTTP servers humming
  dnsServer.processNextRequest();
  server.handleClient();

  // checking every 5 seconds
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 5000) {
    checkClients();
    lastCheck = millis();
  }
}
