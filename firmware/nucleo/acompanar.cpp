// Lo que hace que el bicho acompañe el día (v1.7, docs/motor-de-vida.md,
// "Acompañar"): cuidarte (el agua, la pausa activa, el "¿cómo estás?") y los
// juegos con toques y movimiento. Son métodos del Sistema: usan el panel, el
// globo, el motor y el protocolo.
#include "fuentes.h"
#include "globo.h"
#include "sistema.h"
#include "texto.h"
#include "vida_base.h"

namespace sintetica {

namespace {
int minutosDesde(int desde, int hasta) { return ((hasta - desde) % 1440 + 1440) % 1440; }

constexpr uint32_t PREGUNTA_MS = 45000;      // cuánto espera la respuesta al "¿cómo estás?"
constexpr uint32_t DOBLE_MS = 400;           // un segundo toque antes de esto es "doble"
constexpr uint32_t JUEGO_QUIETO_MS = 30000;  // sin que juegues, el juego se termina
constexpr uint32_t RESULTADO_MS = 3000;      // el resultado queda en pantalla

const char* const NOMBRES_JUEGO[] = {"ppt", "reflejos", "mirada", "dado", "moneda"};
const char* const PPT[] = {"Piedra", "Papel", "Tijera"};
}  // namespace

// ---------------------------------------------------------------- cuidarte

void Sistema::cuidar(uint32_t ahora) {
  if (vistaPrevia_ || apagado_ || emparejando_ || motor_.dormido()) return;
  const Cuidado& c = config_.cuidado;
  const int m = minutoDelDia();
  const bool despierto = m >= 0 && minutosDesde(config_.vida.despierta, m) < minutosDesde(config_.vida.despierta, config_.vida.duerme);
  // Agua: cada tantas horas mientras está despierto (por horario).
  if (c.agua && despierto) {
    const uint32_t cada = c.agua * 3600000u;
    if (!proximaAgua_) proximaAgua_ = ahora + cada;
    if (static_cast<int32_t>(ahora - proximaAgua_) >= 0) {
      proximaAgua_ = ahora + cada;
      motor_.anotarHoy(Hoy::Agua);
      motor_.reaccionar(config_.resolverCara("sorpresa"), ahora, 1500);
      hablar(Situacion::Agua);
    }
  }
  // Pausa activa: mucho rato sin que lo toques en el horario de trabajo.
  const float ignorado = motor_.variable(VarVida::Ignorado);  // minutos
  if (ignorado < 1) pausaDicha_ = false;
  if (c.pausa && m >= 0 && !pausaDicha_ && ignorado >= c.pausa && m >= c.desde && m < c.hasta) {
    const uint32_t e = epoch();
    const int semana = e ? static_cast<int>((e / 86400 + 3) % 7) : -1;
    if (semana < 0 || (c.dias & (1u << semana))) {
      pausaDicha_ = true;
      const int i = config_.vida.indiceComportamiento("estirarse");
      if (i >= 0) motor_.comportamiento(i, ahora);
      hablar(Situacion::Pausa);
    }
  }
  // "¿Cómo estás?": una vez por día, cuando estás (lo tocaste hace poco), de
  // una hora después de despertarse a una antes de dormir.
  const uint32_t ep = epoch();
  const uint32_t ultima = motor_.memoria().ultimaInteraccion;
  // v3: si anduviste bajoneado estos días, pregunta otra vez unas horas después.
  const bool otraVez = !animoOtra_ && animoPregMs_ && motor_.empatico() && caracter::OTRA_PREGUNTA_H > 0 &&
                       ahora - animoPregMs_ >= caracter::OTRA_PREGUNTA_H * 3600000u;
  // Te vio hace menos de 2 min (y no recién prendido: que no te frene el primer toque).
  const bool estas = ep && ultima && ep >= ultima && ep - ultima < 120 && ahora - arranque_ >= 10u * 60000u;
  if (c.animo && !preguntando_ && despierto && estas && !globo_ && !hayPendiente_ && !alarma_ &&
      recVisible_ < 0 && juego_ == Juego::Ninguno && minutosDesde(config_.vida.despierta, m) >= 60 &&
      minutosDesde(m, config_.vida.duerme) >= 60) {
    const uint32_t e = epoch();
    const uint16_t dia = e ? static_cast<uint16_t>(e / 86400u + 1u) : 0;
    const Memoria& mem = motor_.memoria();
    const bool respondioHoy = mem.hoyAnimo && mem.hoyDia == static_cast<uint8_t>((e / 86400u) % 251u + 1u);
    if (dia && dia != animoDia_ && !respondioHoy) {
      animoDia_ = dia;
      animoOtra_ = false;
      animoPregMs_ = ahora ? ahora : 1;
      preguntarAnimo(ahora);
    } else if (dia && dia == animoDia_ && otraVez) {
      animoOtra_ = true;
      preguntarAnimo(ahora);
    }
  }
  // Sin respuesta: se olvida (la nube puede preguntar por Telegram).
  if (preguntando_ && ahora - pregDesde_ >= PREGUNTA_MS) {
    preguntando_ = false;
    globoHasta_ = ahora;
  }
  // Un toque solo (sin el segundo a tiempo): "bien".
  if (preguntando_ && pregToques_ == 1 && !pregApretado_ && ahora - pregToque_ >= DOBLE_MS) responderAnimo(0);
}

void Sistema::preguntarAnimo(uint32_t ahora) {
  char t[LARGO_FRASE];
  if (!elegirFrase(config_.habla, Situacion::ComoEstas, presetCercano(motor_.rasgos()), config_.vida.dueno, nombre(), azar_,
                   t, sizeof t))
    copiarTexto(t, sizeof t, "\xC2\xBF" "C\xC3\xB3mo est\xC3\xA1s?");
  decir(t, config_.resolverCara("curioso"));
  preguntando_ = true;
  pregDesde_ = ahora;
  pregToques_ = 0;
  pregApretado_ = false;
}

void Sistema::responderAnimo(uint8_t r) {
  if (r > 2) return;
  const uint32_t ahora = hal_.ahoraMs();
  preguntando_ = false;
  pregToques_ = 0;
  if (globo_) globoHasta_ = ahora;
  hayPendiente_ = false;
  motor_.anotarHoy(Hoy::Animo, static_cast<uint8_t>(r + 1));
  motor_.anotarRegistro(6, r);
  evento(Evento::Animo);
  notificarEvento(Evento::Animo, 6, r);
  // Con empatía: si estás mal, se pone cariñoso.
  static const char* const CARAS[] = {"emocionado", "contento", "enamorado"};
  static const Situacion FRASES[] = {Situacion::AnimoBien, Situacion::AnimoMedio, Situacion::AnimoMal};
  motor_.reaccionar(config_.resolverCara(CARAS[r]), ahora, r == 2 ? 8000 : 4000);
  hablar(FRASES[r]);
  log("ánimo respondido");
}

// ---------------------------------------------------------------- juegos

bool Sistema::jugar(Juego j) {
  const uint32_t ahora = hal_.ahoraMs();
  if (j == Juego::Ninguno) {
    if (juego_ != Juego::Ninguno) terminarJuego(3);
    return true;
  }
  if (static_cast<int>(j) > static_cast<int>(Juego::Moneda)) return false;
  if (motor_.dormido()) motor_.despertar(ahora, false);
  if (alarma_) apagarAlarma();
  globo_ = hayPendiente_ = false;
  preguntando_ = false;
  juego_ = j;
  juegoFase_ = 0;
  juegoRonda_ = juegoPuntos_ = juegoDato_ = 0;
  juegoLado_ = 0;
  juegoDesde_ = juegoAccion_ = ahora;
  juegoProximo_ = ahora;
  juegoApretado_ = false;
  switch (j) {
    case Juego::Dado:
    case Juego::Moneda: copiarTexto(juegoTexto_, sizeof juegoTexto_, "\xC2\xA1Sacudime!"); break;
    case Juego::Reflejos:
      copiarTexto(juegoTexto_, sizeof juegoTexto_, "Toc\xC3\xA1 en el verde");
      juegoProximo_ = ahora + 1500 + azar_.entre(0, 2500);
      break;
    case Juego::Mirada:
      copiarTexto(juegoTexto_, sizeof juegoTexto_, "Inclinalo adonde miro");
      juegoProximo_ = ahora + 1500;
      break;
    default: copiarTexto(juegoTexto_, sizeof juegoTexto_, PPT[0]); break;
  }
  motor_.reaccionar(config_.resolverCara("emocionado"), ahora, 1500);
  actividad();
  log("a jugar");
  return true;
}

void Sistema::tactoJuego(bool apretado, uint32_t ahora) {
  if (apretado == juegoApretado_) return;
  juegoApretado_ = apretado;
  if (!apretado) return;
  juegoAccion_ = ahora;
  actividad();
  switch (juego_) {
    case Juego::Ppt:
      if (juegoFase_ == 0) {
        // Tu elección es la que se veía; la suya, al azar.
        const int vos = juegoDato_ % 3, yo = static_cast<int>(azar_.entre(0, 2));
        size_t n = copiarTexto(juegoTexto_, sizeof juegoTexto_, PPT[vos]);
        n += copiarTexto(juegoTexto_ + n, sizeof juegoTexto_ - n, " vs ");
        copiarTexto(juegoTexto_ + n, sizeof juegoTexto_ - n, PPT[yo]);
        juegoFase_ = 1;
        juegoProximo_ = ahora + RESULTADO_MS;
        // Piedra le gana a tijera, papel a piedra, tijera a papel.
        juegoPuntos_ = vos == yo ? 2 : ((vos + 3 - yo) % 3 == 1 ? 1 : 0);
      }
      break;
    case Juego::Reflejos:
      if (juegoFase_ == 0) {
        copiarTexto(juegoTexto_, sizeof juegoTexto_, "\xC2\xA1Muy r\xC3\xA1pido!");
        juegoFase_ = 2, juegoPuntos_ = 0;
        juegoProximo_ = ahora + RESULTADO_MS;
      } else if (juegoFase_ == 1) {
        const uint32_t ms = ahora - juegoDesde_;
        char t[16];
        int k = 0;
        uint32_t x = ms > 9999 ? 9999 : ms;
        char d[6];
        int nd = 0;
        do d[nd++] = static_cast<char>('0' + x % 10), x /= 10;
        while (x);
        while (nd) t[k++] = d[--nd];
        copiarTexto(t + k, sizeof t - k, " ms");
        copiarTexto(juegoTexto_, sizeof juegoTexto_, t);
        motor_.anotarReflejo(static_cast<uint16_t>(ms > 65535 ? 65535 : ms));
        juegoFase_ = 2, juegoPuntos_ = ms < 450 ? 1 : 0;
        juegoProximo_ = ahora + RESULTADO_MS;
      }
      break;
    case Juego::Dado:
    case Juego::Moneda:
      if (juegoFase_ == 0) juegoFase_ = 1, juegoDesde_ = ahora, juegoProximo_ = ahora;
      break;
    default: break;
  }
}

void Sistema::avanzarJuego(uint32_t ahora) {
  if (juego_ == Juego::Ninguno) return;
  if (ahora - juegoAccion_ >= JUEGO_QUIETO_MS && juegoFase_ == 0) {
    terminarJuego(3);
    return;
  }
  const bool toca = static_cast<int32_t>(ahora - juegoProximo_) >= 0;
  switch (juego_) {
    case Juego::Ppt:
      if (juegoFase_ == 0 && toca) {
        juegoDato_ = static_cast<uint8_t>((juegoDato_ + 1) % 3);
        copiarTexto(juegoTexto_, sizeof juegoTexto_, PPT[juegoDato_]);
        juegoProximo_ = ahora + 450;
      } else if (juegoFase_ == 1 && toca) {
        terminarJuego(juegoPuntos_);
      }
      break;
    case Juego::Reflejos:
      if (juegoFase_ == 0 && toca) {
        juegoFase_ = 1;
        juegoDesde_ = ahora;  // desde el verde
        copiarTexto(juegoTexto_, sizeof juegoTexto_, "\xC2\xA1Ya!");
      } else if (juegoFase_ == 1 && ahora - juegoDesde_ > 3000) {
        copiarTexto(juegoTexto_, sizeof juegoTexto_, "Muy lento");
        juegoFase_ = 2, juegoPuntos_ = 0;
        juegoProximo_ = ahora + RESULTADO_MS;
      } else if (juegoFase_ == 2 && toca) {
        terminarJuego(juegoPuntos_);
      }
      break;
    case Juego::Dado:
    case Juego::Moneda:
      if (juegoFase_ == 1) {
        // Rueda 1,2 s, cambiando cada 90 ms; después, el resultado.
        if (ahora - juegoDesde_ >= 1200) {
          juegoFase_ = 2;
          juegoProximo_ = ahora + RESULTADO_MS;
          motor_.reaccionar(config_.resolverCara("sorpresa"), ahora, 1500);
          sonar("dado");
        } else if (toca) {
          juegoProximo_ = ahora + 90;
          if (juego_ == Juego::Dado) {
            juegoTexto_[0] = static_cast<char>('1' + azar_.entre(0, 5)), juegoTexto_[1] = 0;
          } else {
            copiarTexto(juegoTexto_, sizeof juegoTexto_, azar_.entre(0, 1) ? "Cara" : "Ceca");
          }
        }
      } else if (juegoFase_ == 2 && toca) {
        terminarJuego(3);
      }
      break;
    case Juego::Mirada: {
      // Cinco rondas: mira a un lado y tenés 2,5 s para inclinarlo hacia ahí.
      if (juegoFase_ == 0 && toca) {
        if (juegoRonda_ >= 5) {
          char t[24];
          t[0] = static_cast<char>('0' + juegoPuntos_);
          copiarTexto(t + 1, sizeof t - 1, " de 5");
          copiarTexto(juegoTexto_, sizeof juegoTexto_, t);
          juegoFase_ = 2;
          juegoProximo_ = ahora + RESULTADO_MS;
          break;
        }
        juegoLado_ = azar_.entre(0, 1) ? 1 : -1;
        juegoRonda_++;
        juegoFase_ = 1;
        juegoDesde_ = ahora;
        copiarTexto(juegoTexto_, sizeof juegoTexto_, juegoLado_ > 0 ? "Para all\xC3\xA1" : "Para ac\xC3\xA1");
      }
      if (juegoFase_ == 1) {
        motor_.seguir(static_cast<float>(juegoLado_), 0, ahora);
        float gx = 0, gy = 0;
        const bool inclinado = gestos_.inclinacion(&gx, &gy) && absf(gx) > 0.25f;
        if (inclinado || ahora - juegoDesde_ >= 2500) {
          // En el mismo sentido que la gravedad que "cae" los ojos (Sistema::tick).
          const bool bien = inclinado && (-gx > 0) == (juegoLado_ > 0);
          if (bien) juegoPuntos_++;
          juegoAccion_ = ahora;
          motor_.reaccionar(config_.resolverCara(bien ? "contento" : "confundido"), ahora, 800);
          juegoFase_ = 0;
          juegoProximo_ = ahora + 1200;
        }
      } else if (juegoFase_ == 2 && toca) {
        terminarJuego(juegoPuntos_ >= 4 ? 1 : 0);
      }
      break;
    }
    default: break;
  }
}

void Sistema::terminarJuego(int resultado) {
  if (juego_ == Juego::Ninguno) return;
  const uint32_t ahora = hal_.ahoraMs();
  const int j = static_cast<int>(juego_);
  juego_ = Juego::Ninguno;
  juegoApretado_ = false;
  if (resultado == 3 && ahora - juegoAccion_ >= JUEGO_QUIETO_MS) return;  // nadie jugó: no cuenta
  motor_.anotarHoy(Hoy::Juego);
  if (resultado == 1) motor_.anotarHoy(Hoy::JuegoGanado);
  // v3: para acordarse ("ayer me ganaste al dado") y proponerte el que más jugás.
  static const TipoEpisodio EP[] = {TipoEpisodio::Gane, TipoEpisodio::Perdi, TipoEpisodio::Empate};
  if (resultado <= 2) motor_.recordar(EP[resultado], static_cast<uint8_t>(j));
  const uint8_t dato = static_cast<uint8_t>(j * 16 + resultado);
  motor_.anotarRegistro(5, dato);
  evento(Evento::Juego);
  notificarEvento(Evento::Juego, 5, dato);
  // "Gané" es que ganó él (perdiste vos).
  if (resultado == 0) {
    motor_.reaccionar(config_.resolverCara("orgulloso"), ahora, 2500);
    hablar(Situacion::Gane);
  } else if (resultado == 1) {
    motor_.reaccionar(config_.resolverCara("sorpresa"), ahora, 2500);
    hablar(Situacion::Perdi);
  } else if (resultado == 2) {
    motor_.reaccionar(config_.resolverCara("guino"), ahora, 2000);
    hablar(Situacion::Empate);
  }
}

void Sistema::dibujarJuego(float cx, float W, float H, float alfa) {
  const bool redonda = perfil_.redonda;
  const float cy = H * (redonda ? 0.66f : 0.70f);
  if (juego_ == Juego::Reflejos && juegoFase_ <= 1) {
    // El semáforo: gris esperando, verde para tocar.
    const float r = H * (redonda ? 0.07f : 0.1f);
    const float x = cx - W * (redonda ? 0.22f : 0.3f);
    lienzo_.opacidad(alfa);
    lienzo_.pildora(x, cy, 2 * r, 2 * r, r, juegoFase_ == 1 ? 0x3DDC84u : paleta::LINEA);
  }
  lienzo_.opacidad(alfa);
  const bool grande = juego_ == Juego::Dado && juegoTexto_[0] >= '1' && juegoTexto_[0] <= '6' && !juegoTexto_[1];
  if (grande) {
    const Fuente& fu = fuentes::VOZ_20;
    const float esc = redonda ? 2.6f : 3.0f;
    dibujarTextoEscalado(lienzo_, fu, juegoTexto_, cx - anchoTexto(fu, juegoTexto_) * esc / 2,
                         cy + fu.ascenso * 0.72f * esc / 2, esc, paleta::TINTA);
    return;
  }
  TextoAjustado a;
  ajustarTexto(juegoTexto_, cy, redonda ? W * 0.7f : W - 40, 2, redonda, W, H, 8, &a);
  dibujarAjustado(lienzo_, a, cx, paleta::TINTA);
}

const char* nombreJuego(int j) { return j >= 0 && j < 5 ? NOMBRES_JUEGO[j] : ""; }

}  // namespace sintetica
