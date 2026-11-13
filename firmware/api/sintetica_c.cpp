#include "sintetica_c.h"

#include <stddef.h>
#include <stdlib.h>

#include "rostro.h"
#include "sistema.h"
#include "sprites.h"
#include "vida_base.h"

using namespace sintetica;

static_assert(sizeof(sintetica_manifiesto) == sizeof(ManifiestoOta), "el manifiesto de C y el del núcleo son el mismo");
static_assert(offsetof(sintetica_manifiesto, perfil) == offsetof(ManifiestoOta, perfil), "mismo orden");

namespace {
sintetica_hal g_hal;
char g_visible[LARGO_NOMBRE + 12];
Sistema g_sistema;
uint16_t g_pixeles[640 * 640];
int g_ancho = 0, g_alto = 0;

// Las imágenes de la vista previa, en memoria ("img0" a "img7" y los sprites,
// "spr0" a "spr11"). Con malloc: al doble de la pantalla una foto puede pasar
// el megabyte (las apps tienen lugar).
constexpr int N_ARCHIVOS = PRIMER_SPRITE + MAX_SPRITES;
uint8_t* g_imagenes[N_ARCHIVOS];
uint32_t g_capacidad[N_ARCHIVOS];
uint32_t g_largos[N_ARCHIVOS];

// Los sprites sueltos (para dibujar una expresión sin la instancia) y su caché.
uint8_t* g_sueltos[MAX_SPRITES];
uint32_t g_largoSuelto[MAX_SPRITES];
CacheSprites g_cacheSuelta;

// Si el anfitrión no guarda nada (una vista previa), se guarda en memoria: así
// pedir la config devuelve la última que llegó.
struct Guardado {
  char clave[16];
  uint8_t* datos;
  size_t n;
};
// Las claves fijas (config, ajustes, serie, codigo, claves, imagenes, sprites, red,
// nube, vida), las 8 imágenes y los 12 sprites, con margen.
Guardado g_memoria[40];

// Una config suelta, fuera de la instancia (grande: fuera de la pila): para
// validar y para dibujar una expresión sin tocar el bicho.
Config g_suelta;

Guardado* buscarGuardado(const char* clave, bool crear) {
  for (auto& g : g_memoria)
    if (g.clave[0] && iguales(g.clave, clave)) return &g;
  if (!crear) return nullptr;
  for (auto& g : g_memoria) {
    if (!g.clave[0]) {
      copiarTexto(g.clave, sizeof g.clave, clave);
      return &g;
    }
  }
  return nullptr;
}

int espacioDe(const char* clave) {
  if (clave[0] == 'i' && clave[1] == 'm' && clave[2] == 'g' && clave[3] >= '0' && clave[3] < '0' + MAX_IMAGENES && !clave[4])
    return clave[3] - '0';
  if (clave[0] == 's' && clave[1] == 'p' && clave[2] == 'r' && clave[3] >= '0' && clave[3] <= '9') {
    int n = clave[3] - '0';
    if (clave[4] >= '0' && clave[4] <= '9' && !clave[5] && n == 1) n = 10 + clave[4] - '0';
    else if (clave[4]) return -1;
    return n < MAX_SPRITES ? PRIMER_SPRITE + n : -1;
  }
  return -1;
}

int leerSuelto(void*, int sprite, uint32_t off, uint8_t* d, size_t n) {
  if (sprite < 0 || sprite >= MAX_SPRITES || !g_largoSuelto[sprite]) return -1;
  if (off >= g_largoSuelto[sprite]) return 0;
  const uint32_t m = mini(static_cast<int>(n), static_cast<int>(g_largoSuelto[sprite] - off));
  copiarBytes(d, g_sueltos[sprite] + off, m);
  return static_cast<int>(m);
}
}  // namespace

