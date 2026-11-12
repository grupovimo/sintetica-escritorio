// Convierte lecturas crudas (botón o sensor táctil, acelerómetro) en eventos:
// toque, doble toque, caricia, muchos toques, levantado, sacudida (suave o
// fuerte), caída libre, inclinado y boca abajo.
#pragma once

#include "formato.h"
#include "mate.h"

namespace sintetica {

namespace umbrales {
constexpr uint32_t TOQUE_MAX_MS = 400;        // más largo deja de ser un toque
constexpr uint32_t DOBLE_TOQUE_MS = 350;      // ventana para el segundo toque
constexpr uint32_t CARICIA_MS = 600;          // mantener apretado = caricia
constexpr uint32_t MANTENIDO_MS = 5000;       // mantener 5 s = mostrar el QR para vincular
constexpr float SACUDIDA_G = 1.0f;            // desvío de |a| respecto de 1 g
constexpr int SACUDIDA_PICOS = 3;
constexpr uint32_t SACUDIDA_VENTANA_MS = 800;
constexpr float QUIETO_G = 0.06f;
constexpr uint32_t QUIETO_MS = 2000;
constexpr float LEVANTADO_G = 0.18f;
constexpr float BOCA_ABAJO_G = -0.75f;        // componente z con la pantalla hacia abajo
constexpr uint32_t BOCA_ABAJO_MS = 1000;
constexpr uint32_t ENFRIAMIENTO_MS = 2000;
constexpr float SACUDIDA_FUERTE_G = 2.0f;     // un pico de más de esto: sacudida fuerte
constexpr int MUCHOS_TOQUES = 4;              // tantos toques en...
constexpr uint32_t MUCHOS_TOQUES_MS = 2500;   // ...este tiempo
constexpr float CAIDA_G = 0.35f;              // |a| menor que esto: cayendo
constexpr uint32_t CAIDA_MS = 120;
constexpr float INCLINADO_G = 0.35f;          // componente en el plano de la pantalla
}  // namespace umbrales

class Gestos {
 public:
  // Estado del botón o del sensor táctil.
  void tacto(bool apretado, uint32_t ahora);
  // Aceleración en g. Por convención, z sale de la pantalla hacia el frente.
  void imu(float ax, float ay, float az, uint32_t ahora);
  // Avanza los temporizadores (el toque simple se confirma al vencer la ventana).
  void tick(uint32_t ahora);

  // Saca el próximo evento; devuelve false si no hay.
  bool sacar(Evento* e);
  // Hacia dónde está inclinado (la gravedad en el plano de la pantalla, en g),
  // si está quieto y más inclinado que INCLINADO_G.
  bool inclinacion(float* x, float* y) const {
    if (!inclinado_) return false;
    *x = gx_, *y = gy_;
    return true;
  }
  // true una vez cuando el botón estuvo apretado 5 s seguidos.
  bool sacarMantenido() {
    bool m = mantenido_;
    mantenido_ = false;
    return m;
  }

 private:
  void poner(Evento e);

  Evento cola_[8];
  uint8_t ini_ = 0, n_ = 0;

  bool apretado_ = false;
  uint32_t desde_ = 0;
  bool caricia_ = false;
  bool largo_ = false, mantenido_ = false;
  bool toquePendiente_ = false;
  uint32_t finToque_ = 0;

  uint32_t toques_[umbrales::MUCHOS_TOQUES] = {};
  uint8_t nToques_ = 0;
  float picoMax_ = 0;
  uint32_t cayendoDesde_ = 0;
  bool cayendo_ = false;
  float gx_ = 0, gy_ = 0;
  bool inclinado_ = false, avisadoInclinado_ = false;
  int picos_ = 0;
  uint32_t primerPico_ = 0, ultimoPico_ = 0;
  uint32_t enfriamiento_ = 0;
  uint32_t quietoDesde_ = 0;
  bool midiendoQuieto_ = false;
  bool quieto_ = false;
  uint32_t abajoDesde_ = 0;
  bool abajo_ = false, avisadoAbajo_ = false;
};

}  // namespace sintetica
