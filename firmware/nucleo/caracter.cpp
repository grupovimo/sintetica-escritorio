// El motor de vida v3 (docs/motor-de-vida.md, "v3"): lo que lo hace único
// (los tics de su semilla), lo que crece (las etapas), lo que recuerda (la
// memoria episódica y el diario), la empatía que dura, el clima, las fechas
// (los feriados, los meses juntos, las tuyas) y las rutinas que aprende. Las
// tablas son datos: shared/formato/vida/caracter.json, embebido en vida_base.h.
#include "motor_vida.h"
#include "protocolo.h"
#include "vida_base.h"

namespace sintetica {

namespace {
int minutosHasta(int desde, int hasta) { return ((hasta - desde) % 1440 + 1440) % 1440; }

uint32_t mezclar32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

// Fecha civil (Howard Hinnant) de un día desde 1970.
void fechaDe(int32_t diaEpoch, int32_t* anio, int32_t* mes, int32_t* dia) {
  const int32_t z = diaEpoch + 719468;
  const int32_t era = z / 146097, doe = z - era * 146097;
  const int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int32_t mp = (5 * doy + 2) / 153;
  *dia = doy - (153 * mp + 2) / 5 + 1;
  *mes = mp < 10 ? mp + 3 : mp - 9;
  *anio = yoe + era * 400 + (*mes <= 2 ? 1 : 0);
}

int32_t diaDeFecha(int32_t y, int32_t m, int32_t d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const int32_t yoe = y - era * 400;
  const int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

// El domingo de pascua de ese año (algoritmo anónimo gregoriano), en días desde 1970.
int32_t pascua(int32_t y) {
  const int32_t a = y % 19, b = y / 100, c = y % 100, d = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3;
  const int32_t h = (19 * a + b - d - g + 15) % 30, i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
  const int32_t m = (a + 11 * h + 22 * l) / 451;
  const int32_t mes = (h + l - 7 * m + 114) / 31, dia = (h + l - 7 * m + 114) % 31 + 1;
  return diaDeFecha(y, mes, dia);
}

void u16a(uint8_t* s, size_t& i, uint16_t v) { s[i++] = static_cast<uint8_t>(v), s[i++] = static_cast<uint8_t>(v >> 8); }
uint16_t u16de(const uint8_t* s, size_t& i) {
  const uint16_t v = static_cast<uint16_t>(s[i] | s[i + 1] << 8);
  i += 2;
  return v;
}
}  // namespace

// ---------------------------------------------------------------- los recuerdos en la flash

size_t codificarRecuerdos(const Recuerdos& r, uint8_t* s, size_t cap) {
  if (cap < LARGO_RECUERDOS) return 0;
  llenarBytes(s, 0, LARGO_RECUERDOS);
  size_t i = 0;
  s[i++] = 1;  // versión
  auto ep = [&](const Episodio& e) { u16a(s, i, e.dia), s[i++] = e.tipo, s[i++] = e.dato; };
  for (const Episodio& e : r.episodios) ep(e);
  s[i++] = r.nEpisodios, s[i++] = r.iniEpisodios;
  for (const Episodio& e : r.diario) ep(e);
  s[i++] = r.nDiario, s[i++] = r.iniDiario;
  u16a(s, i, r.diaDiario);
  copiarBytes(s + i, r.animos, 7), i += 7;
  u16a(s, i, r.diaAnimos);
  u16a(s, i, r.minLlegada), u16a(s, i, r.minDormis);
  s[i++] = r.etapa, s[i++] = r.meses, s[i++] = r.anioFeriados;
  protocolo::escribir32(s + i, r.feriados), i += 4;
  s[i++] = r.anioTuyas;
  u16a(s, i, r.tuyas);
  copiarBytes(s + i, r.juegos, 5), i += 5;
  s[i++] = r.hoyCaricias;
  u16a(s, i, r.diaCaricias);
  protocolo::escribir32(s + LARGO_RECUERDOS - 4, protocolo::crc32(s, LARGO_RECUERDOS - 4));
  return LARGO_RECUERDOS;
}

bool decodificarRecuerdos(const uint8_t* s, size_t n, Recuerdos* salida) {
  if (n != LARGO_RECUERDOS || s[0] != 1) return false;
  if (protocolo::leer32(s + LARGO_RECUERDOS - 4) != protocolo::crc32(s, LARGO_RECUERDOS - 4)) return false;
  Recuerdos r;
  size_t i = 1;
  auto ep = [&](Episodio& e) { e.dia = u16de(s, i), e.tipo = s[i++], e.dato = s[i++]; };
  for (Episodio& e : r.episodios) ep(e);
  r.nEpisodios = s[i++], r.iniEpisodios = s[i++];
  for (Episodio& e : r.diario) ep(e);
  r.nDiario = s[i++], r.iniDiario = s[i++];
  r.diaDiario = u16de(s, i);
  copiarBytes(r.animos, s + i, 7), i += 7;
  r.diaAnimos = u16de(s, i);
  r.minLlegada = u16de(s, i), r.minDormis = u16de(s, i);
  r.etapa = s[i++], r.meses = s[i++], r.anioFeriados = s[i++];
  r.feriados = protocolo::leer32(s + i), i += 4;
  r.anioTuyas = s[i++];
  r.tuyas = u16de(s, i);
  copiarBytes(r.juegos, s + i, 5), i += 5;
  r.hoyCaricias = s[i++];
  r.diaCaricias = u16de(s, i);
  if (r.nEpisodios > MAX_EPISODIOS || r.iniEpisodios >= MAX_EPISODIOS || r.nDiario > MAX_DIARIO ||
      r.iniDiario >= MAX_DIARIO)
    return false;
  if (r.etapa >= caracter::N_ETAPAS) r.etapa = static_cast<uint8_t>(caracter::N_ETAPAS - 1);
  if (r.minLlegada >= 1440) r.minLlegada = 0xFFFF;
  if (r.minDormis >= 1440) r.minDormis = 0xFFFF;
  *salida = r;
  return true;
}

void MotorVida::restaurarRecuerdos(const Recuerdos& r) { rec_ = r; }

void MotorVida::guardarRecuerdos() {
  if (!salidas_.guardarRecuerdos) return;
  uint8_t b[LARGO_RECUERDOS];
  const size_t n = codificarRecuerdos(rec_, b, sizeof b);
  salidas_.guardarRecuerdos(salidas_.ctx, b, n);
}

// ---------------------------------------------------------------- único: los tics

void MotorVida::identidad(uint32_t semilla) {
  Azar a;
  a.sembrar(mezclar32(semilla ^ 0x51A7E71Cu) | 1u);
  auto entre = [&](float lo, float hi) { return lo + (hi - lo) * a.real(); };
  Caracter c;
  c.hay = true;
  c.parpadeo = entre(caracter::PARPADEO_MIN, caracter::PARPADEO_MAX);
  c.dobles = entre(caracter::DOBLES_MIN, caracter::DOBLES_MAX);
  c.velocidad = entre(caracter::VELOCIDAD_MIN, caracter::VELOCIDAD_MAX);
  const uint32_t p = a.entre(0, caracter::N_PENSAR - 1);
  c.pensarX = caracter::PENSAR[p][0], c.pensarY = caracter::PENSAR[p][1];
  c.favorito = static_cast<int8_t>(a.entre(0, caracter::N_FAVORITOS - 1));
  c.microgesto = static_cast<int8_t>(a.entre(0, caracter::N_MICROGESTOS - 1));
  c.puerta = a.entre(0, 1) ? 1.0f : -1.0f;
  caracter_ = c;
  proximoMicro_ = static_cast<float>(t_) + entre(caracter::MICROGESTOS[c.microgesto].cadaMin, caracter::MICROGESTOS[c.microgesto].cadaMax);
  resolverV3();
}

void MotorVida::resolverV3() {
  favoritoIdx_ = caraPensar_ = microParam_ = -1;
  if (!config_ || !caracter_.hay) return;
  favoritoIdx_ = static_cast<int8_t>(config_->vida.indiceComportamiento(caracter::FAVORITOS[caracter_.favorito]));
  caraPensar_ = static_cast<int8_t>(config_->vida.indiceCara(caracter::CARAS_PENSAR[0]));
  // "der.lt" → el párpado del ojo derecho; "lb" → los dos ojos; "ftilt" → la cara.
  const char* n = caracter::MICROGESTOS[caracter_.microgesto].param;
  int lado = -1;
  if (iguales(n, "izq.", 4)) lado = 0, n += 4;
  else if (iguales(n, "der.", 4)) lado = 1, n += 4;
  bool esOjo = false;
  const int i = parametroPorNombre(n, &esOjo);
  if (i < 0) return;
  microAmbos_ = esOjo && lado < 0;
  microParam_ = static_cast<int8_t>(esOjo && lado == 1 ? i + param::DER : i);
}

// ---------------------------------------------------------------- crece: las etapas

const char* MotorVida::nombreEtapa() const { return caracter::ETAPAS[rec_.etapa < caracter::N_ETAPAS ? rec_.etapa : 0].nombre; }

void MotorVida::revisarEtapa() {
  if (!mem_.nacimiento) return;  // sin un primer día con hora, no cuenta
  int e = 0;
  for (int i = 0; i < caracter::N_ETAPAS; i++)
    if (mem_.dias >= caracter::ETAPAS[i].dias && estado_[2] >= caracter::ETAPAS[i].afecto) e = i;
  if (e <= rec_.etapa) return;  // nunca baja: lo que destrabó queda
  rec_.etapa = static_cast<uint8_t>(e);
  recordar(TipoEpisodio::Crecio, static_cast<uint8_t>(e));
  anotar(0, static_cast<uint8_t>(Evento::Crecio));
  crecioPendiente_ = true;
  guardarRecuerdos();
}

// ---------------------------------------------------------------- recuerda: los episodios

uint16_t MotorVida::diaVida() const { return ctx_.epoch > 4u * 3600u ? static_cast<uint16_t>((ctx_.epoch - 4u * 3600u) / 86400u) : 0; }

void MotorVida::recordar(TipoEpisodio t, uint8_t dato, bool sumar) {
  const uint16_t dia = diaVida();
  if (!dia || static_cast<int>(t) >= static_cast<int>(TipoEpisodio::Cantidad)) return;
  if ((t == TipoEpisodio::Gane || t == TipoEpisodio::Perdi || t == TipoEpisodio::Empate) && dato < 5 && rec_.juegos[dato] < 255)
    rec_.juegos[dato]++;
  // Uno por tipo y por día: si ya pasó hoy, se actualiza.
  for (int k = 0; k < rec_.nEpisodios; k++) {
    Episodio& e = rec_.episodios[(rec_.iniEpisodios + k) % MAX_EPISODIOS];
    if (e.dia == dia && e.tipo == static_cast<uint8_t>(t)) {
      e.dato = sumar ? static_cast<uint8_t>(e.dato < 255 ? e.dato + 1 : 255) : dato;
      return;
    }
  }
  Episodio& e = rec_.episodios[(rec_.iniEpisodios + rec_.nEpisodios) % MAX_EPISODIOS];
  e.dia = dia, e.tipo = static_cast<uint8_t>(t), e.dato = sumar ? 1 : dato;
  if (rec_.nEpisodios < MAX_EPISODIOS) rec_.nEpisodios++;
  else rec_.iniEpisodios = static_cast<uint8_t>((rec_.iniEpisodios + 1) % MAX_EPISODIOS);
}

bool MotorVida::ocurrio(uint16_t dia, TipoEpisodio t, uint8_t* dato) const {
  for (int k = 0; k < rec_.nEpisodios; k++) {
    const Episodio& e = rec_.episodios[(rec_.iniEpisodios + k) % MAX_EPISODIOS];
    if (e.dia == dia && e.tipo == static_cast<uint8_t>(t)) {
      if (dato) *dato = e.dato;
      return true;
    }
  }
  return false;
}

bool MotorVida::hayEpisodio(uint16_t dia) const {
  for (int k = 0; k < rec_.nEpisodios; k++)
    if (rec_.episodios[(rec_.iniEpisodios + k) % MAX_EPISODIOS].dia == dia) return true;
  return false;
}

// El más importante de ese día (sin los que excluye): false si no hay.
bool MotorVida::masImportante(uint16_t dia, Episodio* salida, uint32_t excluir) const {
  int mejor = -1, imp = -1;
  for (int k = 0; k < rec_.nEpisodios; k++) {
    const Episodio& e = rec_.episodios[(rec_.iniEpisodios + k) % MAX_EPISODIOS];
    if (e.dia != dia || e.tipo >= caracter::N_EPISODIOS || (excluir & (1u << e.tipo))) continue;
    const int i = caracter::EPISODIOS[e.tipo].importancia;
    if (i > imp) imp = i, mejor = k;
  }
  if (mejor < 0) return false;
  *salida = rec_.episodios[(rec_.iniEpisodios + mejor) % MAX_EPISODIOS];
  return true;
}

int MotorVida::escribirDiario(uint16_t dia) {
  if (!dia || rec_.diaDiario >= dia) return 0;
  rec_.diaDiario = dia;
  uint32_t usados = 0;
  int n = 0;
  Episodio e;
  while (n < caracter::DIARIO_LINEAS && masImportante(dia, &e, usados)) {
    usados |= 1u << e.tipo;
    Episodio& d = rec_.diario[(rec_.iniDiario + rec_.nDiario) % MAX_DIARIO];
    d = e;
    if (rec_.nDiario < MAX_DIARIO) rec_.nDiario++;
    else rec_.iniDiario = static_cast<uint8_t>((rec_.iniDiario + 1) % MAX_DIARIO);
    n++;
  }
  if (!n) {
    Episodio& d = rec_.diario[(rec_.iniDiario + rec_.nDiario) % MAX_DIARIO];
    d.dia = dia, d.tipo = DIARIO_TRANQUILO, d.dato = 0;
    if (rec_.nDiario < MAX_DIARIO) rec_.nDiario++;
    else rec_.iniDiario = static_cast<uint8_t>((rec_.iniDiario + 1) % MAX_DIARIO);
    n = 1;
  }
  for (int k = 0; k < n; k++) {
    const Episodio& d = rec_.diario[(rec_.iniDiario + rec_.nDiario - n + k) % MAX_DIARIO];
    anotar(9, d.tipo);
    if (salidas_.diario) salidas_.diario(salidas_.ctx, d.dia, d.tipo, d.dato);
  }
  guardarRecuerdos();
  return n;
}

// ---------------------------------------------------------------- empatía

float MotorVida::bajon() const {
  const uint16_t hoy = diaVida();
  if (!hoy || !rec_.diaAnimos) return -1;
  float suma = 0;
  int n = 0;
  for (int k = 0; k < caracter::EMPATIA_DIAS; k++) {
    const int i = static_cast<int>(hoy) - static_cast<int>(rec_.diaAnimos) + k;  // el día hoy-k en animos
    if (i < 0 || i >= 7) continue;
    const uint8_t a = rec_.animos[i];
    if (!a) continue;
    suma += a == 3 ? 1.0f : a == 2 ? 0.5f : 0.0f;
    n++;
  }
  return n >= caracter::EMPATIA_MINIMO ? suma / n : -1;
}

bool MotorVida::empatico() const { return bajon() >= caracter::EMPATIA_UMBRAL; }

void MotorVida::anotarAnimo(uint8_t a) {
  const uint16_t hoy = diaVida();
  if (!hoy) return;
  if (rec_.diaAnimos != hoy) {
    // Corre los días: [0] es hoy.
    const int corrida = rec_.diaAnimos && hoy > rec_.diaAnimos ? hoy - rec_.diaAnimos : 7;
    for (int i = 6; i >= 0; i--) rec_.animos[i] = i - corrida >= 0 ? rec_.animos[i - corrida] : 0;
    rec_.diaAnimos = hoy;
  }
  // Lo peor del día manda (un "mal" no lo borra un "bien" de compromiso).
  if (a > rec_.animos[0]) rec_.animos[0] = a;
  static const TipoEpisodio EP[] = {TipoEpisodio::AnimoBien, TipoEpisodio::AnimoMedio, TipoEpisodio::AnimoMal};
  if (a >= 1 && a <= 3) recordar(EP[a - 1], a);
  guardarRecuerdos();
}

// ---------------------------------------------------------------- rutinas

void MotorVida::aprender(uint16_t& minuto, int m) {
  if (m < 0) return;
  if (minuto >= 1440) {
    minuto = static_cast<uint16_t>(m);
    return;
  }
  int d = m - minuto;
  if (d > 720) d -= 1440;
  if (d < -720) d += 1440;
  minuto = static_cast<uint16_t>(((minuto + static_cast<int>(d * caracter::APRENDER + (d < 0 ? -0.5f : 0.5f))) % 1440 + 1440) % 1440);
}

void MotorVida::presencia(bool estas) {
  ausente_ = !estas;
  const int m = ctx_.minutoDelDia;
  if (estas && !enHorarioDormir()) aprender(rec_.minLlegada, m);
  // Te vas de noche (después de las 20): a esa hora te dormís.
  if (!estas && m >= 0 && (m >= 20 * 60 || m < 4 * 60)) aprender(rec_.minDormis, m);
}

// Cada 250 ms: lo que ajusta los rasgos y los resortes (etapa, empatía, clima,
// estación), si te espera y la hora en que te dormís.
void MotorVida::actualizarV3() {
  for (float& a : ajusteRasgos_) a = 0;
  rigidezV3_ = caracter_.velocidad;
  revisarEtapa();
  // Sin identidad (el motor suelto de los tests de la v2), es el de la v2: lo
  // demás de la v3 cambia la forma de ser solo en un robot con su semilla.
  if (!caracter_.hay) return;
  if (mem_.nacimiento)
    for (int r = 0; r < N_RASGOS; r++) ajusteRasgos_[r] += caracter::ETAPAS[rec_.etapa].rasgos[r];
  if (empatico()) {
    for (int r = 0; r < N_RASGOS; r++) ajusteRasgos_[r] += caracter::EMPATIA_RASGOS[r];
    rigidezV3_ *= caracter::EMPATIA_RIGIDEZ;
  }
  if (lluvia_) {
    ajusteRasgos_[0] += caracter::LLUVIA_ENERGIA, ajusteRasgos_[4] += caracter::LLUVIA_CURIOSIDAD;
    rigidezV3_ *= caracter::LLUVIA_RIGIDEZ;
  }
  if (ctx_.epoch) {
    int32_t y, mes, d;
    fechaDe(static_cast<int32_t>(ctx_.epoch / 86400), &y, &mes, &d);
    for (const EstacionBase& e : caracter::ESTACIONES)
      if (e.meses & (1u << mes)) ajusteRasgos_[0] += e.energia, ajusteRasgos_[4] += e.curiosidad;
  }
  // La hora en que te dormís (la aprendió): un rato antes, bosteza.
  const int m = ctx_.minutoDelDia;
  dormisPeso_ = 0;
  if (m >= 0 && rec_.minDormis < 1440 && modo_ == Modo::Despierto) {
    const int falta = minutosHasta(m, rec_.minDormis);
    if (falta > 0 && falta <= caracter::DORMIS_ANTES && caracter::DORMIS_ANTES > 0) {
      dormisPeso_ = static_cast<float>(caracter::DORMIS_ANTES - falta) / caracter::DORMIS_ANTES;
      const uint16_t dia = diaVida();
      if (dormisDia_ != dia && actual_ == -1 && !fija_.existe()) {
        dormisDia_ = dia;
        const int b = config_->vida.indiceComportamiento("bostezar");
        if (b >= 0) comportamiento(b, ahora_);
      }
    }
  }
}

// ---------------------------------------------------------------- fechas (en revisarHitos)

int MotorVida::fechasV3(int32_t diaEpoch) {
  int nuevos = 0;
  int32_t y, mes, d;
  fechaDe(diaEpoch, &y, &mes, &d);
  const int hoy = mes * 100 + d;
  // Los meses juntos: el mismo día del mes que se conocieron (hasta el año: el aniversario ya está).
  if (mem_.nacimiento) {
    int32_t yn, mn, dn;
    fechaDe(static_cast<int32_t>(mem_.nacimiento / 86400), &yn, &mn, &dn);
    const int meses = (y - yn) * 12 + (mes - mn) - (d < dn ? 1 : 0);
    if (d == dn && meses >= 1 && meses <= caracter::MESES_JUNTOS && rec_.meses < meses) {
      rec_.meses = static_cast<uint8_t>(meses), nuevos++;
      hitoSituacion_ = static_cast<int8_t>(Situacion::MesJuntos);
    }
  }
  // Los feriados (una vez por año cada uno).
  if (rec_.anioFeriados != static_cast<uint8_t>(y)) rec_.anioFeriados = static_cast<uint8_t>(y), rec_.feriados = 0;
  const int32_t p = pascua(y);
  for (int i = 0; i < caracter::N_FERIADOS && i < 32; i++) {
    const FeriadoBase& f = caracter::FERIADOS[i];
    const bool es = f.fecha ? f.fecha == hoy : diaEpoch == p + f.pascua;
    if (!es || (rec_.feriados & (1u << i))) continue;
    rec_.feriados |= 1u << i, nuevos++;
    feriadoHoy_ = static_cast<int8_t>(i);
    hitoSituacion_ = static_cast<int8_t>(Situacion::Feriado);
    hitoSinFestejo_ = !f.festejo;
  }
  // Las tuyas (config "fechas"): el día antes avisa; ese día, lo recuerda.
  if (rec_.anioTuyas != static_cast<uint8_t>(y)) rec_.anioTuyas = static_cast<uint8_t>(y), rec_.tuyas = 0;
  int32_t ym, mm, dm;
  fechaDe(diaEpoch + 1, &ym, &mm, &dm);
  const int manana = mm * 100 + dm;
  for (int i = 0; i < config_->fechas.n; i++) {
    const int f = config_->fechas.lista[i].fecha;
    if (f == manana && !(rec_.tuyas & (1u << (8 + i)))) {
      rec_.tuyas = static_cast<uint16_t>(rec_.tuyas | (1u << (8 + i))), nuevos++;
      fechaTuya_ = static_cast<int8_t>(i);
      hitoSituacion_ = static_cast<int8_t>(Situacion::FechaManana);
      hitoSinFestejo_ = true;
    }
    if (f == hoy && !(rec_.tuyas & (1u << i))) {
      rec_.tuyas = static_cast<uint16_t>(rec_.tuyas | (1u << i)), nuevos++;
      fechaTuya_ = static_cast<int8_t>(i);
      hitoSituacion_ = static_cast<int8_t>(Situacion::FechaTuya);
      hitoSinFestejo_ = true;
    }
  }
  if (nuevos) guardarRecuerdos();
  return nuevos;
}

// El sueño de la noche sale de lo que pasó en el día: la partícula de un
// episodio de hoy (o de ayer, si es de madrugada); -1 si no pasó nada.
int MotorVida::particulaDeSueno() {
  const uint16_t hoy = diaVida();
  if (!hoy) return -1;
  for (int intento = 0; intento < 2; intento++) {
    const uint16_t dia = static_cast<uint16_t>(hoy - intento);
    int n = 0;
    for (int k = 0; k < rec_.nEpisodios; k++) n += rec_.episodios[(rec_.iniEpisodios + k) % MAX_EPISODIOS].dia == dia;
    if (!n) continue;
    int elegido = static_cast<int>(azar_.entre(0, static_cast<uint32_t>(n - 1)));
    for (int k = 0; k < rec_.nEpisodios; k++) {
      const Episodio& e = rec_.episodios[(rec_.iniEpisodios + k) % MAX_EPISODIOS];
      if (e.dia != dia || elegido--) continue;
      TipoParticula t;
      if (e.tipo < caracter::N_EPISODIOS && particulaPorNombre(caracter::EPISODIOS[e.tipo].sueno, &t)) return static_cast<int>(t);
      return -1;
    }
  }
  return -1;
}

}  // namespace sintetica
