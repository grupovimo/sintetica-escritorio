#include "sistema.h"

#include "emparejamiento.h"
#include "globo.h"
#include "fuentes.h"
#include "texto.h"
#include "vida_base.h"

namespace sintetica {

namespace {
constexpr const char* CLAVE_CONFIG = "config";
constexpr const char* CLAVE_AJUSTES = "ajustes";
constexpr const char* CLAVE_SERIE = "serie";
constexpr const char* CLAVE_CODIGO = "codigo";
constexpr const char* CLAVE_CLAVES = "claves";
constexpr const char* CLAVE_IMAGENES = "imagenes";
constexpr const char* CLAVE_SPRITES = "sprites";
constexpr const char* CLAVE_RED = "red";
constexpr const char* CLAVE_NUBE = "nube";
constexpr const char* CLAVE_VIDA = "vida";

// Por la nube los trozos pueden ser más grandes que por BLE.
constexpr size_t TROZO_NUBE = 1024;
uint8_t g_trozo[TROZO_NUBE + 16];

// "img0" a "img7"; los sprites (espacios 8 a 19), "spr0" a "spr11".
void claveImagen(char* destino, int espacio) {
  if (espacio < PRIMER_SPRITE) {
    copiarTexto(destino, 6, "img0");
    destino[3] = static_cast<char>('0' + espacio);
    return;
  }
  const int n = espacio - PRIMER_SPRITE;
  copiarTexto(destino, 6, "spr");
  if (n >= 10) destino[3] = '1', destino[4] = static_cast<char>('0' + n - 10), destino[5] = 0;
  else destino[3] = static_cast<char>('0' + n), destino[4] = 0;
}

bool esSprite(int espacio) { return espacio >= PRIMER_SPRITE && espacio < PRIMER_SPRITE + MAX_SPRITES; }
bool espacioValido(int espacio) { return (espacio >= 0 && espacio < MAX_IMAGENES) || esSprite(espacio); }

uint16_t g_filaImagen[MAX_ANCHO];
constexpr uint32_t QR_POR_BOTON_MS = 120000;  // el QR del botón dura 2 min
constexpr uint8_t FALLOS_MAXIMOS = 5;
constexpr uint32_t BLOQUEO_MS = 30000;
// Sin letras ni números que se confundan (0/O, 1/I/L).
constexpr int64_t MARGEN_ARCHIVOS = 16 * 1024;  // bloques de 4 KB y metadatos de LittleFS
const char ALFABETO_CODIGO[] = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";


const char CONFIG_VACIA[] = "{\"version\":1}";

bool esUrlNube(const char* u) {
  return iguales(u, "mqtt://", 7) || iguales(u, "mqtts://", 8);
}
}  // namespace

void Sistema::iniciar(const Hal& hal, uint16_t* pixeles, const Perfil& perfil) {
  hal_ = hal;
  perfil_ = perfil;
  vistaPrevia_ = false;
  apagado_ = false;
  botonApretado_ = false;
  ignorarSoltar_ = false;
  posLista_ = 0;
  largoUltimoEstado_ = 0;  // que se notifique el estado del arranque
  receptor_.reiniciar();
  autenticado_ = false;
  fallos_ = 0;
  leerIdentidad();
  lienzo_.iniciar(pixeles, perfil.ancho, perfil.alto, perfil.filasFranja);
  vista_ = Vista::para(perfil.ancho, perfil.alto, perfil.redonda);
  // El resplandor de los ojos cuesta: el broche (sin FPU) no lo dibuja.
  vistaRostro_ = VistaRostro::para(perfil.ancho, perfil.alto, perfil.redonda, perfil.boton != Perfil::Boton::Broche);
  horaRef_ = horaRefMs_ = 0;
  ultimaBateria_ = ultimoCargando_ = -1;
  sinWifiDesde_ = 0;
  avisoSinWifi_ = false;
  depurar_ = false;

  ResultadoConfig r = cargarBase(config_);
  if (!r.ok()) log(r.detalle);

  // Configuración guardada.
  cargarGuardada();
  panel_ = Panel::Ninguno;
  fasePanel_ = 0;
  recVisible_ = -1;
  recPendientes_ = 0;
  for (auto& d : recDia_) d = 0;
  recMinuto_ = -1;
  tragarSoltar_ = false;
  caraHasta_ = 0;
  reiniciarPomodoro();
  faseProtector_ = 0;
  globo_ = hayPendiente_ = false;
  globoTexto_[0] = 0;
  hablador_ = Hablador();
  alarma_ = alarmaApretado_ = tragarAlarma_ = false;
  pospuestaHasta_ = 0;
  alarmaMinuto_ = -1;
  alarmaDia_ = 0;
  clima_ = DatosClima();
  proximaCaraClima_ = 0;
  proximaAgua_ = 0;
  pausaDicha_ = preguntando_ = pregApretado_ = tragarPregunta_ = false;
  animoDia_ = 0;
  juego_ = Juego::Ninguno;
  toqueHasta_ = fuisteMs_ = habDesde_ = habToque_ = habReintento_ = musicaHasta_ = musicaFirma_ = 0;
  fuisteEmpujado_ = habApretado_ = false;
  habPregunta_ = habReintentoIdx_ = -1;
  for (int i = 0; i < MAX_HABITOS; i++) habDiaHecho_[i] = habDiaPreg_[i] = 0, habRacha_[i] = 0;

  canal_ = Canal::Ble;
  wifi_ = nube_ = false;
  ota_ = Ota::Ninguna;
  animPedida_ = animVisible_ = -1;
  leerRed();

  const uint32_t ahora = hal_.ahoraMs();
  arranque_ = ahora;
  ultimoCuadro_ = ahora;
  motor_.iniciar(&config_, hal_.semilla ? hal_.semilla() : ahora, ahora);
  SalidasVida sv;
  sv.ctx = this;
  sv.sonido = [](void* c, const char* n) {
    static_cast<Sistema*>(c)->sonar(n);
  };
  sv.led = [](void* c, uint32_t rgb) {
    auto* s = static_cast<Sistema*>(c);
    if (s->hal_.led) s->hal_.led(rgb);
  };
  sv.animacion = [](void* c, int i) { static_cast<Sistema*>(c)->reproducir(i, true); };
  sv.decir = [](void* c, uint8_t s) {
    if (s < N_SITUACIONES) static_cast<Sistema*>(c)->hablar(static_cast<Situacion>(s));
  };
  sv.guardar = [](void* c, const uint8_t* d, size_t n) {
    auto* s = static_cast<Sistema*>(c);
    if (s->hal_.guardar) s->hal_.guardar(CLAVE_VIDA, d, n);
  };
  sv.guardarRecuerdos = [](void* c, const uint8_t* d, size_t n) { static_cast<Sistema*>(c)->guardarRecuerdos(d, n); };
  sv.diario = [](void* c, uint16_t dia, uint8_t tipo, uint8_t dato) { static_cast<Sistema*>(c)->lineaDiarioNueva(dia, tipo, dato); };
  motor_.salidas(sv);
  leerMemoria();
  leerRecuerdos();
  // v3: sus tics salen del identificador (la MAC): los mismos para siempre.
  motor_.identidad(protocolo::crc32(reinterpret_cast<const uint8_t*>(id_), largo(id_)));
  leerAjustes();
  leerImagenes();
  leerSprites();
  cache_.vaciar();
  firmaCache_ = 0;
  opSprites_ = ocultarOjos_ = 0;
  actualizarSprites();
  azar_.sembrar(hal_.semilla ? hal_.semilla() : ahora);
  subiendo_ = -1;
  mostrandoImagen_ = -1;
  imagenSucia_ = true;
  proximaVueltaCara_ = vueltaCaraHasta_ = 0;
  ultimaRotacion_ = ahora;
  ultimaActividad_ = ahora;
  tamanoVisto_ = tamanoObjetivo();  // al prender, ya del tamaño de la config (sin fundido)
  aplicarLista(ahora);
  log("SintéticaOS listo");
}

size_t Sistema::leerGuardada(char* buf) const {
  const int n = hal_.leer ? hal_.leer(CLAVE_CONFIG, reinterpret_cast<uint8_t*>(buf), MAX_CONFIG_BYTES) : -1;
  if (n <= 0) return 0;
  buf[n] = 0;
  return static_cast<size_t>(n);
}

void Sistema::cargarGuardada() {
  // El texto no queda en RAM: se lee de la flash cuando hace falta.
  char* buf = receptor_.prestar();
  const size_t n = buf ? leerGuardada(buf) : 0;
  crcConfig_ = 0;
  if (n == 0) {
    cargarBase(config_);
    return;
  }
  ResultadoConfig r = cargarConfig(config_, buf, n);
  if (!r.ok()) {
    log("config guardada inválida; uso la base");
    log(r.detalle);
    return;
  }
  crcConfig_ = protocolo::crc32(reinterpret_cast<const uint8_t*>(buf), n);
}

void Sistema::leerIdentidad() {
  const char* id = hal_.identificador ? hal_.identificador() : nullptr;
  copiarTexto(id_, sizeof id_, id && id[0] ? id : "000000000000");

  serie_[0] = 0;
  int n = hal_.leer ? hal_.leer(CLAVE_SERIE, reinterpret_cast<uint8_t*>(serie_), sizeof serie_ - 1) : -1;
  serie_[n > 0 ? n : 0] = 0;

  // El código del QR se genera una vez, en el primer arranque, y queda fijo.
  n = hal_.leer ? hal_.leer(CLAVE_CODIGO, reinterpret_cast<uint8_t*>(codigo_), 6) : -1;
  if (n != 6) {
    for (int i = 0; i < 6; i++) {
      uint32_t r = hal_.semilla ? hal_.semilla() : hal_.ahoraMs() * 2654435761u + static_cast<uint32_t>(i);
      codigo_[i] = ALFABETO_CODIGO[r % (sizeof ALFABETO_CODIGO - 1)];
    }
    if (hal_.guardar) hal_.guardar(CLAVE_CODIGO, reinterpret_cast<const uint8_t*>(codigo_), 6);
  }
  codigo_[6] = 0;

  uint8_t b[4 * 16];
  n = hal_.leer ? hal_.leer(CLAVE_CLAVES, b, sizeof b) : -1;
  nClaves_ = n > 0 ? static_cast<uint8_t>(n / 16) : 0;
  copiarBytes(claves_, b, nClaves_ * 16u);
  emparejando_ = nClaves_ == 0;  // sin ningún celu vinculado, muestra el QR
  emparejarHasta_ = 0;
  armarQr();
}

void Sistema::armarQr() {
  copiarTexto(qr_, sizeof qr_, PREFIJO_QR);
  size_t n = largo(qr_);
  n += copiarTexto(qr_ + n, sizeof qr_ - n, id_);
  n += copiarTexto(qr_ + n, sizeof qr_ - n, "/");
  copiarTexto(qr_ + n, sizeof qr_ - n, codigo_);
}

void Sistema::fijarSerie(const char* serie) {
  copiarTexto(serie_, sizeof serie_, serie);
  if (hal_.guardar) hal_.guardar(CLAVE_SERIE, reinterpret_cast<const uint8_t*>(serie_), largo(serie_));
}

void Sistema::guardarClaves() {
  if (hal_.guardar) hal_.guardar(CLAVE_CLAVES, &claves_[0][0], nClaves_ * 16u);
}

bool Sistema::bloqueado(uint32_t ahora) const {
  return fallos_ >= FALLOS_MAXIMOS && static_cast<int32_t>(ahora - bloqueoHasta_) < 0;
}

void Sistema::falla(uint32_t ahora) {
  if (fallos_ < 255) fallos_++;
  if (fallos_ >= FALLOS_MAXIMOS) bloqueoHasta_ = ahora + BLOQUEO_MS;
}

void Sistema::bleConexion(bool conectado) {
  autenticado_ = vistaPrevia_;
  receptor_.reiniciar();
  log(conectado ? "celu conectado" : "celu desconectado");
}

void Sistema::leerAjustes() {
  nombre_[0] = 0;
  fijaGuardada_[0] = 0;
  uint8_t b[64];
  int n = hal_.leer ? hal_.leer(CLAVE_AJUSTES, b, sizeof b) : -1;
  // [versión=2][n][nombre][n][expresión fija][posición en la lista] (la v1 no tenía la posición)
  if (n < 3 || (b[0] != 1 && b[0] != 2)) return;
  size_t i = 1;
  size_t nn = b[i++];
  if (i + nn >= static_cast<size_t>(n)) return;
  copiarTexto(nombre_, sizeof nombre_, reinterpret_cast<const char*>(b + i), nn);
  i += nn;
  size_t ne = b[i++];
  if (i + ne > static_cast<size_t>(n)) return;
  copiarTexto(fijaGuardada_, sizeof fijaGuardada_, reinterpret_cast<const char*>(b + i), ne);
  i += ne;
  if (!motor_.fijar(fijaGuardada_)) fijaGuardada_[0] = 0;
  if (b[0] == 2 && i < static_cast<size_t>(n)) posLista_ = b[i];
}

void Sistema::guardarAjustes() {
  if (!hal_.guardar) return;
  uint8_t b[64];
  size_t i = 0;
  b[i++] = 2;
  size_t nn = largo(nombre_), ne = largo(fijaGuardada_);
  b[i++] = static_cast<uint8_t>(nn);
  copiarBytes(b + i, nombre_, nn);
  i += nn;
  b[i++] = static_cast<uint8_t>(ne);
  copiarBytes(b + i, fijaGuardada_, ne);
  i += ne;
  b[i++] = posLista_;
  hal_.guardar(CLAVE_AJUSTES, b, i);
}

void Sistema::leerMemoria() {
  uint8_t b[LARGO_MEMORIA];
  const int n = hal_.leer ? hal_.leer(CLAVE_VIDA, b, sizeof b) : -1;
  Memoria m;
  if (n == static_cast<int>(LARGO_MEMORIA) && decodificarMemoria(b, LARGO_MEMORIA, &m)) motor_.restaurarMemoria(m);
}

void Sistema::evento(Evento e) {
  actividad();  // cualquier evento saca el protector de pantalla
  // Levantarlo apaga el despertador.
  if (alarma_ && (e == Evento::Levantado || e == Evento::SacudidaFuerte)) apagarAlarma();
  // Sacudirlo tira el dado o la moneda.
  if ((juego_ == Juego::Dado || juego_ == Juego::Moneda) && juegoFase_ == 0 &&
      (e == Evento::Sacudida || e == Evento::SacudidaSuave || e == Evento::SacudidaFuerte || e == Evento::Levantado)) {
    juegoFase_ = 1;
    juegoDesde_ = juegoProximo_ = juegoAccion_ = hal_.ahoraMs();
  }
  // Un toque de un amigo también se devuelve con una caricia (aunque llegue como evento).
  if (toqueHasta_ && e == Evento::Caricia) {
    devolverToque(hal_.ahoraMs());
    return;
  }
  motor_.evento(e, hal_.ahoraMs());
}

void Sistema::hora(uint32_t epochUtc, int zonaMinutos) {
  if (hal_.fijarHora) hal_.fijarHora(epochUtc, zonaMinutos);
  horaRef_ = static_cast<uint32_t>(static_cast<int64_t>(epochUtc) + static_cast<int64_t>(zonaMinutos) * 60);
  horaRefMs_ = hal_.ahoraMs();
}

uint32_t Sistema::epoch() const {
  const uint32_t e = hal_.epoch ? hal_.epoch() : 0;
  if (e) return e;
  if (!horaRef_) return 0;
  return horaRef_ + (hal_.ahoraMs() - horaRefMs_) / 1000u;
}

int Sistema::minutoDelDia() const {
  const int m = hal_.minutoDelDia ? hal_.minutoDelDia() : -1;
  if (m >= 0) return m;
  const uint32_t e = epoch();
  return e ? static_cast<int>((e % 86400u) / 60u) : -1;
}

Contexto Sistema::contexto() const {
  Contexto c;
  c.minutoDelDia = minutoDelDia();
  c.epoch = epoch();
  c.bateria = hal_.bateria ? hal_.bateria() : -1;
  c.cargando = hal_.cargando ? hal_.cargando() : -1;
  return c;
}

int Sistema::fpsSugerido() const {
  if (apagado_) return 1;
  if (motor_.dormido()) return 5;
  return 30;
}

// Convierte lo que pasa alrededor (batería, cargador, WiFi) en eventos.
float Sistema::tamanoObjetivo() const {
  // Lo más grande que entra: los ojos más abiertos y separados de la marca
  // (medio ancho ~0,5 y medio alto ~0,55 unidades, con lo que salta y las chispas)
  // dentro de la pantalla, o del círculo en la redonda.
  const float W = static_cast<float>(perfil_.ancho), H = static_cast<float>(perfil_.alto), u = vistaRostro_.unidad;
  float maximo = perfil_.redonda ? (minf(W, H) / 2) / (0.52f * u) : minf((W / 2) / (0.5f * u), (H / 2) / (0.55f * u));
  maximo = limitar(maximo, 1.0f, 1.5f);
  return limitar(config_.tamano, 0.5f, maximo);
}

void Sistema::vigilar(uint32_t ahora) {
  const Contexto c = contexto();
  if (c.bateria >= 0 && ultimaBateria_ >= 0) {
    if (c.bateria < 20 && ultimaBateria_ >= 20) evento(Evento::BateriaBaja);
    if (c.bateria < 5 && ultimaBateria_ >= 5) evento(Evento::BateriaCritica);
    if (c.bateria >= 100 && ultimaBateria_ < 100 && c.cargando == 1) evento(Evento::Cargado);
  }
  if (c.cargando >= 0 && ultimoCargando_ >= 0 && c.cargando != ultimoCargando_)
    evento(c.cargando ? Evento::Enchufado : Evento::Desenchufado);
  ultimaBateria_ = c.bateria;
  ultimoCargando_ = c.cargando;
  // Sin WiFi un minuto (teniéndolo configurado): lo busca con la mirada.
  if (redConfigurada_ && !wifi_ && !vistaPrevia_) {
    if (!sinWifiDesde_) sinWifiDesde_ = ahora;
    if (!avisoSinWifi_ && ahora - sinWifiDesde_ >= 60000) {
      avisoSinWifi_ = true;
      evento(Evento::SinWifi);
    }
  } else {
    sinWifiDesde_ = 0;
    avisoSinWifi_ = false;
  }
}

void Sistema::leerImagenes() {
  for (auto& m : imagenes_) m = MetaImagen();
  uint8_t b[MAX_IMAGENES * 8];
  const int n = hal_.leer ? hal_.leer(CLAVE_IMAGENES, b, sizeof b) : -1;
  if (n != static_cast<int>(sizeof b)) return;
  for (int i = 0; i < MAX_IMAGENES; i++) {
    imagenes_[i].largo = protocolo::leer32(b + i * 8);
    imagenes_[i].crc = protocolo::leer32(b + i * 8 + 4);
  }
}

void Sistema::guardarImagenes() {
  if (!hal_.guardar) return;
  uint8_t b[MAX_IMAGENES * 8];
  for (int i = 0; i < MAX_IMAGENES; i++) {
    protocolo::escribir32(b + i * 8, imagenes_[i].largo);
    protocolo::escribir32(b + i * 8 + 4, imagenes_[i].crc);
  }
  hal_.guardar(CLAVE_IMAGENES, b, sizeof b);
}

void Sistema::leerSprites() {
  for (auto& m : sprites_) m = MetaImagen();
  uint8_t b[MAX_SPRITES * 8];
  const int n = hal_.leer ? hal_.leer(CLAVE_SPRITES, b, sizeof b) : -1;
  if (n != static_cast<int>(sizeof b)) return;
  for (int i = 0; i < MAX_SPRITES; i++) {
    sprites_[i].largo = protocolo::leer32(b + i * 8);
    sprites_[i].crc = protocolo::leer32(b + i * 8 + 4);
  }
}

void Sistema::guardarSprites() {
  if (!hal_.guardar) return;
  uint8_t b[MAX_SPRITES * 8];
  for (int i = 0; i < MAX_SPRITES; i++) {
    protocolo::escribir32(b + i * 8, sprites_[i].largo);
    protocolo::escribir32(b + i * 8 + 4, sprites_[i].crc);
  }
  hal_.guardar(CLAVE_SPRITES, b, sizeof b);
}

Sistema::MetaImagen* Sistema::meta(int espacio) {
  if (espacio >= 0 && espacio < MAX_IMAGENES) return &imagenes_[espacio];
  if (esSprite(espacio)) return &sprites_[espacio - PRIMER_SPRITE];
  return nullptr;
}

int Sistema::leerSprite(void* contexto, int sprite, uint32_t desplazamiento, uint8_t* destino, size_t n) {
  auto* s = static_cast<Sistema*>(contexto);
  if (!s->hal_.leerTrozo || sprite < 0 || sprite >= MAX_SPRITES || !s->sprites_[sprite].largo) return -1;
  char clave[6];
  claveImagen(clave, PRIMER_SPRITE + sprite);
  return s->hal_.leerTrozo(clave, desplazamiento, destino, n);
}

// Un QOI de 3 o 4 canales de hasta 128 × 128.
bool Sistema::spriteValido(const char* clave, const char** porQue) {
  *porQue = "un sprite es un QOI de hasta 128 × 128";
  uint8_t cab[14];
  uint32_t w = 0, h = 0;
  return leerArchivo(clave, 0, cab, sizeof cab) && cabeceraSprite(cab, sizeof cab, &w, &h);
}

void Sistema::actualizarSprites() {
  uint8_t m = 0;
  for (int i = 0; i < config_.vida.nImagenes; i++) {
    const int s = config_.vida.imagenes[i].sprite[static_cast<int>(ParteSprite::Ojo)];
    if (s >= 0 && s < MAX_SPRITES && sprites_[s].largo) m = static_cast<uint8_t>(m | (1u << i));
  }
  motor_.imagenesDisponibles(m);
}

// De qué es la cara de imagen `cara` (0 = ninguna): sus partes, su geometría
// y los CRC de sus sprites. Si cambia algo, se vuelve a cargar con un fundido.
uint32_t Sistema::firmaSprites(int cara) const {
  if (cara < 0 || cara >= config_.vida.nCaras || config_.vida.caras[cara].imagen < 0) return 0;
  const CaraImagen& c = config_.vida.imagenes[config_.vida.caras[cara].imagen];
  // Por parte: espacio + CRC (5); después espejar (1), tamaño, tamaño de boca,
  // altura de boca y tamaño de la cara (2 cada uno) y el cierre (1).
  uint8_t b[N_PARTES_SPRITE * 5 + 1 + 4 * 2 + 1];
  size_t n = 0;
  for (int i = 0; i < N_PARTES_SPRITE; i++) {
    b[n++] = static_cast<uint8_t>(c.sprite[i]);
    protocolo::escribir32(b + n, c.sprite[i] >= 0 ? sprites_[c.sprite[i]].crc : 0);
    n += 4;
  }
  b[n++] = c.espejar ? 1 : 0;
  b[n++] = static_cast<uint8_t>(c.tamano), b[n++] = static_cast<uint8_t>(c.tamano >> 8);
  b[n++] = static_cast<uint8_t>(c.bocaTamano), b[n++] = static_cast<uint8_t>(c.bocaTamano >> 8);
  b[n++] = static_cast<uint8_t>(c.bocaY), b[n++] = static_cast<uint8_t>(c.bocaY >> 8);
  // El tamaño de la cara (cara.tamano): con otro, se vuelven a cargar a esa escala.
  const uint16_t t = static_cast<uint16_t>(tamanoObjetivo() * 1000 + 0.5f);
  b[n++] = static_cast<uint8_t>(t), b[n++] = static_cast<uint8_t>(t >> 8);
  b[n++] = 0x5A;
  return protocolo::crc32(b, n) | 1u;
}

// El fundido entre la SDF y la cara de imagen (nunca un corte): si la que
// toca no es la que está cargada, se apaga, se carga la nueva y se prende.
void Sistema::avanzarSprites(uint32_t dt) {
  const float paso = static_cast<float>(dt < 200 ? dt : 200) / 200.0f;
  const int cara = motor_.caraImagen();
  const uint32_t objetivo = firmaSprites(cara);
  if (objetivo != firmaCache_) {
    opSprites_ = maxf(0, opSprites_ - paso);
    if (opSprites_ <= 0) {
      firmaCache_ = 0;
      cache_.vaciar();
      if (objetivo) {
        const CaraImagen& c = config_.vida.imagenes[config_.vida.caras[cara].imagen];
        if (cache_.cargar(c, vistaRostro_.unidad * tamanoObjetivo(), leerSprite, this)) {
          firmaCache_ = objetivo;
        } else {
          log("no pude leer los sprites de la cara; uso la de siempre");
          motor_.imagenesDisponibles(0);  // hasta que cambien la config o los sprites
        }
      }
    }
  } else if (firmaCache_) {
    opSprites_ = minf(1, opSprites_ + paso);
  }
  // Los ojos de la SDF se apagan mientras haya una cara de imagen (o mientras entra).
  const float quiere = objetivo ? 1.0f : 0.0f;
  ocultarOjos_ = quiere > ocultarOjos_ ? minf(quiere, ocultarOjos_ + paso) : maxf(quiere, ocultarOjos_ - paso);
}

int Sistema::leerParaQoi(void* contexto, uint32_t desplazamiento, uint8_t* destino, size_t n) {
  auto* s = static_cast<Sistema*>(contexto);
  if (!s->hal_.leerTrozo) return -1;
  char clave[6];
  claveImagen(clave, s->imagenLeyendo_);
  return s->hal_.leerTrozo(clave, s->baseImagen_ + desplazamiento, destino, n);
}

// Lee exactamente n bytes de un archivo (la lectura puede venir de a partes).
bool Sistema::leerArchivo(const char* clave, uint32_t desplazamiento, uint8_t* destino, size_t n) {
  if (!hal_.leerTrozo) return false;
  size_t leidos = 0;
  while (leidos < n) {
    const int m = hal_.leerTrozo(clave, desplazamiento + static_cast<uint32_t>(leidos), destino + leidos, n - leidos);
    if (m <= 0) return false;
    leidos += static_cast<size_t>(m);
  }
  return true;
}

// Un QOI del tamaño de la pantalla, o una animación cuyos cuadros lo son.
bool Sistema::imagenValida(const char* clave, uint32_t largo, const char** porQue) {
  const uint32_t w = static_cast<uint32_t>(perfil_.ancho), h = static_cast<uint32_t>(perfil_.alto);
  *porQue = "tiene que ser un QOI del tamaño de la pantalla";
  uint8_t cab[14];
  if (!leerArchivo(clave, 0, cab, animacion::CABECERA)) return false;
  if (!animacion::es(cab, animacion::CABECERA)) {
    uint32_t qw = 0, qh = 0;
    return leerArchivo(clave, 0, cab, sizeof cab) && DecodificadorQoi::leerCabecera(cab, sizeof cab, &qw, &qh) &&
           qw == w && qh == h;
  }
  *porQue = "la animación no es válida o no es del tamaño de la pantalla";
  int cuadros = 0;
  uint32_t aw = 0, ah = 0;
  if (!animacion::leerCabecera(cab, animacion::CABECERA, &cuadros, &aw, &ah) || aw != w || ah != h) return false;
  const uint32_t tabla = static_cast<uint32_t>(animacion::CABECERA + animacion::POR_CUADRO * cuadros);
  for (int i = 0; i < cuadros; i++) {
    uint8_t e[animacion::POR_CUADRO];
    if (!leerArchivo(clave, static_cast<uint32_t>(animacion::CABECERA + animacion::POR_CUADRO * i), e, sizeof e))
      return false;
    const animacion::Cuadro c = animacion::leerCuadro(e);
    if (c.ms < animacion::MIN_MS || c.desplazamiento < tabla || c.largo < 22 || c.desplazamiento > largo ||
        c.largo > largo - c.desplazamiento)
      return false;
    uint32_t qw = 0, qh = 0;
    if (!leerArchivo(clave, c.desplazamiento, cab, sizeof cab) ||
        !DecodificadorQoi::leerCabecera(cab, sizeof cab, &qw, &qh) || qw != w || qh != h)
      return false;
  }
  return true;
}

// Al empezar a mostrar una imagen: si es animada, su primer cuadro.
bool Sistema::prepararImagen(int espacio, uint32_t ahora) {
  imagenPreparada_ = espacio;
  animCuadros_ = animCuadro_ = 0;
  baseImagen_ = 0;
  char clave[6];
  claveImagen(clave, espacio);
  uint8_t cab[animacion::CABECERA];
  if (!leerArchivo(clave, 0, cab, sizeof cab)) return false;
  if (!animacion::es(cab, sizeof cab)) return true;
  int cuadros = 0;
  uint32_t w = 0, h = 0;
  if (!animacion::leerCabecera(cab, sizeof cab, &cuadros, &w, &h)) return false;
  animCuadros_ = static_cast<uint8_t>(cuadros);
  return cargarCuadro(espacio, 0, ahora);
}

bool Sistema::cargarCuadro(int espacio, int cuadro, uint32_t ahora) {
  char clave[6];
  claveImagen(clave, espacio);
  uint8_t e[animacion::POR_CUADRO];
  if (!leerArchivo(clave, static_cast<uint32_t>(animacion::CABECERA + animacion::POR_CUADRO * cuadro), e, sizeof e))
    return false;
  const animacion::Cuadro c = animacion::leerCuadro(e);
  animCuadro_ = static_cast<uint8_t>(cuadro);
  baseImagen_ = c.desplazamiento;
  animProximo_ = ahora + (c.ms < animacion::MIN_MS ? animacion::MIN_MS : c.ms);
  return true;
}

// Decodifica la imagen en la franja actual del lienzo. false si no se puede leer.
bool Sistema::dibujarImagen(int espacio, float alfa) {
  const int y0 = lienzo_.franja();
  if (y0 == 0) {
    imagenLeyendo_ = espacio;
    if (!qoi_.empezar(leerParaQoi, this) || qoi_.ancho() != static_cast<uint32_t>(lienzo_.ancho()) ||
        qoi_.alto() != static_cast<uint32_t>(lienzo_.alto()))
      return false;
  }
  for (int y = y0; y < y0 + lienzo_.filasFranja(); y++) {
    if (!qoi_.siguienteFila(g_filaImagen)) return false;
    if (alfa >= 1) lienzo_.fila(y, g_filaImagen);
    else lienzo_.filaMezclada(y, g_filaImagen, alfa);
  }
  return true;
}

int Sistema::imagenDeGaleria(int desde) const {
  for (int k = 0; k < MAX_IMAGENES; k++) {
    const int i = (desde + k) % MAX_IMAGENES;
    if (imagenes_[i].largo > 0 && subiendo_ != i) return i;
  }
  return -1;
}

// Pasar a la siguiente imagen de la galería: se funde a la cara y vuelve con la otra.
void Sistema::pasarGaleria(uint32_t ahora) {
  const int actual = imagenDeGaleria(posGaleria_);
  if (galeriaFundido_ || actual < 0 || imagenDeGaleria(actual + 1) == actual) return;
  galeriaFundido_ = ahora ? ahora : 1;
  galeriaPaso_ = false;
}

// La opacidad de la imagen de la galería: baja, cambia a la siguiente en el
// medio (con la cara de fondo) y vuelve a subir.
float Sistema::alfaGaleria(uint32_t ahora) {
  constexpr uint32_t MEDIO_MS = 350;
  if (!galeriaFundido_) return 1;
  const uint32_t t = ahora - galeriaFundido_;
  if (t < MEDIO_MS) return 1 - suave(static_cast<float>(t) / MEDIO_MS);
  if (!galeriaPaso_) {
    const int sig = imagenDeGaleria(imagenDeGaleria(posGaleria_) + 1);
    if (sig >= 0) posGaleria_ = static_cast<uint8_t>(sig);
    galeriaPaso_ = true;
  }
  if (t < 2 * MEDIO_MS) return suave(static_cast<float>(t - MEDIO_MS) / MEDIO_MS);
  galeriaFundido_ = 0;
  galeriaDesde_ = ahora;
  return 1;
}

// La imagen que corresponde mostrar ahora, o -1 si va una cara.
int Sistema::imagenParaMostrar(uint32_t ahora) {
  // El protector de pantalla (mientras entra, está o sale).
  if (faseProtector_ > 0 && !apagado_ && !emparejando_) return config_.protector.imagen;
  // Galería: solo las imágenes, sin la cara (si no hay ninguna, la mascota de siempre).
  if (config_.modo == Modo::Galeria) {
    if (apagado_ || emparejando_ || alarma_ || globo_ || recVisible_ >= 0) return -1;
    return imagenDeGaleria(posGaleria_);
  }
  if (apagado_ || emparejando_ || motor_.hayFija() || animPedida_ >= 0 || largoLista() == 0 ||
      panel_ != Panel::Ninguno || recVisible_ >= 0 || config_.modo == Modo::Reloj || config_.modo == Modo::Pomodoro ||
      config_.modo == Modo::Clima || alarma_ || globo_)
    return -1;
  const ElementoLista e = elementoLista(posLista_);
  if (e.tipo != ElementoLista::Tipo::Imagen || imagenes_[e.indice].largo == 0 || subiendo_ == e.indice) return -1;
  // Cada tanto vuelve la cara un par de segundos: nunca una imagen quieta del todo.
  if (proximaVueltaCara_ == 0)
    proximaVueltaCara_ = ahora + azar_.entre(tiemposImagen::VUELTA_MIN_MS, tiemposImagen::VUELTA_MAX_MS);
  if (static_cast<int32_t>(ahora - proximaVueltaCara_) >= 0) {
    vueltaCaraHasta_ = ahora + tiemposImagen::VUELTA_DURA_MS;
    proximaVueltaCara_ = vueltaCaraHasta_ + azar_.entre(tiemposImagen::VUELTA_MIN_MS, tiemposImagen::VUELTA_MAX_MS);
  }
  if (static_cast<int32_t>(ahora - vueltaCaraHasta_) < 0) return -1;
  return e.indice;
}

void Sistema::bleImagen(uint8_t op, const uint8_t* d, size_t n) {
  using namespace protocolo;
  const uint8_t car = CarImagen;
  char clave[6];
  switch (op) {
    case ImgInicio: {
      if (n != 10 || !espacioValido(d[1])) return responder(car, op, Malformado);
      const uint32_t largo = leer32(d + 2);
      const uint32_t limite = esSprite(d[1]) ? LIMITE_SPRITE : limiteImagen(perfil_.ancho, perfil_.alto);
      if (largo < 22 || largo > limite) return responder(car, op, MuyGrande);
      // Que entre: lo que ya ocupa ese espacio se libera al reemplazarlo, y
      // queda un margen para los bloques y los metadatos del sistema de archivos.
      if (hal_.espacioLibre && static_cast<int64_t>(largo) + MARGEN_ARCHIVOS > hal_.espacioLibre() + meta(d[1])->largo)
        return responder(car, op, MuyGrande, "no hay lugar");
      subiendo_ = d[1];
      subidaLargo_ = largo;
      subidaCrc_ = leer32(d + 6);
      subidaRecibido_ = 0;
      subidaCrcAcum_ = 0;
      *meta(subiendo_) = MetaImagen();
      if (esSprite(subiendo_)) {
        // La cara que lo usa se va con un fundido (los sprites que se ven
        // siguen en la RAM) y vuelve cuando llega el nuevo.
        guardarSprites();
        actualizarSprites();
      } else {
        guardarImagenes();
        imagenSucia_ = true;
        imagenPreparada_ = -1;
      }
      return responder(car, op, Ok);
    }
    case ImgBloque: {
      if (subiendo_ < 0) return responder(car, op, SinInicio);
      if (n < 5) return responder(car, op, Malformado);
      const uint32_t off = leer32(d + 1);
      const size_t m = n - 5;
      claveImagen(clave, subiendo_);
      if (off != subidaRecibido_ || subidaRecibido_ + m > subidaLargo_ || !hal_.escribirTrozo ||
          !hal_.escribirTrozo(clave, off, d + 5, m)) {
        if (hal_.borrar) hal_.borrar(clave);
        subiendo_ = -1;
        return responder(car, op, off != subidaRecibido_ ? FueraDeOrden : MuyGrande);
      }
      subidaCrcAcum_ = crc32(d + 5, m, subidaCrcAcum_);
      subidaRecibido_ += static_cast<uint32_t>(m);
      return;  // los bloques buenos no se confirman uno por uno
    }
    case ImgFin: {
      if (subiendo_ < 0) return responder(car, op, SinInicio);
      const int espacio = subiendo_;
      subiendo_ = -1;
      claveImagen(clave, espacio);
      if (subidaRecibido_ != subidaLargo_ || subidaCrcAcum_ != subidaCrc_) {
        if (hal_.borrar) hal_.borrar(clave);
        return responder(car, op, subidaRecibido_ != subidaLargo_ ? FueraDeOrden : CrcDistinto);
      }
      const char* porQue = "";
      if (esSprite(espacio) ? !spriteValido(clave, &porQue) : !imagenValida(clave, subidaLargo_, &porQue)) {
        if (hal_.borrar) hal_.borrar(clave);
        return responder(car, op, ImagenInvalida, porQue);
      }
      meta(espacio)->largo = subidaLargo_;
      meta(espacio)->crc = subidaCrc_;
      if (esSprite(espacio)) {
        guardarSprites();
        actualizarSprites();
        log("sprite guardado");
      } else {
        guardarImagenes();
        imagenSucia_ = true;
        imagenPreparada_ = -1;
        log("imagen guardada");
      }
      return responder(car, op, Ok);
    }
    case ImgBorrar: {
      if (n != 2 || !espacioValido(d[1])) return responder(car, op, Malformado);
      claveImagen(clave, d[1]);
      if (hal_.borrar) hal_.borrar(clave);
      *meta(d[1]) = MetaImagen();
      if (esSprite(d[1])) {
        guardarSprites();
        actualizarSprites();
      } else {
        guardarImagenes();
        imagenSucia_ = true;
        imagenPreparada_ = -1;
      }
      return responder(car, op, Ok);
    }
    case ImgListarSprites: {
      uint8_t b[4 + MAX_SPRITES * 9];
      size_t i = 4;
      uint16_t mascara = 0;
      for (int k = 0; k < MAX_SPRITES; k++) {
        if (!sprites_[k].largo) continue;
        mascara = static_cast<uint16_t>(mascara | (1u << k));
        b[i++] = static_cast<uint8_t>(PRIMER_SPRITE + k);
        escribir32(b + i, sprites_[k].largo);
        escribir32(b + i + 4, sprites_[k].crc);
        i += 8;
      }
      b[0] = op | Respuesta;
      b[1] = Ok;
      b[2] = static_cast<uint8_t>(mascara), b[3] = static_cast<uint8_t>(mascara >> 8);
      enviar(car, b, i);
      return;
    }
    case ImgListar: {
      uint8_t b[3 + MAX_IMAGENES * 9];
      size_t i = 3;
      uint8_t mascara = 0;
      for (int k = 0; k < MAX_IMAGENES; k++) {
        if (!imagenes_[k].largo) continue;
        mascara |= static_cast<uint8_t>(1u << k);
        b[i++] = static_cast<uint8_t>(k);
        escribir32(b + i, imagenes_[k].largo);
        escribir32(b + i + 4, imagenes_[k].crc);
        i += 8;
      }
      b[0] = op | Respuesta;
      b[1] = Ok;
      b[2] = mascara;
      enviar(car, b, i);
      return;
    }
    case ImgPedir: {
      if (n != 2 || !espacioValido(d[1])) return responder(car, op, Malformado);
      if (!meta(d[1])->largo) return responder(car, op, NoExiste);
      return enviarImagen(d[1]);
    }
    default:
      return responder(car, op, Malformado);
  }
}

int Sistema::largoLista() const {
  // Sin lista propia, el broche recorre las caras; la mascota no tiene lista.
  if (config_.nLista > 0) return config_.nLista;
  return perfil_.boton == Perfil::Boton::Broche ? config_.nExpresiones : 0;
}

ElementoLista Sistema::elementoLista(int i) const {
  if (config_.nLista > 0) return config_.lista[i % config_.nLista];
  ElementoLista e;
  e.indice = static_cast<uint8_t>(i % config_.nExpresiones);
  return e;
}

void Sistema::aplicarLista(uint32_t ahora) {
  const int n = largoLista();
  if (n == 0) {
    motor_.base(Config::Cara());
    return;
  }
  if (posLista_ >= n) posLista_ = 0;
  const ElementoLista e = elementoLista(posLista_);
  // Las imágenes y las animaciones se dibujan aparte; mientras tanto la cara de
  // base es la neutral.
  if (e.tipo == ElementoLista::Tipo::Cara) {
    motor_.base(e.indice);
  } else if (e.tipo == ElementoLista::Tipo::CaraVida) {
    Config::Cara c;
    c.vida = static_cast<int8_t>(e.indice);
    motor_.base(c);
  } else {
    motor_.base(Config::Cara());
  }
  animListaDesde_ = ahora;
}

void Sistema::siguiente() {
  const int n = largoLista();
  if (n == 0) return;
  posLista_ = static_cast<uint8_t>((posLista_ + 1) % n);
  ultimaRotacion_ = hal_.ahoraMs();
  aplicarLista(ultimaRotacion_);
  guardarAjustes();
}

void Sistema::mostrarQr() {
  emparejando_ = true;
  emparejarHasta_ = hal_.ahoraMs() + QR_POR_BOTON_MS;
}

void Sistema::dormir() {
  apagado_ = true;
  guardarAjustes();
  log("a dormir");
}

void Sistema::tacto(bool apretado) {
  const uint32_t ahora = hal_.ahoraMs();
  actividad();
  // Sonando el despertador: un toque lo pospone; mantenerlo apretado, lo apaga
  // (en tick, a los 1,5 s). Ni el toque ni el soltar hacen nada más.
  if (alarma_) {
    if (apretado) {
      alarmaApretado_ = true;
      alarmaToque_ = ahora;
    } else if (alarmaApretado_) {
      alarmaApretado_ = false;
      if (ahora - alarmaToque_ < tiemposPanel::ALARMA_APAGAR_MS) posponerAlarma();
      else apagarAlarma();
    }
    return;
  }
  if (tragarAlarma_) {
    if (!apretado) tragarAlarma_ = false;
    return;
  }
  // Jugando, los toques son del juego.
  if (juego_ != Juego::Ninguno) {
    tactoJuego(apretado, ahora);
    return;
  }
  // Esperando el "¿cómo estás?": toque = bien, doble = más o menos, mantener = mal.
  if (preguntando_) {
    if (apretado) {
      pregApretado_ = true;
      pregToque_ = ahora;
      pregToques_++;
    } else if (pregApretado_) {
      pregApretado_ = false;
      if (ahora - pregToque_ >= 800) responderAnimo(2);  // mantener: mal
      else if (pregToques_ >= 2) responderAnimo(1);
      else pregToque_ = ahora;  // espera el segundo toque (Sistema::cuidar)
    }
    return;
  }
  // Preguntando "¿{habito}?": toque = hecho, mantener = todavía no (en tick, a los 0,8 s).
  if (habPregunta_ >= 0) {
    if (apretado) {
      habApretado_ = true;
      habToque_ = ahora;
    } else if (habApretado_) {
      habApretado_ = false;
      if (ahora - habToque_ >= 800) todaviaNo(ahora);
      else marcarHabito(habPregunta_, 0, true);
    }
    return;
  }
  // Un toque de un amigo: en los 30 s, tocarlo lo devuelve (y nada más).
  if (apretado && toqueHasta_) {
    devolverToque(ahora);
    tragarSoltar_ = true;
    return;
  }
  // Un toque cierra el recordatorio, y ese toque no hace nada más.
  if (apretado && recVisible_ >= 0) {
    cerrarRecordatorio();
    motor_.interaccion(ahora);
    tragarSoltar_ = true;
    hablar(Situacion::RecordatorioHecho);
    return;
  }
  // Un toque cierra el globo (y tampoco hace nada más).
  if (apretado && globo_) {
    globoHasta_ = ahora;
    hayPendiente_ = false;
    motor_.interaccion(ahora);
    tragarSoltar_ = true;
    return;
  }
  if (!apretado && tragarSoltar_) {
    tragarSoltar_ = false;
    return;
  }
  // En modo reloj, tocarlo muestra la cara un rato.
  if (apretado && (config_.modo == Modo::Reloj || config_.modo == Modo::Clima))
    caraHasta_ = ahora + tiemposPanel::CARA_EN_RELOJ_MS;
  if (perfil_.boton == Perfil::Boton::Mascota) {
    gestos_.tacto(apretado, ahora);
    return;
  }
  botonBroche(apretado, ahora);
}

void Sistema::botonBroche(bool apretado, uint32_t ahora) {
  if (apretado == botonApretado_) return;
  botonApretado_ = apretado;
  if (apretado) {
    botonDesde_ = ahora;
    qrPorBoton_ = false;
    // Dormido, un toque lo despierta (en la placa, arranca de nuevo).
    ignorarSoltar_ = apagado_;
    if (apagado_) {
      apagado_ = false;
      motor_.interaccion(ahora);
      log("despierto");
    }
    return;
  }
  if (ignorarSoltar_) {
    ignorarSoltar_ = false;
    return;
  }
  const uint32_t dur = ahora - botonDesde_;
  if (qrPorBoton_) return;
  if (dur < tiemposBroche::TOQUE_MS) {
    motor_.interaccion(ahora);
    // En pomodoro, el toque arranca o pausa; en reloj ya mostró la cara.
    if (config_.modo == Modo::Pomodoro) pomoCorriendo_ = !pomoCorriendo_;
    else if (config_.modo != Modo::Reloj) siguiente();
  } else if (dur >= tiemposBroche::DORMIR_MS) {
    dormir();
  }
}

void Sistema::dibujarPantalla(uint32_t ahora) {
  (void)ahora;
  if (apagado_) {
    lienzo_.limpiar(0x000000);
    return;
  }
  if (emparejando_) {
    dibujarEmparejamiento(lienzo_, serie_, qr_, "ESPERANDO VÍNCULO", perfil_.redonda);
    return;
  }
  if (panel_ != Panel::Ninguno && fasePanel_ > 0) {
    dibujarPanel();
    if (depurar_) dibujarDepuracion();
    return;
  }
  if (animVisible_ >= 0) {
    lienzo_.limpiar(paleta::PANTALLA);
    const Animacion& a = config_.animaciones[animVisible_];
    if (a.sobre >= 0) dibujarExpresion(lienzo_, vista_, config_.expresiones[a.sobre], motor_.estado());
    dibujarAnimacion(lienzo_, vista_, a, animT_, motor_.estado());
    if (depurar_) dibujarDepuracion();
    return;
  }
  // El rostro paramétrico siempre; un dibujo propio del usuario (formas) va
  // encima con fundido, nunca con un corte.
  const int dibujo = motor_.dibujo();
  const float op = motor_.opacidadDibujo();
  if (dibujo >= 0 && op >= 0.999f) {
    lienzo_.limpiar(paleta::PANTALLA);
  } else {
    dibujarRostro(lienzo_, vistaCara(), motor_.cuadro(), &cache_, opSprites_, ocultarOjos_);
  }
  if (dibujo >= 0 && op > 0.001f) {
    lienzo_.opacidad(op);
    if (op < 0.999f) {
      // Tapar el rostro de a poco.
      lienzo_.opacidad(op);
      lienzo_.pildora(lienzo_.ancho() / 2.0f, (lienzo_.franja() + lienzo_.filasFranja() / 2.0f), lienzo_.ancho() + 4.0f,
                      lienzo_.filasFranja() + 4.0f, 0, paleta::PANTALLA);
    }
    dibujarExpresion(lienzo_, vista_, config_.expresiones[dibujo], motor_.estado());
    lienzo_.opacidad(1);
  }
  if (depurar_) dibujarDepuracion();
}

void Sistema::dibujarDepuracion() {
  // Arriba a la izquierda: el comportamiento, el estado interno en barras y los fps.
  const float x = perfil_.redonda ? 52 : 6;
  float y = perfil_.redonda ? 52 : 4;
  char linea[48];
  const char* c = motor_.nombreComportamiento();
  size_t n = copiarTexto(linea, sizeof linea, c[0] ? c : "-");
  for (size_t i = 0; i < n; i++)
    if (linea[i] >= 'a' && linea[i] <= 'z') linea[i] = static_cast<char>(linea[i] - 32);
  dibujarTexto(lienzo_, fuentes::MONO_9, linea, x, y + 9, paleta::TINTA);
  y += 13;
  const float* e = motor_.estadoInterno();
  for (int i = 0; i < N_ESTADO; i++) {
    lienzo_.rectangulo(static_cast<int>(x), static_cast<int>(y + i * 5), 40, 3, paleta::LINEA);
    lienzo_.rectangulo(static_cast<int>(x), static_cast<int>(y + i * 5), static_cast<int>(40 * limitar(e[i], 0, 1)), 3,
                       i == 6 ? 0xFF3820 : paleta::NARANJA);
  }
  y += N_ESTADO * 5 + 2;
  const int f = static_cast<int>(fps_ + 0.5f);
  linea[0] = static_cast<char>('0' + (f / 10) % 10), linea[1] = static_cast<char>('0' + f % 10);
  copiarTexto(linea + 2, sizeof linea - 2, " FPS");
  dibujarTexto(lienzo_, fuentes::MONO_9, linea, x, y + 9, paleta::TINTA_SUAVE);
}

void Sistema::tick() {
  const uint32_t ahora = hal_.ahoraMs();
  const uint32_t dtCuadro = ahora - ultimoCuadro_;
  ultimoCuadro_ = ahora;
  if (dtCuadro > 0 && dtCuadro < 2000) fps_ = mezclar(fps_, 1000.0f / dtCuadro, 0.1f);
  gestos_.tick(ahora);
  Evento e;
  while (gestos_.sacar(&e)) {
    // En pomodoro, un toque arranca o pausa (y el bicho igual reacciona).
    if (e == Evento::Tacto && config_.modo == Modo::Pomodoro) pomoCorriendo_ = !pomoCorriendo_;
    // En galería, un toque pasa a la siguiente imagen.
    if (e == Evento::Tacto && config_.modo == Modo::Galeria) pasarGaleria(ahora), galeriaDesde_ = ahora;
    evento(e);
  }
  // Inclinado: los ojos "caen" hacia donde está la gravedad.
  float gx, gy;
  if (gestos_.inclinacion(&gx, &gy)) motor_.seguir(-gx / 0.7f, -gy / 0.7f, ahora);
  vigilar(ahora);
  revisarRecordatorios(ahora);
  revisarDespertador(ahora);
  avanzarAlarma(ahora);
  cuidar(ahora);
  revisarCompania(ahora);
  avanzarJuego(ahora);
  revisarCharla(ahora);
  // Mantenerlo apretado mientras pregunta "¿cómo estás?" ya es "mal".
  if (preguntando_ && pregApretado_ && ahora - pregToque_ >= 800) {
    pregApretado_ = false;
    responderAnimo(2);
  }
  avanzarPomodoro(ahora, dtCuadro);
  // El tamaño de la cara (cara.tamano): hacia el de la config con un fundido
  // de ~150 ms que no depende de los cuadros por segundo (nunca de golpe).
  if (dtCuadro > 0) {
    const float k = 1.0f - exponencial(-static_cast<float>(dtCuadro < 2000 ? dtCuadro : 2000) / 150.0f);
    tamanoVisto_ += (tamanoObjetivo() - tamanoVisto_) * k;
  }
  // v3: si llueve, más tranquilo.
  motor_.clima(climaVigente() && situacionClima(clima_) == Situacion::ClimaLluvia);
  motor_.tick(ahora, contexto());
  avanzarVoz(ahora);
  avanzarSprites(dtCuadro);
  avanzarPanel(ahora, dtCuadro);
  avanzarProtector(ahora, dtCuadro);
  // El broche, de noche y ya dormido, apaga la pantalla hasta la mañana (no
  // con un recordatorio en pantalla ni un pomodoro andando).
  if (perfil_.boton == Perfil::Boton::Broche && !apagado_ && !vistaPrevia_ && motor_.quiereSuenoProfundo() &&
      recVisible_ < 0 && !pomoCorriendo_ && !alarma_)
    dormir();

  // Manteniendo el botón del broche para dormirlo: se le cierran los ojos.
  if (perfil_.boton == Perfil::Boton::Broche && botonApretado_ && !ignorarSoltar_ && !alarma_ &&
      ahora - botonDesde_ >= tiemposBroche::DORMIR_MS && !qrPorBoton_)
    motor_.reaccionar(config_.resolverCara("dormido"), ahora, 150);
  // Mantener el botón (5 s la mascota, 8 s el broche) muestra el QR para vincular otro celu.
  bool qr = gestos_.sacarMantenido() && perfil_.qrPorMantener;
  if (perfil_.boton == Perfil::Boton::Broche && botonApretado_ && !ignorarSoltar_ && !qrPorBoton_ &&
      ahora - botonDesde_ >= tiemposBroche::QR_MS) {
    qrPorBoton_ = true;
    qr = true;
  }
  if (qr) {
    emparejando_ = true;
    emparejarHasta_ = ahora + QR_POR_BOTON_MS;
  }
  // La galería pasa sola cada tantos segundos.
  if (config_.modo == Modo::Galeria && !galeriaFundido_ &&
      ahora - galeriaDesde_ >= static_cast<uint32_t>(config_.galeriaSegundos) * 1000u)
    pasarGaleria(ahora), galeriaDesde_ = ahora;
  // La lista rota sola cada tantos minutos.
  if (config_.rotarMinutos && largoLista() > 1 &&
      ahora - ultimaRotacion_ >= static_cast<uint32_t>(config_.rotarMinutos) * 60000u)
    siguiente();
  if (emparejando_ && emparejarHasta_ && nClaves_ > 0 && static_cast<int32_t>(ahora - emparejarHasta_) >= 0)
    emparejando_ = false;

  if (vistaPrevia_) emparejando_ = false;
  elegirAnimacion(ahora);
  const float alfaGal = config_.modo == Modo::Galeria ? alfaGaleria(ahora) : 1;
  int imagen = imagenParaMostrar(ahora);
  if (imagen >= 0 && imagen != imagenPreparada_ && !prepararImagen(imagen, ahora)) {
    // No se pudo leer: se marca vacía para no intentarlo en cada cuadro.
    imagenes_[imagen] = MetaImagen();
    imagen = -1;
    log("no pude leer la imagen; la saco");
  }
  if (imagen >= 0 && imagen == mostrandoImagen_ && animCuadros_ > 1 && static_cast<int32_t>(ahora - animProximo_) >= 0) {
    // Una animación: el cuadro que sigue.
    if (cargarCuadro(imagen, (animCuadro_ + 1) % animCuadros_, ahora)) imagenSucia_ = true;
  }
  if (imagen < 0) imagenPreparada_ = -1;
  // Entrando o saliendo del protector: la imagen sobre la cara, con fundido.
  float alfaImagen = imagen >= 0 && faseProtector_ > 0 ? suave(faseProtector_) : 1;
  if (alfaGal < alfaImagen) alfaImagen = alfaGal;
  if (imagen >= 0 && imagen == mostrandoImagen_ && !imagenSucia_ && alfaImagen >= 1) {
    // La imagen ya está en la pantalla: no hace falta redibujarla.
    notificarEstadoSiCambio();
    return;
  }
  mostrandoImagen_ = imagen;
  imagenSucia_ = false;
  bool imagenOk = imagen >= 0;
  // De a una franja: en placas sin PSRAM no entra la pantalla entera.
  for (int y0 = 0; y0 < lienzo_.alto(); y0 += lienzo_.filasPorFranja()) {
    lienzo_.empezarFranja(y0);
    if (imagenOk && alfaImagen < 1) dibujarPantalla(ahora);
    if (imagenOk) imagenOk = dibujarImagen(imagen, alfaImagen);
    if (!imagenOk) dibujarPantalla(ahora);
    if (hal_.presentarFranja) hal_.presentarFranja(lienzo_.pixeles(), lienzo_.ancho(), y0, lienzo_.filasFranja());
  }
  if (!hal_.presentarFranja && hal_.presentar && lienzo_.filasPorFranja() >= lienzo_.alto())
    hal_.presentar(lienzo_.pixeles(), lienzo_.ancho(), lienzo_.alto());
  if (imagen >= 0 && !imagenOk) {
    // No se pudo leer: se marca vacía para no intentarlo en cada cuadro.
    imagenes_[imagen] = MetaImagen();
    mostrandoImagen_ = -1;
    log("no pude leer la imagen; la saco");
  }
  notificarEstadoSiCambio();
}

// ---------------------------------------------------------------- recordatorios, reloj y pomodoro

namespace {
// Un texto partido en líneas: [ini, fin) de cada una, en bytes.
constexpr int MAX_LINEAS = 4;
struct Lineas {
  int n = 0;
  uint8_t ini[MAX_LINEAS], fin[MAX_LINEAS];
};

float anchoTramo(const Fuente& f, const char* s, int a, int b) {
  char t[LARGO_RECORDATORIO];
  copiarTexto(t, sizeof t, s + a, static_cast<size_t>(b - a));
  return anchoTexto(f, t);
}

// Parte `s` en hasta `max` líneas, la i-ésima de `anchos[i]` píxeles como
// mucho. Sin `cortar`, solo entre palabras. false si no entra.
bool partir(const Fuente& f, const char* s, const float* anchos, int max, bool cortar, Lineas* l) {
  const int n = static_cast<int>(largo(s));
  int pos = 0;
  l->n = 0;
  while (true) {
    while (pos < n && s[pos] == ' ') pos++;
    if (pos >= n) break;
    if (l->n >= max) return false;
    const float w = anchos[l->n];
    int fin = pos, k = pos;
    while (k < n) {
      int e = k;
      while (e < n && s[e] != ' ') e++;
      if (anchoTramo(f, s, pos, e) > w) break;
      fin = e;
      k = e;
      while (k < n && s[k] == ' ') k++;
    }
    if (fin == pos) {
      // Una palabra que no entra: se corta por letras (sin partir una tilde).
      if (!cortar) return false;
      int e = pos;
      while (e < n) {
        int sig = e + 1;
        while (sig < n && (static_cast<uint8_t>(s[sig]) & 0xC0) == 0x80) sig++;
        if (e > pos && anchoTramo(f, s, pos, sig) > w) break;
        e = sig;
      }
      fin = e;
    }
    l->ini[l->n] = static_cast<uint8_t>(pos);
    l->fin[l->n] = static_cast<uint8_t>(fin);
    l->n++;
    pos = fin;
  }
  return l->n > 0;
}

// Las fuentes mono solo tienen mayúsculas (con tildes y Ñ).
void aMayusculas(char* s) {
  for (; *s; s++) {
    const uint8_t c = static_cast<uint8_t>(*s);
    if (c >= 'a' && c <= 'z') *s = static_cast<char>(c - 32);
    else if (c == 0xC3 && s[1] && static_cast<uint8_t>(s[1]) >= 0xA0 && static_cast<uint8_t>(s[1]) <= 0xBE &&
             static_cast<uint8_t>(s[1]) != 0xB7)
      s[1] = static_cast<char>(static_cast<uint8_t>(s[1]) - 0x20), s++;
  }
}

int minutosEntre(int desde, int hasta) { return ((hasta - desde) % 1440 + 1440) % 1440; }
}  // namespace

const char* Sistema::vocalizacion(const char* sonido) const {
  if (!vozModo_ || !hal_.audio || !sonido) return nullptr;
  if (iguales(sonido, "voz:", 4)) {
    const char* k = sonido + 4;
    for (const VozBase& v : caracter::VOZ_CARAS)
      if (iguales(v.clave, k)) return v.texto;
    for (const VozBase& v : caracter::VOZ_EVENTOS)
      if (iguales(v.clave, k)) return v.texto;
    return nullptr;
  }
  for (const VozBase& v : caracter::VOZ_SONIDOS)
    if (iguales(v.clave, sonido)) return v.texto;
  return nullptr;
}

void Sistema::sonar(const char* nombre) {
  // "voz": volumen 0 silencia todo.
  if (config_.voz.volumen == 0) return;
  // Con la voz propia, los de "voz.sonidos" (y las vocalizaciones "voz:…")
  // los balbucea ella y no salen por Hal::sonido: el que tiene parlante no
  // los duplica. Si ya está diciendo algo, no lo pisa (y tampoco suena).
  const char* v = vocalizacion(nombre);
  if (v) {
    if (voz_.hablando()) return;
    const float* est = motor_.estadoInterno();
    voz_.empezar(v, paramsVoz(static_cast<int>(config_.voz.tipo), config_.voz.volumen, motor_.rasgos(), est[0], est[1]));
    vozSuelta_ = true;
    vozDesde_ = hal_.ahoraMs();
    vozEnviadas_ = 0;
    return;
  }
  if (iguales(nombre, "voz:", 4)) return;  // una vocalización sin voz propia: nada
  if (hal_.sonido) hal_.sonido(nombre);
}

// Un evento que genera el sistema (recordatorio, pomodoro): además del
// registro, se avisa ya, como una respuesta de Registro, por BLE y por la
// nube: [evento] o, con un tipo (un recordatorio, un juego, el ánimo),
// [evento][tipo con su dato].
void Sistema::notificarEvento(Evento e, int tipo, uint8_t dato) {
  using namespace protocolo;
  EntradaRegistro r[1];
  const uint32_t seg = motor_.registro(r, 1) == 1 ? r[0].segundos : 0;
  uint8_t b[3 + 2 * 6];
  size_t n = 3;
  b[0] = static_cast<uint8_t>(CmdRegistro | Respuesta);
  b[1] = Ok;
  b[2] = tipo >= 0 ? 2 : 1;
  escribir32(b + n, seg);
  b[n + 4] = 0;
  b[n + 5] = static_cast<uint8_t>(e);
  n += 6;
  if (tipo >= 0) {
    escribir32(b + n, seg);
    b[n + 4] = static_cast<uint8_t>(tipo);
    b[n + 5] = dato;
    n += 6;
  }
  if (hal_.notificar) hal_.notificar(CarComando, b, n);
  if (hal_.notificarNube) hal_.notificarNube(CarComando, b, n);
}

int Sistema::segundosHastaDespertar() const {
  int s = motor_.segundosHastaDespertar();
  const int m = minutoDelDia();
  if (m < 0) return s;
  const uint32_t e = epoch();
  const int seg = e ? static_cast<int>(e % 60u) : 0;
  for (int i = 0; i < config_.nRecordatorios; i++) {
    const Recordatorio& r = config_.recordatorios[i];
    if (!r.activo) continue;
    int d = minutosEntre(m, r.minuto);
    if (d == 0) d = 1440;
    const int x = maxi(1, d * 60 - seg);
    if (s < 0 || x < s) s = x;
  }
  // El despertador (y uno pospuesto) también lo despiertan.
  if (config_.despertador.hay && config_.despertador.activo) {
    int d = minutosEntre(m, config_.despertador.minuto);
    if (d == 0) d = 1440;
    const int x = maxi(1, d * 60 - seg);
    if (s < 0 || x < s) s = x;
  }
  if (pospuestaHasta_) {
    const int x = maxi(1, static_cast<int>((pospuestaHasta_ - hal_.ahoraMs()) / 1000u));
    if (s < 0 || x < s) s = x;
  }
  return s;
}

// ---------------------------------------------------------------- lo que dice, el despertador y el clima

void Sistema::decir(const char* texto, Config::Cara cara, bool hablando) {
  copiarTexto(globoPendiente_, sizeof globoPendiente_, texto);
  caraPendiente_ = cara;
  pendienteHablando_ = hablando;
  hayPendiente_ = true;
  // Si ya hay uno, primero se va (con su fundido) y después entra este.
  if (globo_) globoHasta_ = hal_.ahoraMs();
  actividad();
}

void Sistema::mostrarGlobo(uint32_t ahora) {
  hayPendiente_ = false;
  copiarTexto(globoTexto_, sizeof globoTexto_, globoPendiente_);
  globo_ = true;
  const uint32_t dura = duracionGlobo(globoTexto_);
  globoHasta_ = ahora + dura + tiemposPanel::FUNDIDO_MS;
  if (motor_.dormido()) motor_.despertar(ahora, false);
  // Con parlante, lo dice con su voz (el balbuceo sigue el texto del globo).
  if (vozModo_ == 1 && hal_.audio && config_.voz.volumen > 0) {
    vozSuelta_ = false;
    const float* est = motor_.estadoInterno();
    voz_.empezar(globoTexto_, paramsVoz(static_cast<int>(config_.voz.tipo), config_.voz.volumen, motor_.rasgos(), est[0], est[1]));
    vozDesde_ = ahora;
    vozEnviadas_ = 0;
  }
  if (caraPendiente_.existe()) {
    motor_.reaccionar(caraPendiente_, ahora, dura);
  } else if (pendienteHablando_) {
    const Config::Cara h = config_.resolverCara("hablando");
    if (h.existe()) motor_.reaccionar(h, ahora, dura < 2500 ? dura : 2500);
  }
  actividad();
}

void Sistema::hablar(Situacion s, Plantillas extra) {
  if (apagado_ || emparejando_) return;
  if (!extra.n && hablarV3(s)) return;  // las de la v3 llevan sus plantillas
  const bool siempre = situacionSiempre(s);
  // "Callate": una hora sin frases espontáneas.
  if (!siempre && calladoHasta_ && static_cast<int32_t>(hal_.ahoraMs() - calladoHasta_) < 0) return;
  // Las espontáneas no esperan: si ya está mostrando algo, no dice nada.
  if (!siempre && (globo_ || hayPendiente_ || recVisible_ >= 0 || alarma_ || juego_ != Juego::Ninguno)) return;
  // El buen día con el tiempo: si llueve o hace frío, mejor el consejo.
  char c[40] = {};
  if (climaVigente()) {
    textoClima(clima_, c, sizeof c);
    const Situacion sc = situacionClima(clima_);
    if (s == Situacion::BuenDia && (sc == Situacion::ClimaLluvia || sc == Situacion::ClimaFrio)) s = sc;
  }
  const uint32_t e = epoch();
  if (!hablador_.quiere(s, hal_.ahoraMs() / 1000u, e ? e / 86400u : 0, config_.habla.frecuencia, azar_)) return;
  char t[LARGO_FRASE];
  if (!elegirFrase(config_.habla, s, presetCercano(motor_.rasgos()), config_.vida.dueno, nombre(), azar_, t, sizeof t, c,
                   extra))
    return;
  decir(t);
}

void Sistema::revisarDespertador(uint32_t ahora) {
  if (pospuestaHasta_ && !alarma_ && static_cast<int32_t>(ahora - pospuestaHasta_) >= 0) {
    pospuestaHasta_ = 0;
    sonarAlarma(false);
    return;
  }
  const Despertador& d = config_.despertador;
  if (vistaPrevia_ || !d.hay || !d.activo || alarma_) return;
  const int m = minutoDelDia();
  if (m < 0 || m == alarmaMinuto_) return;
  alarmaMinuto_ = static_cast<int16_t>(m);
  // Hasta un minuto tarde (el broche se despierta del sueño profundo justo a la hora).
  const int tarde = minutosEntre(d.minuto, m);
  if (tarde > 1) return;
  const uint32_t e = epoch();
  if (e) {
    const uint32_t dia = (e - static_cast<uint32_t>(tarde) * 60u) / 86400u;
    const int semana = static_cast<int>((dia + 3) % 7);  // 0 = lunes
    if (!(d.dias & (1u << semana))) return;
    const uint16_t clave = static_cast<uint16_t>(dia + 1);
    if (alarmaDia_ == clave) return;
    alarmaDia_ = clave;
  } else if (d.dias != 0x7F || tarde != 0) {
    return;
  }
  sonarAlarma(true);
}

void Sistema::sonarAlarma(bool porHora) {
  const uint32_t ahora = hal_.ahoraMs();
  alarma_ = true;
  alarmaDesde_ = alarmaTono_ = ahora;
  alarmaApretado_ = false;
  pospuestaHasta_ = 0;
  globo_ = hayPendiente_ = false;
  // La cara de despertar (si dormía, la rutina "despertar", sin mal humor).
  if (motor_.dormido()) {
    motor_.despertar(ahora, false);
  } else {
    const int i = config_.vida.indiceComportamiento("despertar");
    if (i >= 0) motor_.comportamiento(i, ahora);
  }
  char t[LARGO_FRASE];
  // v3: si ayer la pospusiste 3 veces o más, "¿Hoy también vas a posponer?".
  uint8_t veces = 0;
  const uint16_t hoy = motor_.diaVida();
  Episodio ayer;
  ayer.dia = static_cast<uint16_t>(hoy - 1), ayer.tipo = static_cast<uint8_t>(TipoEpisodio::Posponer);
  const bool pospusoAyer = porHora && hoy && motor_.ocurrio(ayer.dia, TipoEpisodio::Posponer, &veces) && veces >= 3;
  ayer.dato = veces;
  if (pospusoAyer && textoEpisodio(ayer, false, t, sizeof t)) {
    // (ya está en t)
  } else if (config_.despertador.texto[0]) completarFrase(config_.despertador.texto, config_.vida.dueno, nombre(), t, sizeof t);
  else if (!elegirFrase(config_.habla, Situacion::Despertador, presetCercano(motor_.rasgos()), config_.vida.dueno, nombre(),
                        azar_, t, sizeof t))
    copiarTexto(t, sizeof t, "\xC2\xA1" "Buen d\xC3\xAD" "a!");
  size_t n = copiarTexto(globoTexto_, sizeof globoTexto_, t);
  // Y el clima, si hay (y entra).
  if (climaVigente()) {
    char c[40];
    const size_t k = textoClima(clima_, c, sizeof c);
    if (n + 1 + k < sizeof globoTexto_) {
      globoTexto_[n++] = ' ';
      copiarTexto(globoTexto_ + n, sizeof globoTexto_ - n, c);
    }
  }
  if (porHora) {
    evento(Evento::Despertador);
    notificarEvento(Evento::Despertador);
  }
  actividad();
  log("despertador");
}

void Sistema::avanzarAlarma(uint32_t ahora) {
  if (!alarma_) return;
  if (alarmaApretado_ && ahora - alarmaToque_ >= tiemposPanel::ALARMA_APAGAR_MS) {
    // Mantenerlo apretado lo apaga (y el soltar no hace nada más).
    alarmaApretado_ = false;
    tragarAlarma_ = true;
    apagarAlarma();
    return;
  }
  if (ahora - alarmaDesde_ >= tiemposPanel::ALARMA_MAX_MS) {
    apagarAlarma();
    return;
  }
  if (config_.despertador.sonido && static_cast<int32_t>(ahora - alarmaTono_) >= 0) {
    // Cada vez más fuerte: despertador:1 … despertador:5 (el perfil elige el volumen).
    const int nivel = mini(5, 1 + static_cast<int>((ahora - alarmaDesde_) / tiemposPanel::ALARMA_SUBE_MS));
    char s[16] = "despertador:1";
    s[12] = static_cast<char>('0' + nivel);
    sonar(s);
    alarmaTono_ = ahora + tiemposPanel::ALARMA_TONO_MS;
  }
}

void Sistema::posponerAlarma() {
  if (!alarma_) return;
  const uint32_t ahora = hal_.ahoraMs();
  alarma_ = false;
  pospuestaHasta_ = ahora + config_.despertador.posponer * 60000u;
  motor_.recordar(TipoEpisodio::Posponer, 0, true);  // v3: cuántas veces hoy
  motor_.interaccion(ahora);
  hablar(Situacion::Posponer);
}

void Sistema::apagarAlarma() {
  if (!alarma_ && !pospuestaHasta_) return;
  const uint32_t ahora = hal_.ahoraMs();
  alarma_ = false;
  pospuestaHasta_ = 0;
  motor_.interaccion(ahora);
  motor_.reaccionar(config_.resolverCara("contento"), ahora);
}

void Sistema::revisarRecordatorios(uint32_t ahora) {
  if (vistaPrevia_ || config_.nRecordatorios == 0) return;
  const int m = minutoDelDia();
  if (m < 0 || m == recMinuto_) return;
  recMinuto_ = static_cast<int16_t>(m);
  const uint32_t e = epoch();
  for (int i = 0; i < config_.nRecordatorios; i++) {
    const Recordatorio& r = config_.recordatorios[i];
    if (!r.activo) continue;
    // Hasta un minuto tarde (el broche se despierta del sueño profundo justo a la hora).
    const int tarde = minutosEntre(r.minuto, m);
    if (tarde > 1) continue;
    if (e) {
      // Con fecha: el día de la semana y una vez por día.
      const uint32_t dia = (e - static_cast<uint32_t>(tarde) * 60u) / 86400u;
      const int semana = static_cast<int>((dia + 3) % 7);  // el 1/1/1970 fue jueves; 0 = lunes
      if (!(r.dias & (1u << semana))) continue;
      const uint16_t clave = static_cast<uint16_t>(dia + 1);
      if (recDia_[i] == clave) continue;
      recDia_[i] = clave;
    } else {
      // Solo con el minuto del día: no se sabe qué día es (solo los de todos
      // los días) y se dispara al entrar en el minuto.
      if (r.dias != 0x7F || tarde != 0) continue;
    }
    if (recVisible_ >= 0 || (panel_ == Panel::Recordatorio && fasePanel_ > 0)) recPendientes_ |= static_cast<uint16_t>(1u << i);
    else mostrarRecordatorio(i, ahora);
  }
}

void Sistema::mostrarRecordatorio(int i, uint32_t ahora) {
  const Recordatorio& r = config_.recordatorios[i];
  recVisible_ = static_cast<int8_t>(i);
  recDesde_ = ahora;
  copiarTexto(recTexto_, sizeof recTexto_, r.texto);
  size_t k = copiarTexto(recNombre_, sizeof recNombre_, "recordatorio:");
  if (i >= 10) recNombre_[k++] = static_cast<char>('0' + i / 10);
  recNombre_[k++] = static_cast<char>('0' + i % 10);
  recNombre_[k] = 0;
  if (motor_.dormido()) motor_.despertar(ahora, false);
  Config::Cara c;
  c.vida = r.caraVida, c.dibujo = r.caraDibujo;
  motor_.fijar(c);  // por los resortes: la cara cambia sin corte
  if (r.sonido) sonar("recordatorio");
  evento(Evento::Recordatorio);
  motor_.anotarRegistro(4, static_cast<uint8_t>(i));
  notificarEvento(Evento::Recordatorio, 4, static_cast<uint8_t>(i));
  log("recordatorio");
}

void Sistema::cerrarRecordatorio() {
  if (recVisible_ < 0) return;
  recVisible_ = -1;
  motor_.fijar(fijaGuardada_);
}

void Sistema::reiniciarPomodoro() {
  pomoCorriendo_ = false;
  pomoDescanso_ = false;
  pomoRestanteMs_ = config_.pomodoro.trabajo * 60000u;
}

const char* Sistema::nombrePomodoro() const {
  if (!pomoCorriendo_) return "pomodoro:pausa";
  return pomoDescanso_ ? "pomodoro:descanso" : "pomodoro:trabajo";
}

void Sistema::avanzarPomodoro(uint32_t ahora, uint32_t dt) {
  if (config_.modo != Modo::Pomodoro || !pomoCorriendo_) return;
  if (dt < pomoRestanteMs_) {
    pomoRestanteMs_ -= dt;
    return;
  }
  // Terminó el tramo: avisa y pasa solo al siguiente (uno por cuadro, aunque
  // haya pasado mucho tiempo).
  dt -= pomoRestanteMs_;
  const bool eraTrabajo = !pomoDescanso_;
  pomoDescanso_ = eraTrabajo;
  pomoRestanteMs_ = (pomoDescanso_ ? config_.pomodoro.descanso : config_.pomodoro.trabajo) * 60000u;
  pomoRestanteMs_ -= dt < pomoRestanteMs_ ? dt : pomoRestanteMs_ - 1;
  motor_.reaccionar(config_.resolverCara(eraTrabajo ? "sorpresa" : "contento"), ahora);
  sonar("pomodoro");
  evento(Evento::Pomodoro);
  notificarEvento(Evento::Pomodoro);
}

void Sistema::avanzarPanel(uint32_t ahora, uint32_t dt) {
  if (recVisible_ >= 0 && ahora - recDesde_ >= tiemposPanel::RECORDATORIO_MS) cerrarRecordatorio();
  // Los que vencieron mientras se veía otro, cuando ese ya se fue.
  if (recVisible_ < 0 && recPendientes_ && (panel_ != Panel::Recordatorio || fasePanel_ <= 0)) {
    int i = 0;
    while (!(recPendientes_ & (1u << i))) i++;
    recPendientes_ = static_cast<uint16_t>(recPendientes_ & ~(1u << i));
    if (i < config_.nRecordatorios) mostrarRecordatorio(i, ahora);
  }
  // El globo se va solo a su tiempo; el que espera entra cuando el anterior ya salió.
  if (globo_ && static_cast<int32_t>(ahora - globoHasta_) >= 0) globo_ = false;
  if (!globo_ && hayPendiente_ && recVisible_ < 0 && !alarma_ &&
      (panel_ != Panel::Globo || fasePanel_ <= 0))
    mostrarGlobo(ahora);
  const bool caraPorToque = static_cast<int32_t>(ahora - caraHasta_) < 0;
  Panel deseado = Panel::Ninguno;
  if (recVisible_ >= 0) deseado = Panel::Recordatorio;
  else if (alarma_) deseado = Panel::Despertador;
  else if (juego_ != Juego::Ninguno) deseado = Panel::Juego;
  else if (globo_) deseado = Panel::Globo;
  else if (config_.modo == Modo::Reloj && !caraPorToque) deseado = Panel::Reloj;
  else if (config_.modo == Modo::Pomodoro) deseado = Panel::Pomodoro;
  else if (config_.modo == Modo::Clima && !caraPorToque) deseado = Panel::Clima;
  // En modo clima, la cara reacciona al tiempo cada tanto.
  if (panel_ == Panel::Clima && deseado == Panel::Clima && climaVigente() && static_cast<int32_t>(ahora - proximaCaraClima_) >= 0) {
    proximaCaraClima_ = ahora + tiemposPanel::CARA_CLIMA_MS;
    if (const char* cara = caraClima(clima_)) motor_.reaccionar(config_.resolverCara(cara), ahora, 6000);
  }
  // Nunca un corte: si cambia lo que se muestra, primero se va lo que había.
  const float paso = static_cast<float>(dt < 200 ? dt : 200) / tiemposPanel::FUNDIDO_MS;
  if (deseado != panel_) {
    fasePanel_ -= paso;
    if (fasePanel_ <= 0) {
      fasePanel_ = 0;
      panel_ = deseado;
    }
  } else if (panel_ != Panel::Ninguno) {
    fasePanel_ = minf(1, fasePanel_ + paso);
  }
}

// El protector de pantalla: después de `espera` segundos sin que pase nada,
// la imagen elegida con un fundido; cualquier cosa vuelve a la cara.
void Sistema::avanzarProtector(uint32_t ahora, uint32_t dt) {
  const int img = config_.protector.imagen;
  // Una reacción o un reflejo también es "que pase algo".
  if (motor_.reaccionando(ahora)) ultimaActividad_ = ahora;
  const bool hay = img >= 0 && img < MAX_IMAGENES && imagenes_[img].largo > 0 && subiendo_ != img;
  if (!hay) {
    faseProtector_ = 0;  // sin imagen (o se está subiendo otra): no hay protector
    return;
  }
  const bool quiere = !apagado_ && !emparejando_ && !motor_.dormido() && recVisible_ < 0 && animPedida_ < 0 && !alarma_ &&
                      !globo_ &&
                      ahora - ultimaActividad_ >= config_.protector.espera * 1000u;
  const float paso = static_cast<float>(dt < 200 ? dt : 200) / tiemposPanel::FUNDIDO_MS;
  const float antes = faseProtector_;
  faseProtector_ = quiere ? minf(1, faseProtector_ + paso) : maxf(0, faseProtector_ - paso);
  // Al terminar de entrar, un último cuadro con la imagen entera.
  if (faseProtector_ >= 1 && antes < 1) imagenSucia_ = true;
}

void Sistema::dibujarPanel() {
  const float f = suave(fasePanel_);
  const float W = static_cast<float>(lienzo_.ancho()), H = static_cast<float>(lienzo_.alto());
  const bool redonda = perfil_.redonda;
  // La cara se achica y sube; abajo aparece el texto.
  VistaRostro v = vistaCara();
  v.unidad = mezclar(v.unidad, v.unidad * 0.45f, f);
  v.origenY = mezclar(v.origenY, H * 0.27f, f);
  dibujarRostro(lienzo_, v, motor_.cuadro(), &cache_, opSprites_, ocultarOjos_);
  const int dibujo = motor_.dibujo();
  const float op = motor_.opacidadDibujo() * (1 - f);
  if (dibujo >= 0 && op > 0.001f) {
    lienzo_.opacidad(op);
    dibujarExpresion(lienzo_, vista_, config_.expresiones[dibujo], motor_.estado());
  }
  lienzo_.opacidad(f);
  const float cx = W / 2;
  if (panel_ == Panel::Recordatorio) {
    // Centrado y, si hace falta, en varias líneas que entren en la pantalla
    // (en la redonda, en el ancho del círculo a esa altura).
    const float cy = H * 0.66f;
    struct Intento {
      const Fuente* f;
      int max;
      bool cortar, mayus;
    };
    const Intento intentos[] = {{&fuentes::VOZ_20, redonda ? 3 : 2, false, false},
                                {&fuentes::VOZ_20, redonda ? 3 : 2, true, false},
                                {&fuentes::MONO_13, 3, true, true},
                                {&fuentes::MONO_9, 4, true, true}};
    char t[LARGO_RECORDATORIO];
    Lineas l;
    const Fuente* fu = &fuentes::MONO_9;
    float top = 0, lh = 0;
    for (const Intento& in : intentos) {
      copiarTexto(t, sizeof t, recTexto_);
      if (in.mayus) aMayusculas(t);
      fu = in.f;
      lh = static_cast<float>(fu->ascenso + fu->descenso);
      bool ok = false;
      for (int n = 1; n <= in.max && !ok; n++) {
        float anchos[MAX_LINEAS];
        top = cy - n * lh / 2;
        for (int i = 0; i < n; i++) {
          if (!redonda) {
            anchos[i] = W - 32;
            continue;
          }
          const float y0 = top + i * lh, y1 = y0 + lh, r = W / 2 - 10;
          const float dy = maxf(absf(y0 - H / 2), absf(y1 - H / 2));
          anchos[i] = 2 * raiz(r * r - dy * dy) - 8;
        }
        ok = partir(*fu, t, anchos, n, in.cortar, &l);
      }
      if (ok) break;
    }
    for (int i = 0; i < l.n; i++) {
      char linea[LARGO_RECORDATORIO];
      copiarTexto(linea, sizeof linea, t + l.ini[i], l.fin[i] - l.ini[i]);
      dibujarTexto(lienzo_, *fu, linea, cx - anchoTexto(*fu, linea) / 2, top + i * lh + fu->ascenso, paleta::TINTA);
    }
  } else if (panel_ == Panel::Reloj) {
    char t[6] = "--:--";
    const int m = minutoDelDia();
    if (m >= 0) {
      t[0] = static_cast<char>('0' + m / 600), t[1] = static_cast<char>('0' + (m / 60) % 10);
      t[3] = static_cast<char>('0' + (m % 60) / 10), t[4] = static_cast<char>('0' + m % 10);
    }
    const Fuente& fu = fuentes::VOZ_20;
    const float esc = redonda ? 2.4f : 2.8f;
    const float alto = fu.ascenso * 0.72f * esc;
    dibujarTextoEscalado(lienzo_, fu, t, cx - anchoTexto(fu, t) * esc / 2, H * (redonda ? 0.64f : 0.68f) + alto / 2, esc,
                         paleta::TINTA);
  } else if (panel_ == Panel::Pomodoro) {
    // La cuenta regresiva, una barra de progreso y en qué tramo va.
    const uint32_t s = (pomoRestanteMs_ + 999) / 1000;
    const uint32_t mm = s / 60, ss = s % 60;
    char t[8];
    int k = 0;
    if (mm >= 100) t[k++] = static_cast<char>('0' + mm / 100);
    t[k++] = static_cast<char>('0' + (mm / 10) % 10), t[k++] = static_cast<char>('0' + mm % 10), t[k++] = ':';
    t[k++] = static_cast<char>('0' + ss / 10), t[k++] = static_cast<char>('0' + ss % 10), t[k] = 0;
    const Fuente& fu = fuentes::VOZ_20;
    const float esc = redonda ? 2.0f : 2.4f;
    const float alto = fu.ascenso * 0.72f * esc;
    const float base = H * (redonda ? 0.60f : 0.62f) + alto / 2;
    dibujarTextoEscalado(lienzo_, fu, t, cx - anchoTexto(fu, t) * esc / 2, base, esc, paleta::TINTA);
    const float total = (pomoDescanso_ ? config_.pomodoro.descanso : config_.pomodoro.trabajo) * 60000.0f;
    const float progreso = limitar(1 - pomoRestanteMs_ / total, 0, 1);
    const float bw = W * (redonda ? 0.5f : 0.6f), by = base + 12, bh = 5;
    lienzo_.pildora(cx, by, bw, bh, bh / 2, paleta::LINEA);
    const float pw = bw * progreso;
    if (pw >= 1) lienzo_.pildora(cx - bw / 2 + pw / 2, by, pw, bh, minf(bh / 2, pw / 2), paleta::NARANJA);
    const char* etiqueta = !pomoCorriendo_ ? "PAUSA" : (pomoDescanso_ ? "DESCANSO" : "TRABAJO");
    dibujarTexto(lienzo_, fuentes::MONO_9, etiqueta, cx - anchoTexto(fuentes::MONO_9, etiqueta) / 2, by + 18,
                 paleta::TINTA_SUAVE);
  } else if (panel_ == Panel::Globo || panel_ == Panel::Despertador) {
    // Lo que dice, en un globo claro que sale de la boca.
    const float cy = H * (redonda ? 0.68f : 0.71f);
    const float anchoMax = redonda ? W * 0.84f : W - 28;
    const float boca = v.origenY + v.unidad * 0.32f;
    dibujarGlobo(lienzo_, textoGlobo(), cx, cy, anchoMax, redonda ? 3 : 2, redonda, W, H, cx, boca, f);
    if (panel_ == Panel::Despertador) {
      // Cuánto suena: unos puntitos que laten, cada vez más.
      const int nivel = alarma_ ? mini(5, 1 + static_cast<int>((hal_.ahoraMs() - alarmaDesde_) / tiemposPanel::ALARMA_SUBE_MS)) : 1;
      const float late = 0.55f + 0.45f * seno(hal_.ahoraMs() / 1000.0f * 6.0f);
      const float y = H * (redonda ? 0.86f : 0.95f);
      for (int i = 0; i < 5; i++) {
        lienzo_.opacidad(f * (i < nivel ? late : 0.25f));
        lienzo_.pildora(cx + (i - 2) * 12.0f, y, 6, 6, 3, paleta::NARANJA);
      }
      lienzo_.opacidad(f);
    }
  } else if (panel_ == Panel::Clima) {
    dibujarClima(cx, W, H, f);
  } else if (panel_ == Panel::Juego) {
    dibujarJuego(cx, W, H, f);
  }
  lienzo_.opacidad(1);
}

// El modo clima: el ícono, la temperatura grande y la máxima y la mínima.
void Sistema::dibujarClima(float cx, float W, float H, float alfa) {
  const bool redonda = perfil_.redonda;
  const bool hay = climaVigente();
  const float t = hal_.ahoraMs() / 1000.0f;
  const float tam = redonda ? H * 0.22f : H * 0.36f;
  const float iy = H * (redonda ? 0.64f : 0.70f);
  const float ix = redonda ? W * 0.36f : W * 0.31f;
  dibujarIconoClima(lienzo_, hay ? cieloDe(clima_.codigo) : Cielo::Nublado, !hay || clima_.dia, ix, iy, tam, t,
                    alfa * (hay ? 1 : 0.35f));
  lienzo_.opacidad(alfa);
  char g[12] = "--\xC2\xB0";
  if (hay) textoTemperatura(clima_.temperatura(), g, sizeof g);
  const Fuente& fu = fuentes::VOZ_20;
  const float esc = redonda ? 1.9f : 2.3f;
  const float alto = fu.ascenso * 0.72f * esc;
  const float x = redonda ? W * 0.5f : W * 0.49f;
  const float base = iy + alto / 2 - (hay ? 6 : 0);
  dibujarTextoEscalado(lienzo_, fu, g, x, base, esc, paleta::TINTA);
  if (!hay) return;
  char m[40], a[12], b[12];
  textoTemperatura(clima_.maxima, a, sizeof a);
  textoTemperatura(clima_.minima, b, sizeof b);
  size_t n = copiarTexto(m, sizeof m, "M\xC3\x81X ");
  n += copiarTexto(m + n, sizeof m - n, a);
  n += copiarTexto(m + n, sizeof m - n, " \xC2\xB7 M\xC3\x8DN ");
  copiarTexto(m + n, sizeof m - n, b);
  const Fuente& mo = fuentes::MONO_9;
  const float mx = redonda ? cx - anchoTexto(mo, m) / 2 : x;
  dibujarTexto(lienzo_, mo, m, mx, redonda ? H * 0.80f : base + 15, paleta::TINTA_SUAVE);
}

size_t Sistema::estadoBle(uint8_t* salida, size_t cap) const {
  protocolo::EstadoPublico p;
  const int bat = hal_.bateria ? hal_.bateria() : -1;
  p.bateria = bat < 0 ? 255 : static_cast<uint8_t>(bat);
  p.humor = static_cast<int8_t>(motor_.humor() * 100);
  p.modo = static_cast<uint8_t>(config_.modo);
  p.dormido = motor_.dormido();
  p.fija = motor_.hayFija();
  p.emparejando = emparejando_;
  p.autenticado = autenticado_;
  // Un recordatorio en pantalla es "recordatorio:<n>"; el pomodoro,
  // "pomodoro:trabajo", "pomodoro:descanso" o "pomodoro:pausa".
  if (recVisible_ >= 0) p.expresion = recNombre_;
  else if (alarma_) p.expresion = "despertador";
  else if (juego_ != Juego::Ninguno) p.expresion = visibleJuego_;
  else if (globo_) p.expresion = "globo";
  else if (config_.modo == Modo::Clima && panel_ == Panel::Clima) p.expresion = "clima";
  else if (config_.modo == Modo::Pomodoro) p.expresion = nombrePomodoro();
  else p.expresion = animVisible_ >= 0 ? visible_ : motor_.nombreVisible();
  p.nombre = nombre();
  p.serie = serie_;
  p.id = id_;
  p.ancho = static_cast<uint16_t>(perfil_.ancho);
  p.alto = static_cast<uint16_t>(perfil_.alto);
  p.redonda = perfil_.redonda;
  p.wifi = wifi_;
  p.nube = nube_;
  p.redConfigurada = redConfigurada_;
  p.actualizando = ota_ != Ota::Ninguna;
  p.version = perfil_.version ? perfil_.version : "";
  p.perfil = perfil_.nombre ? perfil_.nombre : "";
  p.crcConfig = crcConfig_;
  p.encendido = (hal_.ahoraMs() - arranque_) / 1000u;
  return protocolo::codificarEstado(p, salida, cap);
}

void Sistema::notificarEstadoSiCambio() {
  if (!hal_.notificar && !hal_.notificarNube) return;
  uint8_t b[protocolo::MAX_ESTADO];
  size_t n = estadoBle(b, sizeof b);
  // El humor cambia de a poco y el tiempo encendido, siempre (los últimos 4
  // bytes): no notificar por eso solo.
  bool igual = n == largoUltimoEstado_;
  for (size_t i = 0; igual && i + 4 < n; i++) igual = i == 2 || b[i] == ultimoEstado_[i];
  if (igual) return;
  copiarBytes(ultimoEstado_, b, n);
  largoUltimoEstado_ = n;
  if (hal_.notificar) hal_.notificar(protocolo::CarEstado, b, n);
  if (hal_.notificarNube) hal_.notificarNube(protocolo::CarEstado, b, n);
}

void Sistema::enviar(uint8_t car, const uint8_t* b, size_t n) {
  if (canal_ == Canal::Nube) {
    if (hal_.notificarNube) hal_.notificarNube(car, b, n);
  } else if (hal_.notificar) {
    hal_.notificar(car, b, n);
  }
}

size_t Sistema::tamanoTrozo() const {
  return canal_ == Canal::Nube ? TROZO_NUBE : protocolo::MAX_NOTIFICACION - 9;
}

void Sistema::responder(uint8_t car, uint8_t op, protocolo::Codigo c, const char* detalle) {
  uint8_t b[protocolo::MAX_NOTIFICACION];
  size_t i = 0;
  b[i++] = static_cast<uint8_t>(op | protocolo::Respuesta);
  b[i++] = c;
  if (detalle) {
    size_t n = largo(detalle);
    if (n > sizeof b - i) n = sizeof b - i;
    copiarBytes(b + i, detalle, n);
    i += n;
  }
  enviar(car, b, i);
}

void Sistema::enviarConfig() {
  // Pedirla en medio de una transferencia la aborta (0x84 es el código de los
  // trozos: no hay forma de responder "ocupado" sin confundir).
  receptor_.reiniciar();
  char* texto = receptor_.prestar();
  size_t total = leerGuardada(texto);
  if (total == 0) total = copiarTexto(texto, MAX_CONFIG_BYTES + 1, CONFIG_VACIA);
  uint8_t* b = g_trozo;
  const size_t porTrozo = canal_ == Canal::Nube ? TROZO_NUBE : protocolo::MAX_NOTIFICACION - 5;
  for (size_t off = 0; off < total; off += porTrozo) {
    size_t n = total - off < porTrozo ? total - off : porTrozo;
    b[0] = protocolo::ConfigTrozo;
    protocolo::escribir32(b + 1, static_cast<uint32_t>(off));
    copiarBytes(b + 5, texto + off, n);
    enviar(protocolo::CarConfig, b, 5 + n);
  }
  b[0] = protocolo::ConfigCierre;
  protocolo::escribir32(b + 1, static_cast<uint32_t>(total));
  protocolo::escribir32(b + 5, protocolo::crc32(reinterpret_cast<const uint8_t*>(texto), total));
  enviar(protocolo::CarConfig, b, 9);
}

void Sistema::enviarImagen(int espacio) {
  using namespace protocolo;
  char clave[6];
  claveImagen(clave, espacio);
  const uint8_t ok[2] = {static_cast<uint8_t>(ImgPedir | Respuesta), Ok};
  enviar(CarImagen, ok, 2);
  const size_t porTrozo = canal_ == Canal::Nube ? TROZO_NUBE : MAX_NOTIFICACION - 5;
  uint32_t off = 0, crc = 0;
  while (off < meta(espacio)->largo && hal_.leerTrozo) {
    const int m = hal_.leerTrozo(clave, off, g_trozo + 5, porTrozo);
    if (m <= 0) break;
    g_trozo[0] = ImgTrozo;
    escribir32(g_trozo + 1, off);
    enviar(CarImagen, g_trozo, 5 + static_cast<size_t>(m));
    crc = crc32(g_trozo + 5, static_cast<size_t>(m), crc);
    off += static_cast<uint32_t>(m);
  }
  g_trozo[0] = ImgCierre;
  g_trozo[1] = static_cast<uint8_t>(espacio);
  escribir32(g_trozo + 2, off);
  escribir32(g_trozo + 6, crc);
  enviar(CarImagen, g_trozo, 10);
}

void Sistema::leerRed() {
  char ssid[33], clave[65];
  redConfigurada_ = red(ssid, sizeof ssid, clave, sizeof clave);
}

bool Sistema::red(char* ssid, size_t cs, char* clave, size_t cc) const {
  uint8_t b[2 + 32 + 64];
  const int n = hal_.leer ? hal_.leer(CLAVE_RED, b, sizeof b) : -1;
  if (n < 2) return false;
  size_t k = protocolo::leerTextoCorto(b, static_cast<size_t>(n), ssid, cs);
  if (!k || !ssid[0]) return false;
  return protocolo::leerTextoCorto(b + k, static_cast<size_t>(n) - k, clave, cc) > 0;
}

bool Sistema::nube(char* url, size_t cu, char* usuario, size_t cus, char* clave, size_t cc) const {
  uint8_t b[3 + 160 + 64 + 128];
  const int n = hal_.leer ? hal_.leer(CLAVE_NUBE, b, sizeof b) : -1;
  if (n < 3) return false;
  size_t i = protocolo::leerTextoCorto(b, static_cast<size_t>(n), url, cu);
  if (!i || !esUrlNube(url)) return false;
  size_t k = protocolo::leerTextoCorto(b + i, static_cast<size_t>(n) - i, usuario, cus);
  if (!k || !usuario[0]) return false;
  i += k;
  return protocolo::leerTextoCorto(b + i, static_cast<size_t>(n) - i, clave, cc) > 0;
}

void Sistema::conexiones(bool wifi, bool nube) {
  if (wifi != wifi_ || nube != nube_) log(nube ? "conectado a la nube" : wifi ? "conectado al WiFi" : "sin red");
  wifi_ = wifi;
  nube_ = nube;
}

void Sistema::comandoRed(uint8_t op, const uint8_t* d, size_t n) {
  using namespace protocolo;
  const uint8_t car = CarComando;
  switch (op) {
    case CmdRed: {
      char ssid[33], clave[65];
      size_t i = leerTextoCorto(d + 1, n - 1, ssid, sizeof ssid);
      size_t k = i ? leerTextoCorto(d + 1 + i, n - 1 - i, clave, sizeof clave) : 0;
      if (!i || !k || !ssid[0] || 1 + i + k != n) return responder(car, op, Malformado, "ssid de 1 a 32 bytes, clave de 0 a 64");
      if (!hal_.guardar || !hal_.guardar(CLAVE_RED, d + 1, n - 1)) return responder(car, op, FalloOta, "no pude guardar");
      leerRed();
      responder(car, op, Ok);
      log("WiFi nuevo");
      break;
    }
    case CmdNube: {
      char url[161], usuario[65], clave[129];
      size_t i = leerTextoCorto(d + 1, n - 1, url, sizeof url);
      size_t k = i ? leerTextoCorto(d + 1 + i, n - 1 - i, usuario, sizeof usuario) : 0;
      size_t m = k ? leerTextoCorto(d + 1 + i + k, n - 1 - i - k, clave, sizeof clave) : 0;
      if (!i || !k || !m || 1 + i + k + m != n || !esUrlNube(url) || !usuario[0] || !clave[0])
        return responder(car, op, Malformado, "url mqtt:// o mqtts://, usuario y clave");
      if (!hal_.guardar || !hal_.guardar(CLAVE_NUBE, d + 1, n - 1)) return responder(car, op, FalloOta, "no pude guardar");
      responder(car, op, Ok);
      log("nube nueva");
      break;
    }
    case CmdOlvidarRed: {
      const uint8_t vacio[1] = {0};
      if (hal_.guardar) {
        hal_.guardar(CLAVE_RED, vacio, 1);
        hal_.guardar(CLAVE_NUBE, vacio, 1);
      }
      leerRed();
      responder(car, op, Ok);
      log("me olvidé del WiFi y de la nube");
      break;
    }
    default:
      return;
  }
  if (hal_.redCambio) hal_.redCambio();
}

void Sistema::ota(uint8_t op, const uint8_t* d, size_t n) {
  using namespace protocolo;
  const uint8_t car = CarOta;
  switch (op) {
    case OtaInicio:
    case OtaDescargar: {
      if (ota_ != Ota::Ninguna) return responder(car, op, Ocupado);
      ManifiestoOta m;
      const size_t k = leerManifiesto(d + 1, n - 1, &m);
      if (!k) return responder(car, op, Malformado);
      if (!iguales(m.perfil, perfil_.nombre ? perfil_.nombre : "")) return responder(car, op, PerfilDistinto, perfil_.nombre);
      if (op == OtaInicio) {
        if (1 + k != n) return responder(car, op, Malformado);
        if (!hal_.otaEmpezar || !hal_.otaEscribir || !hal_.otaTerminar) return responder(car, op, NoImplementado);
        const Codigo c = static_cast<Codigo>(hal_.otaEmpezar(m));
        if (c == Ok) {
          ota_ = Ota::Subida;
          otaTamano_ = m.tamano;
          otaRecibido_ = 0;
          otaPorcentaje_ = 0;
          log("OTA: recibiendo");
        }
        return responder(car, op, c);
      }
      // Descargar: [manifiesto][u16 n][url]
      if (1 + k + 2 > n) return responder(car, op, Malformado);
      const size_t nu = static_cast<size_t>(d[1 + k]) | static_cast<size_t>(d[2 + k]) << 8;
      char url[256];
      if (nu == 0 || nu >= sizeof url || 1 + k + 2 + nu != n) return responder(car, op, Malformado);
      copiarTexto(url, sizeof url, reinterpret_cast<const char*>(d + 3 + k), nu);
      if (!hal_.otaDescargar) return responder(car, op, NoImplementado);
      if (!wifi_) return responder(car, op, SinRed);
      const Codigo c = static_cast<Codigo>(hal_.otaDescargar(m, url));
      if (c == Ok) {
        ota_ = Ota::Descarga;
        otaPorcentaje_ = 0;
        log("OTA: descargando");
      }
      return responder(car, op, c);
    }
    case OtaBloque: {
      if (ota_ != Ota::Subida) return responder(car, op, SinInicio);
      if (n < 5) return responder(car, op, Malformado);
      const uint32_t off = leer32(d + 1);
      const size_t m = n - 5;
      Codigo c = off != otaRecibido_ ? FueraDeOrden : otaRecibido_ + m > otaTamano_ ? MuyGrande : Ok;
      if (c == Ok) c = static_cast<Codigo>(hal_.otaEscribir(off, d + 5, m));
      if (c != Ok) {
        if (hal_.otaCancelar) hal_.otaCancelar();
        ota_ = Ota::Ninguna;
        return responder(car, op, c);
      }
      otaRecibido_ += static_cast<uint32_t>(m);
      const uint8_t pct = static_cast<uint8_t>(static_cast<uint64_t>(otaRecibido_) * 99u / (otaTamano_ ? otaTamano_ : 1));
      if (pct != otaPorcentaje_) {
        otaPorcentaje_ = pct;
        const uint8_t b[3] = {static_cast<uint8_t>(OtaProgreso | Respuesta), Ok, pct};
        enviar(car, b, 3);
      }
      return;
    }
    case OtaFin: {
      if (ota_ != Ota::Subida) return responder(car, op, SinInicio);
      ota_ = Ota::Ninguna;
      if (otaRecibido_ != otaTamano_) {
        if (hal_.otaCancelar) hal_.otaCancelar();
        return responder(car, op, FueraDeOrden);
      }
      const Codigo c = static_cast<Codigo>(hal_.otaTerminar());
      log(c == Ok ? "OTA: lista, reinicio" : "OTA: rechazada");
      return responder(car, op, c);
    }
    case OtaCancelar: {
      if (ota_ != Ota::Ninguna && hal_.otaCancelar) hal_.otaCancelar();
      ota_ = Ota::Ninguna;
      return responder(car, op, Ok);
    }
    default:
      return responder(car, op, Malformado);
  }
}

void Sistema::otaProgreso(uint8_t codigo, uint8_t porcentaje, const char* detalle) {
  using namespace protocolo;
  uint8_t b[MAX_NOTIFICACION];
  size_t i = 0;
  b[i++] = static_cast<uint8_t>(OtaProgreso | Respuesta);
  b[i++] = codigo;
  b[i++] = porcentaje;
  if (detalle) {
    size_t m = largo(detalle);
    if (m > sizeof b - i) m = sizeof b - i;
    copiarBytes(b + i, detalle, m);
    i += m;
  }
  if (codigo != Ok || porcentaje >= 100) ota_ = Ota::Ninguna;
  otaPorcentaje_ = porcentaje;
  if (hal_.notificar) hal_.notificar(CarOta, b, i);
  if (hal_.notificarNube) hal_.notificarNube(CarOta, b, i);
}

void Sistema::reproducir(int animacion, bool unaVez) {
  if (animacion < 0 || animacion >= config_.nAnimaciones) {
    animPedida_ = -1;
    return;
  }
  animPedida_ = animacion;
  animUnaVez_ = unaVez;
  animDesde_ = hal_.ahoraMs();
}

void Sistema::elegirAnimacion(uint32_t ahora) {
  animVisible_ = -1;
  if (apagado_ || emparejando_) return;
  if (animPedida_ >= 0 && animPedida_ < config_.nAnimaciones) {
    const Animacion& a = config_.animaciones[animPedida_];
    const uint32_t pasado = ahora - animDesde_;
    if (animUnaVez_ && pasado >= a.duracion) {
      animPedida_ = -1;
    } else {
      animVisible_ = animPedida_;
      animT_ = animUnaVez_ ? pasado : pasado % (a.duracion ? a.duracion : 1);
    }
  }
  if (animVisible_ < 0 && motor_.expresionFija() < 0 && !motor_.dormido() && !motor_.reaccionando(ahora) &&
      largoLista() > 0) {
    const ElementoLista e = elementoLista(posLista_);
    if (e.tipo == ElementoLista::Tipo::Animacion && e.indice < config_.nAnimaciones) {
      const Animacion& a = config_.animaciones[e.indice];
      animVisible_ = e.indice;
      // En la lista se repite siempre.
      animT_ = (ahora - animListaDesde_) % (a.duracion ? a.duracion : 1);
    }
  }
  if (animVisible_ >= 0) {
    const size_t k = copiarTexto(visible_, sizeof visible_, "animacion:");
    copiarTexto(visible_ + k, sizeof visible_ - k, config_.animaciones[animVisible_].nombre);
  }
}

void Sistema::enviarVida() {
  // [0x9B][0][7 × estado][6 × rasgos][flags][n][comportamiento][n][visible][n cand][n][nombre][puntaje]...[fps]
  using namespace protocolo;
  uint8_t b[MAX_NOTIFICACION];
  size_t i = 0;
  b[i++] = static_cast<uint8_t>(CmdVida | Respuesta);
  b[i++] = Ok;
  const float* e = motor_.estadoInterno();
  for (int k = 0; k < N_ESTADO; k++) b[i++] = static_cast<uint8_t>(limitar(e[k], 0, 1) * 255 + 0.5f);
  const Personalidad p = motor_.rasgos();
  for (int k = 0; k < N_RASGOS; k++) b[i++] = static_cast<uint8_t>(limitar(p[k], 0, 1) * 255 + 0.5f);
  b[i++] = static_cast<uint8_t>((motor_.dormido() ? 1 : 0) | (motor_.sonando() ? 2 : 0) | (depurar_ ? 4 : 0));
  auto texto = [&](const char* t) {
    size_t m = largo(t);
    if (m > 24) m = 24;
    b[i++] = static_cast<uint8_t>(m);
    copiarBytes(b + i, t, m);
    i += m;
  };
  texto(motor_.nombreComportamiento());
  texto(motor_.nombreVisible());
  const char* nombres[3];
  float puntajes[3];
  const int nc = motor_.candidatos(nombres, puntajes, 3);
  b[i++] = static_cast<uint8_t>(nc);
  for (int k = 0; k < nc; k++) {
    texto(nombres[k]);
    b[i++] = static_cast<uint8_t>(limitar(puntajes[k], 0, 2.55f) * 100);
  }
  b[i++] = static_cast<uint8_t>(limitar(fps_, 0, 255));
  enviar(CarComando, b, i);
}

void Sistema::enviarRegistro() {
  using namespace protocolo;
  EntradaRegistro r[28];
  const int n = motor_.registro(r, 28);
  uint8_t b[3 + 28 * 6];
  size_t i = 0;
  b[i++] = static_cast<uint8_t>(CmdRegistro | Respuesta);
  b[i++] = Ok;
  b[i++] = static_cast<uint8_t>(n);
  for (int k = 0; k < n; k++) {
    escribir32(b + i, r[k].segundos);
    b[i + 4] = r[k].tipo;
    b[i + 5] = r[k].dato;
    i += 6;
  }
  enviar(CarComando, b, i);
}

void Sistema::bleEscribir(uint8_t car, const uint8_t* d, size_t n) {
  canal_ = Canal::Ble;
  procesar(car, d, n);
}

void Sistema::nubeEscribir(uint8_t car, const uint8_t* d, size_t n) {
  canal_ = Canal::Nube;
  procesar(car, d, n);
  canal_ = Canal::Ble;
}

void Sistema::procesar(uint8_t car, const uint8_t* d, size_t n) {
  using namespace protocolo;
  if (n < 1) return;
  actividad();  // un comando de la app (una expresión fija, la hora...) saca el protector
  const uint8_t op = d[0];
  const uint32_t ahora = hal_.ahoraMs();

  if (canal_ == Canal::Nube && car == CarComando &&
      (op == CmdVincular || op == CmdAutenticar || op == CmdRed || op == CmdNube))
    return responder(car, op, NoAutorizado, "eso solo se hace por BLE");

  // Vincular y autenticar son lo único que se puede hacer sin estar autenticado.
  if (car == CarComando && op == CmdVincular) {
    if (bloqueado(ahora)) return responder(car, op, NoAutorizado, "demasiados intentos: esperá 30 s");
    if (!emparejando_) return responder(car, op, NoEmparejando, "mantené el botón 5 s para ver el QR");
    if (n != 7 || !iguales(reinterpret_cast<const char*>(d + 1), codigo_, 6)) {
      falla(ahora);
      return responder(car, op, NoAutorizado, "el código no coincide");
    }
    // Clave nueva para este celu; si ya hay 4, se olvida la más vieja.
    if (nClaves_ == 4) {
      for (int i = 0; i < 3; i++) copiarBytes(claves_[i], claves_[i + 1], 16);
      nClaves_ = 3;
    }
    uint8_t* clave = claves_[nClaves_++];
    for (int i = 0; i < 16; i += 4) {
      uint32_t r = hal_.semilla ? hal_.semilla() : ahora * 2654435761u + static_cast<uint32_t>(i);
      for (int k = 0; k < 4; k++) clave[i + k] = static_cast<uint8_t>(r >> (8 * k));
    }
    guardarClaves();
    fallos_ = 0;
    autenticado_ = true;
    emparejando_ = false;
    uint8_t r[18] = {static_cast<uint8_t>(op | Respuesta), Ok};
    copiarBytes(r + 2, clave, 16);
    enviar(car, r, sizeof r);
    evento(Evento::Vinculado);
    log("celu vinculado");
    return;
  }
  if (car == CarComando && op == CmdAutenticar) {
    if (bloqueado(ahora)) return responder(car, op, NoAutorizado, "demasiados intentos: esperá 30 s");
    bool ok = false;
    for (int k = 0; n == 17 && k < nClaves_; k++) {
      uint8_t dif = 0;  // comparar todo, sin cortar en el primer byte distinto
      for (int i = 0; i < 16; i++) dif |= claves_[k][i] ^ d[1 + i];
      ok = ok || dif == 0;
    }
    if (!ok) {
      falla(ahora);
      return responder(car, op, NoAutorizado, "clave desconocida: hay que vincular de nuevo");
    }
    fallos_ = 0;
    autenticado_ = true;
    return responder(car, op, Ok);
  }
  if (canal_ != Canal::Nube && !autenticado_) return responder(car, op, NoAutorizado, "vinculá o autenticá primero");

  if (car == CarConfig) {
    switch (op) {
      case ConfigInicio:
        if (n != 9) return responder(car, op, Malformado);
        return responder(car, op, receptor_.inicio(leer32(d + 1), leer32(d + 5)));
      case ConfigBloque:
        if (n < 5) return responder(car, op, Malformado);
        {
          Codigo c = receptor_.bloque(leer32(d + 1), d + 5, n - 5);
          // Los bloques bien recibidos no se confirman uno por uno.
          if (c != Ok) responder(car, op, c);
        }
        return;
      case ConfigFin: {
        Codigo c = receptor_.fin();
        if (c != Ok) return responder(car, op, c);
        const Modo modoAntes = config_.modo;
        const TramosPomodoro tramosAntes = config_.pomodoro;
        ResultadoConfig r = cargarConfig(config_, receptor_.texto(), receptor_.largo());
        if (!r.ok()) {
          // config_ quedó con la base: se vuelve a la que estaba guardada.
          receptor_.reiniciar();
          cargarGuardada();
          return responder(car, op, r.error == ErrorConfig::JsonInvalido ? JsonInvalido : ConfigInvalida, r.detalle);
        }
        crcConfig_ = crc32(reinterpret_cast<const uint8_t*>(receptor_.texto()), receptor_.largo());
        animPedida_ = animVisible_ = -1;
        if (hal_.guardar) hal_.guardar(CLAVE_CONFIG, reinterpret_cast<const uint8_t*>(receptor_.texto()), receptor_.largo());
        receptor_.reiniciar();
        // Un nombre nuevo en la config gana sobre el puesto por comando.
        nombre_[0] = 0;
        if (!config_.resolverCara(fijaGuardada_).existe()) fijaGuardada_[0] = 0;
        guardarAjustes();
        // El motor sigue con su estado y su memoria: solo cambian los datos.
        motor_.reconfigurar(&config_);
        actualizarSprites();
        // Los recordatorios cambiaron: el que se veía se va con un fundido
        // (su texto quedó copiado) y se revisa de nuevo el minuto actual.
        recVisible_ = -1;
        recPendientes_ = 0;
        recMinuto_ = -1;
        alarmaMinuto_ = -1;
        proximaAgua_ = 0;
        if (config_.modo != modoAntes || config_.pomodoro.trabajo != tramosAntes.trabajo ||
            config_.pomodoro.descanso != tramosAntes.descanso)
          reiniciarPomodoro();
        motor_.fijar(fijaGuardada_);
        aplicarLista(hal_.ahoraMs());
        evento(Evento::CambioDeCara);
        log("config nueva aplicada");
        return responder(car, op, Ok);
      }
      case ConfigPedir:
        return enviarConfig();
      default:
        return responder(car, op, Malformado);
    }
  }

  if (car == CarComando) {
    switch (op) {
      case CmdExpresion: {
        char nombre[LARGO_NOMBRE];
        if (n - 1 >= LARGO_NOMBRE) return responder(car, op, NoExiste);
        copiarTexto(nombre, sizeof nombre, reinterpret_cast<const char*>(d + 1), n - 1);
        if (n > 1 && !config_.resolverCara(nombre).existe()) return responder(car, op, NoExiste);
        motor_.fijar(n == 1 ? "" : nombre);
        copiarTexto(fijaGuardada_, sizeof fijaGuardada_, n == 1 ? "" : nombre);
        guardarAjustes();
        responder(car, op, Ok);
        // "Se mira en el espejo y prueba la cara nueva".
        if (n > 1) evento(Evento::CambioDeCara);
        return;
      }
      case CmdNombre: {
        if (n < 2 || n - 1 > LARGO_NOMBRE - 1) return responder(car, op, Malformado);
        copiarTexto(nombre_, sizeof nombre_, reinterpret_cast<const char*>(d + 1), n - 1);
        guardarAjustes();
        return responder(car, op, Ok);
      }
      case CmdDesvincular: {
        nClaves_ = 0;
        llenarBytes(claves_, 0, sizeof claves_);
        guardarClaves();
        emparejando_ = true;
        emparejarHasta_ = 0;
        responder(car, op, Ok);
        autenticado_ = false;
        return;
      }
      case CmdEvento: {
        if (n != 2 || d[1] >= static_cast<uint8_t>(Evento::Cantidad)) return responder(car, op, Malformado);
        evento(static_cast<Evento>(d[1]));
        return responder(car, op, Ok);
      }
      case CmdRed:
      case CmdNube:
      case CmdOlvidarRed:
        return comandoRed(op, d, n);
      case CmdHora: {
        if (n != 7) return responder(car, op, Malformado, "[u32 epoch][i16 zona en minutos]");
        const int16_t zona = static_cast<int16_t>(d[5] | d[6] << 8);
        if (zona < -840 || zona > 840) return responder(car, op, Malformado, "zona de -14 h a +14 h");
        hora(leer32(d + 1), zona);
        return responder(car, op, Ok);
      }
      case CmdVida: {
        if (n == 2) depurar_ = d[1] != 0;
        else if (n != 1) return responder(car, op, Malformado);
        return enviarVida();
      }
      case CmdMemoria: {
        uint8_t b[2 + LARGO_MEMORIA];
        b[0] = static_cast<uint8_t>(op | Respuesta);
        b[1] = Ok;
        codificarMemoria(motor_.memoria(), b + 2, LARGO_MEMORIA);
        enviar(car, b, sizeof b);
        return;
      }
      case CmdRestaurar: {
        Memoria m;
        if (n != 1 + LARGO_MEMORIA || !decodificarMemoria(d + 1, LARGO_MEMORIA, &m))
          return responder(car, op, Malformado, "memoria de 112 bytes con su CRC");
        motor_.restaurarMemoria(m);
        if (hal_.guardar) hal_.guardar(CLAVE_VIDA, d + 1, LARGO_MEMORIA);
        log("memoria restaurada (mudanza)");
        return responder(car, op, Ok);
      }
      case CmdRegistro:
        return enviarRegistro();
      case CmdComportamiento: {
        char nombre[LARGO_VIDA];
        if (n < 2 || n - 1 >= LARGO_VIDA) return responder(car, op, Malformado);
        copiarTexto(nombre, sizeof nombre, reinterpret_cast<const char*>(d + 1), n - 1);
        const int i = config_.vida.indiceComportamiento(nombre);
        if (i < 0) return responder(car, op, NoExiste);
        if (motor_.dormido()) motor_.despertar(ahora, true);
        return responder(car, op, motor_.comportamiento(i, ahora) ? Ok : Ocupado);
      }
      case CmdDecir: {
        // [n][expresión][texto]
        if (n < 3 || 2u + d[1] >= n) return responder(car, op, Malformado, "[n][expresión][texto de 1 a 60 bytes]");
        const size_t ne = d[1], nt = n - 2 - ne;
        if (nt < 1 || nt >= static_cast<size_t>(LARGO_FRASE) || !textoUtf8Valido(reinterpret_cast<const char*>(d + 2 + ne), nt))
          return responder(car, op, Malformado, "texto de 1 a 60 bytes UTF-8");
        Config::Cara cara;
        if (ne) {
          char e[LARGO_NOMBRE];
          if (ne >= LARGO_NOMBRE) return responder(car, op, NoExiste);
          copiarTexto(e, sizeof e, reinterpret_cast<const char*>(d + 2), ne);
          cara = config_.resolverCara(e);
          if (!cara.existe()) return responder(car, op, NoExiste);
        }
        char t[LARGO_FRASE];
        copiarTexto(t, sizeof t, reinterpret_cast<const char*>(d + 2 + ne), nt);
        decir(t, cara, !cara.existe());
        return responder(car, op, Ok);
      }
      case CmdClima: {
        DatosClima c;
        if (!leerClima(d + 1, n - 1, &c)) return responder(car, op, Malformado, "[i16 décimas][u8 código][i8 máx][i8 mín][u8 flags][n][ciudad]");
        c.recibido = ahora;
        clima_ = c;
        if (config_.modo == Modo::Clima) {
          // La cara reacciona ya (y después cada tanto) y, quizás, lo comenta.
          if (const char* cara = caraClima(clima_)) motor_.reaccionar(config_.resolverCara(cara), ahora, 6000);
          proximaCaraClima_ = ahora + tiemposPanel::CARA_CLIMA_MS;
          const Situacion s = situacionClima(clima_);
          if (s != Situacion::Cantidad) hablar(s);
        }
        return responder(car, op, Ok);
      }
      case CmdAlarma: {
        if (n != 2 || d[1] > 3) return responder(car, op, Malformado, "0 apagar, 1 posponer, 2 sonar, 3 despertar");
        switch (d[1]) {
          case 0:
            if (!alarma_ && !pospuestaHasta_) return responder(car, op, NoExiste, "no está sonando");
            apagarAlarma();
            break;
          case 1:
            if (!alarma_) return responder(car, op, NoExiste, "no está sonando");
            posponerAlarma();
            break;
          case 2: sonarAlarma(false); break;
          default:
            if (motor_.dormido()) motor_.despertar(ahora, false);
            hablar(Situacion::Despertar);
            break;
        }
        return responder(car, op, Ok);
      }
      case CmdJugar: {
        if (n != 2 || (d[1] > 4 && d[1] != 255)) return responder(car, op, n != 2 ? Malformado : NoExiste);
        jugar(static_cast<Juego>(d[1]));
        if (d[1] != 255) {
          const size_t k = copiarTexto(visibleJuego_, sizeof visibleJuego_, "juego:");
          copiarTexto(visibleJuego_ + k, sizeof visibleJuego_ - k, nombreJuego(d[1]));
        }
        return responder(car, op, Ok);
      }
      case CmdAviso: {
        // [tipo][n][app][n][contacto]
        char app[LARGO_APP] = {}, contacto[LARGO_APP] = {};
        size_t i = 2;
        if (n < 2 || d[1] > 6) return responder(car, op, Malformado, "[u8 tipo][n][app][n][contacto]");
        if (i < n) {
          const size_t k = leerTextoCorto(d + i, n - i, app, sizeof app);
          if (!k) return responder(car, op, Malformado, "app de hasta 24 bytes");
          i += k;
        }
        if (i < n) {
          const size_t k = leerTextoCorto(d + i, n - i, contacto, sizeof contacto);
          if (!k) return responder(car, op, Malformado, "contacto de hasta 24 bytes");
          i += k;
        }
        if (!textoUtf8Valido(app, largo(app)) || !textoUtf8Valido(contacto, largo(contacto)))
          return responder(car, op, Malformado, "app y contacto en UTF-8");
        // v1.9: los de afuera (pago, mail, github, pedido, otro): app es el título y contacto, quién.
        if (d[1] >= 2) {
          avisoEntrante(d[1], app, contacto);
          return responder(car, op, Ok);
        }
        const bool llamada = d[1] == 1;
        evento(llamada ? Evento::Llamada : Evento::Notificacion);
        const int a = buscarAviso(config_.avisos, llamada, app, contacto);
        if (a >= 0) {
          const Aviso& x = config_.avisos.lista[a];
          Config::Cara c;
          c.vida = x.caraVida, c.dibujo = x.caraDibujo;
          if (x.comportamiento >= 0) motor_.comportamiento(x.comportamiento, ahora);
          if (c.existe()) motor_.reaccionar(c, ahora, 4000);
          if (x.sonido) sonar(llamada ? "llamada" : "notificacion");
          if (x.decir[0]) {
            char t[LARGO_FRASE];
            completarFrase(x.decir, config_.vida.dueno, nombre(), t, sizeof t, nullptr, app, contacto);
            decir(t, c);
          }
        }
        return responder(car, op, Ok);
      }
      case CmdToque: {
        // [tipo][n][de][n][expresión]
        char de[LARGO_APP] = {}, ex[LARGO_APP] = {};
        if (n < 4 || d[1] > 3) return responder(car, op, Malformado, "[u8 tipo][n][de][n][expresión]");
        size_t i = 2;
        size_t k = leerTextoCorto(d + i, n - i, de, sizeof de);
        if (k < 2 || !textoUtf8Valido(de, largo(de))) return responder(car, op, Malformado, "de quién: de 1 a 24 bytes");
        i += k;
        if (i < n) {
          k = leerTextoCorto(d + i, n - i, ex, sizeof ex);
          if (!k || !textoUtf8Valido(ex, largo(ex))) return responder(car, op, Malformado, "expresión de hasta 24 bytes");
        }
        return responder(car, op, toque(d[1], de, ex) ? Ok : NoExiste);
      }
      case CmdAgenda: {
        // [u16 minutos][u16 duración][n][título]
        char t[LARGO_RECORDATORIO] = {};
        const size_t k = n >= 6 ? leerTextoCorto(d + 5, n - 5, t, sizeof t) : 0;
        if (k < 2 || 5 + k != n || !textoUtf8Valido(t, largo(t)))
          return responder(car, op, Malformado, "[u16 minutos][u16 duración][n][título de 1 a 40 bytes]");
        agenda(static_cast<uint16_t>(d[1] | d[2] << 8), static_cast<uint16_t>(d[3] | d[4] << 8), t);
        return responder(car, op, Ok);
      }
      case CmdPresencia: {
        if (n != 2 || d[1] > 1) return responder(car, op, Malformado, "0 llegaste, 1 te fuiste");
        presencia(d[1] == 0);
        return responder(car, op, Ok);
      }
      case CmdHabito: {
        if (n != 4 || d[1] >= MAX_HABITOS || d[2] > 1) return responder(car, op, Malformado, "[u8 índice 0-3][u8 0 hecho, 1 deshacer][u8 racha]");
        return responder(car, op, habito(d[1], d[2] == 0, d[3]) ? Ok : NoExiste);
      }
      case CmdDiario:
        if (n != 1) return responder(car, op, Malformado, "sin datos");
        return enviarDiario();
      case CmdCharlar: {
        // [n][texto]
        if (n < 3 || d[1] == 0 || 2u + d[1] != n || d[1] > MAX_ENTRADA_CHARLA ||
            !textoUtf8Valido(reinterpret_cast<const char*>(d + 2), d[1]))
          return responder(car, op, Malformado, "[n][texto de 1 a 200 bytes UTF-8]");
        RespuestaCharla r;
        charlar(reinterpret_cast<const char*>(d + 2), d[1], &r, true);
        responderCharla(r, true);
        return;
      }
      case CmdMusica: {
        // [u8 bpm][u8 flags][n][título][n][artista]
        char t[LARGO_RECORDATORIO] = {}, a[LARGO_APP] = {};
        if (n < 3) return responder(car, op, Malformado, "[u8 bpm][u8 flags][n][título][n][artista]");
        size_t i = 3;
        if (i < n) {
          const size_t k = leerTextoCorto(d + i, n - i, t, sizeof t);
          if (!k || !textoUtf8Valido(t, largo(t))) return responder(car, op, Malformado, "título de hasta 40 bytes");
          i += k;
        }
        if (i < n) {
          const size_t k = leerTextoCorto(d + i, n - i, a, sizeof a);
          if (!k || !textoUtf8Valido(a, largo(a))) return responder(car, op, Malformado, "artista de hasta 24 bytes");
        }
        musica(d[1], (d[2] & 1) != 0, t, a);
        return responder(car, op, Ok);
      }
      case CmdAnimo: {
        if (n != 2 || d[1] > 2) return responder(car, op, Malformado, "0 bien, 1 más o menos, 2 mal");
        responderAnimo(d[1]);
        return responder(car, op, Ok);
      }
      case CmdMirar: {
        if (n != 3) return responder(car, op, Malformado, "[i8 x][i8 y]");
        const float x = limitar(static_cast<int8_t>(d[1]) / 100.0f, -1, 1), y = limitar(static_cast<int8_t>(d[2]) / 100.0f, -1, 1);
        motor_.estimulo(x, y);
        motor_.seguir(x, y, ahora);
        return responder(car, op, Ok);
      }
      case CmdReproducir: {
        if (n == 1) {
          reproducir(-1, false);
          return responder(car, op, Ok);
        }
        char nombre[LARGO_NOMBRE];
        if (n - 1 >= LARGO_NOMBRE) return responder(car, op, NoExiste);
        copiarTexto(nombre, sizeof nombre, reinterpret_cast<const char*>(d + 1), n - 1);
        const int idx = config_.indiceAnimacion(nombre);
        if (idx < 0) return responder(car, op, NoExiste);
        reproducir(idx, !config_.animaciones[idx].repetir);
        return responder(car, op, Ok);
      }
      default:
        return responder(car, op, Malformado);
    }
  }

  if (car == CarImagen) return bleImagen(op, d, n);
  if (car == CarOta) return ota(op, d, n);
}

}  // namespace sintetica
