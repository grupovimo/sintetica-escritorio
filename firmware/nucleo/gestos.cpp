#include "gestos.h"

namespace sintetica {

void Gestos::poner(Evento e) {
  if (n_ >= 8) return;
  cola_[(ini_ + n_) % 8] = e;
  n_++;
}

bool Gestos::sacar(Evento* e) {
  if (n_ == 0) return false;
  *e = cola_[ini_];
  ini_ = (ini_ + 1) % 8;
  n_--;
  return true;
}

void Gestos::tacto(bool apretado, uint32_t ahora) {
  if (apretado == apretado_) return;
  apretado_ = apretado;
  if (apretado) {
    desde_ = ahora;
    caricia_ = false;
    largo_ = false;
    return;
  }
  const uint32_t dur = ahora - desde_;
  if (caricia_ || dur > umbrales::TOQUE_MAX_MS) return;
  // Muchos toques seguidos (cosquillas).
  for (int i = umbrales::MUCHOS_TOQUES - 1; i > 0; i--) toques_[i] = toques_[i - 1];
  toques_[0] = ahora;
  if (nToques_ < umbrales::MUCHOS_TOQUES) nToques_++;
  if (nToques_ >= umbrales::MUCHOS_TOQUES && ahora - toques_[umbrales::MUCHOS_TOQUES - 1] <= umbrales::MUCHOS_TOQUES_MS) {
    nToques_ = 0;
    toquePendiente_ = false;
    poner(Evento::MuchosToques);
    return;
  }
  if (toquePendiente_ && ahora - finToque_ <= umbrales::DOBLE_TOQUE_MS) {
    toquePendiente_ = false;
    poner(Evento::DobleToque);
  } else {
    toquePendiente_ = true;
    finToque_ = ahora;
  }
}

void Gestos::tick(uint32_t ahora) {
  if (apretado_ && !largo_ && ahora - desde_ >= umbrales::MANTENIDO_MS) {
    largo_ = true;
    mantenido_ = true;
  }
  if (apretado_ && !caricia_ && ahora - desde_ >= umbrales::CARICIA_MS) {
    caricia_ = true;
    toquePendiente_ = false;
    poner(Evento::Caricia);
  }
  if (toquePendiente_ && !apretado_ && ahora - finToque_ > umbrales::DOBLE_TOQUE_MS) {
    toquePendiente_ = false;
    poner(Evento::Tacto);
  }
}

void Gestos::imu(float ax, float ay, float az, uint32_t ahora) {
  const float modulo = raiz(ax * ax + ay * ay + az * az);
  const float desvio = absf(modulo - 1.0f);
  const bool enfriando = static_cast<int32_t>(ahora - enfriamiento_) < 0;

  // Caída libre: casi sin gravedad un rato.
  if (modulo < umbrales::CAIDA_G) {
    if (!cayendo_) cayendo_ = true, cayendoDesde_ = ahora;
    if (ahora - cayendoDesde_ >= umbrales::CAIDA_MS && !enfriando) {
      enfriamiento_ = ahora + umbrales::ENFRIAMIENTO_MS;
      quieto_ = false;
      cayendo_ = false;
      poner(Evento::Caida);
      return;
    }
  } else {
    cayendo_ = false;
  }

  // Sacudida: varios picos fuertes en poco tiempo (fuerte si alguno pasó 2 g).
  if (desvio > umbrales::SACUDIDA_G && ahora - ultimoPico_ > 80) {
    if (picos_ == 0 || ahora - primerPico_ > umbrales::SACUDIDA_VENTANA_MS) picos_ = 0, primerPico_ = ahora, picoMax_ = 0;
    picos_++;
    picoMax_ = maxf(picoMax_, desvio);
    ultimoPico_ = ahora;
    if (picos_ >= umbrales::SACUDIDA_PICOS && !enfriando) {
      picos_ = 0;
      quieto_ = false;
      enfriamiento_ = ahora + umbrales::ENFRIAMIENTO_MS;
      poner(picoMax_ > umbrales::SACUDIDA_FUERTE_G ? Evento::SacudidaFuerte : Evento::SacudidaSuave);
      return;
    }
  }

  // Inclinado: quieto y con la gravedad corrida en el plano de la pantalla.
  if (desvio < 0.15f) {
    gx_ = ax, gy_ = ay;
    const bool inc = ax * ax + ay * ay > umbrales::INCLINADO_G * umbrales::INCLINADO_G && az > -0.5f;
    if (inc && !avisadoInclinado_ && !enfriando) {
      avisadoInclinado_ = true;
      poner(Evento::Inclinado);
    }
    if (!inc) avisadoInclinado_ = false;
    inclinado_ = inc;
  } else {
    inclinado_ = false;
  }

  // Levantado: estaba quieto un rato y se movió suave.
  if (desvio < umbrales::QUIETO_G) {
    if (!midiendoQuieto_) midiendoQuieto_ = true, quietoDesde_ = ahora;
    if (ahora - quietoDesde_ >= umbrales::QUIETO_MS) quieto_ = true;
  } else {
    midiendoQuieto_ = false;
    if (quieto_ && desvio > umbrales::LEVANTADO_G && desvio <= umbrales::SACUDIDA_G && !enfriando) {
      quieto_ = false;
      enfriamiento_ = ahora + umbrales::ENFRIAMIENTO_MS;
      poner(Evento::Levantado);
    } else if (desvio > umbrales::SACUDIDA_G) {
      quieto_ = false;
    }
  }

  // Boca abajo: la pantalla mira al piso por un segundo.
  const bool abajo = az < umbrales::BOCA_ABAJO_G;
  if (abajo && !abajo_) abajoDesde_ = ahora;
  abajo_ = abajo;
  if (!abajo) avisadoAbajo_ = false;
  if (abajo && !avisadoAbajo_ && ahora - abajoDesde_ >= umbrales::BOCA_ABAJO_MS) {
    avisadoAbajo_ = true;
    poner(Evento::BocaAbajo);
  }
}

}  // namespace sintetica
