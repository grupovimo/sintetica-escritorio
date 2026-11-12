// La charla sin IA en el robot (v1.10): Sistema::charlar arma el contexto
// (la hora, el clima, los hábitos, los gustos), le pregunta al motor de
// charla (charla.cpp), hace la acción y contesta con su globo y su voz propia.
#include "sistema.h"

namespace sintetica {

namespace {
constexpr const char* CLAVE_GUSTOS = "gustos";
constexpr uint32_t CALLADO_MS = 60u * 60000u;  // "callate": una hora sin frases espontáneas
constexpr uint32_t VOZ_ADELANTO = 2400;        // muestras (150 ms) que se mandan de más
}  // namespace

size_t Sistema::gustos(char* salida, size_t cap) const {
  if (!cap) return 0;
  salida[0] = 0;
  if (!hal_.leer) return 0;
  const int n = hal_.leer(CLAVE_GUSTOS, reinterpret_cast<uint8_t*>(salida), cap - 1);
  if (n <= 0) {
    salida[0] = 0;
    return 0;
  }
  salida[n] = 0;
  return static_cast<size_t>(n);
}

bool Sistema::charlar(const char* texto, size_t n, RespuestaCharla* r, bool siNoEntiende) {
  const uint32_t ahora = hal_.ahoraMs();
  ContextoCharla ctx;
  ctx.dueno = config_.vida.dueno;
  ctx.robot = nombre();
  ctx.preset = presetCercano(motor_.rasgos());
  ctx.minuto = minutoDelDia();
  ctx.epoch = epoch();
  char clima[40] = {};
  if (climaVigente() && textoClima(clima_, clima, sizeof clima)) ctx.clima = clima;
  for (int i = 0; i < config_.habitos.n; i++) ctx.habitos[i] = config_.habitos.lista[i].nombre;
  ctx.nHabitos = config_.habitos.n;
  char gs[LARGO_LISTA_GUSTOS + 1];
  if (gustos(gs, sizeof gs)) ctx.gustos = gs;
  ctx.diasJuntos = motor_.memoria().dias ? motor_.memoria().dias : -1;
  ctx.alarmaSonando = alarma_;
  ctx.segundos = ahora / 1000u;
  ctx.semilla = azar_.siguiente();
  sintetica::charlar(texto, n, ctx, charla_, r);
  actividad();
  motor_.interaccion(ahora);
  if (r->noEntendi && !siNoEntiende) {
    // Que conteste otro (la IA): mientras, piensa.
    motor_.reaccionar(config_.resolverCara("pensando"), ahora, 2500);
    return false;
  }
  hacerAccion(*r, ahora);
  log(r->noEntendi ? "charla: no entendí" : "charla");
  return !r->noEntendi;
}

void Sistema::hacerAccion(const RespuestaCharla& r, uint32_t ahora) {
  const EntidadesCharla& e = r.e;
  // Cuánto falta (ms) para la hora que pidió (una vez).
  auto falta = [&]() -> uint32_t {
    if (e.enMinutos >= 0 && e.minuto < 0) return static_cast<uint32_t>(e.enMinutos) * 60000u;
    const int m = minutoDelDia();
    if (e.minuto < 0 || m < 0) return e.enMinutos >= 0 ? static_cast<uint32_t>(e.enMinutos) * 60000u : 0;
    int d = (e.dia > 0 ? e.dia : 0) * 1440 + e.minuto - m;
    if (d <= 0) d += 1440;
    const uint32_t ep = epoch();
    const int seg = ep ? static_cast<int>(ep % 60u) : 0;
    return static_cast<uint32_t>(d * 60 - seg) * 1000u;
  };
  bool conGlobo = r.frase[0] != 0;
  switch (r.accion) {
    case AccionCharla::Alarma:
      if (e.dias && e.minuto >= 0) {
        // Todos los días (o algunos): el despertador de la config (en la RAM;
        // la nube o la app lo guardan con el aviso 0xCC).
        config_.despertador.hay = config_.despertador.activo = true;
        config_.despertador.minuto = static_cast<uint16_t>(e.minuto);
        config_.despertador.dias = e.dias;
        alarmaMinuto_ = -1;
      } else if (const uint32_t ms = falta()) {
        alarmaCharlaMs_ = ahora + ms;
        if (!alarmaCharlaMs_) alarmaCharlaMs_ = 1;
      }
      break;
    case AccionCharla::CancelarAlarma:
      alarmaCharlaMs_ = 0;
      pospuestaHasta_ = 0;
      if (alarma_) apagarAlarma();
      break;
    case AccionCharla::Posponer: posponerAlarma(); break;
    case AccionCharla::ApagarAlarma: apagarAlarma(); break;
    case AccionCharla::Recordatorio:
      if (e.dias && e.minuto >= 0 && config_.nRecordatorios < MAX_RECORDATORIOS) {
        Recordatorio& x = config_.recordatorios[config_.nRecordatorios++];
        x = Recordatorio();
        x.minuto = static_cast<uint16_t>(e.minuto);
        x.dias = e.dias;
        copiarTexto(x.texto, sizeof x.texto, e.texto);
        recMinuto_ = -1;
      } else if (const uint32_t ms = falta()) {
        recCharlaMs_ = ahora + ms;
        if (!recCharlaMs_) recCharlaMs_ = 1;
        copiarTexto(recCharla_, sizeof recCharla_, e.texto);
      }
      break;
    case AccionCharla::Habito:
      if (r.dato >= 0 && r.dato < config_.habitos.n) marcarHabito(r.dato, 0, true);
      break;
    case AccionCharla::Jugar:
      if (r.dato >= 0 && r.dato <= 4) {
        jugar(static_cast<Juego>(r.dato));
        const size_t k = copiarTexto(visibleJuego_, sizeof visibleJuego_, "juego:");
        copiarTexto(visibleJuego_ + k, sizeof visibleJuego_ - k, nombreJuego(r.dato));
        conGlobo = false;  // el juego tiene su panel
      }
      break;
    case AccionCharla::Modo:
      if (r.dato >= 0 && r.dato <= 3) {
        const Modo antes = config_.modo;
        config_.modo = static_cast<Modo>(r.dato);
        if (config_.modo != antes) reiniciarPomodoro();
      }
      break;
    case AccionCharla::Gusto:
    case AccionCharla::OlvidarGusto: {
      char gs[LARGO_LISTA_GUSTOS + 1];
      gustos(gs, sizeof gs);
      const size_t k = r.accion == AccionCharla::Gusto ? sumarGusto(gs, sizeof gs, e.texto) : sacarGusto(gs, sizeof gs, e.texto);
      if (hal_.guardar) hal_.guardar(CLAVE_GUSTOS, reinterpret_cast<const uint8_t*>(gs), k);
      break;
    }
    case AccionCharla::Dormir: dormirTrasGlobo_ = true; break;
    case AccionCharla::Despertar:
      if (motor_.dormido()) motor_.despertar(ahora, false);
      break;
    case AccionCharla::Dueno: copiarTexto(config_.vida.dueno, sizeof config_.vida.dueno, e.texto); break;
    case AccionCharla::Volumen: {
      const int v = config_.voz.volumen + (r.dato > 0 ? 20 : -20);
      config_.voz.volumen = static_cast<uint8_t>(v < 10 ? 10 : v > 100 ? 100 : v);
      break;
    }
    case AccionCharla::Callar:
      calladoHasta_ = ahora + CALLADO_MS;
      if (!calladoHasta_) calladoHasta_ = 1;
      break;
    case AccionCharla::Animo:
      if (r.dato >= 0 && r.dato <= 2) {
        preguntando_ = false;
        motor_.anotarHoy(Hoy::Animo, static_cast<uint8_t>(r.dato + 1));
        motor_.anotarRegistro(6, static_cast<uint8_t>(r.dato));
        evento(Evento::Animo);
        notificarEvento(Evento::Animo, 6, static_cast<uint8_t>(r.dato));
      }
      break;
    default: break;  // Toque, Música: los hace la nube (con el aviso 0xCC)
  }
  if (r.comportamiento[0]) {
    const int c = config_.vida.indiceComportamiento(r.comportamiento);
    if (c >= 0) motor_.comportamiento(c, ahora);
  }
  if (conGlobo) {
    const Config::Cara cara = r.expresion[0] ? config_.resolverCara(r.expresion) : Config::Cara();
    decir(r.frase, cara, !cara.existe());
  }
}

// [0xCC][código][flags: bit 0 no entendió, bit 1 falta, bit 2 hizo una acción, bit 3 pregunta si lo hace][confianza]
// [n][intención][u8 acción][i8 dato][i16 minuto][i16 en minutos][i8 día][u8 días][n][texto][n][frase]
void Sistema::responderCharla(const RespuestaCharla& r, bool porComando) {
  using namespace protocolo;
  uint8_t b[MAX_NOTIFICACION];
  size_t i = 0;
  b[i++] = static_cast<uint8_t>(CmdCharlar | Respuesta);
  b[i++] = Ok;
  b[i++] = static_cast<uint8_t>((r.noEntendi ? 1 : 0) | (r.falta ? 2 : 0) | (r.accion != AccionCharla::Ninguna ? 4 : 0) |
                                (r.confirma ? 8 : 0));
  b[i++] = r.confianza;
  auto texto = [&](const char* s, size_t max) {
    size_t k = largo(s);
    if (k > max) k = max;
    b[i++] = static_cast<uint8_t>(k);
    copiarBytes(b + i, s, k);
    i += k;
  };
  texto(r.nombre(), 24);
  b[i++] = static_cast<uint8_t>(r.accion);
  b[i++] = static_cast<uint8_t>(r.dato);
  b[i++] = static_cast<uint8_t>(r.e.minuto), b[i++] = static_cast<uint8_t>(static_cast<uint16_t>(r.e.minuto) >> 8);
  b[i++] = static_cast<uint8_t>(r.e.enMinutos), b[i++] = static_cast<uint8_t>(static_cast<uint16_t>(r.e.enMinutos) >> 8);
  b[i++] = static_cast<uint8_t>(r.e.dia);
  b[i++] = r.e.dias;
  texto(r.e.texto, LARGO_TEXTO_CHARLA - 1);
  texto(r.frase, LARGO_FRASE - 1);
  if (porComando) {
    enviar(CarComando, b, i);
  } else {
    if (hal_.notificar) hal_.notificar(CarComando, b, i);
    if (hal_.notificarNube) hal_.notificarNube(CarComando, b, i);
  }
}

void Sistema::avisarCharla(const RespuestaCharla& r) { responderCharla(r, false); }

void Sistema::revisarCharla(uint32_t ahora) {
  if (alarmaCharlaMs_ && !alarma_ && static_cast<int32_t>(ahora - alarmaCharlaMs_) >= 0) {
    alarmaCharlaMs_ = 0;
    sonarAlarma(true);
  }
  if (recCharlaMs_ && static_cast<int32_t>(ahora - recCharlaMs_) >= 0) {
    recCharlaMs_ = 0;
    if (motor_.dormido()) motor_.despertar(ahora, false);
    sonar("recordatorio");
    evento(Evento::Recordatorio);
    decir(recCharla_[0] ? recCharla_ : "\xC2\xA1Tiempo!", config_.resolverCara("sorpresa"));
    log("recordatorio de la charla");
  }
  // "Andá a dormir": cuando se va el globo de las buenas noches.
  if (dormirTrasGlobo_ && !globo_ && !hayPendiente_) {
    dormirTrasGlobo_ = false;
    if (!motor_.dormido()) motor_.siesta(ahora, 20u * 60u);
  }
}

void Sistema::avanzarVoz(uint32_t ahora) {
  if (!voz_.hablando()) {
    vozSuelta_ = false;
    return;
  }
  // Si el globo se fue (un toque, otro globo), se calla (una vocalización
  // sin globo sigue hasta el final).
  if ((!globo_ && !vozSuelta_) || !hal_.audio || !vozModo_) {
    voz_.parar();
    vozSuelta_ = false;
    return;
  }
  const uint32_t objetivo = (ahora - vozDesde_) * (MUESTRAS_POR_SEGUNDO_VOZ / 1000) + VOZ_ADELANTO;
  int16_t b[256];
  while (vozEnviadas_ < objetivo && voz_.hablando()) {
    const uint32_t quiero = objetivo - vozEnviadas_;
    const size_t k = voz_.generar(b, quiero < 256 ? quiero : 256);
    if (k) hal_.audio(b, k);
    vozEnviadas_ += static_cast<uint32_t>(k);
    if (!k) break;
  }
}

}  // namespace sintetica
