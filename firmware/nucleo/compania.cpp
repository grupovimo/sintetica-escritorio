// La v1.9 (docs/motor-de-vida.md, "Compañía"): los toques de los amigos (y
// devolverlos), la agenda, cuando llegás y cuando te vas, los hábitos (y la
// pregunta a su hora), la música que suena y los avisos de afuera. Son
// métodos del Sistema: usan el globo, el motor y el protocolo. Lo que dice y
// cómo se mueve son datos (vida/frases.json, vida/comportamientos.json).
#include "sistema.h"

namespace sintetica {

namespace {
constexpr uint32_t DEVOLVER_MS = 30000;            // la ventana para devolver un toque
constexpr uint32_t PREGUNTA_HABITO_MS = 60000;     // cuánto espera la respuesta al "¿{habito}?"
constexpr uint32_t MANTENER_MS = 800;              // mantener apretado: "todavía no"
constexpr uint32_t OTRA_VEZ_MS = 60u * 60000u;     // "todavía no": pregunta una vez más en 1 h
constexpr uint32_t ABURRIRSE_MS = 15u * 60000u;    // te fuiste: un rato después, se aburre
constexpr uint32_t MUSICA_VALE_MS = 20u * 60000u;  // sin noticias de la música, deja de bailar
constexpr int VENTANA_HABITO_MIN = 10;             // si a la hora justa estaba ocupado, hasta 10 min después
constexpr int MINUTOS_AGENDA = 15;

int minutosDesde(int desde, int hasta) { return ((hasta - desde) % 1440 + 1440) % 1440; }

// Un número en texto (para {minutos}, {cantidad}, {racha}).
void numero(uint32_t v, char* t, size_t cap) {
  char d[10];
  int n = 0;
  do d[n++] = static_cast<char>('0' + v % 10), v /= 10;
  while (v && n < 10);
  size_t k = 0;
  while (n && k + 1 < cap) t[k++] = d[--n];
  t[k] = 0;
}

const char* const SONIDOS_TOQUE[] = {"toque", "abrazo", "beso", "toque"};
// El toque, el abrazo (se achica y se pone colorado) y el beso (corazones).
const char* const CARAS_TOQUE[] = {"contento", "verguenza", "enamorado"};
const Situacion SITUACIONES_AVISO[] = {Situacion::Pago, Situacion::Mail, Situacion::Github, Situacion::Pedido,
                                       Situacion::AvisoOtro};
}  // namespace

uint16_t Sistema::diaHoy() const {
  const uint32_t e = epoch();
  return e ? static_cast<uint16_t>(e / 86400u + 1u) : 0;
}

// Una frase de esa situación ya (las de la v1.9 las piden la nube o el dueño:
// no pasan por el Hablador). false si no hay ninguna que sirva.
bool Sistema::decirFrase(Situacion s, Plantillas extra, Config::Cara cara) {
  if (apagado_ || emparejando_) return false;
  char t[LARGO_FRASE];
  if (!elegirFrase(config_.habla, s, presetCercano(motor_.rasgos()), config_.vida.dueno, nombre(), azar_, t, sizeof t,
                   nullptr, extra))
    return false;
  decir(t, cara);
  return true;
}

// ---------------------------------------------------------------- amigos

bool Sistema::toque(uint8_t tipo, const char* de, const char* expresion) {
  if (tipo > 3) return false;
  const uint32_t ahora = hal_.ahoraMs();
  Config::Cara cara = config_.resolverCara(tipo < 3 ? CARAS_TOQUE[tipo] : (expresion && expresion[0] ? expresion : "contento"));
  if (!cara.existe()) return false;
  // Lo despierta suave (sin el mal humor de que lo despierten).
  if (motor_.dormido()) motor_.despertar(ahora, false);
  evento(Evento::ToqueRecibido);
  motor_.anotarRegistro(7, tipo);
  motor_.recordar(TipoEpisodio::Toque, tipo);
  sonar(SONIDOS_TOQUE[tipo]);
  copiarTexto(toqueDe_, sizeof toqueDe_, de);
  toqueTipo_ = tipo;
  toqueHasta_ = ahora + DEVOLVER_MS;
  if (!toqueHasta_) toqueHasta_ = 1;
  const Plantilla p[] = {{"de", toqueDe_}};
  if (!decirFrase(Situacion::ToqueRecibido, {p, 1}, cara)) motor_.reaccionar(cara, ahora, 4000);
  log("un toque de un amigo");
  return true;
}

void Sistema::devolverToque(uint32_t ahora) {
  toqueHasta_ = 0;
  if (globo_) globoHasta_ = ahora;
  hayPendiente_ = false;
  motor_.interaccion(ahora);
  motor_.evento(Evento::DevolverToque, ahora);
  motor_.anotarRegistro(7, toqueTipo_);
  notificarEvento(Evento::DevolverToque, 7, toqueTipo_);
  sonar(SONIDOS_TOQUE[toqueTipo_ < 4 ? toqueTipo_ : 0]);
  const Config::Cara cara = config_.resolverCara("emocionado");
  const Plantilla p[] = {{"de", toqueDe_}};
  if (!decirFrase(Situacion::ToqueDevuelto, {p, 1}, cara)) motor_.reaccionar(cara, ahora, 2000);
  log("toque devuelto");
}

// ---------------------------------------------------------------- agenda

void Sistema::agenda(uint16_t minutos, uint16_t duracion, const char* titulo) {
  const uint32_t ahora = hal_.ahoraMs();
  char n[8];
  if (minutos == 0xFFFF) {
    // El resumen de mañana (a la hora de dormir): cuántos y el primero.
    numero(duracion, n, sizeof n);
    const Plantilla p[] = {{"titulo", titulo}, {"cantidad", n}};
    decirFrase(Situacion::AgendaManana, {p, 2}, config_.resolverCara("pensando"));
    return;
  }
  if (minutos > MINUTOS_AGENDA) return;  // falta: todavía nada
  if (motor_.dormido()) motor_.despertar(ahora, false);
  evento(Evento::Agenda);
  const int c = config_.vida.indiceComportamiento("apurado");
  if (c >= 0) motor_.comportamiento(c, ahora);
  numero(minutos, n, sizeof n);
  const Plantilla p[] = {{"titulo", titulo}, {"minutos", n}};
  decirFrase(Situacion::Agenda, {p, 2});
}

// ---------------------------------------------------------------- presencia

void Sistema::presencia(bool llegaste) {
  const uint32_t ahora = hal_.ahoraMs();
  motor_.presencia(llegaste);  // v3: aprende a qué hora llegás y a cuál te vas a dormir
  if (llegaste) {
    const uint32_t fuera = fuisteMs_ ? ahora - fuisteMs_ : 0;
    fuisteMs_ = 0;
    fuisteEmpujado_ = false;
    if (motor_.dormido()) motor_.despertar(ahora, false);
    evento(Evento::Llegaste);
    // El ánimo de cuánto tardaste: un ratito, un guiño; horas, emocionado; un día, enamorado.
    const char* cara = !fuera                    ? "contento"
                       : fuera < 15u * 60000u    ? "guino"
                       : fuera < 4u * 3600000u   ? "contento"
                       : fuera < 20u * 3600000u  ? "emocionado"
                                                 : "enamorado";
    motor_.empujarEstado(VarVida::Aburrimiento, -0.4f);
    motor_.empujarEstado(VarVida::Animo, fuera >= 4u * 3600000u ? 0.2f : 0.08f);
    const Config::Cara c = config_.resolverCara(cara);
    const Situacion s = fuera >= 20u * 3600000u ? Situacion::TeExtrane : Situacion::Volviste;
    if (!decirFrase(s, {}, c)) motor_.reaccionar(c, ahora, 3000);
    return;
  }
  fuisteMs_ = ahora ? ahora : 1;
  fuisteEmpujado_ = false;
  evento(Evento::TeFuiste);
  if (!config_.presencia.preguntar || motor_.dormido()) return;
  if (config_.presencia.texto[0]) {
    char t[LARGO_FRASE];
    completarFrase(config_.presencia.texto, config_.vida.dueno, nombre(), t, sizeof t);
    decir(t, config_.resolverCara("curioso"));
  } else {
    decirFrase(Situacion::TeVas, {}, config_.resolverCara("curioso"));
  }
}

// ---------------------------------------------------------------- hábitos

bool Sistema::habito(int i, bool hecho, uint8_t racha) {
  if (i < 0 || i >= config_.habitos.n) return false;
  if (hecho) {
    marcarHabito(i, racha, false);
  } else {
    // Deshacer: nada visible.
    if (habDiaHecho_[i] == diaHoy()) habDiaHecho_[i] = 0;
    habRacha_[i] = racha;
  }
  return true;
}

void Sistema::marcarHabito(int i, uint8_t racha, bool enElRobot) {
  const uint32_t ahora = hal_.ahoraMs();
  const uint16_t hoy = diaHoy();
  if (enElRobot) {
    // La racha la sabe la nube; acá, la que se puede contar: ayer + 1.
    racha = hoy && habDiaHecho_[i] == hoy                         ? habRacha_[i]
            : hoy && habDiaHecho_[i] && habDiaHecho_[i] + 1 == hoy ? static_cast<uint8_t>(habRacha_[i] < 255 ? habRacha_[i] + 1 : 255)
                                                                   : 1;
  }
  habDiaHecho_[i] = hoy;
  habRacha_[i] = racha;
  if (habPregunta_ == i) {
    habPregunta_ = -1;
    if (globo_) globoHasta_ = ahora;
    hayPendiente_ = false;
  }
  if (habReintentoIdx_ == i) habReintento_ = 0, habReintentoIdx_ = -1;
  if (motor_.dormido()) motor_.despertar(ahora, false);
  motor_.interaccion(ahora);
  motor_.evento(Evento::Habito, ahora);
  motor_.anotarRegistro(8, static_cast<uint8_t>(i));
  motor_.recordar(TipoEpisodio::Habito, static_cast<uint8_t>(i));
  if (enElRobot) notificarEvento(Evento::Habito, 8, static_cast<uint8_t>(i));
  const int c = config_.vida.indiceComportamiento("orgulloso");
  if (c >= 0) motor_.comportamiento(c, ahora);
  char r[6];
  numero(racha, r, sizeof r);
  const Plantilla p[] = {{"habito", config_.habitos.lista[i].nombre}, {"racha", r}};
  decirFrase(racha >= 3 ? Situacion::HabitoRacha : Situacion::HabitoHecho, {p, 2});
  log("hábito hecho");
}

void Sistema::preguntarHabito(int i, uint32_t ahora) {
  const Habito& h = config_.habitos.lista[i];
  const uint16_t hoy = diaHoy();
  habDiaPreg_[i] = hoy;
  habPregunta_ = static_cast<int8_t>(i);
  habDesde_ = ahora;
  habApretado_ = false;
  const Config::Cara cara = config_.resolverCara("curioso");
  // Si venía con racha y ayer no lo hizo, se cortó: lo dice (y pregunta igual).
  const bool cortada = habRacha_[i] >= 3 && hoy && habDiaHecho_[i] && habDiaHecho_[i] + 1 < hoy;
  const Plantilla p[] = {{"habito", h.nombre}};
  if (!(cortada && decirFrase(Situacion::HabitoCortado, {p, 1}, cara))) {
    char t[LARGO_FRASE];
    size_t k = copiarTexto(t, sizeof t, "\xC2\xBF");
    k += copiarTexto(t + k, sizeof t - k, h.nombre);
    copiarTexto(t + k, sizeof t - k, "?");
    decir(t, cara);
  }
  if (cortada) habRacha_[i] = 0;
  sonar("habito");
}

void Sistema::todaviaNo(uint32_t ahora) {
  const int i = habPregunta_;
  habPregunta_ = -1;
  if (globo_) globoHasta_ = ahora;
  hayPendiente_ = false;
  motor_.interaccion(ahora);
  motor_.reaccionar(config_.resolverCara("guino"), ahora, 1200);
  // Pregunta una vez más, en una hora.
  if (i >= 0 && habReintentoIdx_ < 0) {
    habReintentoIdx_ = static_cast<int8_t>(i);
    habReintento_ = ahora + OTRA_VEZ_MS;
    if (!habReintento_) habReintento_ = 1;
  }
}

// ---------------------------------------------------------------- en cada vuelta

void Sistema::revisarCompania(uint32_t ahora) {
  // La ventana para devolver el toque se cierra sola.
  if (toqueHasta_ && static_cast<int32_t>(ahora - toqueHasta_) >= 0) toqueHasta_ = 0;
  // Te fuiste hace un rato: se aburre (y se duerme antes).
  if (fuisteMs_ && !fuisteEmpujado_ && ahora - fuisteMs_ >= ABURRIRSE_MS) {
    fuisteEmpujado_ = true;
    motor_.empujarEstado(VarVida::Aburrimiento, 0.3f);
    motor_.empujarEstado(VarVida::Sueno, 0.15f);
  }
  // La música: sin noticias en 20 min, deja de bailar.
  if (musicaHasta_ && static_cast<int32_t>(ahora - musicaHasta_) >= 0) {
    musicaHasta_ = musicaFirma_ = 0;
    motor_.musica(0);
  }
  // La pregunta del hábito: el globo queda mientras espera; mantener = todavía no.
  if (habPregunta_ >= 0) {
    if (habPregunta_ >= config_.habitos.n || ahora - habDesde_ >= PREGUNTA_HABITO_MS) {
      habPregunta_ = -1;
      if (globo_) globoHasta_ = ahora;
    } else if (habApretado_ && ahora - habToque_ >= MANTENER_MS) {
      habApretado_ = false;
      todaviaNo(ahora);
    } else if (globo_ && !hayPendiente_ && static_cast<int32_t>(globoHasta_ - ahora) < 1000) {
      globoHasta_ = ahora + 1000;
    }
    return;
  }
  if (vistaPrevia_ || apagado_ || emparejando_ || !config_.habitos.n) return;
  if (globo_ || hayPendiente_ || alarma_ || recVisible_ >= 0 || juego_ != Juego::Ninguno || preguntando_) return;
  const int m = minutoDelDia();
  const uint16_t hoy = diaHoy();
  if (m < 0 || !hoy) return;
  const int semana = static_cast<int>((epoch() / 86400u + 3u) % 7u);
  if (habReintento_ && static_cast<int32_t>(ahora - habReintento_) >= 0) {
    const int i = habReintentoIdx_;
    habReintento_ = 0;
    if (i >= 0 && i < config_.habitos.n && habDiaHecho_[i] != hoy) return preguntarHabito(i, ahora);
  }
  for (int i = 0; i < config_.habitos.n; i++) {
    const Habito& h = config_.habitos.lista[i];
    if (!h.conHora || !(h.dias & (1u << semana)) || habDiaPreg_[i] == hoy || habDiaHecho_[i] == hoy) continue;
    if (minutosDesde(h.minuto, m) < VENTANA_HABITO_MIN) return preguntarHabito(i, ahora);
  }
}

// ---------------------------------------------------------------- música y avisos

void Sistema::musica(uint8_t bpm, bool sonando, const char* titulo, const char* artista) {
  const uint32_t ahora = hal_.ahoraMs();
  if (!sonando) {
    musicaHasta_ = musicaFirma_ = 0;
    motor_.musica(0);
    return;
  }
  motor_.musica(bpm ? bpm : 100);
  musicaHasta_ = ahora + MUSICA_VALE_MS;
  if (!musicaHasta_) musicaHasta_ = 1;
  // ¿Otro tema? Baila (y, quizás, lo comenta).
  uint32_t f = protocolo::crc32(reinterpret_cast<const uint8_t*>(titulo), largo(titulo));
  f = protocolo::crc32(reinterpret_cast<const uint8_t*>(artista), largo(artista), f);
  if (!f) f = 1;
  if (f == musicaFirma_) return;
  musicaFirma_ = f;
  if (motor_.dormido()) return;
  motor_.recordar(TipoEpisodio::Musica);
  const int c = config_.vida.indiceComportamiento("bailar-ritmo");
  if (c >= 0) motor_.comportamiento(c, ahora);
  if (titulo[0] || artista[0]) {
    const Plantilla p[] = {{"titulo", titulo}, {"artista", artista}};
    hablar(Situacion::Musica, {p, 2});
  }
}

void Sistema::avisoEntrante(uint8_t tipo, const char* titulo, const char* de) {
  if (tipo < 2 || tipo > 6) return;
  const uint32_t ahora = hal_.ahoraMs();
  evento(Evento::Notificacion);
  Config::Cara cara;
  if (tipo == 2) {
    cara = config_.resolverCara("emocionado");
    sonar("pago");
  } else {
    sonar("notificacion");
  }
  const Plantilla p[] = {{"titulo", titulo}, {"de", de}};
  if (!decirFrase(SITUACIONES_AVISO[tipo - 2], {p, 2}, cara) && cara.existe()) motor_.reaccionar(cara, ahora, 3000);
}

}  // namespace sintetica
