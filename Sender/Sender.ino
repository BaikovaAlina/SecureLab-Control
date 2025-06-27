#include <WiFiUdp.h>
#include <WiFi.h>
#include "ESPmDNS.h"
#include <mbedtls/md.h>
#include <map>

const char* ssid = "WhiteHackers2";
const char* password = "PASS122602";
const char* hmacKey = "secureSecretKey123!";
unsigned int localPort = 8888;
WiFiUDP Udp;

// Структура для хранения информации о командах
struct CommandHistory {
  uint32_t lastNonce;
  unsigned long lastTimestamp;
  int executionCount;
};

std::map<String, CommandHistory> commandHistory; // Хранит историю команд для каждого отправителя
std::map<String, unsigned long> criticalCommandTimers; // Таймеры для критических команд

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

bool isCriticalCommand(String cmd) {
  return cmd == "ACTIVATE_LASER_40KW" || 
         cmd == "OPEN_VACUUM_CHAMBER" || 
         cmd == "CALIBRATE_SPECTROMETER" ||
         cmd == "EMERGENCY_COOLDOWN" ||
         cmd == "START_DATA_STREAM";
}

bool checkCommandLimits(String cmd, unsigned long currentTime) {
  if (cmd == "ACTIVATE_LASER_40KW" && criticalCommandTimers[cmd] > currentTime - 60000) {
    if (++criticalCommandTimers[cmd + "_count"] >= 5) return false;
  }
  else if (cmd == "OPEN_VACUUM_CHAMBER" && criticalCommandTimers[cmd] > currentTime - 10000) {
    return false; // Блокируем повторное открытие
  }
  else if (cmd == "CALIBRATE_SPECTROMETER" && criticalCommandTimers[cmd] > currentTime - 600000) {
    if (++criticalCommandTimers[cmd + "_count"] >= 10) return false;
  }
  else if (cmd == "EMERGENCY_COOLDOWN" && criticalCommandTimers[cmd] > currentTime - 30000) {
    return false; // Блокируем повторное экстренное охлаждение
  }
  else if (cmd == "START_DATA_STREAM" && criticalCommandTimers[cmd] > currentTime - 5000) {
    return false; // Блокируем повторный запуск потока данных
  }
  
  criticalCommandTimers[cmd] = currentTime;
  return true;
}

bool validateCommand(String rawMessage, String senderIP) {
  String cmd = extractValue(rawMessage, "CMD:");
  String tsStr = extractValue(rawMessage, "TS:");
  String nonceStr = extractValue(rawMessage, "NONCE:");
  String prevStr = extractValue(rawMessage, "PREV:");
  String hmac = extractValue(rawMessage, "HMAC:");
  
  unsigned long timestamp = tsStr.toInt();
  uint32_t nonce = strtoul(nonceStr.c_str(), NULL, 10);
  uint32_t prevNonce = strtoul(prevStr.c_str(), NULL, 10);
  
  // Проверка временного окна (2 секунды)
  if (millis() - timestamp > 2000) {
    Serial.println("[BLOCKED] Expired message");
    return false;
  }
  
  // Проверка последовательности nonce
  if (commandHistory.find(senderIP) != commandHistory.end()) {
    if (prevNonce != commandHistory[senderIP].lastNonce) {
      Serial.println("[BLOCKED] Invalid nonce sequence");
      return false;
    }
  }
  
  // Проверка HMAC
  String toVerify = cmd + tsStr + nonceStr + prevStr;
  String calculatedHmac = calculateHMAC(toVerify);
  
  if (calculatedHmac != hmac) {
    Serial.println("[BLOCKED] HMAC mismatch");
    return false;
  }
  
  // Проверка критических команд
  if (isCriticalCommand(cmd)) {  // Added missing closing parenthesis
    if (!checkCommandLimits(cmd, millis())) {
      Serial.println("[BLOCKED] Critical command limit exceeded: " + cmd);
      return false;
    }
  }
  
  // Обновляем историю команд
  CommandHistory history;
  history.lastNonce = nonce;
  history.lastTimestamp = millis();
  commandHistory[senderIP] = history;
  
  return true;
}

String extractValue(String data, String key) {
  int startIndex = data.indexOf(key);
  if (startIndex == -1) return "";
  
  startIndex += key.length();
  int endIndex = data.indexOf(";", startIndex);
  if (endIndex == -1) endIndex = data.length();
  
  return data.substring(startIndex, endIndex);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected");
  if (!MDNS.begin("SecureReceiver")) {
    Serial.println("Error starting mDNS");
  }
  
  Udp.begin(localPort);
  
  // Инициализация счетчиков критических команд
  criticalCommandTimers["ACTIVATE_LASER_40KW"] = 0;
  criticalCommandTimers["ACTIVATE_LASER_40KW_count"] = 0;
  criticalCommandTimers["CALIBRATE_SPECTROMETER"] = 0;
  criticalCommandTimers["CALIBRATE_SPECTROMETER_count"] = 0;
}

void loop() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    char packetBuffer[255];
    int len = Udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = '\0';
      String senderIP = Udp.remoteIP().toString();
      String message(packetBuffer);
      
      if (validateCommand(message, senderIP)) {
        String cmd = extractValue(message, "CMD:");
        Serial.println("[OK] Valid command: " + cmd);
        
        // Имитация выполнения команды
        if (cmd == "ACTIVATE_LASER_40KW") {
          Serial.println("Executing: ACTIVATE_LASER_40KW (5 sec)");
          delay(5000);
        } else if (cmd == "CALIBRATE_SPECTROMETER") {
          Serial.println("Executing: CALIBRATE_SPECTROMETER (5 sec)");
          delay(5000);
        }
      } else {
        Serial.println("[BLOCKED] Replay attack or invalid command from " + senderIP);
      }
    }
  }
  
  // Очистка старых nonces для предотвращения переполнения
  if (millis() % 60000 == 0) { // Каждую минуту
    for (auto it = commandHistory.begin(); it != commandHistory.end(); ) {
      if (millis() - it->second.lastTimestamp > 60000) { // Удаляем записи старше 1 минуты
        it = commandHistory.erase(it);
      } else {
        ++it;
      }
    }
  }
  
  delay(10);
}
