#include <WiFiUdp.h>
#include "ESPmDNS.h"
#include <mbedtls/md.h>
#include <WiFi.h>

const char* ssid = "WhiteHackers2";
const char* password = "PASS122602";
const char* hmacKey = "secureSecretKey123!";
unsigned int serverPort = 8888;
WiFiUDP udpClient;
IPAddress server;

struct SecureCommand {
  String command;
  unsigned long timestamp;
  String hmac;
  uint32_t nonce;
  uint32_t prevNonce;
};

String calculateHMAC(String message) {
  byte hmacResult[32];
  
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)hmacKey, strlen(hmacKey));
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)message.c_str(), message.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
  
  char buffer[65];
  for(int i=0; i<32; i++) {
    sprintf(buffer+i*2, "%02x", hmacResult[i]);
  }
  buffer[64] = 0;
  
  return String(buffer);
}

SecureCommand createSecureCommand(String cmd, uint32_t prevNonce) {
  SecureCommand sc;
  sc.command = cmd;
  sc.timestamp = millis();
  sc.nonce = esp_random();
  sc.prevNonce = prevNonce;
  
  String toSign = cmd + String(sc.timestamp) + String(sc.nonce) + String(sc.prevNonce);
  sc.hmac = calculateHMAC(toSign);
  
  return sc;
}

bool sendSecureCommand(SecureCommand cmd) {
  String message;
  message += "CMD:" + cmd.command + ";";
  message += "TS:" + String(cmd.timestamp) + ";";
  message += "NONCE:" + String(cmd.nonce) + ";";
  message += "PREV:" + String(cmd.prevNonce) + ";";
  message += "HMAC:" + cmd.hmac;
  
  udpClient.beginPacket(server, serverPort);
  // Явное преобразование String в const uint8_t*
  udpClient.write((const uint8_t*)message.c_str(), message.length());
  if (udpClient.endPacket() == 1) {
    Serial.println("Sent secure command: " + cmd.command);
    return true;
  } else {
    Serial.println("Failed to send command: " + cmd.command);
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected");
  if (!MDNS.begin("SecureSender")) {
    Serial.println("Error starting mDNS");
  }
  
  // Поиск сервера
  int attempts = 0;
  while (attempts < 10) {
    server = MDNS.queryHost("SecureReceiver");
    if (server.toString() != "0.0.0.0") break;
    delay(500);
    attempts++;
  }
  
  if (server.toString() == "0.0.0.0") {
    Serial.println("Failed to find server");
    while(1) delay(1000);
  }
  
  Serial.print("Server found at: ");
  Serial.println(server);
  
  udpClient.begin(serverPort);
}

uint32_t lastNonce = 0;

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    Serial.println("[DEBUG] Received: " + input); // Что пришло из Serial?

    if (input.startsWith("CMD:")) {
      String cmd = input.substring(4);
      Serial.println("[DEBUG] Processing command: " + cmd);
      
      SecureCommand sc = createSecureCommand(cmd, lastNonce);
      lastNonce = sc.nonce;
      
      if (sendSecureCommand(sc)) {
        Serial.println("[DEBUG] Command sent successfully!");
      } else {
        Serial.println("[DEBUG] Send failed!");
      }
    } else {
      Serial.println("[DEBUG] Invalid format. Use 'CMD:COMMAND'");
    }
  }
  delay(10);
}
