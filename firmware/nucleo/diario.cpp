// La v3 en el Sistema (docs/motor-de-vida.md, "v3"): los recuerdos en la
// flash (clave "recuerdos"), el diario que escribe el robot (comando 0x4D y su
// aviso 0xCD) y las frases que salen de lo que pasó (el recuerdo de ayer, la
// propuesta al volver, los feriados, las fechas tuyas, los meses juntos).
#include "sistema.h"
#include "vida_base.h"

namespace sintetica {

namespace {
constexpr const char* CLAVE_RECUERDOS = "recuerdos";

void numeroTexto(uint32_t v, char* t, size_t cap) {
  char d[10];
  int n = 0;
  do d[n++] = static_cast<char>('0' + v % 10), v /= 10;
  while (v && n < 10);
  size_t k = 0;
  while (n && k + 1 < cap) t[k++] = d[--n];
  t[k] = 0;
}
}  // namespace

void Sistema::leerRecuerdos() {
  uint8_t b[LARGO_RECUERDOS];
  const int n = hal_.leer ? hal_.leer(CLAVE_RECUERDOS, b, sizeof b) : -1;
  Recuerdos r;
  if (n == static_cast<int>(LARGO_RECUERDOS) && decodificarRecuerdos(b, LARGO_RECUERDOS, &r)) motor_.restaurarRecuerdos(r);
}

void Sistema::guardarRecuerdos(const uint8_t* d, size_t n) {
  if (hal_.guardar) hal_.guardar(CLAVE_RECUERDOS, d, n);
}

// El texto de un episodio (el recuerdo de ayer o la línea del diario), con
// sus plantillas. Determinista: la misma línea da siempre la misma frase.
bool Sistema::textoEpisodio(const Episodio& e, bool diario, char* salida, size_t cap) const {
  int primera, cantidad;
  if (e.tipo == DIARIO_TRANQUILO) {
    if (!diario) return false;
    primera = caracter::TRANQUILO, cantidad = caracter::N_TRANQUILO;
  } else if (e.tipo < caracter::N_EPISODIOS) {
    const EpisodioBase& b = caracter::EPISODIOS[e.tipo];
    primera = diario ? b.diario : b.recuerdo, cantidad = diario ? b.nDiario : b.nRecuerdo;
  } else {
    return false;
  }
  const char* dueno = config_.vida.dueno;
  char cant[8];
  numeroTexto(e.dato, cant, sizeof cant);
  const bool esJuego = e.tipo <= static_cast<uint8_t>(TipoEpisodio::Empate);
  const char* habito = e.tipo == static_cast<uint8_t>(TipoEpisodio::Habito) && e.dato < config_.habitos.n
                           ? config_.habitos.lista[e.dato].nombre
                           : "";
  const Plantilla p[] = {{"juego", esJuego ? caracter::JUEGOS[e.dato % 5] : ""},
                         {"toque", caracter::TOQUES[e.dato % 4]},
                         {"cantidad", cant},
                         {"habito", habito}};
  // Las que sirven: sin dueño, no las que empiezan con {dueno}; ni las que
  // usan una plantilla vacía.
  auto sirve = [&](const char* t) {
    if (!dueno[0] && iguales(t, "{dueno}", 7)) return false;
    for (const Plantilla& x : p)
      if (!x.valor[0])
        for (const char* c = t; *c; c++)
          if (*c == '{' && iguales(c + 1, x.clave, largo(x.clave)) && c[1 + largo(x.clave)] == '}') return false;
    return true;
  };
  int n = 0;
  for (int i = 0; i < cantidad; i++) n += sirve(caracter::TEXTOS[primera + i]);
  if (!n) return false;
  int k = static_cast<int>((static_cast<uint32_t>(e.dia) * 2654435761u + e.tipo * 40503u + e.dato) % static_cast<uint32_t>(n));
  for (int i = 0; i < cantidad; i++) {
    const char* t = caracter::TEXTOS[primera + i];
    if (!sirve(t) || k--) continue;
    completarFrase(t, dueno, nombre(), salida, cap, nullptr, nullptr, nullptr, {p, 4});
    return true;
  }
  return false;
}

// Una línea del diario: [0xCD][0x00][u16 día][u8 i][u8 n][u8 tipo][u8 dato][u8 etapa][texto UTF-8 hasta el final].
void Sistema::enviarLineaDiario(const Episodio& e, uint8_t i, uint8_t n, bool aviso) {
  using namespace protocolo;
  uint8_t b[9 + LARGO_FRASE];
  b[0] = static_cast<uint8_t>(CmdDiario | Respuesta);
  b[1] = Ok;
  b[2] = static_cast<uint8_t>(e.dia), b[3] = static_cast<uint8_t>(e.dia >> 8);
  b[4] = i, b[5] = n, b[6] = e.tipo, b[7] = e.dato, b[8] = static_cast<uint8_t>(motor_.etapa());
  char t[LARGO_FRASE] = {};
  if (n) textoEpisodio(e, true, t, sizeof t);
  const size_t k = largo(t);
  copiarBytes(b + 9, t, k);
  if (aviso) {
    // Como los avisos del 0x9E: por BLE y por la nube (la nube lo guarda).
    if (hal_.notificar) hal_.notificar(CarComando, b, 9 + k);
    if (hal_.notificarNube) hal_.notificarNube(CarComando, b, 9 + k);
  } else {
    enviar(CarComando, b, 9 + k);
  }
}

void Sistema::enviarDiario() {
  const Recuerdos& r = motor_.recuerdos();
  if (!r.nDiario) return enviarLineaDiario(Episodio(), 0, 0, false);
  for (int k = 0; k < r.nDiario; k++)
    enviarLineaDiario(r.diario[(r.iniDiario + k) % MAX_DIARIO], static_cast<uint8_t>(k), r.nDiario, false);
}

void Sistema::lineaDiarioNueva(uint16_t dia, uint8_t tipo, uint8_t dato) {
  Episodio e;
  e.dia = dia, e.tipo = tipo, e.dato = dato;
  enviarLineaDiario(e, 0, 1, true);
  log("diario: una línea nueva");
}

// Las plantillas de las situaciones de la v3 ({juego}, {gusto}, {titulo}, {cantidad}).
bool Sistema::hablarV3(Situacion s) {
  const uint32_t e = epoch();
  char t[LARGO_FRASE];
  const char* preset = presetCercano(motor_.rasgos());
  const bool siempre = situacionSiempre(s);
  if (!siempre && (globo_ || hayPendiente_ || recVisible_ >= 0 || alarma_ || juego_ != Juego::Ninguno)) return true;
  if (!siempre && calladoHasta_ && static_cast<int32_t>(hal_.ahoraMs() - calladoHasta_) < 0) return true;
  if (s == Situacion::Recuerdo) {
    if (!hablador_.quiere(s, hal_.ahoraMs() / 1000u, e ? e / 86400u : 0, config_.habla.frecuencia, azar_)) return true;
    if (textoEpisodio(motor_.recuerdoElegido(), false, t, sizeof t)) decir(t);
    return true;
  }
  char a[48] = {}, b[LARGO_LISTA_GUSTOS + 1] = {};
  const char* clave1 = "titulo";
  const char* clave2 = "cantidad";
  if (s == Situacion::Propuesta) {
    // Lo que más jugás y uno de tus gustos (de la charla).
    const Recuerdos& r = motor_.recuerdos();
    int mejor = -1;
    for (int j = 0; j < 5; j++)
      if (r.juegos[j] && (mejor < 0 || r.juegos[j] > r.juegos[mejor])) mejor = j;
    if (mejor >= 0) copiarTexto(a, sizeof a, caracter::JUEGOS[mejor]);
    char gs[LARGO_LISTA_GUSTOS + 1];
    const size_t ng = gustos(gs, sizeof gs);
    if (ng) {
      // Uno al azar de la lista ("el mate\nla pizza").
      int lineas = 1;
      for (size_t i = 0; i < ng; i++) lineas += gs[i] == '\n';
      int cual = static_cast<int>(azar_.entre(0, static_cast<uint32_t>(lineas - 1)));
      size_t i = 0;
      while (cual && i < ng)
        if (gs[i++] == '\n') cual--;
      size_t k = 0;
      while (i < ng && gs[i] != '\n' && k + 1 < sizeof b) b[k++] = gs[i++];
      b[k] = 0;
    }
    clave1 = "juego", clave2 = "gusto";
  } else if (s == Situacion::Feriado) {
    const int f = motor_.feriadoHoy();
    if (f >= 0 && f < caracter::N_FERIADOS) copiarTexto(a, sizeof a, caracter::FERIADOS[f].titulo);
  } else if (s == Situacion::FechaTuya || s == Situacion::FechaManana) {
    const int f = motor_.fechaTuyaHoy();
    if (f >= 0 && f < config_.fechas.n) copiarTexto(a, sizeof a, config_.fechas.lista[f].texto);
  } else if (s == Situacion::MesJuntos) {
    const int m = motor_.mesesJuntos();
    if (m == 1) copiarTexto(b, sizeof b, "un mes");
    else {
      numeroTexto(static_cast<uint32_t>(m), b, sizeof b);
      copiarTexto(b + largo(b), sizeof b - largo(b), " meses");
    }
  } else {
    return false;
  }
  const Plantilla p[] = {{clave1, a}, {clave2, b}};
  if (!hablador_.quiere(s, hal_.ahoraMs() / 1000u, e ? e / 86400u : 0, config_.habla.frecuencia, azar_)) return true;
  if (elegirFrase(config_.habla, s, preset, config_.vida.dueno, nombre(), azar_, t, sizeof t, nullptr, {p, 2})) decir(t);
  return true;
}

}  // namespace sintetica