extern "C" {

void sintetica_iniciar(const sintetica_hal* hal, int ancho, int alto) {
  sintetica_iniciar_perfil(hal, ancho, alto, 0, 0);
}

void sintetica_iniciar_perfil(const sintetica_hal* hal, int ancho, int alto, int redonda, int botonBroche) {
  g_hal = hal ? *hal : sintetica_hal{};
  g_ancho = mini(maxi(ancho, 16), MAX_ANCHO);
  g_alto = mini(maxi(alto, 16), MAX_ALTO);
  Hal h;
  h.ahoraMs = [] { return g_hal.ahora_ms ? g_hal.ahora_ms(g_hal.contexto) : 0u; };
  h.identificador = [] { return g_hal.identificador; };
  h.semilla = [] { return g_hal.semilla ? g_hal.semilla(g_hal.contexto) : 0x5157E71Cu; };
  h.minutoDelDia = [] { return g_hal.minuto_del_dia ? g_hal.minuto_del_dia(g_hal.contexto) : -1; };
  h.bateria = [] { return g_hal.bateria ? g_hal.bateria(g_hal.contexto) : -1; };
  if (g_hal.epoch) h.epoch = [] { return g_hal.epoch(g_hal.contexto); };
  if (g_hal.sonido) h.sonido = [](const char* nombre) { g_hal.sonido(g_hal.contexto, nombre); };
  if (g_hal.cargando) h.cargando = [] { return g_hal.cargando(g_hal.contexto); };
  if (g_hal.audio) h.audio = [](const int16_t* m, size_t n) { g_hal.audio(g_hal.contexto, m, n); };
  h.guardar = [](const char* c, const uint8_t* d, size_t n) {
    if (g_hal.guardar) return g_hal.guardar(g_hal.contexto, c, d, n) != 0;
    Guardado* g = buscarGuardado(c, true);
    if (!g) return false;
    auto* p = static_cast<uint8_t*>(realloc(g->datos, n ? n : 1));
    if (!p) return false;
    copiarBytes(p, d, n);
    g->datos = p, g->n = n;
    return true;
  };
  h.leer = [](const char* c, uint8_t* d, size_t cap) {
    if (g_hal.leer) return g_hal.leer(g_hal.contexto, c, d, cap);
    const Guardado* g = buscarGuardado(c, false);
    if (!g || g->n > cap) return -1;
    copiarBytes(d, g->datos, g->n);
    return static_cast<int>(g->n);
  };
  h.notificar = [](uint8_t car, const uint8_t* d, size_t n) {
    if (g_hal.notificar) g_hal.notificar(g_hal.contexto, car, d, n);
  };
  h.registrar = [](const char* m) {
    if (g_hal.registrar) g_hal.registrar(g_hal.contexto, m);
  };
  h.notificarNube = [](uint8_t car, const uint8_t* d, size_t n) {
    if (g_hal.notificar_nube) g_hal.notificar_nube(g_hal.contexto, car, d, n);
  };
  h.redCambio = [] {
    if (g_hal.red_cambio) g_hal.red_cambio(g_hal.contexto);
  };
  if (g_hal.ota_empezar)
    h.otaEmpezar = [](const ManifiestoOta& m) {
      return static_cast<uint8_t>(g_hal.ota_empezar(g_hal.contexto, reinterpret_cast<const sintetica_manifiesto*>(&m)));
    };
  if (g_hal.ota_escribir)
    h.otaEscribir = [](uint32_t off, const uint8_t* d, size_t n) {
      return static_cast<uint8_t>(g_hal.ota_escribir(g_hal.contexto, off, d, n));
    };
  if (g_hal.ota_terminar) h.otaTerminar = [] { return static_cast<uint8_t>(g_hal.ota_terminar(g_hal.contexto)); };
  if (g_hal.ota_descargar)
    h.otaDescargar = [](const ManifiestoOta& m, const char* url) {
      return static_cast<uint8_t>(
          g_hal.ota_descargar(g_hal.contexto, reinterpret_cast<const sintetica_manifiesto*>(&m), url));
    };
  h.otaCancelar = [] {
    if (g_hal.ota_cancelar) g_hal.ota_cancelar(g_hal.contexto);
  };
  h.escribirTrozo = [](const char* c, uint32_t off, const uint8_t* d, size_t n) {
    if (g_hal.escribir_trozo) return g_hal.escribir_trozo(g_hal.contexto, c, off, d, n) != 0;
    const int e = espacioDe(c);
    if (e < 0 || (off != 0 && off != g_largos[e])) return false;
    if (off + n > g_capacidad[e]) {
      const uint32_t nueva = static_cast<uint32_t>(off + n) * 2 + 4096;
      auto* p = static_cast<uint8_t*>(realloc(g_imagenes[e], nueva));
      if (!p) return false;
      g_imagenes[e] = p;
      g_capacidad[e] = nueva;
    }
    copiarBytes(g_imagenes[e] + off, d, n);
    g_largos[e] = off + static_cast<uint32_t>(n);
    return true;
  };
  h.leerTrozo = [](const char* c, uint32_t off, uint8_t* d, size_t n) {
    if (g_hal.leer_trozo) return g_hal.leer_trozo(g_hal.contexto, c, off, d, n);
    const int e = espacioDe(c);
    if (e < 0 || !g_largos[e]) return -1;
    if (off >= g_largos[e]) return 0;
    const uint32_t m = mini(static_cast<int>(n), static_cast<int>(g_largos[e] - off));
    copiarBytes(d, g_imagenes[e] + off, m);
    return static_cast<int>(m);
  };
  h.borrar = [](const char* c) {
    if (g_hal.borrar) return g_hal.borrar(g_hal.contexto, c) != 0;
    const int e = espacioDe(c);
    if (e >= 0) g_largos[e] = 0;
    return true;
  };
  Perfil p;
  p.ancho = g_ancho, p.alto = g_alto, p.redonda = redonda != 0;
  p.boton = botonBroche ? Perfil::Boton::Broche : Perfil::Boton::Mascota;
  if (g_hal.perfil) p.nombre = g_hal.perfil;
  if (g_hal.version) p.version = g_hal.version;
  g_sistema.iniciar(h, g_pixeles, p);
}

void sintetica_vista_previa(void) { g_sistema.modoVistaPrevia(); }
void sintetica_tick(void) { g_sistema.tick(); }
const uint16_t* sintetica_pixeles(void) { return g_pixeles; }
int sintetica_ancho(void) { return g_ancho; }
int sintetica_alto(void) { return g_alto; }

void sintetica_pixeles_rgba(uint8_t* s) {
  for (int i = 0; i < g_ancho * g_alto; i++) {
    uint32_t c = de565(g_pixeles[i]);
    s[i * 4] = static_cast<uint8_t>(c >> 16);
    s[i * 4 + 1] = static_cast<uint8_t>(c >> 8);
    s[i * 4 + 2] = static_cast<uint8_t>(c);
    s[i * 4 + 3] = 255;
  }
}

void sintetica_tacto(int apretado) { g_sistema.tacto(apretado != 0); }
void sintetica_imu(float ax, float ay, float az) { g_sistema.imu(ax, ay, az); }
void sintetica_evento(int e) {
  if (e >= 0 && e < static_cast<int>(Evento::Cantidad)) g_sistema.evento(static_cast<Evento>(e));
}

void sintetica_ble_conexion(int conectado) { g_sistema.bleConexion(conectado != 0); }
void sintetica_ble_escribir(int car, const uint8_t* d, size_t n) { g_sistema.bleEscribir(static_cast<uint8_t>(car), d, n); }
size_t sintetica_ble_estado(uint8_t* s, size_t cap) { return g_sistema.estadoBle(s, cap); }

int sintetica_cantidad_expresiones(void) { return g_sistema.config().nExpresiones; }
const char* sintetica_nombre_expresion(int i) {
  return (i >= 0 && i < g_sistema.config().nExpresiones) ? g_sistema.config().expresiones[i].nombre : "";
}
const char* sintetica_expresion_visible(void) {
  const int a = g_sistema.animacionVisible();
  if (a >= 0) {
    const size_t k = copiarTexto(g_visible, sizeof g_visible, "animacion:");
    copiarTexto(g_visible + k, sizeof g_visible - k, g_sistema.config().animaciones[a].nombre);
    return g_visible;
  }
  return g_sistema.config().expresiones[g_sistema.motor().expresionVisible()].nombre;
}

void sintetica_nube_escribir(int car, const uint8_t* d, size_t n) { g_sistema.nubeEscribir(static_cast<uint8_t>(car), d, n); }
void sintetica_conexiones(int wifi, int nube) { g_sistema.conexiones(wifi != 0, nube != 0); }
int sintetica_red(char* ssid, size_t cs, char* clave, size_t cc) { return g_sistema.red(ssid, cs, clave, cc) ? 1 : 0; }
int sintetica_nube(char* url, size_t cu, char* usuario, size_t cus, char* clave, size_t cc) {
  return g_sistema.nube(url, cu, usuario, cus, clave, cc) ? 1 : 0;
}
void sintetica_ota_progreso(int codigo, int pct, const char* detalle) {
  g_sistema.otaProgreso(static_cast<uint8_t>(codigo), static_cast<uint8_t>(limitar(static_cast<float>(pct), 0, 100)), detalle);
}
uint32_t sintetica_crc_config(void) { return g_sistema.crcConfig(); }
int sintetica_quiere_dormir(void) { return g_sistema.quiereDormir() ? 1 : 0; }
size_t sintetica_mensaje_ota(const sintetica_manifiesto* m, char* s, size_t cap) {
  return m ? protocolo::mensajeOta(*reinterpret_cast<const ManifiestoOta*>(m), s, cap) : 0;
}

int sintetica_validar_config(const char* json, size_t n, char* detalle, size_t cap) {
  ResultadoConfig r = cargarConfig(g_suelta, json, n);
  if (detalle && cap) copiarTexto(detalle, cap, r.detalle ? r.detalle : "");
  return static_cast<int>(r.error);
}

int sintetica_dibujar_expresion(const char* json, size_t n, const char* nombre, int ancho, int alto, int redonda,
                                float t, uint8_t* rgba, char* detalle, size_t cap) {
  return sintetica_dibujar_expresion_cierre(json, n, nombre, ancho, alto, redonda, t, 0, rgba, detalle, cap);
}

int sintetica_sprite_suelto(int sprite, const uint8_t* qoi, size_t n) {
  if (sprite < 0 || sprite >= MAX_SPRITES) return -1;
  g_cacheSuelta.vaciar();
  if (n == 0) {
    g_largoSuelto[sprite] = 0;
    return 0;
  }
  uint32_t w = 0, h = 0;
  if (!qoi || n > LIMITE_SPRITE || !cabeceraSprite(qoi, n, &w, &h)) return -1;
  auto* p = static_cast<uint8_t*>(realloc(g_sueltos[sprite], n));
  if (!p) return -1;
  copiarBytes(p, qoi, n);
  g_sueltos[sprite] = p;
  g_largoSuelto[sprite] = static_cast<uint32_t>(n);
  return 0;
}

int sintetica_dibujar_expresion_cierre(const char* json, size_t n, const char* nombre, int ancho, int alto, int redonda,
                                       float t, float cierre, uint8_t* rgba, char* detalle, size_t cap) {
  if (detalle && cap) detalle[0] = 0;
  if (!rgba || !nombre || ancho < 16 || alto < 16 || ancho > MAX_ANCHO || alto > MAX_ALTO) return -1;
  static const char vacia[] = "{\"version\":2}";
  if (!json || !n) json = vacia, n = sizeof vacia - 1;
  ResultadoConfig r = cargarConfig(g_suelta, json, n);
  if (!r.ok()) {
    if (detalle && cap) copiarTexto(detalle, cap, r.detalle ? r.detalle : "");
    return static_cast<int>(r.error);
  }
  const DatosVida& vida = g_suelta.vida;
  const int i = vida.indiceCara(nombre);
  if (i < 0) return -1;
  // Lo mismo que el editor del simulador (sim_editor_rostro): la neutral, la
  // cara encima, y las fases de la micro-vida en el instante t.
  CuadroRostro c;
  const int neutral = vida.indiceCara("neutral");
  if (neutral >= 0) vida.caras[neutral].aplicar(c.r);
  vida.caras[i].aplicar(c.r);
  c.r.limitar();
  // El parpadeo cierra los párpados hacia el medio (como el motor).
  const float bl = limitar(cierre, 0, 1);
  for (int lado = 0; lado < 2; lado++) {
    const int b = lado * param::POR_OJO;
    c.r[b + param::LT] += (0.5f - c.r[b + param::LT]) * bl;
    c.r[b + param::LB] += (0.5f - c.r[b + param::LB]) * bl;
  }
  c.tiempo = t;
  c.faseSpin = t * 3.2f * c.r[param::SPIN];
  c.faseOrbita = t * 4.5f * c.r[param::ORBIT];
  const float ph = t * 1.15f - piso(t * 1.15f);
  auto g = [](float x) { return exponencial(-x * x); };
  c.latido = g((ph - 0.1f) * 22) + 0.65f * g((ph - 0.3f) * 22);
  c.pulso = 1 + c.r[param::PULSE] * (0.09f + 0.09f * seno(t * 9) * seno(t * 2.3f));
  c.glitch = c.r[param::GLITCH] * (seno(t * 23) > 0.6f ? 1.0f : 0.35f);
  c.semillaGlitch = piso(t * 8);
  c.progreso = t * 0.15f - piso(t * 0.15f);
  c.colorOjos = g_suelta.hayColorOjos ? g_suelta.colorOjos : paleta::NARANJA;
  c.brillo = vida.brilloMax;
  if (vida.caras[i].particula >= 0) {
    Particula& p = c.particulas[0];
    p.viva = true;
    p.tipo = static_cast<TipoParticula>(vida.caras[i].particula);
    p.x = 0.33f, p.y = 0.3f, p.vida = 2, p.edad = 0.6f, p.tamano = 0.06f;
  }
  // Por franjas, con poca memoria: el anfitrión puede ser una app.
  static uint16_t franja[MAX_ANCHO * 32];
  Lienzo lienzo;
  lienzo.iniciar(franja, ancho, alto, 32);
  const VistaRostro v = VistaRostro::para(ancho, alto, redonda != 0);
  // Una cara de imagen: con los sprites sueltos.
  const CacheSprites* sprites = nullptr;
  if (vida.caras[i].imagen >= 0 && g_cacheSuelta.cargar(vida.imagenes[vida.caras[i].imagen], v.unidad, leerSuelto, nullptr))
    sprites = &g_cacheSuelta;
  for (int y0 = 0; y0 < alto; y0 += lienzo.filasPorFranja()) {
    lienzo.empezarFranja(y0);
    dibujarRostro(lienzo, v, c, sprites, 1, sprites ? 1 : 0);
    const int filas = lienzo.filasFranja();
    for (int k = 0; k < filas * ancho; k++) {
      const uint32_t px = de565(franja[k]);
      uint8_t* s = rgba + (static_cast<size_t>(y0) * ancho + k) * 4;
      s[0] = static_cast<uint8_t>(px >> 16), s[1] = static_cast<uint8_t>(px >> 8), s[2] = static_cast<uint8_t>(px), s[3] = 255;
    }
  }
  return 0;
}
int sintetica_vida_base(int tipo, int indice, const char** nombre, const char** json) {
  const DatoVidaBase* l = tipo == 0 ? CARAS_VIDA_BASE : COMPORTAMIENTOS_BASE;
  const int n = tipo == 0 ? CANTIDAD_CARAS_VIDA_BASE : tipo == 1 ? CANTIDAD_COMPORTAMIENTOS_BASE : 0;
  if (indice >= 0 && indice < n) {
    if (nombre) *nombre = l[indice].nombre;
    if (json) *json = l[indice].json;
  }
  return n;
}
const char* sintetica_qr(void) { return g_sistema.contenidoQr(); }
int sintetica_imagen_visible(void) { return g_sistema.imagenVisible(); }
const char* sintetica_version(void) { return VERSION_SINTETICAOS; }

const char* sintetica_comportamiento(void) { return g_sistema.motor().nombreComportamiento(); }
int sintetica_estado_interno(float* s, int cap) {
  const float* e = g_sistema.motor().estadoInterno();
  for (int i = 0; s && i < cap && i < N_ESTADO; i++) s[i] = e[i];
  return N_ESTADO;
}
int sintetica_voz(int* volumen, int* microfono, int* tipo, const char** palabra) {
  const Voz& v = g_sistema.config().voz;
  if (volumen) *volumen = v.volumen;
  if (microfono) *microfono = v.microfono;
  if (tipo) *tipo = static_cast<int>(v.tipo);
  if (palabra) *palabra = v.palabra[0] ? v.palabra : g_sistema.nombre();
  return v.conversar ? 1 : 0;
}
int sintetica_fps_sugerido(void) { return g_sistema.fpsSugerido(); }
void sintetica_qr_por_mantener(int si) { g_sistema.qrPorMantener(si != 0); }
void sintetica_sin_pantalla_qr(int si) { g_sistema.sinPantallaQr(si != 0); }
void sintetica_globo_afuera(int si) { g_sistema.globoAfuera(si != 0); }
void sintetica_mostrar_qr(void) { g_sistema.mostrarQr(); }
const char* sintetica_globo(void) { return g_sistema.globoVisible() ? g_sistema.textoGlobo() : ""; }
void sintetica_seguir(float x, float y) { g_sistema.seguir(x, y); }
void sintetica_depurar(int si) { g_sistema.depurar(si != 0); }

// ---------------------------------------------------------------- v1.10: la charla y la voz

static_assert(sizeof(EstadoCharla) <= SINTETICA_LARGO_ESTADO_CHARLA, "el estado de la charla entra en el blob");

namespace {
void copiarSalida(const RespuestaCharla& r, sintetica_charla* s) {
  if (!s) return;
  *s = sintetica_charla{};
  copiarTexto(s->intencion, sizeof s->intencion, r.nombre());
  s->confianza = r.confianza;
  s->no_entendi = r.noEntendi ? 1 : 0;
  s->falta = r.falta ? 1 : 0;
  s->confirma = r.confirma ? 1 : 0;
  s->accion = static_cast<int>(r.accion);
  copiarTexto(s->accion_nombre, sizeof s->accion_nombre, nombreAccionCharla(r.accion));
  s->dato = r.dato;
  s->minuto = r.e.minuto;
  s->en_minutos = r.e.enMinutos;
  s->dia = r.e.dia;
  s->semana = r.e.semana;
  s->dias = r.e.dias;
  copiarTexto(s->texto, sizeof s->texto, r.e.texto);
  copiarTexto(s->frase, sizeof s->frase, r.frase);
  copiarTexto(s->expresion, sizeof s->expresion, r.expresion);
  copiarTexto(s->comportamiento, sizeof s->comportamiento, r.comportamiento);
}
bool textoCharlaValido(const char* t, size_t n) { return t && n > 0 && n <= MAX_ENTRADA_CHARLA && textoUtf8Valido(t, n); }
VozPropia g_voz;
char g_textoVoz[256];
}  // namespace

int sintetica_charlar(const char* texto, size_t n, sintetica_charla* salida) {
  if (!textoCharlaValido(texto, n)) return -1;
  RespuestaCharla r;
  const bool entendio = g_sistema.charlar(texto, n, &r, false);
  if (entendio && r.accion != AccionCharla::Ninguna) g_sistema.avisarCharla(r);
  copiarSalida(r, salida);
  return entendio ? 1 : 0;
}

int sintetica_decir(const char* texto, const char* expresion) {
  const size_t n = texto ? largo(texto) : 0;
  if (n < 1 || n >= static_cast<size_t>(LARGO_FRASE) || !textoUtf8Valido(texto, n)) return -1;
  Config::Cara cara;
  if (expresion && expresion[0]) {
    cara = g_sistema.config().resolverCara(expresion);
    if (!cara.existe()) return -2;
  }
  g_sistema.decir(texto, cara, !cara.existe());
  return 0;
}

void sintetica_voz_propia(int modo) { g_sistema.vozPropia(modo); }

int sintetica_charlar_con(const sintetica_charla_ctx* c, const char* texto, size_t n, uint8_t* estado, sintetica_charla* salida) {
  if (!textoCharlaValido(texto, n) || !estado) return -1;
  ContextoCharla ctx;
  if (c) {
    ctx.dueno = c->dueno, ctx.robot = c->robot, ctx.preset = c->preset;
    ctx.minuto = c->minuto_del_dia >= 0 && c->minuto_del_dia < 1440 ? c->minuto_del_dia : -1;
    ctx.epoch = c->epoch;
    ctx.clima = c->clima, ctx.climaManana = c->clima_manana;
    ctx.agenda = c->agenda, ctx.agendaManana = c->agenda_manana;
    ctx.musica = c->musica;
    ctx.nHabitos = c->n_habitos < 0 ? 0 : c->n_habitos > MAX_HABITOS ? MAX_HABITOS : c->n_habitos;
    for (int i = 0; i < ctx.nHabitos; i++) ctx.habitos[i] = c->habitos[i];
    ctx.gustos = c->gustos;
    ctx.diasJuntos = c->dias_juntos;
    ctx.alarmaSonando = c->alarma_sonando != 0;
    ctx.segundos = c->segundos;
    ctx.semilla = c->semilla;
  }
  // El blob: en ceros es "sin contexto"; si no, el EstadoCharla tal cual.
  EstadoCharla e;
  bool vacio = true;
  for (size_t i = 0; i < sizeof(EstadoCharla); i++)
    if (estado[i]) vacio = false;
  if (!vacio) copiarBytes(&e, estado, sizeof e);
  if (e.ultima != -1 && !nombreIntencionCharla(e.ultima)[0]) e = EstadoCharla();
  RespuestaCharla r;
  charlar(texto, n, ctx, e, &r);
  copiarBytes(estado, &e, sizeof e);
  copiarSalida(r, salida);
  return r.noEntendi ? 0 : 1;
}

uint32_t sintetica_voz_empezar(const char* texto, int tipo, int volumen, float energia, float animo) {
  copiarTexto(g_textoVoz, sizeof g_textoVoz, texto ? texto : "");
  Personalidad p;
  const ParamsVoz pv = paramsVoz(tipo, volumen < 0 ? 0 : volumen > 100 ? 100 : volumen, p, energia, animo);
  g_voz.empezar(g_textoVoz, pv);
  return VozPropia::duracion(g_textoVoz, pv);
}

size_t sintetica_voz_generar(int16_t* muestras, size_t n) { return muestras ? g_voz.generar(muestras, n) : 0; }
}
