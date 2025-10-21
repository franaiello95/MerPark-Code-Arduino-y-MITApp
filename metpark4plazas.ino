#include <Servo.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN 2
#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ===== Pines =====
const int sensorEntradaPin = 53;
const int lugarPin[2] = {33, 31};
const int servoPin = 51;

// ===== Barrera =====
Servo barrera;
const int ANGULO_ABIERTO = 160;
const int ANGULO_CERRADO = 90;
unsigned long tiempoEspera = 1500;
bool barreraAbierta = false;
unsigned long tiempoCierreProgramado = 0;

// ===== Reservas =====
unsigned long tiempoReservaRestante[2] = {0,0};
unsigned long reservaInicio[2] = {0,0};
bool reservaActiva[2] = {false,false};

// ===== Estado =====
String estadoAnterior[2] = {"",""};
unsigned long lastEnvioEstado[2] = {0,0};
const unsigned long intervaloEnvio = 10000;

// ===== LED CONTROL =====
void setColor(uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}

void actualizarLedPlaza1() {
  if (reservaActiva[0]) {
    setColor(255, 255, 0);  // 🟡 Amarillo = reservado
  } else {
    bool libre = digitalRead(lugarPin[0]) == HIGH;
    if (libre) setColor(0, 255, 0);  // 🟢 Verde = libre
    else setColor(255, 0, 0);        // 🔴 Rojo = ocupado
  }
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);

  barrera.attach(servoPin);
  barrera.write(ANGULO_CERRADO);

  pinMode(sensorEntradaPin, INPUT);
  for (int i = 0; i < 2; i++) pinMode(lugarPin[i], INPUT);

  pixels.begin();
  setColor(0, 0, 0);  // apagado al inicio

  Serial.println("🟢 Mega listo con 2 plazas + LED Neopixel plaza1");
  delay(2000);
}

// ===== Envío de estado =====
void enviarEstado(int plaza, const String& estado) {
  Serial1.println("estado" + String(plaza+1) + "=" + estado);
  Serial.print("→ Enviado plaza");
  Serial.print(plaza+1);
  Serial.print(": ");
  Serial.println(estado);
}

// ===== Control barrera =====
void controlarBarrera(bool lugarLibre, int entradaDetectada) {
  if (lugarLibre && entradaDetectada == LOW && !barreraAbierta) {
    barrera.write(ANGULO_ABIERTO);
    barreraAbierta = true;
    Serial.println("🚗 Barrera abierta");
  }
  if (barreraAbierta && entradaDetectada == HIGH && tiempoCierreProgramado == 0) {
    tiempoCierreProgramado = millis() + tiempoEspera;
    Serial.println("⏳ Auto salió, cerrando barrera en 1.5s...");
  }
  if (tiempoCierreProgramado > 0 && millis() >= tiempoCierreProgramado) {
    barrera.write(ANGULO_CERRADO);
    barreraAbierta = false;
    tiempoCierreProgramado = 0;
    Serial.println("⛔ Barrera cerrada automáticamente");
  }
}

// ===== Activar reserva =====
void activarReserva(int plaza, unsigned long tiempoMs) {
  tiempoReservaRestante[plaza] = tiempoMs;
  reservaInicio[plaza] = millis();
  reservaActiva[plaza] = true;
  Serial.print("📅 Reserva activada plaza ");
  Serial.print(plaza+1);
  Serial.print(" por (ms): ");
  Serial.println(tiempoMs);

  actualizarLedPlaza1();
}

// ===== Loop =====
void loop() {
  int entradaDetectada = digitalRead(sensorEntradaPin);

  for (int i = 0; i < 2; i++) {
    bool lugarLibre = digitalRead(lugarPin[i]) == HIGH;

    if (reservaActiva[i]) {
      unsigned long elapsed = millis() - reservaInicio[i];
      if (elapsed >= tiempoReservaRestante[i]) {
        reservaActiva[i] = false;
        tiempoReservaRestante[i] = 0;
        Serial.print("⌛ Reserva expirada plaza ");
        Serial.println(i+1);
        Serial1.println("reserva" + String(i+1) + "=expirada");

        if (lugarLibre) {
          enviarEstado(i, "libre");
          estadoAnterior[i] = "libre";
          lastEnvioEstado[i] = millis();
        }
      }
    }

    String estadoActual;
    if (!lugarLibre) estadoActual = "ocupado";
    else if (reservaActiva[i]) estadoActual = "reservado";
    else estadoActual = "libre";

    if (estadoActual != estadoAnterior[i]) {
      enviarEstado(i, estadoActual);
      estadoAnterior[i] = estadoActual;
      lastEnvioEstado[i] = millis();
    } else if (millis() - lastEnvioEstado[i] >= intervaloEnvio) {
      enviarEstado(i, estadoActual);
      lastEnvioEstado[i] = millis();
    }
  }

  bool algunLugarLibre = digitalRead(lugarPin[0]) == HIGH || digitalRead(lugarPin[1]) == HIGH;
  controlarBarrera(algunLugarLibre, entradaDetectada);

  // Leer mensajes del Wemos
  if (Serial1.available()) {
    String comando = Serial1.readStringUntil('\n');
    comando.trim();

    if (comando.startsWith("reservar1:")) {
      unsigned long tiempoMs = comando.substring(10).toInt();
      activarReserva(0, tiempoMs);
    } 
    else if (comando.startsWith("reservar2:")) {
      unsigned long tiempoMs = comando.substring(10).toInt();
      activarReserva(1, tiempoMs);
    }
    else if (comando == "reserva:1") {
      reservaActiva[0] = true;
      actualizarLedPlaza1();
    }
    else if (comando == "reserva:1_fin") {
      reservaActiva[0] = false;
      actualizarLedPlaza1();
    }
  }

  actualizarLedPlaza1();  // mantenerlo actualizado constantemente
  delay(100);
}
