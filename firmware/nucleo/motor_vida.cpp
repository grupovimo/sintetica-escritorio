#include "motor_vida.h"

#include "protocolo.h"
#include "formato_vida.h"
#include "vida_base.h"

namespace sintetica {

namespace {

constexpr float DT = 1.0f / 60.0f;

// La tabla de "Transiciones": rigidez y fricción por grupo.
struct ParResorte {
  float k, c;
};
const ParResorte GRUPOS[static_cast<int>(GrupoResorte::Cantidad)] = {
    {170, 17},  // forma de ojos: elástica, con leve rebote
    {260, 24},  // párpados: rápidos, sin rebote
    {380, 28},  // mirada (saccade): salto seco
    {120, 11},  // cara entera: gelatinosa
    {60, 15},   // color y brillo: lenta
};
constexpr ParResorte SEGUIMIENTO = {90, 18};
constexpr ParResorte PARPADEO = {900, 40};
constexpr ParResorte APLASTAR = {260, 11};  // la anticipación (el "sq" del v0)

// Escalonado: primero mira, después siente.
const float RETRASO[static_cast<int>(GrupoResorte::Cantidad)] = {0.09f, 0.04f, 0.0f, 0.06f, 0.0f};

// Las caras del ánimo de fondo.
enum Animo { ACansado, ATriste, AContento, AAburrido, AGrunon, AMiedo, ACurioso, AOfendido, N_ANIMO };
const char* const NOMBRES_ANIMO[N_ANIMO] = {"cansado", "triste", "contento", "aburrido", "grunon", "miedo", "curioso", "ofendido"};
int g_idxAnimo[N_ANIMO];

// Qué estímulos cuentan como gustos (memoria).
const Evento GUSTOS[8] = {Evento::Caricia, Evento::Tacto,  Evento::Voz,           Evento::Musica,
                          Evento::Nombre,  Evento::Levantado, Evento::SacudidaSuave, Evento::AppAbierta};

bool esInteraccion(Evento e) {
  switch (e) {
    case Evento::Tacto:
    case Evento::DobleToque:
    case Evento::Levantado:
    case Evento::Sacudida:
    case Evento::BocaAbajo:
    case Evento::AppAbierta:
    case Evento::Despertado:
    case Evento::Caricia:
    case Evento::MuchosToques:
    case Evento::SacudidaSuave:
    case Evento::SacudidaFuerte:
    case Evento::Caida:
    case Evento::Voz:
    case Evento::Nombre:
    case Evento::Vinculado:
      return true;
    default:
      return false;
  }
}

bool esToque(Evento e) {
  return e == Evento::Tacto || e == Evento::Caricia || e == Evento::MuchosToques || e == Evento::DobleToque;
}

// Hash entero para el ruido determinista.
uint32_t mezclarBits(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

float normal(Azar& a) {
  // Irwin–Hall: la suma de 4 uniformes, centrada y escalada.
  return (a.real() + a.real() + a.real() + a.real() - 2.0f) * 1.7320508f;
}

float entre(Azar& a, float lo, float hi) { return lo + (hi - lo) * a.real(); }

// Minutos desde `desde` hasta `hasta` (dando la vuelta a la medianoche).
int minutosHasta(int desde, int hasta) { return ((hasta - desde) % 1440 + 1440) % 1440; }

}  // namespace

// ---------------------------------------------------------------- memoria

size_t codificarMemoria(const Memoria& m, uint8_t* s, size_t cap) {
  if (cap < LARGO_MEMORIA) return 0;
  llenarBytes(s, 0, LARGO_MEMORIA);
  size_t i = 0;
  s[i++] = 1;  // versión
  auto u32 = [&](uint32_t v) { protocolo::escribir32(s + i, v), i += 4; };
  auto u16 = [&](uint16_t v) { s[i++] = static_cast<uint8_t>(v), s[i++] = static_cast<uint8_t>(v >> 8); };
  u32(m.nacimiento), u32(m.ultimaInteraccion), u32(m.rencorHasta), u32(m.ultimoDia);
  u16(m.dias), u16(m.racha), u16(m.rachaMax);
  u32(m.caricias), u32(m.toques), u32(m.sacudidas);
  copiarBytes(s + i, m.horas, 24), i += 24;
  copiarBytes(s + i, m.gustos, 8), i += 8;
  u16(m.hitos), u16(m.anioCumpleBicho), u16(m.anioCumpleDueno);
  copiarBytes(s + i, m.deriva, N_RASGOS), i += N_RASGOS;
  copiarBytes(s + i, m.estado, N_ESTADO), i += N_ESTADO;
  u32(m.epochEstado);
  u16(m.ausenciaMaxima);
  s[i++] = m.afectoHoy;
  // v1.7 (byte 93 en adelante; antes eran ceros).
  u16(m.juegosGanados), u16(m.juegosJugados), u16(m.mejorReflejo);
  s[i++] = m.hoyJuegos, s[i++] = m.hoySuenos, s[i++] = m.hoySonoCon, s[i++] = m.hoyAnimo, s[i++] = m.hoyEspero;
  s[i++] = m.hoyDia, s[i++] = m.anioFechas, s[i++] = m.fechas, s[i++] = m.hoyAgua;
  // CRC al final.
  protocolo::escribir32(s + LARGO_MEMORIA - 4, protocolo::crc32(s, LARGO_MEMORIA - 4));
  return LARGO_MEMORIA;
}

bool decodificarMemoria(const uint8_t* s, size_t n, Memoria* salida) {
  if (n != LARGO_MEMORIA || s[0] != 1) return false;
  if (protocolo::leer32(s + LARGO_MEMORIA - 4) != protocolo::crc32(s, LARGO_MEMORIA - 4)) return false;
  Memoria m;
  size_t i = 1;
  auto u32 = [&]() {
    const uint32_t v = protocolo::leer32(s + i);
    i += 4;
    return v;
  };
  auto u16 = [&]() {
    const uint16_t v = static_cast<uint16_t>(s[i] | s[i + 1] << 8);
    i += 2;
    return v;
  };
  m.nacimiento = u32(), m.ultimaInteraccion = u32(), m.rencorHasta = u32(), m.ultimoDia = u32();
  m.dias = u16(), m.racha = u16(), m.rachaMax = u16();
  m.caricias = u32(), m.toques = u32(), m.sacudidas = u32();
  copiarBytes(m.horas, s + i, 24), i += 24;
  copiarBytes(m.gustos, s + i, 8), i += 8;
  m.hitos = u16(), m.anioCumpleBicho = u16(), m.anioCumpleDueno = u16();
  copiarBytes(m.deriva, s + i, N_RASGOS), i += N_RASGOS;
  copiarBytes(m.estado, s + i, N_ESTADO), i += N_ESTADO;
  m.epochEstado = u32();
  m.ausenciaMaxima = u16();
  m.afectoHoy = s[i++];
  m.juegosGanados = u16(), m.juegosJugados = u16(), m.mejorReflejo = u16();
  m.hoyJuegos = s[i++], m.hoySuenos = s[i++], m.hoySonoCon = s[i++], m.hoyAnimo = s[i++], m.hoyEspero = s[i++];
  m.hoyDia = s[i++], m.anioFechas = s[i++], m.fechas = s[i++], m.hoyAgua = s[i++];
  for (int8_t& d : m.deriva) d = static_cast<int8_t>(limitar(d, -10, 10));
  *salida = m;
  return true;
}

// ---------------------------------------------------------------- arranque

void MotorVida::iniciar(const Config* config, uint32_t semilla, uint32_t ahora) {
  config_ = config;
  semilla_ = config->vida.semilla ? config->vida.semilla : (semilla ? semilla : 1);
  azar_.sembrar(semilla_);
  for (int i = 0; i < N_ANIMO; i++) g_idxAnimo[i] = config->vida.indiceCara(NOMBRES_ANIMO[i]);
  ahora_ = inicio_ = ultimoTick_ = ahora;
  mem_ = Memoria();  // la que estaba guardada la pone el sistema (restaurarMemoria)
  rec_ = Recuerdos();  // y los recuerdos (v3), restaurarRecuerdos
  caracter_ = Caracter();  // sin identidad, sin tics (los tests del motor solo)
  favoritoIdx_ = caraPensar_ = microParam_ = -1;
  microDesde_ = -10;
  for (float& a : ajusteRasgos_) a = 0;
  rigidezV3_ = 1;
  lluvia_ = ausente_ = fueSolo_ = hitoSinFestejo_ = crecioPendiente_ = false;
  feriadoHoy_ = fechaTuya_ = -1;
  recuerdoDia_ = anticipoDia_ = dormisDia_ = 0;
  primerContacto_ = -1e9f;
  dormisPeso_ = 0;
  primeroDelDia_ = hitoPendiente_ = false;
  hitoSituacion_ = -1;
  acumulado_ = 0;
  t_ = 0;
  pasos_ = 0;
  fija_ = base_ = Config::Cara();
  capaCara_ = capaAnterior_ = reflejo_ = -1;
  capaPeso_ = capaPesoAnterior_ = 0;
  capaCambio_ = -10;
  dibujo_ = dibujoObjetivo_ = -1;
  opDibujo_ = 0;
  actual_ = -1;
  modo_ = Modo::Despierto;
  siesta_ = false;
  forzadoHasta_ = -1;
  ovejasHoy_ = false;
  for (float& f : enfriadoHasta_) f = 0;
  for (int8_t& h : historia_) h = -1;
  nCand_ = 0;
  proximoOcio_ = entre(azar_, 3, 8);
  proximoParpadeo_ = entre(azar_, 0.8f, 2.5f);
  proximaSacada_ = entre(azar_, 0.3f, 1.5f);
  proximaPostura_ = entre(azar_, 20, 60);
  proximaCuriosidad_ = entre(azar_, 120, 400);
  ultimaInteraccion_ = 0;
  for (float& h : habituacion_) h = 0;
  rafaga_ = 0;
  parpadeos_ = dobles_ = empezados_ = 0;
  nRegistro_ = iniRegistro_ = 0;
  // El estado y la personalidad de reposo.
  const Personalidad p = rasgos();
  estado_[0] = 0.6f + 0.2f * (p.energia - 0.5f);
  estado_[1] = 0.55f;
  estado_[2] = 0.3f;
  estado_[3] = 0.1f;
  estado_[4] = 0.2f + 0.5f * p.curiosidad;
  estado_[5] = 0.2f;
  estado_[6] = 0.0f;
  presionSueno_ = 0.15f;
  // Los resortes arrancan en la neutral.
  calcularObjetivos(0);
  for (int i = 0; i < N_PARAMETROS; i++) res_[i].x = obj_[i], res_[i].v = 0;
  parpadeo_ = Resorte();
  aplastar_.x = 1, aplastar_.v = 0;
  sacX_ = sacY_ = Resorte();
  cuadro_ = CuadroRostro();
  armarCuadro(0);
}

void MotorVida::reconfigurar(const Config* config) {
  config_ = config;
  if (config->vida.semilla) semilla_ = config->vida.semilla;
  for (int i = 0; i < N_ANIMO; i++) g_idxAnimo[i] = config->vida.indiceCara(NOMBRES_ANIMO[i]);
  if (actual_ != -1) terminarComportamiento();
  fija_ = base_ = Config::Cara();
  capaCara_ = capaAnterior_ = reflejo_ = -1;
  for (float& f : enfriadoHasta_) f = 0;
  for (int8_t& h : historia_) h = -1;
  for (int8_t& a : animoCaras_) a = -1;
  caraEstado_ = -1;
  nCand_ = 0;
  resolverV3();
}

void MotorVida::restaurarMemoria(const Memoria& m) {
  mem_ = m;
  // El estado guardado, si es de hace poco (si no, el reposo).
  if (m.epochEstado) {
    for (int i = 0; i < N_ESTADO; i++) estado_[i] = m.estado[i] / 255.0f;
  }
  estado_[2] = m.estado[2] / 255.0f;  // el afecto es la relación: siempre
}

Personalidad MotorVida::rasgos() const {
  Personalidad p = config_ ? config_->vida.personalidad : Personalidad();
  for (int i = 0; i < N_RASGOS; i++) p[i] = limitar(p[i] + mem_.deriva[i] / 100.0f + ajusteRasgos_[i], 0, 1);
  return p;
}

// ---------------------------------------------------------------- tick

void MotorVida::tick(uint32_t ahora, const Contexto& ctx) {
  ctx_ = ctx;
  const uint32_t dt = ahora - ultimoTick_;
  ultimoTick_ = ahora;
  ahora_ = ahora;
  if (dt > tiempos::SALTO_MS) {
    saltar(dt);
    return;
  }
  acumulado_ += dt;
  int n = 0;
  while (acumulado_ >= tiempos::PASO_MS && n < 200) {
    acumulado_ -= tiempos::PASO_MS;
    paso(DT);
    n++;
  }
  armarCuadro(0);
}

void MotorVida::saltar(uint32_t ms) {
  // Sin resortes ni micro-vida: el estado, el ritmo y los comportamientos
  // avanzan en tramos de un minuto como mucho.
  float resto = ms / 1000.0f;
  while (resto > 0) {
    const float tramo = minf(resto, 60.0f);
    t_ += tramo;
    resto -= tramo;
    lento(tramo);
    avanzarComportamiento();
  }
  acumulado_ = 0;
  calcularObjetivos(0);
  for (int i = 0; i < N_PARAMETROS; i++) res_[i].x = obj_[i], res_[i].v = 0;
  parpadeo_ = Resorte();
  parpadeoObj_ = 0;
  aplastar_.x = 1, aplastar_.v = 0;
  proximoParpadeo_ = static_cast<float>(t_) + entre(azar_, 0.5f, 2.0f);
  proximaSacada_ = static_cast<float>(t_) + entre(azar_, 0.3f, 1.0f);
  for (Particula& p : cuadro_.particulas) p.viva = false;
  armarCuadro(0);
}

void MotorVida::paso(float dt) {
  t_ += dt;
  pasos_++;
  if (pasos_ % (tiempos::LENTO_MS / tiempos::PASO_MS) == 0) lento(tiempos::LENTO_MS / 1000.0f);
  avanzarComportamiento();
  microVida(dt);
  calcularObjetivos(dt);
  // Resortes: la rigidez depende de la energía y la personalidad; un reflejo
  // los endurece un momento.
  const float energia = 0.5f * estado_[0] + 0.5f * rasgos().energia;
  float f = 0.8f + 0.4f * energia;
  if (modo_ == Modo::Dormido) f *= 0.75f;
  f *= rigidezV3_;
  const float t = static_cast<float>(t_);
  if (t < reflejoHasta_ && reflejo_ >= 0) f *= 1.0f + 1.5f * limitar((reflejoHasta_ - t) / tiempos::REFLEJO_MS * 1000.0f, 0, 1);
  rigidez_ = f;
  for (int i = 0; i < N_PARAMETROS; i++) {
    const ParResorte g = GRUPOS[static_cast<int>(infoParametro(i).grupo)];
    const float k = g.k * f * f, c = g.c * f;
    Resorte& r = res_[i];
    const float a = k * (obj_[i] - r.x) - c * r.v;
    r.v += a * dt;
    r.x += r.v * dt;
  }
  // Parpadeo, anticipación y mirada de la micro-vida.
  auto mover = [dt](Resorte& r, float objetivo, ParResorte p) {
    const float a = p.k * (objetivo - r.x) - p.c * r.v;
    r.v += a * dt;
    r.x += r.v * dt;
  };
  mover(parpadeo_, parpadeoObj_, PARPADEO);
  mover(aplastar_, 1.0f, APLASTAR);
  const bool siguiendo = t < seguirHasta_;
  const ParResorte pm =
      (siguiendo || mirada_ == Mirada::Perdida || mirada_ == Mirada::Ovejas || mirada_ == Mirada::Particula) ? SEGUIMIENTO : GRUPOS[2];
  mover(sacX_, sacBaseX_ + microX_, pm);
  mover(sacY_, sacBaseY_ + microY_, pm);
  // Partículas.
  for (Particula& p : cuadro_.particulas) {
    if (!p.viva) continue;
    p.edad += dt;
    if (p.edad >= p.vida) {
      p.viva = false;
      continue;
    }
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    if (p.enBurbuja) continue;  // sube con su burbuja
    if (p.tipo == TipoParticula::Zzz || p.tipo == TipoParticula::Nota) p.x += 0.03f * seno(p.fase + p.edad * 3) * dt;
    if (p.tipo == TipoParticula::Estrella) {
      const float a = p.fase + p.edad * 2.6f;
      p.x = 0.33f * coseno(a), p.y = 0.3f + 0.05f * seno(a * 2);
    }
    if (p.tipo == TipoParticula::Zzz) p.tamano = 0.035f + 0.02f * (p.edad / p.vida);
  }
  faseSpin_ += dt * 3.2f * res_[param::SPIN].x;
  faseOrbita_ += dt * 4.5f * res_[param::ORBIT].x;
  faseLatido_ += dt * 1.15f;
  if (faseLatido_ > 1) faseLatido_ -= 1;
  if (faseSpin_ > 1000) faseSpin_ -= 1000 * VUELTA / VUELTA;  // (solo para que no crezca sin fin)
  if (faseOrbita_ > VUELTA) faseOrbita_ -= VUELTA;
}

// ---------------------------------------------------------------- micro-vida

void MotorVida::parpadear(bool doble, float cierre) {
  parpadeoObj_ = 1;
  finParpadeo_ = static_cast<float>(t_) + cierre;
  ultimoParpadeo_ = static_cast<float>(t_);
  parpadeos_++;
  if (doble) {
    dobles_++;
    segundoParpadeo_ = static_cast<float>(t_) + cierre + 0.14f;
  }
}

void MotorVida::microVida(float dt) {
  const float t = static_cast<float>(t_);
  const Personalidad p = rasgos();
  const bool despierto = modo_ == Modo::Despierto;

  // Parpadeo: log-normal entre 2 y 6 s; 15 % doble; más seguido con sueño o
  // estrés, menos concentrado o sorprendido.
  if (t >= finParpadeo_) parpadeoObj_ = 0;
  if (segundoParpadeo_ > 0 && t >= segundoParpadeo_) {
    segundoParpadeo_ = -1;
    parpadeoObj_ = 1;
    finParpadeo_ = t + 0.085f;
  }
  if (rafaga_ > 0 && t >= proximaRafaga_) {
    rafaga_--;
    parpadear(false, 0.06f);
    proximaRafaga_ = t + 0.2f;
  }
  Parpadeo modo = parpadeoPaso_;
  const int capa = reflejo_ >= 0 && t < reflejoHasta_ ? reflejo_ : capaCara_;
  if (modo == Parpadeo::Normal && capa >= 0) modo = config_->vida.caras[capa].parpadeo;
  const bool cerrado = !despierto || modo == Parpadeo::No;
  if (t >= proximoParpadeo_) {
    float intervalo = limitar(exponencial(1.194f + 0.38f * normal(azar_)), 2.0f, 6.0f);
    intervalo *= 1.0f - 0.25f * maxf(maxf(estado_[5] - 0.4f, 0) / 0.6f, estado_[6]);
    if (modo == Parpadeo::Menos) intervalo *= 1.6f;
    if (modo == Parpadeo::Mas) intervalo *= 0.75f;
    intervalo *= caracter_.parpadeo;  // cada uno parpadea a su ritmo (v3)
    proximoParpadeo_ = t + intervalo;
    if (!cerrado) parpadear(azar_.real() < caracter_.dobles);
  }

  // Mirada: saccades cada 0,7 a 3 s cerca del centro, con preferencia por el
  // último estímulo; micro-saccades entre una y otra.
  if (t < seguirHasta_) {
    sacBaseX_ = seguirX_ * 0.13f, sacBaseY_ = seguirY_ * 0.09f;
  } else if (t >= proximaSacada_) {
    const float energia = 0.5f * (estado_[0] + p.energia);
    proximaSacada_ = t + entre(azar_, 0.7f, 3.0f) * (1.3f - 0.6f * energia);
    float x = 0, y = 0;
    switch (mirada_) {
      case Mirada::Frente: x = entre(azar_, -0.02f, 0.02f), y = entre(azar_, -0.01f, 0.01f); break;
      case Mirada::Arriba: x = entre(azar_, -0.04f, 0.04f), y = 0.1f; break;
      case Mirada::Abajo: x = entre(azar_, -0.04f, 0.04f), y = -0.12f; break;
      case Mirada::Izquierda: x = -0.15f, y = entre(azar_, -0.02f, 0.02f); break;
      case Mirada::Derecha: x = 0.15f, y = entre(azar_, -0.02f, 0.02f); break;
      case Mirada::Lado: x = 0.16f * ladoMirada_, y = entre(azar_, -0.02f, 0.03f); break;
      case Mirada::Alrededor:
        x = entre(azar_, -0.18f, 0.18f), y = entre(azar_, -0.08f, 0.1f);
        proximaSacada_ = t + entre(azar_, 0.4f, 0.9f);
        break;
      case Mirada::Perdida:
        if (caracter_.hay) x = 0.08f * caracter_.pensarX, y = 0.1f * caracter_.pensarY;
        else x = 0.08f * ladoMirada_, y = 0.1f;
        break;
      case Mirada::Puerta:
        x = 0.17f * caracter_.puerta, y = 0.01f;
        if (azar_.real() < 0.2f) x = entre(azar_, -0.02f, 0.02f);  // de vez en cuando, al frente
        break;
      case Mirada::Particula: x = sacBaseX_, y = sacBaseY_; break;  // la sigue (abajo), continuo: sin saltos
      case Mirada::Reojo: x = 0.2f * ladoMirada_, y = -0.02f; break;
      case Mirada::Estimulo: x = estX_ * 0.15f, y = estY_ * 0.1f; break;
      case Mirada::Rem:
        x = entre(azar_, -0.05f, 0.05f), y = entre(azar_, -0.02f, 0.02f);
        proximaSacada_ = t + entre(azar_, 0.12f, 0.3f);
        break;
      case Mirada::Ovejas: break;  // abajo, continuo
      default: {
        if (!despierto) break;
        // Libre: cerca del centro, con algo de preferencia por el estímulo.
        x = mezclar(entre(azar_, -0.07f, 0.07f), estX_ * 0.13f, 0.3f);
        y = mezclar(entre(azar_, -0.04f, 0.04f), estY_ * 0.09f, 0.3f);
        // Ánimo bajo: mira más abajo. Tímido, también.
        y -= 0.04f * maxf(0, 0.45f - estado_[1]) / 0.45f + 0.02f * p.timidez;
        // Si a esta hora suele aparecer el dueño, lo espera: mirando "la
        // puerta" si ya aprendió a qué hora llegás (v3), si no al frente.
        if (esperandoDueno_ && rec_.minLlegada < 1440 && azar_.real() < 0.6f) x = 0.17f * caracter_.puerta, y = 0.01f;
        else if (esperandoDueno_) x *= 0.4f, y *= 0.4f;
        if (azar_.real() < 0.3f) x *= 0.2f, y *= 0.2f;
      }
    }
    const float salto = absf(x - sacBaseX_) + absf(y - sacBaseY_);
    sacBaseX_ = x, sacBaseY_ = y;
    // Siempre parpadea al terminar un movimiento grande de mirada.
    if (salto > 0.12f && despierto && !cerrado && t - ultimoParpadeo_ > 0.6f) {
      parpadear(false);
      proximoParpadeo_ = maxf(proximoParpadeo_, t + 1.5f);
    }
  }
  if (mirada_ == Mirada::Particula) {
    // La última partícula viva (persigue la estrella, mira su dibujito).
    const Particula* q = nullptr;
    for (const Particula& pa : cuadro_.particulas)
      if (pa.viva && !pa.enBurbuja && (!q || pa.edad < q->edad)) q = &pa;
    if (q) sacBaseX_ = limitar(q->x * 0.4f, -0.2f, 0.2f), sacBaseY_ = limitar(q->y * 0.3f, -0.12f, 0.12f);
  }
  // El microgesto propio (v3): cada tanto, el suyo.
  if (despierto && microParam_ >= 0 && t >= proximoMicro_) {
    const MicrogestoBase& g = caracter::MICROGESTOS[caracter_.microgesto];
    microDesde_ = t;
    proximoMicro_ = t + entre(azar_, g.cadaMin, g.cadaMax);
  }
  if (mirada_ == Mirada::Ovejas) {
    const float f = fraccion(t / 1.3f);
    sacBaseX_ = -0.12f + 0.24f * f;
    sacBaseY_ = -0.02f + 0.05f * absf(seno(f * MEDIA_VUELTA));
  }
  if (t >= proximaMicro_) {
    proximaMicro_ = t + entre(azar_, 0.15f, 0.4f);
    const bool quieto = mirada_ == Mirada::Perdida || mirada_ == Mirada::Ovejas || mirada_ == Mirada::Particula ||
                        (capa >= 0 && config_->vida.caras[capa].movimiento == Movimiento::Quieto);
    const float a = quieto ? 0.002f : entre(azar_, 0.005f, 0.01f);
    microX_ = entre(azar_, -1, 1) * a, microY_ = entre(azar_, -1, 1) * a * 0.6f;
  }

  // Ajustes de postura cada 20 a 60 s.
  if (t >= proximaPostura_) {
    proximaPostura_ = t + entre(azar_, 20, 60);
    posturaTilt_ = entre(azar_, -0.035f, 0.035f);
    posturaEy_ = entre(azar_, -0.012f, 0.012f);
  }

  // Respiración: 16 por minuto (más rápida con estrés); dormido, más lenta y honda.
  const float bpm = despierto ? 16.0f * (1.0f + 0.6f * estado_[6]) : 8.6f;
  faseResp_ += dt * VUELTA * bpm / 60.0f;
  if (faseResp_ > VUELTA) faseResp_ -= VUELTA;

  // El glitch salta de banda cada 40 a 180 ms.
  if (res_[param::GLITCH].x > 0.05f && t >= proximoGlitch_) {
    glitchSemilla_ = azar_.real() * 100.0f;
    proximoGlitch_ = t + entre(azar_, 0.04f, 0.18f);
  }
  progresoCarga_ = ctx_.cargando == 1 && ctx_.bateria >= 0 ? ctx_.bateria / 100.0f : fraccion(t * 0.12f);
}

float MotorVida::ruido(float x, uint32_t canal) const {
  // Ruido de valor 1D: suave, determinista y distinto por semilla y canal.
  const float i = piso(x);
  const float f = x - i;
  const int32_t k = static_cast<int32_t>(i);
  auto v = [&](int32_t n) {
    return (mezclarBits(static_cast<uint32_t>(n) * 0x9E3779B1u ^ semilla_ * 0x85EBCA77u ^ canal * 0xC2B2AE3Du) >> 8) *
               (2.0f / 16777216.0f) -
           1.0f;
  };
  const float u = f * f * (3 - 2 * f);
  return mezclar(v(k), v(k + 1), u);
}

// ---------------------------------------------------------------- objetivos

void MotorVida::cambioDeCapa(int cara, float peso) {
  if (cara == capaCara_ && absf(peso - capaPeso_) < 0.01f) return;
  const float t = static_cast<float>(t_);
  // Anticipación: se aplasta y rebota, más fuerte cuanto más cambia y cuanto
  // más energía tiene.
  const float energia = 0.5f * (estado_[0] + rasgos().energia);
  float intensidad = 0.6f;
  if (cara != capaCara_) {
    Rostro a, b;
    if (capaCara_ >= 0) config_->vida.caras[capaCara_].aplicar(a);
    if (cara >= 0) config_->vida.caras[cara].aplicar(b);
    float d = 0;
    for (int i = 0; i < N_PARAMETROS; i++) {
      const InfoParametro& inf = infoParametro(i);
      d += absf(a[i] - b[i]) / (inf.maximo - inf.minimo);
    }
    intensidad = limitar(d / 1.5f, 0.3f, 1.0f);
    if (modo_ == Modo::Despierto) aplastar_.v -= 2.2f * intensidad * (0.6f + 0.8f * energia);
  }
  capaAnterior_ = capaCara_;
  capaPesoAnterior_ = capaPeso_;
  capaCara_ = cara;
  capaPeso_ = peso;
  capaCambio_ = t;
  capaDesde_ = t;
  // La voz propia sin texto (si hay parlante): un "¡oh!" al sorprenderse, un
  // "mmm..." al pensar (caracter.json "voz.caras"; el sistema decide si suena).
  if (cara >= 0 && cara != capaCara_ && peso >= 0.5f && modo_ == Modo::Despierto) vocalizar(config_->vida.caras[cara].nombre, true);
  // La partícula propia de la cara, al llegar.
  if (cara >= 0 && config_->vida.caras[cara].particula >= 0 && modo_ == Modo::Despierto)
    soltar(static_cast<TipoParticula>(config_->vida.caras[cara].particula));
  proximaParticula_ = t + entre(azar_, 1.0f, 2.5f);
}

bool MotorVida::esImagen(int cara) const {
  if (cara < 0 || cara >= config_->vida.nCaras) return false;
  const int i = config_->vida.caras[cara].imagen;
  return i >= 0 && (dispImagenes_ & (1u << i));
}

int MotorVida::caraImagen() const {
  const float t = static_cast<float>(t_);
  if (reflejo_ >= 0 && t < reflejoHasta_ && esImagen(reflejo_)) return reflejo_;
  if (capaCara_ >= 0 && capaPeso_ >= 0.5f && esImagen(capaCara_)) return capaCara_;
  return personaImagen_ >= 0 && esImagen(personaImagen_) ? personaImagen_ : -1;
}

int MotorVida::caraEstado() {
  // Las reglas de estado ("bateria<15" → una cara mientras se cumpla). Las de
  // batería van como una capa del ánimo (capaBateria_), no como el reposo.
  capaBateria_ = false;
  for (int i = 0; i < config_->nReglas; i++) {
    const Regla& r = config_->reglas[i];
    if (r.si.tipo == Condicion::Tipo::Evento) continue;
    float v;
    if (r.si.variable == Variable::Bateria) {
      if (ctx_.bateria < 0) continue;
      v = static_cast<float>(ctx_.bateria);
    } else {
      v = humor();
    }
    const bool ok = r.si.tipo == Condicion::Tipo::Menor ? v < r.si.valor : v > r.si.valor;
    if (!ok) continue;
    for (int a = 0; a < r.nAcciones; a++)
      if (r.entonces[a].tipo == Accion::Tipo::Expresion || r.entonces[a].tipo == Accion::Tipo::Reflejo) {
        const Config::Cara c = config_->resolverCara(r.entonces[a].argumento);
        capaBateria_ = r.si.variable == Variable::Bateria && c.vida >= 0;
        if (c.vida >= 0) return c.vida;
        if (c.dibujo >= 0) return 1000 + c.dibujo;
      }
  }
  return -1;
}

void MotorVida::calcularObjetivos(float dt) {
  const DatosVida& v = config_->vida;
  const float t = static_cast<float>(t_);
  Rostro N;
  const int neutral = v.indiceCara("neutral");
  if (neutral >= 0) v.caras[neutral].aplicar(N);

  // La cara de reposo: fija > estado (batería) > base de la lista > dormido >
  // la de la config ("cara": {"expresion": …}) > neutral.
  int reposo = v.reposo >= 0 ? v.reposo : neutral;
  // La cara de imagen de la persona: la primera de imagen entre la fija, la de
  // estado, la base y la de la config; una fija o una base que no es de imagen
  // corta la búsqueda (la eligió el dueño). Dormido sigue siendo la misma cara,
  // con los ojos cerrados.
  personaImagen_ = -1;
  if (fija_.existe()) {
    personaImagen_ = esImagen(fija_.vida) ? fija_.vida : -2;
  }
  if (personaImagen_ == -1 && caraEstado_ >= 0 && caraEstado_ < 1000 && esImagen(caraEstado_)) personaImagen_ = caraEstado_;
  if (personaImagen_ == -1 && base_.existe()) personaImagen_ = esImagen(base_.vida) ? base_.vida : -2;
  if (personaImagen_ == -1 && esImagen(v.reposo)) personaImagen_ = v.reposo;
  if (personaImagen_ < 0) personaImagen_ = -1;
  int dibujo = -1;
  bool conAnimo = true;
  if (fija_.existe()) {
    reposo = fija_.vida >= 0 ? fija_.vida : neutral;
    dibujo = fija_.dibujo;
    conAnimo = false;
  } else if (caraEstado_ >= 0 && !capaBateria_) {
    if (caraEstado_ >= 1000) dibujo = caraEstado_ - 1000;
    else reposo = caraEstado_;
  } else if (base_.existe()) {
    reposo = base_.vida >= 0 ? base_.vida : neutral;
    dibujo = base_.dibujo;
  } else if (modo_ == Modo::Dormido) {
    const int d = v.indiceCara("dormido");
    if (d >= 0) reposo = d;
    conAnimo = false;
  }
  float T[N_PARAMETROS];
  {
    Rostro R = N;
    if (reposo >= 0) v.caras[reposo].aplicar(R);
    for (int i = 0; i < N_PARAMETROS; i++) T[i] = R[i];
  }

  // Ánimo de fondo: base + Σ peso·delta.
  if (conAnimo) {
    for (int k = 0; k < 6; k++) {
      if (animoCaras_[k] < 0 || animoPesos_[k] <= 0.001f) continue;
      const CaraVida& c = v.caras[animoCaras_[k]];
      for (int a = 0; a < c.nAjustes; a++) T[c.ajustes[a].param] += animoPesos_[k] * (c.ajustes[a].valor() - N[c.ajustes[a].param]);
    }
    // La batería baja: más apagado, pero sin tapar todo (enchufado, casi nada).
    if (capaBateria_ && caraEstado_ >= 0 && caraEstado_ < 1000) {
      const float w = ctx_.cargando == 1 ? 0.12f : 0.35f;
      const CaraVida& c = v.caras[caraEstado_];
      for (int a = 0; a < c.nAjustes; a++) T[c.ajustes[a].param] += w * (c.ajustes[a].valor() - N[c.ajustes[a].param]);
    }
  }

  // La capa: reflejo > comportamiento o reacción. Se mezcla desde el reposo,
  // con el escalonado por grupo (primero la mirada, después los párpados, la
  // forma al final).
  int capa = -1;
  float peso = 0;
  if (!fija_.existe()) {
    if (reflejo_ >= 0 && t < reflejoHasta_) capa = reflejo_, peso = 1;
    else if (actual_ != -1 && pasoCara_ >= 0) capa = pasoCara_, peso = pasoPeso_;
    if (reflejo_ >= 0 && t >= reflejoHasta_) reflejo_ = -1;
  }
  if (!fija_.existe() && actual_ == -2 && pasoDibujo_ >= 0) dibujo = pasoDibujo_;
  if (dt > 0) cambioDeCapa(capa, peso);
  else capaCara_ = capa, capaPeso_ = peso, capaAnterior_ = capa, capaPesoAnterior_ = peso;
  const float desdeCambio = t - capaCambio_;
  Rostro E, Eant;
  // Un reflejo no se escalona: todo responde ya.
  const bool escalonando = desdeCambio < 0.1f && !(reflejo_ >= 0 && capaCara_ == reflejo_);
  if (capaCara_ >= 0) {
    E = N;
    v.caras[capaCara_].aplicar(E);
  }
  if (escalonando && capaAnterior_ >= 0) {
    Eant = N;
    v.caras[capaAnterior_].aplicar(Eant);
  }
  float base[N_PARAMETROS];
  for (int i = 0; i < N_PARAMETROS; i++) base[i] = T[i];
  for (int i = 0; i < N_PARAMETROS; i++) {
    const int g = static_cast<int>(infoParametro(i).grupo);
    const bool viejo = escalonando && desdeCambio < RETRASO[g];
    const int c = viejo ? capaAnterior_ : capaCara_;
    const float w = viejo ? capaPesoAnterior_ : capaPeso_;
    if (c < 0 || w <= 0) continue;
    const float e = viejo ? Eant[i] : E[i];
    T[i] = base[i] + w * (e - base[i]) + (T[i] - base[i]);
  }

  // v3: la cara de pensar mira hacia su lado (el de su semilla).
  if (caracter_.hay && caraPensar_ >= 0 && capaCara_ == caraPensar_ && capaPeso_ > 0) {
    const CaraVida& c = v.caras[caraPensar_];
    for (int a = 0; a < c.nAjustes; a++) {
      const int pi = c.ajustes[a].param;
      const float x = c.ajustes[a].valor();
      if (pi == param::GX) T[pi] += capaPeso_ * (absf(x) * caracter_.pensarX - x);
      if (pi == param::GY) T[pi] += capaPeso_ * (absf(x) * caracter_.pensarY - x);
    }
  }
  // El microgesto: un empujoncito suave (seno) que los resortes redondean.
  if (microParam_ >= 0 && modo_ == Modo::Despierto && !fija_.existe()) {
    const MicrogestoBase& g = caracter::MICROGESTOS[caracter_.microgesto];
    const float u = (t - microDesde_) / g.dura;
    if (u >= 0 && u < 1) {
      const float b = g.valor * seno(u * MEDIA_VUELTA);
      T[microParam_] += b;
      if (microAmbos_) T[microParam_ + param::DER] += b;
    }
  }

  // La capa de movimiento de la cara que manda.
  int manda = capaCara_ >= 0 && capaPeso_ >= 0.5f ? capaCara_ : reposo;
  if (manda >= 0) moverCapa(v.caras[manda].movimiento, t - (manda == capaCara_ ? capaDesde_ : 0), T, manda);

  // La música (v1.9): la cabeza al ritmo del tema. El peso sube y baja de a
  // poco (y los resortes hacen el resto: nunca un corte); la fase es continua.
  const bool bailando = ritmoBpm_ > 0 && modo_ == Modo::Despierto;
  if (dt > 0) {
    ritmoPeso_ = bailando ? minf(1, ritmoPeso_ + dt * 1.5f) : maxf(0, ritmoPeso_ - dt * 1.5f);
    if (ritmoBpm_ > 0) faseRitmo_ += dt * ritmoBpm_ / 60.0f;  // en pulsos
    if (faseRitmo_ > 64) faseRitmo_ -= 64;
  } else {
    ritmoPeso_ = bailando ? 1.0f : 0.0f;
  }
  if (ritmoPeso_ > 0.001f) {
    // De costado cada dos pulsos y un saltito en cada uno.
    T[param::FTILT] += 0.09f * ritmoPeso_ * seno(faseRitmo_ * MEDIA_VUELTA);
    T[param::EY] += 0.02f * ritmoPeso_ * absf(seno(faseRitmo_ * MEDIA_VUELTA));
    if (bailando && ritmoPeso_ > 0.5f && t >= proximaNotaRitmo_) {
      soltar(TipoParticula::Nota);
      proximaNotaRitmo_ = t + entre(azar_, 1.5f, 3.5f);
    }
  }

  // Micro-vida sobre todo: postura, ruido de forma.
  T[param::FTILT] += posturaTilt_;
  T[param::EY] += posturaEy_;
  if (modo_ == Modo::Dormido) {
    // De costado (cambia cada tanto, por los resortes), sonrisas y murmullos en
    // sueños, y los ojos que se mueven debajo de los párpados en el REM.
    T[param::FTILT] += 0.07f * ladoSueno_;
    T[param::GX] += 0.035f * ladoSueno_;
    if (t < sonrisaSuenoHasta_) T[param::MCURVE] += 0.4f, T[param::MW] += 0.02f;
    if (t < murmulloHasta_) T[param::MO] += 0.025f * absf(seno(t * 9)), T[param::MW] += 0.02f;
    if (mirada_ == Mirada::Rem)
      for (int lado = 0; lado < 2; lado++) T[lado * param::POR_OJO + param::LT] -= 0.03f * (0.5f + 0.5f * seno(t * 17 + lado));
  }
  for (int lado = 0; lado < 2; lado++) {
    const int b = lado * param::POR_OJO;
    T[b + param::W] *= 1.0f + 0.02f * ruido(t * 0.05f, 10 + lado);
    T[b + param::H] *= 1.0f + 0.02f * ruido(t * 0.045f, 20 + lado);
  }
  Rostro limite;
  for (int i = 0; i < N_PARAMETROS; i++) limite[i] = T[i];
  limite.limitar();
  for (int i = 0; i < N_PARAMETROS; i++) obj_[i] = limite[i];
  // El brillo nunca pasa el tope de la marca.
  dibujoObjetivo_ = dibujo;
  if (dt > 0) {
    if (dibujoObjetivo_ != dibujo_) {
      opDibujo_ -= dt * 5;
      if (opDibujo_ <= 0) opDibujo_ = 0, dibujo_ = dibujoObjetivo_;
    } else if (dibujo_ >= 0) {
      opDibujo_ = minf(1, opDibujo_ + dt * 5);
    }
  } else {
    dibujo_ = dibujoObjetivo_;
    opDibujo_ = dibujo_ >= 0 ? 1.0f : 0.0f;
  }
}

void MotorVida::moverCapa(Movimiento m, float t, float* T, int cara) {
  const float tt = static_cast<float>(t_);
  const bool emitir = tt >= proximaParticula_ && modo_ == Modo::Despierto;
  const int8_t part = config_->vida.caras[cara].particula;
  auto periodica = [&](float lo, float hi) {
    if (!emitir || part < 0) return;
    soltar(static_cast<TipoParticula>(part));
    proximaParticula_ = tt + entre(azar_, lo, hi);
  };
  switch (m) {
    case Movimiento::Rebote: T[param::EY] += 0.018f * (0.5f + 0.5f * seno(t * 5)); break;
    case Movimiento::Saltitos:
      T[param::EY] += 0.045f * maxf(0, seno(t * 8));
      periodica(0.7f, 1.2f);
      break;
    case Movimiento::Risa:
      T[param::SHAKE] = maxf(T[param::SHAKE], seno(t * 9) > 0 ? 0.35f : 0.1f);
      T[param::MO] += 0.02f * absf(seno(t * 10));
      break;
    case Movimiento::MiradaLado: T[param::GX] += 0.12f * seno(t * 1.3f); break;
    case Movimiento::Salto:
      if (t < 0.5f) T[param::EY] += 0.06f * seno(t / 0.5f * MEDIA_VUELTA);
      break;
    case Movimiento::Encogerse:
      T[param::FSCALE] -= 0.04f;
      T[param::SHAKE] = maxf(T[param::SHAKE], 0.3f);
      break;
    case Movimiento::Dormir:
      if (modo_ == Modo::Dormido && tt >= proximaParticula_ && part >= 0) {
        soltar(static_cast<TipoParticula>(part));
        proximaParticula_ = tt + entre(azar_, 5, 9);
      }
      break;
    case Movimiento::Temblor:
      if (t < 0.7f) T[param::SHAKE] = maxf(T[param::SHAKE], 0.6f);
      break;
    case Movimiento::Resoplido: {
      const float f = t - 3.0f * piso(t / 3.0f);
      if (f < 0.25f) T[param::EY] -= 0.03f;
      break;
    }
    case Movimiento::Lagrima: periodica(5, 9); break;
    case Movimiento::Achicarse: T[param::FSCALE] -= 0.05f; break;
    case Movimiento::Picaro: T[param::FTILT] += -0.03f + 0.02f * seno(t * 3); break;
    case Movimiento::Latido: periodica(1.3f, 2.0f); break;
    case Movimiento::Bamboleo:
      T[param::FTILT] += 0.12f * seno(t * 2.2f);
      periodica(1.2f, 1.8f);
      break;
    case Movimiento::Escuchar: T[param::GX] += estX_ * 0.06f; break;
    case Movimiento::Hablar: T[param::MO] = 0.015f + 0.04f * absf(seno(t * 12) * seno(t * 3.7f)); break;
    case Movimiento::Pensar: break;
    case Movimiento::Duda: T[param::FTILT] += 0.05f * seno(t * 2); break;
    case Movimiento::Acercarse: T[param::FSCALE] += 0.03f * (1 - exponencial(-t)); break;
    case Movimiento::Suspiro:
      T[param::GY] -= 0.03f;
      if (t < 0.6f) T[param::FSCALE] += 0.025f * seno(t / 0.6f * MEDIA_VUELTA);
      break;
    case Movimiento::Cabeceo: {
      const float f = t - 5.0f * piso(t / 5.0f);
      const float caida = f < 4 ? 0.22f * f / 4 : 0;
      T[param::LT] += caida, T[param::DER + param::LT] += caida;
      if (f >= 4 && f < 4.3f) T[param::EY] += 0.02f;
      break;
    }
    case Movimiento::Buscar:
      T[param::GX] += 0.2f * seno(t * 1.6f);
      T[param::GY] += 0.05f * seno(t * 2.3f);
      break;
    case Movimiento::Caer:
      T[param::FTILT] += 0.15f * minf(1, t);
      T[param::EY] -= 0.04f * minf(1, t);
      break;
    case Movimiento::Estirarse: T[param::FSCALE] += 0.04f * seno(minf(t, 1.5f) / 1.5f * MEDIA_VUELTA); break;
    default: break;
  }
}

void MotorVida::soltar(TipoParticula tipo, int cuantas) {
  for (int n = 0; n < cuantas; n++) {
    // Si no hay lugar, reemplaza a la más vieja.
    int libre = -1;
    float vieja = -1;
    for (int i = 0; i < MAX_PARTICULAS; i++) {
      const Particula& p = cuadro_.particulas[i];
      if (!p.viva) {
        libre = i;
        break;
      }
      if (p.edad / p.vida > vieja) vieja = p.edad / p.vida, libre = i;
    }
    Particula& p = cuadro_.particulas[libre];
    p = Particula();
    p.viva = true;
    p.tipo = tipo;
    p.fase = azar_.real() * VUELTA;
    const float sep = res_[param::SEP].x;
    switch (tipo) {
      case TipoParticula::Zzz:
        p.x = 0.3f + entre(azar_, 0, 0.05f), p.y = 0.22f, p.vx = 0.04f, p.vy = 0.11f, p.vida = 3.2f, p.tamano = 0.035f;
        break;
      case TipoParticula::Corazon:
        p.x = entre(azar_, -0.4f, 0.4f), p.y = entre(azar_, 0.0f, 0.2f), p.vy = 0.15f, p.vida = 2.0f, p.tamano = 0.045f;
        break;
      case TipoParticula::Lagrima:
        p.x = -sep + 0.03f, p.y = -0.2f, p.vy = -0.22f, p.vida = 1.5f, p.tamano = 0.03f;
        break;
      case TipoParticula::Gota:
        p.x = 0.38f, p.y = 0.25f, p.vy = -0.1f, p.vida = 2.0f, p.tamano = 0.032f;
        break;
      case TipoParticula::Chispa: {
        const float a = azar_.real() * VUELTA;
        p.x = 0.45f * coseno(a), p.y = 0.3f * seno(a), p.vy = 0.05f, p.vida = 0.9f, p.tamano = 0.04f;
        break;
      }
      case TipoParticula::Pregunta:
      case TipoParticula::Exclamacion:
        p.x = 0.28f, p.y = 0.3f, p.vy = 0.05f, p.vida = 1.8f, p.tamano = 0.07f;
        break;
      case TipoParticula::Nota:
        p.x = 0.42f, p.y = entre(azar_, -0.05f, 0.1f), p.vx = 0.08f, p.vy = 0.12f, p.vida = 2.2f, p.tamano = 0.06f;
        break;
      case TipoParticula::Estrella:
        p.vida = 2.4f, p.tamano = 0.04f;
        p.x = 0.33f * coseno(p.fase), p.y = 0.3f;
        break;
      case TipoParticula::Burbuja:
        p.x = -0.3f, p.y = 0.33f, p.vy = 0.03f, p.vida = 3.0f, p.tamano = 0.08f;
        break;
      case TipoParticula::Reloj:
        p.x = 0.36f, p.y = 0.28f, p.vy = 0.04f, p.vida = 1.6f, p.tamano = 0.07f;
        break;
      case TipoParticula::Garabato:
        // Un dibujito abajo de los ojos, que se va haciendo (v3).
        p.x = entre(azar_, -0.12f, 0.12f), p.y = -0.3f, p.vida = 4.0f, p.tamano = 0.085f;
        break;
      default:
        break;
    }
  }
}

void MotorVida::armarCuadro(float) {
  CuadroRostro& c = cuadro_;
  for (int i = 0; i < N_PARAMETROS; i++) c.r[i] = res_[i].x;
  // El parpadeo cierra los párpados hacia el medio (como el v0).
  const float bl = limitar(parpadeo_.x, 0, 1);
  for (int lado = 0; lado < 2; lado++) {
    const int b = lado * param::POR_OJO;
    c.r[b + param::LT] += (0.5f - c.r[b + param::LT]) * bl;
    c.r[b + param::LB] += (0.5f - c.r[b + param::LB]) * bl;
  }
  c.r[param::GX] += sacX_.x;
  c.r[param::GY] += sacY_.x;
  c.r.limitar();
  const bool dormido = modo_ == Modo::Dormido;
  const float t = static_cast<float>(t_);
  const float resp = 1.0f + (dormido ? 0.035f : 0.012f) * seno(faseResp_);
  const float sq = aplastar_.x;
  c.escalaY = resp * sq;
  c.escalaX = 1.0f / raiz(maxf(0.5f, sq));
  c.derivaX = 0.012f * ruido(t * 0.13f, 1);
  c.derivaY = 0.012f * ruido(t * 0.11f, 2) + (dormido ? 0.02f * seno(faseResp_) : 0.0f);
  const float ph = faseLatido_;
  auto g = [](float x) { return exponencial(-x * x); };
  c.latido = g((ph - 0.1f) * 22) + 0.65f * g((ph - 0.3f) * 22);
  const float tm = t - 1000.0f * piso(t / 1000.0f);
  c.pulso = 1 + c.r[param::PULSE] * (0.09f + 0.09f * seno(tm * 9) * seno(tm * 2.3f));
  c.faseSpin = faseSpin_;
  c.faseOrbita = faseOrbita_;
  c.glitch = c.r[param::GLITCH] * (0.35f + 0.65f * (seno(tm * 23) > 0.6f ? 1.0f : 0.0f));
  c.semillaGlitch = glitchSemilla_;
  c.progreso = progresoCarga_;
  c.tiempo = tm;
  c.colorOjos = config_->hayColorOjos ? config_->colorOjos : paleta::NARANJA;
  // Brillo: baja de noche y dormido; nunca pasa el tope de la marca.
  float brillo = 1.0f;
  if (dormido) brillo = 0.4f;
  else if (ctx_.minutoDelDia >= 0) {
    const int falta = minutosHasta(ctx_.minutoDelDia, config_->vida.duerme);
    if (falta < 60) brillo = 1.0f - 0.3f * (60 - falta) / 60.0f;
  }
  if (!dormido) brillo *= 1.0f - caracter::BRILLO_DORMIS * dormisPeso_;
  // Con poca batería (sin enchufar), más apagado.
  if (!dormido && ctx_.bateria >= 0 && ctx_.bateria < 20 && ctx_.cargando != 1) brillo *= 0.85f;
  c.brillo = minf(brillo, config_->vida.brilloMax);

  // Para los dibujos de formas (las caras propias del usuario).
  EstadoCara& e = estadoCara_;
  e.cierre = bl;
  e.miradaX = (sacX_.x + res_[param::GX].x * 0.3f) * 0.6f;
  e.miradaY = -(sacY_.x) * 0.6f;
  e.respiracion = seno(faseResp_) * (dormido ? 1.6f : 1.0f);
  e.tamano = config_->tamano;
  e.hayColorOjos = config_->hayColorOjos;
  e.colorOjos = config_->colorOjos;
  for (int i = 0; i < 8; i++) e.audio[i] = 0.55f + 0.45f * seno(tm * (7.0f + i * 1.7f) + i * 1.3f);
  e.progreso = progresoCarga_;
}

// ---------------------------------------------------------------- estado interno

float MotorVida::variable(VarVida v) const {
  const int i = static_cast<int>(v);
  if (i < N_ESTADO) return estado_[i];
  const Personalidad p = rasgos();
  if (i < N_ESTADO + N_RASGOS) return p[i - N_ESTADO];
  const float t = static_cast<float>(t_);
  switch (v) {
    case VarVida::Bateria: return static_cast<float>(ctx_.bateria);
    case VarVida::Hora: return ctx_.minutoDelDia < 0 ? -1.0f : ctx_.minutoDelDia / 60.0f;
    case VarVida::Humor: return humor();
    case VarVida::Ignorado: return (t - ultimaInteraccion_) / 60.0f;
    case VarVida::Ausencia: return ausenciaHoras_;
    case VarVida::Rencor: return ctx_.epoch && ctx_.epoch < mem_.rencorHasta ? 1.0f : 0.0f;
    case VarVida::Dormido: return modo_ == Modo::Dormido ? 1.0f : 0.0f;
    case VarVida::Noche: return enHorarioDormir() ? 1.0f : 0.0f;
    case VarVida::Cargando: return ctx_.cargando == 1 ? 1.0f : 0.0f;
    case VarVida::Etapa: return static_cast<float>(rec_.etapa);
    case VarVida::Dias: return static_cast<float>(mem_.dias);
    case VarVida::Solo: return caracter_.hay && (ausente_ || (t - ultimaInteraccion_) >= caracter::SOLO_MINUTOS * 60.0f) ? 1.0f : 0.0f;
    case VarVida::Bajon: return maxf(0, bajon());
    case VarVida::Lluvia: return lluvia_ ? 1.0f : 0.0f;
    case VarVida::Esperando: return esperandoDueno_ ? 1.0f : 0.0f;
    default: return 0;
  }
}

bool MotorVida::cumple(const CondVida& c) const {
  const float v = variable(c.var);
  switch (c.op) {
    case CondVida::Op::Mayor: return v > c.valor;
    case CondVida::Op::Menor: return v < c.valor;
    default: return v > 0.5f;
  }
}

void MotorVida::empujar(int var, float delta) {
  if (var < 0 || var >= N_ESTADO) return;
  estado_[var] = limitar(estado_[var] + delta, 0, 1);
  if (var == 5) presionSueno_ = limitar(presionSueno_ + delta, 0, 1);
}

bool MotorVida::enHorarioDormir() const {
  const int m = ctx_.minutoDelDia;
  if (m < 0) return false;
  const int d = config_->vida.duerme, a = config_->vida.despierta;
  return d < a ? (m >= d && m < a) : (m >= d || m < a);
}

void MotorVida::actualizarEstado(float dt) {
  const Personalidad p = rasgos();
  const float t = static_cast<float>(t_);
  const bool dormido = modo_ == Modo::Dormido;
  const int m = ctx_.minutoDelDia;
  const DatosVida& v = config_->vida;
  auto hacia = [dt](float& x, float reposo, float tau) { x += (reposo - x) * (1.0f - exponencial(-dt / tau)); };
  const float hora = 3600.0f;

  // Energía: sigue el ritmo diario.
  float eReposo = 0.55f + 0.25f * (p.energia - 0.5f);
  if (m >= 0) {
    const int desdeDespertar = minutosHasta(v.despierta, m);
    if (desdeDespertar < 180) eReposo += 0.15f;
    if (m >= 13 * 60 + 30 && m < 15 * 60 + 30) eReposo -= 0.12f;
    const int falta = minutosHasta(m, v.duerme);
    if (falta < 120) eReposo -= 0.2f * (120 - falta) / 120.0f;
  }
  if (ctx_.bateria >= 0 && ctx_.bateria < 20) eReposo -= 0.25f;
  if (dormido) hacia(estado_[0], 1.0f, 3 * hora);
  else hacia(estado_[0], limitar(eReposo, 0.05f, 1), hora);

  // Ánimo: vuelve en unas 2 h a un reposo que depende del afecto.
  float aReposo = 0.5f + 0.15f * (estado_[2] - 0.3f);
  if (estado_[3] > 0.6f) aReposo -= 0.12f;
  if (ctx_.bateria >= 0 && ctx_.bateria < 20) aReposo -= 0.1f;
  hacia(estado_[1], aReposo, 2 * hora);

  // Afecto: casi no decae; después de 2 días sin verlo, baja muy despacio.
  if (t - ultimaInteraccion_ > 48 * hora) estado_[2] = maxf(0, estado_[2] - 0.01f * dt / (24 * hora));

  // Aburrimiento: sube sin estímulos (más rápido si es sociable).
  if (!dormido) {
    const float tau = 1.6f * hora * (1.25f - 0.5f * p.sociabilidad);
    estado_[3] += (1 - estado_[3]) * (1 - exponencial(-dt / tau));
  } else {
    hacia(estado_[3], 0, hora);
  }

  // Curiosidad: unos 10 min; cada tanto algo le llama la atención.
  hacia(estado_[4], 0.2f + 0.5f * p.curiosidad, 600);
  if (!dormido && t >= proximaCuriosidad_) {
    proximaCuriosidad_ = t + entre(azar_, 180, 540);
    estado_[4] = minf(1, estado_[4] + 0.25f * p.curiosidad);
    estX_ = entre(azar_, -1, 1), estY_ = entre(azar_, -0.5f, 0.5f);
  }
  for (float& h : habituacion_) h *= exponencial(-dt / 900);

  // Sueño: presión que sube despierto y baja durmiendo, más el ritmo del día.
  if (dormido) presionSueno_ = maxf(0, presionSueno_ - dt / (2.5f * hora));
  else presionSueno_ = minf(1, presionSueno_ + dt / (16 * hora) * (1.3f - 0.6f * p.energia));
  float circ = 0;
  if (m >= 0) {
    const int falta = minutosHasta(m, v.duerme);
    if (enHorarioDormir()) circ = 0.45f;
    else if (falta < 90) circ = 0.35f * (90 - falta) / 90.0f;
    const int desde = minutosHasta(v.despierta, m);
    if (!enHorarioDormir() && desde < 60) circ += 0.15f * (60 - desde) / 60.0f;
    if (m >= 13 * 60 + 30 && m < 15 * 60 + 30) circ += 0.12f;
  }
  circ += caracter::SUENO_DORMIS * dormisPeso_;  // v3: a la hora en que te dormís
  estado_[5] = limitar(presionSueno_ + circ, 0, 1);

  // Estrés: se va en unos 5 min (más lento con mal genio).
  hacia(estado_[6], 0.0f, 300 * (1 + p.malGenio));
}

// ---------------------------------------------------------------- lento

void MotorVida::lento(float dt) {
  const float t = static_cast<float>(t_);
  actualizarV3();
  actualizarEstado(dt);
  ritmo();
  caraEstado_ = caraEstado();
  if (modo_ == Modo::Dormido) vidaDormido();
  else vocesDormido_ = 0;
  // La batería baja, cada tanto, se asoma un momento (sin enchufar y libre).
  if (capaBateria_ && modo_ == Modo::Despierto && ctx_.cargando != 1 && actual_ == -1 && !fija_.existe() &&
      t >= proximaBateria_) {
    if (proximaBateria_ > 0) {
      Config::Cara c;
      c.vida = static_cast<int8_t>(caraEstado_);
      reaccionar(c, ahora_, 2500);
    }
    proximaBateria_ = t + entre(azar_, 60, 120);
  }

  // Ánimo de fondo.
  const Personalidad p = rasgos();
  float w[N_ANIMO] = {};
  w[ACansado] = limitar((estado_[5] - 0.45f) * 1.4f + (0.4f - estado_[0]), 0, 0.6f);
  w[ATriste] = limitar((0.35f - estado_[1]) * 1.6f, 0, 0.5f);
  w[AContento] = limitar((estado_[1] - 0.65f) * 1.2f, 0, 0.35f);
  w[AAburrido] = limitar((estado_[3] - 0.5f) * 1.6f, 0, 0.6f);
  w[AGrunon] = limitar((estado_[6] - 0.5f) * 1.2f * (0.5f + p.malGenio), 0, 0.5f) + (t < grunonHasta_ ? 0.6f : 0);
  w[AMiedo] = limitar((estado_[6] - 0.7f) * 2 * (0.5f + p.timidez), 0, 0.5f);
  w[ACurioso] = limitar((estado_[4] - 0.7f), 0, 0.3f);
  w[AOfendido] = t < ofendidoHasta_ ? 0.7f : 0;
  // Te extraña: si no lo ves en un día o dos, se pone triste (hasta la mitad).
  if (modo_ == Modo::Despierto) w[ATriste] += limitar((variable(VarVida::Ignorado) / 60.0f - 24) / 48, 0, 0.5f);
  float suma = 0;
  for (float x : w) suma += x;
  const float k = suma > 1 ? 1 / suma : 1;
  // Los 6 más fuertes.
  for (int s = 0; s < 6; s++) {
    int mejor = -1;
    for (int i = 0; i < N_ANIMO; i++)
      if (w[i] > 0.001f && (mejor < 0 || w[i] > w[mejor])) mejor = i;
    animoCaras_[s] = static_cast<int8_t>(mejor >= 0 ? g_idxAnimo[mejor] : -1);
    animoPesos_[s] = mejor >= 0 ? w[mejor] * k : 0;
    if (mejor >= 0) w[mejor] = 0;
  }

  // El dueño suele aparecer a esta hora: lo espera.
  esperandoDueno_ = false;
  if (ctx_.minutoDelDia >= 0 && modo_ == Modo::Despierto) {
    int mejor = -1;
    for (int h = 0; h < 24; h++)
      if (mem_.horas[h] >= 3 && (mejor < 0 || mem_.horas[h] > mem_.horas[mejor])) mejor = h;
    if (caracter_.hay && rec_.minLlegada < 1440) {
      // v3: ya sabe a qué minuto llegás.
      const int d = minutosHasta(rec_.minLlegada, ctx_.minutoDelDia);
      esperandoDueno_ = (d <= caracter::ESPERAR_DESPUES || d >= 1440 - caracter::ESPERAR_ANTES) && t - ultimaInteraccion_ > 1800;
      if (esperandoDueno_) anotarHoy(Hoy::Espero, static_cast<uint8_t>(rec_.minLlegada / 60));
    } else if (mejor >= 0) {
      const int d = minutosHasta(mejor * 60, ctx_.minutoDelDia);
      esperandoDueno_ = (d < 30 || d > 1410) && t - ultimaInteraccion_ > 1800;
      if (esperandoDueno_) anotarHoy(Hoy::Espero, static_cast<uint8_t>(mejor));
    }
  }

  // Un hito pendiente se festeja apenas está libre.
  // Creció (v3): festeja (la regla de "crecio") y lo dice.
  if (crecioPendiente_ && modo_ == Modo::Despierto && actual_ == -1 && !fija_.existe()) {
    crecioPendiente_ = false;
    if (!reglaPara(Evento::Crecio)) reglaPara(Evento::Hito);
    decir(Situacion::Crecio);
  }
  if (hitoPendiente_ && modo_ == Modo::Despierto && actual_ == -1 && !fija_.existe()) {
    hitoPendiente_ = false;
    // Las fechas serias (el Día de la Memoria) o las tuyas no se festejan: se dicen.
    if (!hitoSinFestejo_) reglaPara(Evento::Hito);
    hitoSinFestejo_ = false;
    decir(hitoSituacion_ >= 0 ? static_cast<Situacion>(hitoSituacion_) : Situacion::Hito);
    hitoSituacion_ = -1;
  }
  // Elegir qué hacer.
  // Con una cara elegida (fija o de la lista) no hace comportamientos solo.
  if (modo_ == Modo::Despierto && actual_ == -1 && !fija_.existe() && !base_.existe() && t >= proximoOcio_)
    elegirComportamiento();

  // Guardar cada 5 min.
  if (t - guardadoEn_ >= tiempos::GUARDAR_MS / 1000.0f) guardar();
}

// ---------------------------------------------------------------- ritmo diario

void MotorVida::ritmo() {
  const float t = static_cast<float>(t_);
  const float quieto = t - ultimaInteraccion_;
  const bool noche = enHorarioDormir();
  const bool hayHora = ctx_.minutoDelDia >= 0;
  if (modo_ == Modo::Despierto) {
    if (base_.existe() || fija_.existe()) return;  // con una cara elegida no se duerme solo
    const bool ocupado = actual_ != -1 && actual_ != idxOvejas();
    if (forzadoHasta_ > 0 && t < forzadoHasta_) return;
    if (noche && quieto >= 300 && !ocupado) {
      // A dormir: primero cuenta ovejas (una vez por noche).
      if (!ovejasHoy_) {
        const int o = idxOvejas();
        ovejasHoy_ = true;
        if (o >= 0 && comportamiento(o, ahora_)) return;
      }
      if (actual_ == -1) dormir(ahora_);
      return;
    }
    if (!noche && hayHora && quieto >= 1200 && estado_[5] > 0.6f && !ocupado) {
      // Siesta corta (después de comer, si nadie lo toca).
      dormir(ahora_);
      siesta_ = true;
      siestaHasta_ = t + entre(azar_, 900, 1800);
      return;
    }
    if (!hayHora && quieto >= 600 && estado_[5] > 0.8f && !ocupado) dormir(ahora_);
    return;
  }
  // Dormido. v3: si sabe a qué hora llegás, se despierta un poco antes.
  if (caracter_.hay && hayHora && rec_.minLlegada < 1440 && caracter::DESPERTAR_ANTES > 0) {
    const int falta = minutosHasta(ctx_.minutoDelDia, rec_.minLlegada);
    const uint16_t dia = diaVida();
    if (falta > 0 && falta <= caracter::DESPERTAR_ANTES && anticipoDia_ != dia) {
      anticipoDia_ = dia;
      despertar(ahora_, false);
      return;
    }
  }
  if (siesta_ ? t >= siestaHasta_ : (hayHora ? !noche : presionSueno_ < 0.12f)) {
    despertar(ahora_, false);
    return;
  }
  if (forzadoHasta_ > 0 && t < forzadoHasta_) return;
  // Sueños: cada 20 a 40 min, una fase de unos 30 s.
  if (t >= proximoSueno_ && actual_ == -1) {
    proximoSueno_ = t + entre(azar_, 600, 1500);
    // Si hoy lo sacudieron fuerte, más pesadillas (v3).
    const bool sacudido = caracter_.hay && ocurrio(diaVida(), TipoEpisodio::Sacudida);
    const bool pesadilla = estado_[6] > 0.5f || azar_.real() < (sacudido ? 0.6f : 0.2f);
    const int s = config_->vida.indiceComportamiento(pesadilla ? "pesadilla" : "sonar");
    anotarHoy(Hoy::Sueno, pesadilla ? 16 : 0);
    if (s >= 0) comportamiento(s, ahora_);
  }
}

int MotorVida::idxOvejas() const { return config_->vida.indiceComportamiento("contar-ovejas"); }

void MotorVida::dormir(uint32_t) {
  if (modo_ == Modo::Dormido) return;
  modo_ = Modo::Dormido;
  siesta_ = false;
  forzadoHasta_ = -1;
  dormidoDesde_ = static_cast<float>(t_);
  proximoSueno_ = static_cast<float>(t_) + entre(azar_, 600, 1500);
  proximoZzz_ = proximaPosturaSueno_ = proximoGestoSueno_ = proximoRonquido_ = 0;
  if (actual_ != -1) terminarComportamiento();
  anotar(2, 1);
  if (enHorarioDormir()) {
    const float q = (static_cast<float>(t_) - ultimaInteraccion_) / 60.0f;
    if (ctx_.minutoDelDia >= 0 && q < 180 && ultimaInteraccion_ > 0)
      aprender(rec_.minDormis, ((ctx_.minutoDelDia - static_cast<int>(q)) % 1440 + 1440) % 1440);
    escribirDiario(diaVida());
  }
}

void MotorVida::siesta(uint32_t ahora, uint32_t segundos) {
  dormir(ahora);
  if (!enHorarioDormir()) {
    siesta_ = true;
    siestaHasta_ = static_cast<float>(t_) + static_cast<float>(segundos);
  }
}

void MotorVida::despertar(uint32_t ahora, bool forzado) {
  if (modo_ != Modo::Dormido) return;
  modo_ = Modo::Despierto;
  despiertoDesde_ = static_cast<float>(t_);
  siesta_ = false;
  ovejasHoy_ = enHorarioDormir();  // si lo despertaron de noche, no cuenta ovejas de nuevo
  anotar(2, 0);
  ladoSueno_ = 0;
  sonrisaSuenoHasta_ = murmulloHasta_ = -10;
  if (actual_ != -1) terminarComportamiento();
  if (forzado) {
    forzadoHasta_ = static_cast<float>(t_) + tiempos::DESPIERTO_POR_TOQUE_MS / 1000.0f;
    // Gruñón un rato, salvo que lo quiera mucho.
    if (estado_[2] < 0.7f) grunonHasta_ = static_cast<float>(t_) + entre(azar_, 120, 300);
    return;
  }
  forzadoHasta_ = -1;
  const int d = config_->vida.indiceComportamiento("despertar");
  if (d >= 0) comportamiento(d, ahora);
}

// Dormido, que se vea que duerme: zzz que suben (menos mientras sueña),
// cambios de postura, una sonrisa o un murmullo en sueños y un ronquido suave.
// Todo pasa por los resortes (nunca un corte) y los sonidos por el sistema
// (respetan voz.volumen).
void MotorVida::vidaDormido() {
  const float t = static_cast<float>(t_);
  const bool sonando = this->sonando();
  if (proximoZzz_ <= 0 || proximoZzz_ > t + 60) proximoZzz_ = t + entre(azar_, 1.0f, 3.0f);
  if (t >= proximoZzz_) {
    soltar(TipoParticula::Zzz);
    proximoZzz_ = t + (sonando ? entre(azar_, 8.0f, 14.0f) : entre(azar_, 3.5f, 6.5f));
  }
  if (proximaPosturaSueno_ <= 0) proximaPosturaSueno_ = t + entre(azar_, 90, 240);
  if (t >= proximaPosturaSueno_) {
    revolverseEnSuenos(false);
    proximaPosturaSueno_ = t + entre(azar_, 120, 360);
  }
  if (proximoGestoSueno_ <= 0) proximoGestoSueno_ = t + entre(azar_, 120, 400);
  if (t >= proximoGestoSueno_) {
    // Soñando, más seguido; sonríe más si lo quiere.
    if (azar_.real() < 0.4f + 0.4f * estado_[2]) sonrisaSuenoHasta_ = t + entre(azar_, 2.0f, 4.0f);
    else revolverseEnSuenos(true);
    proximoGestoSueno_ = t + (sonando ? entre(azar_, 40, 120) : entre(azar_, 240, 600));
  }
  if (proximoRonquido_ <= 0) proximoRonquido_ = t + entre(azar_, 20, 40);
  if (t >= proximoRonquido_) {
    if (!sonando && salidas_.sonido) salidas_.sonido(salidas_.ctx, "ronquido");
    proximoRonquido_ = t + entre(azar_, 25, 60);
  }
}

// Se mueve en sueños: gira de lado (o se acomoda derecho) o murmura algo.
void MotorVida::revolverseEnSuenos(bool murmurar) {
  const float t = static_cast<float>(t_);
  if (murmurar) {
    murmulloHasta_ = t + entre(azar_, 1.0f, 2.0f);
    if (salidas_.sonido) salidas_.sonido(salidas_.ctx, "murmullo");
    return;
  }
  int8_t nuevo;
  do nuevo = static_cast<int8_t>(static_cast<int>(azar_.entre(0, 2)) - 1);
  while (nuevo == ladoSueno_);
  ladoSueno_ = nuevo;
  aplastar_.v -= 0.6f;  // un acomodo: se aplasta un poco y vuelve
}

bool MotorVida::quiereSuenoProfundo() const {
  return modo_ == Modo::Dormido && !siesta_ && enHorarioDormir() && static_cast<float>(t_) - dormidoDesde_ > 60 &&
         actual_ == -1;
}

int MotorVida::segundosHastaDespertar() const {
  if (ctx_.minutoDelDia < 0) return -1;
  return minutosHasta(ctx_.minutoDelDia, config_->vida.despierta) * 60;
}

bool MotorVida::sonando() const {
  if (actual_ < 0) return false;
  const char* n = config_->vida.comportamientos[actual_].nombre;
  return iguales(n, "sonar") || iguales(n, "pesadilla");
}

// ---------------------------------------------------------------- comportamientos

void MotorVida::elegirComportamiento() {
  const DatosVida& v = config_->vida;
  const Personalidad p = rasgos();
  const float t = static_cast<float>(t_);
  struct Cand {
    int i;
    float puntaje;
  } mejores[3] = {{-1, 0}, {-1, 0}, {-1, 0}};
  // Sin candidatos por un rato, la anti-repetición se afloja a los últimos 2.
  const bool flojo = t - finUltimo_ > 45;
  for (int i = 0; i < v.nComportamientos; i++) {
    const Comportamiento& b = v.comportamientos[i];
    if (b.tipo != TipoComportamiento::Ocioso || t < enfriadoHasta_[i]) continue;
    bool repetido = false;
    for (int h = 0; h < (flojo ? 2 : 5); h++) repetido = repetido || historia_[h] == i;
    if (repetido) continue;
    bool ok = true;
    float necesidad = b.nCuando ? 0 : b.base;
    for (int c = 0; c < b.nCuando && ok; c++) {
      const CondVida& cd = b.cuando[c];
      if (!cumple(cd)) {
        ok = false;
        break;
      }
      const float x = variable(cd.var);
      const bool unidad = static_cast<int>(cd.var) < N_ESTADO + N_RASGOS;
      float e = 1;
      if (cd.op == CondVida::Op::Mayor) e = (x - cd.valor) / (unidad ? maxf(1 - cd.valor, 0.05f) : maxf(cd.valor, 1));
      else if (cd.op == CondVida::Op::Menor) e = (cd.valor - x) / maxf(absf(cd.valor), 0.05f);
      necesidad += (0.5f + 0.5f * limitar(e, 0, 1)) / b.nCuando;
    }
    if (!ok) continue;
    if (b.necesidad >= 0) necesidad = maxf(necesidad, variable(static_cast<VarVida>(b.necesidad)));
    float afinidad = 1;
    for (int r = 0; r < N_RASGOS; r++) afinidad += b.afinidad[r] / 100.0f * (p[r] - 0.5f) * 2;
    afinidad = limitar(afinidad, 0.2f, 2);
    if (i == favoritoIdx_) afinidad *= caracter::FAVORITO;  // su gesto favorito (v3)
    float contexto = 1;
    if (ctx_.bateria >= 0 && ctx_.bateria < 20) contexto *= 0.6f;
    float puntaje = necesidad * afinidad * contexto;
    // Penalización por repetición (cuántas veces está en la historia).
    for (int h = 0; h < 5; h++)
      if (historia_[h] == i) puntaje -= 0.1f;
    puntaje += entre(azar_, -0.12f, 0.12f);
    if (b.probabilidad < 1 && azar_.real() > b.probabilidad) continue;
    if (puntaje <= 0.05f) continue;
    for (int k = 0; k < 3; k++) {
      if (mejores[k].i < 0 || puntaje > mejores[k].puntaje) {
        for (int m = 2; m > k; m--) mejores[m] = mejores[m - 1];
        mejores[k] = {i, puntaje};
        break;
      }
    }
  }
  nCand_ = 0;
  float total = 0;
  for (const Cand& c : mejores) {
    if (c.i < 0) continue;
    candNombres_[nCand_] = v.comportamientos[c.i].nombre;
    candPuntajes_[nCand_++] = c.puntaje;
    total += c.puntaje;
  }
  if (nCand_ == 0) {
    proximoOcio_ = t + 2;
    return;
  }
  // Sorteo ponderado entre los 3 mejores.
  float r = azar_.real() * total;
  int elegido = mejores[0].i;
  for (const Cand& c : mejores) {
    if (c.i < 0) continue;
    if (r < c.puntaje) {
      elegido = c.i;
      break;
    }
    r -= c.puntaje;
  }
  comportamiento(elegido, ahora_);
}

bool MotorVida::comportamiento(int i, uint32_t) {
  const DatosVida& v = config_->vida;
  if (i < 0 || i >= v.nComportamientos) return false;
  const Comportamiento& b = v.comportamientos[i];
  if (b.nPasos == 0) return false;
  // Uno en curso solo se interrumpe si se puede (o si el nuevo es una reacción).
  if (actual_ != -1) {
    const Comportamiento& a = actual_ == -2 ? sintetico_ : v.comportamientos[actual_];
    if (!a.interrumpible && b.tipo == TipoComportamiento::Ocioso) return false;
  }
  // Sus pasos: los de fábrica se leen de la flash recién ahora.
  static Paso nuevos[MAX_PASOS];
  const int n = pasosDe(*config_, b, nuevos);
  if (n == 0) return false;
  if (actual_ != -1) terminarComportamiento();
  for (int k = 0; k < n; k++) enCurso_[k] = nuevos[k];
  nEnCurso_ = static_cast<uint8_t>(n);
  actual_ = i;
  empezados_++;
  if (b.tipo == TipoComportamiento::Ocioso) {
    for (int h = 4; h > 0; h--) historia_[h] = historia_[h - 1];
    historia_[0] = static_cast<int8_t>(i);
  }
  enfriadoHasta_[i] = static_cast<float>(t_) + b.enfriamiento;
  ladoMirada_ = azar_.real() < 0.5f ? -1.0f : 1.0f;
  // Los que hace solo (con "solo" en sus condiciones): al volver, te propone algo.
  for (int c = 0; c < b.nCuando; c++)
    if (b.cuando[c].var == VarVida::Solo) {
      fueSolo_ = true;
      recordar(TipoEpisodio::Solo);
    }
  anotar(1, static_cast<uint8_t>(i));
  empezarPaso(0, true);
  return true;
}

void MotorVida::empezarPaso(int i, bool primera) {
  const Paso& p = enCurso_[i];
  const float t = static_cast<float>(t_);
  pasoActual_ = i;
  if (primera) vecesRestantes_ = static_cast<uint8_t>(azar_.entre(p.vecesMin, p.vecesMax) - 1);
  pasoDesde_ = t;
  pasoHasta_ = t + azar_.entre(p.duraMin, p.duraMax) / 1000.0f;
  pasoCara_ = p.cara >= 0 ? p.cara : config_->vida.indiceCara("neutral");
  pasoPeso_ = p.cara >= 0 ? p.peso() : 0;
  if (actual_ == -2) pasoDibujo_ = sinteticoDibujo_;
  mirada_ = p.mirada;
  parpadeoPaso_ = p.parpadeo;
  proximaSacada_ = t;  // la mirada nueva, ya
  // Si repite, cada repetición cuenta como un cambio (rebota de nuevo).
  if (!primera) capaCara_ = -1;
  switch (p.parpadeo) {
    case Parpadeo::Lento: parpadear(false, 0.25f); break;
    case Parpadeo::Doble: parpadear(true); break;
    case Parpadeo::Rapido:
      parpadear(false, 0.06f);
      rafaga_ = 2;
      proximaRafaga_ = t + 0.2f;
      break;
    default: break;
  }
  if (p.particula >= 0) soltar(static_cast<TipoParticula>(p.particula), p.cuantas);
  if (p.dentro >= 0 && p.particula == static_cast<int8_t>(TipoParticula::Burbuja)) {
    // Lo que sueña, adentro de la burbuja que acaba de salir.
    static const TipoParticula SUENOS[] = {TipoParticula::Corazon, TipoParticula::Estrella, TipoParticula::Nota, TipoParticula::Oveja};
    uint32_t cual = azar_.entre(0, 3);
    TipoParticula que = p.dentro == DENTRO_SUENO ? SUENOS[cual] : static_cast<TipoParticula>(p.dentro);
    // v3: lo que sueña sale de lo que pasó en el día (si pasó algo).
    const int delDia = p.dentro == DENTRO_SUENO && caracter_.hay ? particulaDeSueno() : -1;
    if (delDia >= 0) {
      que = static_cast<TipoParticula>(delDia);
      cual = 4;
      for (uint32_t k = 0; k < 4; k++)
        if (SUENOS[k] == que) cual = k;
    }
    if (p.dentro == DENTRO_SUENO && cual < 4) anotarHoy(Hoy::SonoCon, static_cast<uint8_t>(1u << cual));
    const Particula* b = nullptr;
    for (const Particula& q : cuadro_.particulas)
      if (q.viva && q.tipo == TipoParticula::Burbuja && q.edad == 0) b = &q;
    if (b) {
      const Particula burbuja = *b;
      soltar(que);
      for (Particula& q : cuadro_.particulas)
        if (q.viva && q.tipo == que && q.edad == 0 && !q.enBurbuja) {
          q.x = burbuja.x, q.y = burbuja.y, q.vx = burbuja.vx, q.vy = burbuja.vy, q.vida = burbuja.vida;
          q.tamano = burbuja.tamano * 0.42f, q.enBurbuja = true;
          break;
        }
    }
  }
  if (p.decir >= 0) decir(static_cast<Situacion>(p.decir));
  if (p.sonido[0] && salidas_.sonido) salidas_.sonido(salidas_.ctx, p.sonido);
  if (p.estadoVar >= 0) empujar(p.estadoVar, p.estadoDelta());
}

void MotorVida::avanzarComportamiento() {
  if (actual_ == -1) return;
  const float t = static_cast<float>(t_);
  while (actual_ != -1 && t >= pasoHasta_) {
    if (vecesRestantes_ > 0) {
      vecesRestantes_--;
      empezarPaso(pasoActual_, false);
      pasoDesde_ = pasoHasta_ - (pasoHasta_ - pasoDesde_);
      continue;
    }
    if (pasoActual_ + 1 < nEnCurso_) {
      const float fin = pasoHasta_;
      empezarPaso(pasoActual_ + 1, true);
      // Sin perder tiempo en los saltos grandes.
      const float dura = pasoHasta_ - pasoDesde_;
      pasoDesde_ = fin, pasoHasta_ = fin + dura;
      continue;
    }
    const bool eraOvejas = actual_ == idxOvejas();
    terminarComportamiento();
    if (eraOvejas && enHorarioDormir()) dormir(ahora_);
  }
}

void MotorVida::terminarComportamiento() {
  actual_ = -1;
  pasoCara_ = -1;
  pasoPeso_ = 0;
  pasoDibujo_ = -1;
  mirada_ = Mirada::Libre;
  parpadeoPaso_ = Parpadeo::Normal;
  const float t = static_cast<float>(t_);
  finUltimo_ = t;
  const float energia = 0.5f * (estado_[0] + rasgos().energia);
  proximoOcio_ = t + entre(azar_, 3, 12) * (1.4f - 0.8f * energia);
}

void MotorVida::reaccionar(Config::Cara c, uint32_t ahora, uint32_t duracion) {
  if (!c.existe()) return;
  sintetico_ = Comportamiento();
  copiarTexto(sintetico_.nombre, sizeof sintetico_.nombre, "reaccion");
  sintetico_.tipo = TipoComportamiento::Reaccion;
  sintetico_.nPasos = 1;
  nEnCurso_ = 1;
  Paso& p = enCurso_[0];
  p = Paso();
  p.cara = c.vida;
  p.duraMin = p.duraMax = static_cast<uint16_t>(minf(duracion, 60000));
  sinteticoDibujo_ = c.dibujo;
  if (actual_ != -1) terminarComportamiento();
  actual_ = -2;
  empezados_++;
  empezarPaso(0, true);
  (void)ahora;
}

void MotorVida::reaccionar(int dibujo, uint32_t ahora, uint32_t duracion) {
  if (dibujo < 0 || dibujo >= config_->nExpresiones) return;
  reaccionar(config_->resolverCara(config_->expresiones[dibujo].nombre), ahora, duracion);
}

bool MotorVida::reaccionando(uint32_t) const {
  return actual_ == -2 || (reflejo_ >= 0 && static_cast<float>(t_) < reflejoHasta_);
}

const char* MotorVida::nombreComportamiento() const {
  if (actual_ == -2) return sintetico_.nombre;
  if (actual_ < 0) return "";
  return config_->vida.comportamientos[actual_].nombre;
}

int MotorVida::candidatos(const char** nombres, float* puntajes, int max) const {
  const int n = mini(max, nCand_);
  for (int i = 0; i < n; i++) nombres[i] = candNombres_[i], puntajes[i] = candPuntajes_[i];
  return n;
}

// ---------------------------------------------------------------- fijar y base

bool MotorVida::fijar(const char* nombre) {
  if (!nombre || !nombre[0]) {
    fija_ = Config::Cara();
    return true;
  }
  const Config::Cara c = config_->resolverCara(nombre);
  if (!c.existe()) return false;
  fija_ = c;
  return true;
}

void MotorVida::fijar(int expresion) {
  if (expresion < 0 || expresion >= config_->nExpresiones) {
    fija_ = Config::Cara();
    return;
  }
  fija_ = config_->resolverCara(config_->expresiones[expresion].nombre);
}

void MotorVida::base(int dibujo) {
  if (dibujo < 0 || dibujo >= config_->nExpresiones) {
    base_ = Config::Cara();
    return;
  }
  base_ = config_->resolverCara(config_->expresiones[dibujo].nombre);
}

const char* MotorVida::nombreVisible() const {
  const DatosVida& v = config_->vida;
  if (dibujo_ >= 0 && opDibujo_ >= 0.5f) return config_->expresiones[dibujo_].nombre;
  const float t = static_cast<float>(t_);
  if (fija_.existe()) return fija_.vida >= 0 ? v.caras[fija_.vida].nombre : config_->expresiones[fija_.dibujo].nombre;
  // Lo que se ve es la cara de imagen (las paramétricas solo la mueven).
  const int imagen = caraImagen();
  if (imagen >= 0) return v.caras[imagen].nombre;
  if (reflejo_ >= 0 && t < reflejoHasta_) return v.caras[reflejo_].nombre;
  if (actual_ != -1 && pasoCara_ >= 0 && pasoPeso_ >= 0.5f) return v.caras[pasoCara_].nombre;
  if (modo_ == Modo::Dormido) return "dormido";
  if (caraEstado_ >= 0 && caraEstado_ < 1000 && !capaBateria_) return v.caras[caraEstado_].nombre;
  if (base_.vida >= 0) return v.caras[base_.vida].nombre;
  if (v.reposo >= 0) return v.caras[v.reposo].nombre;
  if (animoCaras_[0] >= 0 && animoPesos_[0] >= 0.3f) return v.caras[animoCaras_[0]].nombre;
  return "neutral";
}

int MotorVida::expresionVisible() const {
  if (dibujo_ >= 0 && opDibujo_ >= 0.5f) return dibujo_;
  const int i = config_->indiceExpresion(nombreVisible());
  if (i >= 0) return i;
  const int n = config_->indiceExpresion("neutral");
  return n >= 0 ? n : 0;
}

// ---------------------------------------------------------------- eventos y reglas

void MotorVida::seguir(float x, float y, uint32_t) {
  seguirX_ = limitar(x, -1, 1), seguirY_ = limitar(y, -1, 1);
  seguirHasta_ = static_cast<float>(t_) + 2.5f;
}

void MotorVida::interaccion(uint32_t ahora) {
  if (modo_ == Modo::Dormido) despertar(ahora, true);
  memoriaInteraccion(Evento::Tacto);
}

bool MotorVida::reglaPara(Evento e) {
  for (int i = 0; i < config_->nReglas; i++) {
    const Regla& r = config_->reglas[i];
    if (r.si.tipo != Condicion::Tipo::Evento || r.si.evento != e) continue;
    bool ok = true;
    for (int k = 0; k < r.nY && ok; k++) ok = cumple(r.y[k]);
    if (!ok) continue;
    aplicarRegla(r);
    return true;
  }
  return false;
}

void MotorVida::evento(Evento e, uint32_t ahora) {
  if (static_cast<int>(e) >= static_cast<int>(Evento::Cantidad)) return;
  ahora_ = ahora;
  const float t = static_cast<float>(t_);
  if (modo_ == Modo::Dormido) {
    // Tocarlo mientras duerme (o algo fuerte) es despertarlo.
    if (esToque(e) || e == Evento::Despertado || e == Evento::SacudidaFuerte || e == Evento::Caida) {
      e = Evento::Despertado;
      despertar(ahora, true);
    } else if (e == Evento::Nombre) {
      // Su nombre lo despierta, pero suave (sin el mal humor de que lo despierten).
      anotar(0, static_cast<uint8_t>(e));
      despertar(ahora, false);
      return;
    } else if (e == Evento::Voz || e == Evento::Ruido || e == Evento::Musica || e == Evento::Silencio) {
      // De noche, con un micrófono, esto pasa todo el tiempo: casi nunca lo
      // despierta (y cada vez menos); si no, se mueve o murmura en sueños.
      const float p = e == Evento::Voz ? 0.06f : e == Evento::Ruido ? 0.03f : 0.0f;
      const bool despierta = azar_.real() < p / (1 + 0.5f * vocesDormido_);
      if (vocesDormido_ < 250) vocesDormido_++;
      anotar(0, static_cast<uint8_t>(e));
      if (despierta) {
        evento(Evento::Despertado, ahora);
        return;
      }
      if (e != Evento::Silencio && azar_.real() < 0.5f) revolverseEnSuenos(azar_.real() < 0.5f);
      return;
    }
  }
  anotar(0, static_cast<uint8_t>(e));
  vocalizar(nombreEvento(e), false);
  const float animoAntes = estado_[1];
  // Percepción: lo nuevo da curiosidad; lo repetido, cada vez menos.
  const int ie = static_cast<int>(e);
  estado_[4] = minf(1, estado_[4] + 0.08f / (1 + habituacion_[ie]));
  habituacion_[ie] += 1;
  if (esInteraccion(e)) {
    // Lo vieron: se le pasa el aburrimiento.
    estado_[3] *= 0.2f;
    estado_[0] = maxf(0, estado_[0] - 0.005f);
    memoriaInteraccion(e);
    // Lo que se dice con vos según el momento del día (antes que las reglas:
    // el globo es de quien lo pide primero).
    rutinaConVos(hayVolvio_);
  }
  if (e == Evento::SacudidaFuerte || e == Evento::Caida) {
    if (ctx_.epoch) mem_.rencorHasta = ctx_.epoch + 86400;
    mem_.sacudidas++;
    if (e == Evento::SacudidaFuerte) recordar(TipoEpisodio::Sacudida);
    mem_.deriva[3] = static_cast<int8_t>(mini(10, mem_.deriva[3] + (azar_.real() < 0.2f ? 1 : 0)));
  }
  if (!reglaPara(e)) {
    const Evento padre = eventoPadre(e);
    if (padre != Evento::Cantidad) reglaPara(padre);
  }
  // Gustos: qué le subió el ánimo.
  for (int g = 0; g < 8; g++)
    if (GUSTOS[g] == e) {
      const int d = static_cast<int>((estado_[1] - animoAntes) * 100);
      mem_.gustos[g] = static_cast<int8_t>(limitar(mem_.gustos[g] + d, -100, 100));
    }
  // Volvió después de una ausencia: el saludo (u ofenderse) va después.
  if (hayVolvio_) {
    hayVolvio_ = false;
    anotar(0, static_cast<uint8_t>(Evento::Volvio));
    reglaPara(Evento::Volvio);
    if (ausenciaHoras_ >= 72) ofendidoHasta_ = t + 60;
  }
  revisarHitos();
}

void MotorVida::aplicarRegla(const Regla& r) {
  const float t = static_cast<float>(t_);
  for (int a = 0; a < r.nAcciones; a++) {
    const Accion& acc = r.entonces[a];
    switch (acc.tipo) {
      case Accion::Tipo::Expresion:
        reaccionar(config_->resolverCara(acc.argumento), ahora_, tiempos::REACCION_MS);
        break;
      case Accion::Tipo::Reflejo: {
        const Config::Cara c = config_->resolverCara(acc.argumento);
        if (c.vida >= 0) {
          reflejo_ = c.vida;
          reflejoHasta_ = t + tiempos::REFLEJO_MS / 1000.0f;
          // Responde ya: los objetivos cambian en este mismo tick.
          calcularObjetivos(DT);
        } else if (c.dibujo >= 0) {
          reaccionar(c, ahora_, tiempos::REFLEJO_MS);
        }
        break;
      }
      case Accion::Tipo::Comportamiento:
        comportamiento(acc.indice, ahora_);
        break;
      case Accion::Tipo::Estado:
        empujar(acc.variable, acc.delta());
        break;
      case Accion::Tipo::Particula:
        soltar(static_cast<TipoParticula>(acc.indice));
        break;
      case Accion::Tipo::Sonido:
        if (salidas_.sonido) salidas_.sonido(salidas_.ctx, acc.argumento);
        break;
      case Accion::Tipo::Decir:
        if (acc.indice >= 0) decir(static_cast<Situacion>(acc.indice));
        break;
      case Accion::Tipo::Animacion:
        if (salidas_.animacion) salidas_.animacion(salidas_.ctx, config_->indiceAnimacion(acc.argumento));
        break;
      case Accion::Tipo::Led: {
        uint32_t rgb;
        bool acento;
        if (salidas_.led && interpretarColor(acc.argumento, &rgb, &acento)) salidas_.led(salidas_.ctx, rgb);
        break;
      }
    }
  }
}

// ---------------------------------------------------------------- memoria y relación

void MotorVida::memoriaInteraccion(Evento e) {
  const float t = static_cast<float>(t_);
  // Cuánto tiempo estuvo sin verte.
  float ausencia = (t - ultimaInteraccion_) / 3600.0f;
  if (ctx_.epoch && mem_.ultimaInteraccion && ctx_.epoch > mem_.ultimaInteraccion)
    ausencia = maxf(ausencia, (ctx_.epoch - mem_.ultimaInteraccion) / 3600.0f);
  if (ausencia * 3600 >= tiempos::AUSENCIA_SALUDO_S && (mem_.ultimaInteraccion || t > 0)) {
    hayVolvio_ = true;
    ausenciaHoras_ = ausencia;
    if (!enHorarioDormir()) aprender(rec_.minLlegada, ctx_.minutoDelDia);
    if (ausencia >= 24) recordar(TipoEpisodio::Volviste, static_cast<uint8_t>(minf(255, ausencia / 24)));
    mem_.ausenciaMaxima = static_cast<uint16_t>(minf(65535, maxf(mem_.ausenciaMaxima, ausencia)));
  }
  // Aprende a qué hora suele aparecer (la primera interacción después de 1 h).
  if (ausencia >= 1 && ctx_.minutoDelDia >= 0) {
    uint8_t& h = mem_.horas[ctx_.minutoDelDia / 60];
    if (h >= 250)
      for (uint8_t& x : mem_.horas) x /= 2;
    h++;
  }
  ultimaInteraccion_ = t;
  ausente_ = false;
  if (e == Evento::Caricia) {
    mem_.caricias++;
    const uint16_t d = diaVida();
    if (d && rec_.diaCaricias != d) rec_.diaCaricias = d, rec_.hoyCaricias = 0;
    if (rec_.hoyCaricias < 255 && ++rec_.hoyCaricias == caracter::CARICIAS_DIA) recordar(TipoEpisodio::Caricias, rec_.hoyCaricias);
  }
  else if (esToque(e)) mem_.toques++;
  // Afecto: la atención repetida a lo largo de los días (con tope diario).
  if (ctx_.epoch) {
    const uint32_t dia = ctx_.epoch / 86400;
    if (!mem_.nacimiento) mem_.nacimiento = ctx_.epoch;
    if (dia != mem_.ultimoDia) {
      primeroDelDia_ = true;
      mem_.racha = (mem_.ultimoDia && dia == mem_.ultimoDia + 1) ? mem_.racha + 1 : 1;
      mem_.rachaMax = maxi(mem_.rachaMax, mem_.racha);
      mem_.dias++;
      mem_.ultimoDia = dia;
      mem_.afectoHoy = 0;
      // Si ayer no durmió de noche (con una cara fija, por ejemplo), el diario de ayer.
      const uint16_t dv = diaVida();
      if (dv > 1 && rec_.diaDiario < dv - 1 && hayEpisodio(static_cast<uint16_t>(dv - 1))) escribirDiario(static_cast<uint16_t>(dv - 1));
    }
    mem_.ultimaInteraccion = ctx_.epoch;
  }
  if (mem_.afectoHoy < 40) {
    const float suma = e == Evento::Caricia ? 0.004f : 0.002f;
    estado_[2] = minf(1, estado_[2] + suma);
    mem_.afectoHoy = static_cast<uint8_t>(mem_.afectoHoy + (e == Evento::Caricia ? 4 : 2));
  }
  // La personalidad deriva según cómo lo tratan (±10 % como mucho).
  if (e == Evento::Caricia && azar_.real() < 0.05f) {
    mem_.deriva[1] = static_cast<int8_t>(mini(10, mem_.deriva[1] + 1));
    mem_.deriva[5] = static_cast<int8_t>(maxi(-10, mem_.deriva[5] - 1));
  }
}

// ---------------------------------------------------------------- el día con vos (v1.7)

void MotorVida::anotarHoy(Hoy que, uint8_t dato) {
  const uint8_t hoy = ctx_.epoch ? static_cast<uint8_t>((ctx_.epoch / 86400u) % 251u + 1u) : 0;
  if (hoy && hoy != mem_.hoyDia) {
    mem_.hoyDia = hoy;
    mem_.hoyJuegos = mem_.hoySuenos = mem_.hoySonoCon = mem_.hoyAnimo = mem_.hoyEspero = mem_.hoyAgua = 0;
  }
  auto mas = [](uint8_t& x) { if (x < 255) x++; };
  switch (que) {
    case Hoy::Juego:
      mas(mem_.hoyJuegos);
      if (mem_.juegosJugados < 65535) mem_.juegosJugados++;
      break;
    case Hoy::JuegoGanado:
      if (mem_.juegosGanados < 65535) mem_.juegosGanados++;
      break;
    case Hoy::Sueno:
      mas(mem_.hoySuenos);
      mem_.hoySonoCon |= dato;
      break;
    case Hoy::SonoCon: mem_.hoySonoCon |= dato; break;
    case Hoy::Animo:
      mem_.hoyAnimo = dato;
      anotarAnimo(dato);
      break;
    case Hoy::Espero:
      if (!mem_.hoyEspero) mem_.hoyEspero = static_cast<uint8_t>(dato + 1);
      break;
    case Hoy::Agua: mas(mem_.hoyAgua); break;
  }
}

void MotorVida::anotarReflejo(uint16_t ms) {
  if (ms && (!mem_.mejorReflejo || ms < mem_.mejorReflejo)) mem_.mejorReflejo = ms;
}

// Lo que te dice según el momento: el "buen día" del primer encuentro (con el
// clima, si hay), "¿comiste?" a la tarde, "¡tardaste!" si llegás mucho más
// tarde que lo de siempre y, antes de su hora de dormir, bosteza y te invita a
// dormir. Las frases son datos (frases.json); el Hablador decide si las dice.
void MotorVida::rutinaConVos(bool volvio) {
  const bool primero = primeroDelDia_;
  primeroDelDia_ = false;
  if (primero) primerContacto_ = static_cast<float>(t_);  // v3: el recuerdo de ayer va un rato después
  if (modo_ != Modo::Despierto) return;
  const int m = ctx_.minutoDelDia;
  if (primero) {
    // El buen día, de la mañana hasta las 14 (de noche, no).
    if (m < 0 || (minutosHasta(config_->vida.despierta, m) < 6 * 60 + 60 && m < 14 * 60) ||
        minutosHasta(m, config_->vida.despierta) <= 120) {
      decir(Situacion::BuenDia);
      return;
    }
  }
  if (volvio && m >= 0) {
    // ¿A qué hora suele aparecer? Si llegó entre 1,5 y 6 h más tarde, lo dice.
    int mejor = -1;
    for (int h = 0; h < 24; h++)
      if (mem_.horas[h] >= 3 && (mejor < 0 || mem_.horas[h] > mem_.horas[mejor])) mejor = h;
    const int tarde = mejor >= 0 ? minutosHasta(mejor * 60, m) : -1;
    if (tarde >= 90 && tarde <= 360) {
      recordar(TipoEpisodio::Tarde, static_cast<uint8_t>(minf(255, tarde / 10)));
      decir(Situacion::Tardaste);
      return;
    }
  }
  // v3: estuvo jugando solo y volviste: te propone algo (lo que más jugás, tus gustos).
  if (fueSolo_ && caracter_.hay) {
    fueSolo_ = false;
    decir(Situacion::Propuesta);
    return;
  }
  // v3: un rato después del primer encuentro del día, se acuerda de ayer.
  const float tt = static_cast<float>(t_);
  const uint16_t hoy = diaVida();
  if (caracter_.hay && !primero && hoy && recuerdoDia_ != hoy && tt - primerContacto_ >= 1200) {
    // El "¿hoy también vas a posponer?" lo dice el despertador.
    if (masImportante(static_cast<uint16_t>(hoy - 1), &recuerdo_, 1u << static_cast<int>(TipoEpisodio::Posponer))) {
      recuerdoDia_ = hoy;
      decir(Situacion::Recuerdo);
      return;
    }
  }
  // Con empatía (estuviste bajoneado estos días): más mimos.
  if (caracter_.hay && empatico() && !primero) decir(Situacion::Mimos);
  if (m < 0) return;
  if (m >= 13 * 60 && m <= 15 * 60 + 30) decir(Situacion::Comiste);
  const int falta = minutosHasta(m, config_->vida.duerme);
  const uint32_t dia = ctx_.epoch / 86400u + 1u;
  if (falta > 0 && falta <= 45 && aDormirDia_ != dia) {
    aDormirDia_ = dia;
    const int b = config_->vida.indiceComportamiento("bostezar");
    if (b >= 0) comportamiento(b, ahora_);
    decir(Situacion::ADormir);
  }
}

void MotorVida::revisarHitos() {
  uint16_t nuevos = 0;
  auto hito = [&](int bit, bool cond) {
    if (cond && !(mem_.hitos & (1u << bit))) mem_.hitos |= static_cast<uint16_t>(1u << bit), nuevos++;
  };
  hito(0, mem_.dias >= 1);
  hito(1, mem_.dias >= 7);
  hito(2, mem_.dias >= 30);
  hito(3, mem_.caricias >= 100);
  hito(4, mem_.dias >= 100);
  hito(5, mem_.caricias >= 1000);
  // Cumpleaños (el del bicho: el día que lo armaron; el del dueño).
  if (ctx_.epoch) {
    // Fecha civil a partir del día (algoritmo de Howard Hinnant).
    const int32_t z = static_cast<int32_t>(ctx_.epoch / 86400) + 719468;
    const int32_t era = z / 146097, doe = z - era * 146097;
    const int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int32_t mp = (5 * doy + 2) / 153;
    const int32_t dia = doy - (153 * mp + 2) / 5 + 1;
    const int32_t mes = mp < 10 ? mp + 3 : mp - 9;
    const int32_t anio = yoe + era * 400 + (mes <= 2 ? 1 : 0);
    const int16_t hoy = static_cast<int16_t>(mes * 100 + dia);
    if (config_->vida.cumpleBicho == hoy && mem_.anioCumpleBicho != anio) {
      mem_.anioCumpleBicho = static_cast<uint16_t>(anio), nuevos++;
      hitoSituacion_ = static_cast<int8_t>(Situacion::CumpleRobot);
    }
    // Las fechas (una vez por año): año nuevo, Navidad, el día del amigo, el
    // del niño (en la Argentina, el tercer domingo de agosto) y el aniversario
    // (el día que se conocieron).
    if (mem_.anioFechas != static_cast<uint8_t>(anio)) mem_.anioFechas = static_cast<uint8_t>(anio), mem_.fechas = 0;
    const int semana = static_cast<int>((ctx_.epoch / 86400 + 3) % 7);  // 0 = lunes
    int aniversario = -1;
    if (mem_.nacimiento && ctx_.epoch / 86400 > mem_.nacimiento / 86400 + 300) {
      const int32_t zn = static_cast<int32_t>(mem_.nacimiento / 86400) + 719468;
      const int32_t en = zn / 146097, dn = zn - en * 146097;
      const int32_t yn = (dn - dn / 1460 + dn / 36524 - dn / 146096) / 365;
      const int32_t dy = dn - (365 * yn + yn / 4 - yn / 100);
      const int32_t mpn = (5 * dy + 2) / 153;
      aniversario = (mpn < 10 ? mpn + 3 : mpn - 9) * 100 + (dy - (153 * mpn + 2) / 5 + 1);
    }
    const struct {
      bool es;
      Situacion s;
    } fechas[] = {{hoy == 101, Situacion::AnioNuevo},
                  {hoy == 1225, Situacion::Navidad},
                  {hoy == 720, Situacion::DiaDelAmigo},
                  {mes == 8 && semana == 6 && dia >= 15 && dia <= 21, Situacion::DiaDelNino},
                  {hoy == aniversario, Situacion::Aniversario}};
    for (int f = 0; f < 5; f++)
      if (fechas[f].es && !(mem_.fechas & (1u << f))) {
        mem_.fechas = static_cast<uint8_t>(mem_.fechas | (1u << f)), nuevos++;
        hitoSituacion_ = static_cast<int8_t>(fechas[f].s);
      }
    nuevos = static_cast<uint16_t>(nuevos + fechasV3(static_cast<int32_t>(ctx_.epoch / 86400)));
    if (config_->vida.cumpleDueno == hoy && mem_.anioCumpleDueno != anio) {
      mem_.anioCumpleDueno = static_cast<uint16_t>(anio), nuevos++;
      hitoSituacion_ = static_cast<int8_t>(Situacion::CumpleDueno);  // el del dueño gana
      hitoSinFestejo_ = false;
    }
  }
  if (nuevos) {
    anotar(3, static_cast<uint8_t>(mem_.hitos & 0xFF));
    // Se festeja cuando esté libre (no le pisa la reacción a lo que pasó).
    hitoPendiente_ = true;
    guardar();
  }
}

void MotorVida::guardar() {
  guardadoEn_ = static_cast<float>(t_);
  for (int i = 0; i < N_ESTADO; i++) mem_.estado[i] = static_cast<uint8_t>(limitar(estado_[i], 0, 1) * 255 + 0.5f);
  mem_.epochEstado = ctx_.epoch;
  if (!salidas_.guardar) return;
  uint8_t b[LARGO_MEMORIA];
  const size_t n = codificarMemoria(mem_, b, sizeof b);
  salidas_.guardar(salidas_.ctx, b, n);
  guardarRecuerdos();
}

void MotorVida::vocalizar(const char* clave, bool cara) {
  if (!salidas_.sonido || !clave) return;
  bool hay = false;
  if (cara) {
    for (const VozBase& v : caracter::VOZ_CARAS) hay = hay || iguales(v.clave, clave);
  } else {
    for (const VozBase& v : caracter::VOZ_EVENTOS) hay = hay || iguales(v.clave, clave);
  }
  if (!hay) return;
  char n[LARGO_VIDA + 8];
  const size_t k = copiarTexto(n, sizeof n, "voz:");
  copiarTexto(n + k, sizeof n - k, clave);
  salidas_.sonido(salidas_.ctx, n);
}

void MotorVida::anotar(uint8_t tipo, uint8_t dato) {
  EntradaRegistro& e = registro_[(iniRegistro_ + nRegistro_) % MAX_REGISTRO];
  e.segundos = static_cast<uint32_t>(t_);
  e.tipo = tipo;
  e.dato = dato;
  if (nRegistro_ < MAX_REGISTRO) nRegistro_++;
  else iniRegistro_ = static_cast<uint8_t>((iniRegistro_ + 1) % MAX_REGISTRO);
}

int MotorVida::registro(EntradaRegistro* salida, int max) const {
  const int n = mini(max, nRegistro_);
  for (int i = 0; i < n; i++) salida[i] = registro_[(iniRegistro_ + nRegistro_ - n + i) % MAX_REGISTRO];
  return n;
}

}  // namespace sintetica
