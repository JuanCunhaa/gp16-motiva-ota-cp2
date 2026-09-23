#include <Arduino.h>

static const char *FW_VERSION = "2.0";
static const uint8_t LED_RED = 25, LED_GREEN = 26, LED_BLUE = 27;
static const uint32_t SESSION_MS = 48000, READ_MS = 2000;

int samples[5];
uint8_t sampleCount = 0;
uint32_t sessionStart = 0, nextRead = 0, nextSession = 0;
unsigned completedSessions = 0;
bool sessionActive = false;
bool alertState = false;  // Estado inicial NORMAL, documentado para a faixa neutra.
char requestedTest = 'R', activeTest = 'R';

void setLed(bool red, bool green, bool blue) {
  digitalWrite(LED_RED, red ? HIGH : LOW);
  digitalWrite(LED_GREEN, green ? HIGH : LOW);
  digitalWrite(LED_BLUE, blue ? HIGH : LOW);
}

void showState() {
  setLed(alertState, !alertState, false);
  Serial.printf("Estado: %s | LED %s\n", alertState ? "ALERTA" : "NORMAL",
                alertState ? "VERMELHO" : "VERDE");
}

void beginSession() {
  sessionStart = millis();
  nextRead = sessionStart;
  nextSession = sessionStart + SESSION_MS;
  sampleCount = 0;
  sessionActive = true;
  activeTest = requestedTest;
  requestedTest = 'R';
  Serial.printf("\n======== SESSAO %u | inicio %lu ms ========\n",
                completedSessions + 1, (unsigned long)sessionStart);
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 2.0");
  if (activeTest != 'R') Serial.printf("[TESTE] Serie controlada: %c\n", activeTest);
}

void readSerialCommand() {
  if (!Serial.available()) return;
  char command = toupper(Serial.read());
  if (command == 'A' || command == 'M' || command == 'N') {
    requestedTest = command;
    Serial.printf("[TESTE] Comando %c aceito para a PROXIMA sessao.\n", command);
  } else if (command == 'R') {
    requestedTest = 'R';
    Serial.println("[TESTE] Proxima sessao volta a valores pseudoaleatorios.");
  }
}

void printArray(const char *label, const int values[5]) {
  Serial.print(label);
  for (int i = 0; i < 5; ++i) {
    if (i) Serial.print(", ");
    Serial.print(values[i]);
  }
  Serial.println(" cm");
}

void finishSession() {
  int ordered[5];
  int total = 0;
  for (int i = 0; i < 5; ++i) {
    ordered[i] = samples[i];
    total += samples[i];
  }
  // Ordenacao na propria aplicacao, preservando samples na ordem original.
  for (int i = 1; i < 5; ++i) {
    int item = ordered[i];
    int j = i - 1;
    while (j >= 0 && ordered[j] > item) {
      ordered[j + 1] = ordered[j];
      --j;
    }
    ordered[j + 1] = item;
  }
  const int median = ordered[2];
  printArray("Ordem original: ", samples);
  printArray("Ordem crescente: ", ordered);
  Serial.printf("Media: %.1f cm | Mediana: %d cm\n", total / 5.0f, median);
  if (median >= 16) {
    alertState = true;
    Serial.println("Histerese: mediana >= 16 -> ALERTA.");
  } else if (median <= 14) {
    alertState = false;
    Serial.println("Histerese: mediana <= 14 -> NORMAL.");
  } else {
    Serial.println("Histerese: 14 < mediana < 16 -> mantem estado anterior.");
  }
  showState();
  Serial.printf("Proxima sessao: t=%lu ms (48 s desde o inicio desta).\n",
                (unsigned long)nextSession);
  ++completedSessions;
  sessionActive = false;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  randomSeed(esp_random());
  Serial.printf("\n=== ESP32 MOTIVA | FIRMWARE %s ===\n", FW_VERSION);
  showState();
  beginSession();
}

void loop() {
  readSerialCommand();
  const uint32_t now = millis();
  if (sessionActive && (int32_t)(now - nextRead) >= 0) {
    static const int alertSamples[5] = {18, 16, 17, 20, 15};
    static const int holdSamples[5] = {13, 15, 15, 16, 20};
    static const int normalSamples[5] = {10, 12, 13, 14, 15};
    samples[sampleCount] = activeTest == 'A' ? alertSamples[sampleCount]
                         : activeTest == 'M' ? holdSamples[sampleCount]
                         : activeTest == 'N' ? normalSamples[sampleCount]
                         : random(10, 21);
    Serial.printf("Leitura %u: %d cm (t=%lu ms)\n", sampleCount + 1,
                  samples[sampleCount], (unsigned long)(now - sessionStart));
    ++sampleCount;
    nextRead += READ_MS;
    if (sampleCount == 5) finishSession();
  }
  if (!sessionActive && (int32_t)(millis() - nextSession) >= 0) beginSession();
  delay(10);
}
