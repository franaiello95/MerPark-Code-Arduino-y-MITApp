#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// ====== CREDENCIALES WIFI ======
const char* ssid = "6_1_2043";
const char* password = "acebalenpastoral";

// ====== FIREBASE (REST) ======
const char* host = "sahurparkingfirebase-default-rtdb.firebaseio.com";
const int httpsPort = 443;
const char* auth = "";  // vacío si la DB es pública

// Recursos Firebase
const char* recursosPlaza[2] = {
  "/Parking_copy_copy_checkpoint18_checkpoint3/plaza1.json",
  "/Parking_copy_copy_checkpoint18_checkpoint3/plaza2.json"
};
const char* recursosContador[2] = {
  "/Parking_copy_copy_checkpoint18_checkpoint3/Contador1.json",
  "/Parking_copy_copy_checkpoint18_checkpoint3/Contador2.json"
};

WiFiClientSecure client;

// ====== VARIABLES GLOBALES ======
String estadoFisico[2] = {"", ""};
String ultimoPublicado[2] = {"", ""};
int contadorActualSeg[2] = {0, 0};

bool reservaActiva[2] = {false, false};
unsigned long reservaFinMs[2] = {0, 0};
unsigned long lastLecturaFirebase = 0;
const unsigned long intervaloLectura = 700;  // 🔄 lectura más rápida
unsigned long lastDataReceived = 0;
const unsigned long avisoIntervalo = 5000;

// ====== WIFI ======
void setupWifi() {
  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  client.setInsecure();
  client.setTimeout(3000);
}

// ====== LECTURA FIREBASE ======
String leerDeFirebase(const char* recurso) {
  if (!client.connect(host, httpsPort)) return "";
  String url = String(recurso);
  if (strlen(auth) > 0) url += "?auth=" + String(auth);

  String request =
    String("GET ") + url + " HTTP/1.1\r\n" +
    "Host: " + host + "\r\n" +
    "Connection: close\r\n\r\n";

  client.print(request);
  String statusLine = client.readStringUntil('\n');
  if (!statusLine.startsWith("HTTP/1.1 200")) {
    client.stop();
    return "";
  }

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String cuerpo = client.readString();
  cuerpo.trim();
  client.stop();

  if (cuerpo.startsWith("\"") && cuerpo.endsWith("\""))
    cuerpo = cuerpo.substring(1, cuerpo.length() - 1);

  return cuerpo;
}

// ====== ESCRITURA FIREBASE ======
bool enviarAFirebase(const char* recurso, const String& valor) {
  if (!client.connect(host, httpsPort)) return false;

  String url = String(recurso);
  if (strlen(auth) > 0) url += "?auth=" + String(auth);
  String cuerpo = "\"" + valor + "\"";

  String request =
    String("PUT ") + url + " HTTP/1.1\r\n" +
    "Host: " + host + "\r\n" +
    "Connection: close\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + String(cuerpo.length()) + "\r\n\r\n" +
    cuerpo;

  client.print(request);
  client.readStringUntil('\n');
  client.stop();
  return true;
}

// ====== SETUP ======
void setup() {
  Serial.begin(9600);  // comunicación con el Mega
  delay(1000);
  setupWifi();
  Serial.println("🚀 Wemos listo (2 plazas con prioridad física y LED rápido)");
  lastDataReceived = millis();
}

// ====== LOOP ======
void loop() {
  // 1️⃣ Leer datos físicos del Mega
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    if (msg.startsWith("estado1=")) estadoFisico[0] = msg.substring(8);
    else if (msg.startsWith("estado2=")) estadoFisico[1] = msg.substring(8);
    lastDataReceived = millis();
  }

  // 2️⃣ Leer Firebase rápido
  if (millis() - lastLecturaFirebase > intervaloLectura) {
    for (int i = 0; i < 2; i++) {
      String contadorStr = leerDeFirebase(recursosContador[i]);
      if (contadorStr == "") continue;

      int nuevoContadorSeg = contadorStr.toInt();

      // Nueva reserva detectada
      if (nuevoContadorSeg > 0) {
        contadorActualSeg[i] = nuevoContadorSeg;
        reservaFinMs[i] = millis() + (unsigned long)nuevoContadorSeg * 1000UL;
        reservaActiva[i] = true;

        // 🔸 Avisar instantáneamente al Mega
        Serial.println("reserva:" + String(i + 1));
      } else if (reservaActiva[i] && millis() >= reservaFinMs[i]) {
        // Fin de reserva
        reservaActiva[i] = false;
        reservaFinMs[i] = 0;
        contadorActualSeg[i] = 0;
        Serial.println("reserva:" + String(i + 1) + "_fin");
      }

      // Prioridad física
      if (estadoFisico[i] == "ocupado") {
        enviarAFirebase(recursosContador[i], "0");
        reservaActiva[i] = false;
        reservaFinMs[i] = 0;
        contadorActualSeg[i] = 0;
      }

      // 3️⃣ Determinar estado a subir
      String estadoParaSubir;
      if (estadoFisico[i] == "ocupado") estadoParaSubir = "ocupado";
      else if (reservaActiva[i]) estadoParaSubir = "reservado";
      else estadoParaSubir = estadoFisico[i];

      if (estadoParaSubir != ultimoPublicado[i] && estadoParaSubir != "") {
        enviarAFirebase(recursosPlaza[i], estadoParaSubir);
        ultimoPublicado[i] = estadoParaSubir;
      }
    }

    lastLecturaFirebase = millis();
  }

  // 4️⃣ Aviso si no llegan datos del Mega
  if (millis() - lastDataReceived > avisoIntervalo) {
    Serial.println("⏳ Esperando datos del Mega...");
    lastDataReceived = millis();
  }

  delay(40);  // ⚡ refresco más rápido
}
