#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

// CP2 Motiva: esta versao e instalada inicialmente no Wokwi.
static const char *FW_VERSION = "1.0";
static const char *WIFI_SSID = "Wokwi-GUEST";
static const char *MANIFEST_URL =
    "https://raw.githubusercontent.com/JuanCunhaa/gp16-motiva-ota-cp2/main/version.json";
static const uint8_t LED_RED = 25, LED_GREEN = 26, LED_BLUE = 27;
static const uint32_t SESSION_MS = 48000, READ_MS = 2000;

int samples[5];
uint8_t sampleCount = 0;
uint32_t sessionStart = 0, nextRead = 0, nextSession = 0;
unsigned completedSessions = 0;
bool sessionActive = false;

void setLed(bool red, bool green, bool blue) {
  digitalWrite(LED_RED, red ? HIGH : LOW);
  digitalWrite(LED_GREEN, green ? HIGH : LOW);
  digitalWrite(LED_BLUE, blue ? HIGH : LOW);
}

void beginSession() {
  sessionStart = millis();
  nextRead = sessionStart;
  nextSession = sessionStart + SESSION_MS;
  sampleCount = 0;
  sessionActive = true;
  Serial.printf("\n======== SESSAO %u | inicio %lu ms ========\n",
                completedSessions + 1, (unsigned long)sessionStart);
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 1.0");
}

bool wifiConnect() {
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.println("[WIFI] Conectando a Wokwi-GUEST...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, "", 6);
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 12000) {
    delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] ERRO: sem conexao. Nova tentativa apos a proxima sessao.");
    return false;
  }
  Serial.print("[WIFI] Conectado. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

// Le somente as duas strings necessarias do manifesto controlado pelo projeto.
bool jsonString(const String &json, const char *name, String &value) {
  const String key = String('"') + name + '"';
  int pos = json.indexOf(key);
  if (pos < 0) return false;
  pos = json.indexOf(':', pos + key.length());
  if (pos < 0) return false;
  pos = json.indexOf('"', pos + 1);
  if (pos < 0) return false;
  const int end = json.indexOf('"', pos + 1);
  if (end < 0) return false;
  value = json.substring(pos + 1, end);
  return value.length() > 0;
}

bool newerVersion(const String &remote) {
  int major = -1, minor = -1;
  if (sscanf(remote.c_str(), "%d.%d", &major, &minor) != 2 ||
      major < 0 || minor < 0) return false;
  return major > 1 || (major == 1 && minor > 0);
}

void checkForUpdate() {
  Serial.println("[OTA] Tres ciclos completos. Consultando versao remota...");
  if (!wifiConnect()) return;

  // Apenas para a simulacao academica: em hardware real, valide certificados.
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient request;
  request.setTimeout(10000);
  if (!request.begin(client, MANIFEST_URL)) {
    Serial.println("[OTA] ERRO: nao foi possivel iniciar a consulta do manifesto.");
    return;
  }
  const int status = request.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("[OTA] ERRO: manifesto inacessivel (HTTP %d).\n", status);
    request.end();
    return;
  }
  const String body = request.getString();
  request.end();

  String version, url;
  if (!jsonString(body, "version", version) ||
      !jsonString(body, "url", url) || !url.startsWith("https://")) {
    Serial.println("[OTA] ERRO: manifesto sem version/url HTTPS validas.");
    return;
  }
  Serial.printf("[OTA] Instalada: %s | Disponivel: %s\n",
                FW_VERSION, version.c_str());
  if (!newerVersion(version)) {
    Serial.println("[OTA] Nenhuma versao mais recente disponivel.");
    return;
  }
  Serial.print("[OTA] Baixando firmware de: ");
  Serial.println(url);
  HTTPUpdate updater;
  updater.rebootOnUpdate(false);
  updater.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  updater.onProgress([](int current, int total) {
    static int lastPercent = -1;
    if (total > 0) {
      int percent = (100LL * current) / total;
      if (percent / 10 != lastPercent / 10) {
        Serial.printf("[OTA] Download/gravação: %d%%\n", percent);
        lastPercent = percent;
      }
    }
  });
  const t_httpUpdate_return result = updater.update(client, url);
  if (result == HTTP_UPDATE_OK) {
    Serial.println("[OTA] Firmware gravado. Reiniciando no FW 2.0...");
    Serial.flush();
    delay(100);
    ESP.restart();
  } else if (result == HTTP_UPDATE_NO_UPDATES) {
    Serial.println("[OTA] Servidor informou que nao ha atualizacao.");
  } else {
    Serial.printf("[OTA] ERRO ao baixar/gravar firmware (%d): %s\n",
                  updater.getLastError(), updater.getLastErrorString().c_str());
  }
}

void finishSession() {
  int total = 0;
  for (int value : samples) total += value;
  Serial.printf("Media da sessao: %.1f cm\n", total / 5.0f);
  Serial.printf("Proxima sessao: t=%lu ms (48 s desde o inicio desta).\n",
                (unsigned long)nextSession);
  ++completedSessions;
  sessionActive = false;
  // Primeira tentativa apos a terceira sessao; repete apos falhas de rede.
  if (completedSessions >= 3) checkForUpdate();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  setLed(false, false, true);
  randomSeed(esp_random());
  Serial.println("\n=== ESP32 MOTIVA | FIRMWARE 1.0 | LED AZUL ===");
  beginSession();
}

void loop() {
  const uint32_t now = millis();
  if (sessionActive && (int32_t)(now - nextRead) >= 0) {
    samples[sampleCount] = random(10, 21);
    Serial.printf("Leitura %u: %d cm (t=%lu ms)\n", sampleCount + 1,
                  samples[sampleCount], (unsigned long)(now - sessionStart));
    ++sampleCount;
    nextRead += READ_MS;
    if (sampleCount == 5) finishSession();
  }
  if (!sessionActive && (int32_t)(millis() - nextSession) >= 0) beginSession();
  delay(10);
}
