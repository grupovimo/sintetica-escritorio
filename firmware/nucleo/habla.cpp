#include "habla.h"

#include "formato.h"
#include "vida_base.h"

namespace sintetica {

namespace {
const char* const NOMBRES_SITUACION[N_SITUACIONES] = {
    "despertar", "buenas-noches", "volviste", "te-extrane", "aburrido", "sueno", "bateria-baja", "cargando",
    "caricia", "saludo", "hito", "cumple-dueno", "cumple-robot", "recordatorio-hecho", "despertador", "posponer",
    "clima-frio", "clima-calor", "clima-lluvia", "clima-lindo",
    "buen-dia", "comiste", "a-dormir", "tardaste", "agua", "pausa", "como-estas", "animo-bien", "animo-medio", "animo-mal",
    "jugar", "gane", "perdi", "empate", "anio-nuevo", "navidad", "dia-del-amigo", "dia-del-nino", "aniversario",
    "toque-recibido", "toque-devuelto", "agenda", "agenda-manana", "te-vas", "habito-hecho", "habito-racha",
    "habito-cortado", "musica", "pago", "mail", "github", "pedido", "aviso",
    "recuerdo", "propuesta", "crecio", "mes-juntos", "feriado", "fecha-tuya", "fecha-manana", "mimos"};

const char* const PRESETS[] = {"equilibrado", "timido", "picaro", "dormilon", "carinoso", "curioso", "grunon", "electrico"};

// La fila de la tabla de fábrica de esa situación (nullptr si no tiene).
const SituacionBase* base(Situacion s) {
  const char* n = nombreSituacion(s);
  for (int i = 0; i < CANTIDAD_SITUACIONES_BASE; i++)
    if (iguales(SITUACIONES_BASE[i].nombre, n)) return &SITUACIONES_BASE[i];
  return nullptr;
}

bool esBool(const Json& j, int t) { return j.tipo(t) == TipoJson::Verdadero || j.tipo(t) == TipoJson::Falso; }

// Solo esas claves en el objeto.
template <size_t N>
bool soloEstas(const Json& j, int obj, const char* const (&claves)[N]) {
  int k = obj + 1;
  for (int i = 0; i < j.cantidad(obj); i++, k = j.saltar(k + 1)) {
    char c[16];
    if (j.texto(k, c, sizeof c) <= 0) return false;
    bool ok = false;
    for (size_t n = 0; n < N && !ok; n++) ok = iguales(c, claves[n]);
    if (!ok) return false;
  }
  return true;
}

bool utf8Valido(const char* s) {
  const auto* p = reinterpret_cast<const uint8_t*>(s);
  while (*p) {
    int n = *p < 0x80 ? 1 : (*p >> 5) == 6 ? 2 : (*p >> 4) == 14 ? 3 : (*p >> 3) == 30 ? 4 : 0;
    if (!n) return false;
    for (int i = 1; i < n; i++)
      if ((p[i] & 0xC0) != 0x80) return false;
    p += n;
  }
  return true;
}

// Un texto de 1 a 60 bytes. -1 si no es texto, 0 si es vacío o muy largo.
int leerFrase(const Json& j, int t, char* destino) {
  char buf[LARGO_FRASE + 8];
  if (!j.valido(t) || j.tipo(t) != TipoJson::Texto) return -1;
  const int n = j.texto(t, buf, sizeof buf);
  if (n < 1 || n >= LARGO_FRASE || !utf8Valido(buf)) return 0;
  copiarTexto(destino, LARGO_FRASE, buf);
  return n;
}

bool hora(const char* s, uint16_t* minuto) {
  auto d = [](char c) { return c >= '0' && c <= '9'; };
  if (largo(s) != 5 || !d(s[0]) || !d(s[1]) || s[2] != ':' || !d(s[3]) || !d(s[4])) return false;
  const int h = (s[0] - '0') * 10 + (s[1] - '0'), m = (s[3] - '0') * 10 + (s[4] - '0');
  if (h > 23 || m > 59) return false;
  *minuto = static_cast<uint16_t>(h * 60 + m);
  return true;
}

// Corta en `max` bytes sin partir una letra de varios bytes.
size_t cortarUtf8(const char* s, size_t n, size_t max) {
  if (n <= max) return n;
  size_t k = max;
  while (k > 0 && (static_cast<uint8_t>(s[k]) & 0xC0) == 0x80) k--;
  return k;
}
}  // namespace

const char* nombreSituacion(Situacion s) {
  const int i = static_cast<int>(s);
  return i < N_SITUACIONES ? NOMBRES_SITUACION[i] : "";
}

bool situacionPorNombre(const char* n, size_t l, Situacion* s) {
  for (int i = 0; i < N_SITUACIONES; i++) {
    if (largo(NOMBRES_SITUACION[i]) == l && iguales(NOMBRES_SITUACION[i], n, l)) {
      *s = static_cast<Situacion>(i);
      return true;
    }
  }
  return false;
}

const char* leerHabla(const Json& j, int raiz, Habla* h) {
  const int ha = j.buscar(raiz, "habla");
  if (ha < 0) return nullptr;
  if (j.tipo(ha) != TipoJson::Objeto) return "habla: no es un objeto";
  static const char* const CLAVES[] = {"frecuencia", "propias"};
  if (!soloEstas(j, ha, CLAVES)) return "habla: claves válidas: frecuencia y propias";
  Habla nuevo;
  const int fr = j.buscar(ha, "frecuencia");
  if (fr >= 0) {
    const float f = j.numero(fr, -1);
    if (j.tipo(fr) != TipoJson::Numero || f < 0 || f > 1) return "habla: frecuencia de 0 a 1";
    nuevo.frecuencia = static_cast<uint8_t>(f * 100 + 0.5f);
  }
  const int pr = j.buscar(ha, "propias");
  if (pr >= 0) {
    if (j.tipo(pr) != TipoJson::Objeto) return "habla: propias es un objeto (situación → lista de frases)";
    int k = pr + 1;
    for (int i = 0; i < j.cantidad(pr); i++, k = j.saltar(k + 1)) {
      char nombre[24];
      Situacion s;
      const int nl = j.texto(k, nombre, sizeof nombre);
      if (nl <= 0 || !situacionPorNombre(nombre, static_cast<size_t>(nl), &s)) return "habla: situación desconocida";
      const int lista = k + 1;
      if (j.tipo(lista) != TipoJson::Arreglo) return "habla: cada situación es una lista de frases";
      for (int e = 0; e < j.cantidad(lista); e++) {
        if (nuevo.nPropias >= MAX_FRASES_PROPIAS) return "habla: hasta 12 frases propias en total";
        FrasePropia& f = nuevo.propias[nuevo.nPropias];
        if (leerFrase(j, j.elemento(lista, e), f.texto) <= 0) return "habla: cada frase de 1 a 60 bytes (una tilde ocupa 2)";
        f.situacion = s;
        nuevo.nPropias++;
      }
    }
  }
  *h = nuevo;
  return nullptr;
}

const char* leerDespertador(const Json& j, int raiz, Despertador* d) {
  const int de = j.buscar(raiz, "despertador");
  if (de < 0) return nullptr;
  if (j.tipo(de) != TipoJson::Objeto) return "despertador: no es un objeto";
  static const char* const CLAVES[] = {"hora", "dias", "activo", "sonido", "texto", "posponer"};
  if (!soloEstas(j, de, CLAVES)) return "despertador: claves válidas: hora, dias, activo, sonido, texto y posponer";
  Despertador n;
  n.hay = true;
  char t[8];
  const int ho = j.buscar(de, "hora");
  if (ho < 0 || j.texto(ho, t, sizeof t) <= 0 || !hora(t, &n.minuto)) return "despertador: hora es HH:MM (obligatoria)";
  const int di = j.buscar(de, "dias");
  if (di >= 0) {
    char s[12];
    const int k = j.texto(di, s, sizeof s);
    if (k < 0 || k > 7 || !leerDias(s, &n.dias))
      return "despertador: dias son letras de L M X J V S D, sin repetir (vacío = todos)";
  }
  const int ac = j.buscar(de, "activo"), so = j.buscar(de, "sonido");
  if ((ac >= 0 && !esBool(j, ac)) || (so >= 0 && !esBool(j, so))) return "despertador: activo y sonido son true o false";
  n.activo = j.booleano(ac, true);
  n.sonido = j.booleano(so, true);
  const int te = j.buscar(de, "texto");
  if (te >= 0 && leerFrase(j, te, n.texto) <= 0) return "despertador: texto de 1 a 60 bytes (una tilde ocupa 2)";
  const int po = j.buscar(de, "posponer");
  if (po >= 0) {
    const float v = j.numero(po, -1);
    if (j.tipo(po) != TipoJson::Numero || v < 1 || v > 30 || v != static_cast<float>(static_cast<int>(v)))
      return "despertador: posponer en minutos, de 1 a 30";
    n.posponer = static_cast<uint8_t>(v);
  }
  *d = n;
  return nullptr;
}

const char* validarUbicacion(const Json& j, int raiz) {
  const int u = j.buscar(raiz, "ubicacion");
  if (u < 0) return nullptr;
  if (j.tipo(u) != TipoJson::Objeto) return "ubicacion: no es un objeto";
  static const char* const CLAVES[] = {"lat", "lon", "ciudad"};
  if (!soloEstas(j, u, CLAVES)) return "ubicacion: claves válidas: lat, lon y ciudad";
  const int la = j.buscar(u, "lat"), lo = j.buscar(u, "lon"), ci = j.buscar(u, "ciudad");
  if (la < 0 || j.tipo(la) != TipoJson::Numero || j.numero(la, 99) < -90 || j.numero(la, 99) > 90)
    return "ubicacion: lat de -90 a 90 (obligatoria)";
  if (lo < 0 || j.tipo(lo) != TipoJson::Numero || j.numero(lo, 999) < -180 || j.numero(lo, 999) > 180)
    return "ubicacion: lon de -180 a 180 (obligatoria)";
  if (ci >= 0) {
    char c[48];
    const int n = j.tipo(ci) == TipoJson::Texto ? j.texto(ci, c, sizeof c) : -1;
    if (n < 1 || n > 40) return "ubicacion: ciudad de 1 a 40 bytes";
  }
  return nullptr;
}

const char* leerCuidado(const Json& j, int raiz, Cuidado* c) {
  const int cu = j.buscar(raiz, "cuidado");
  if (cu < 0) return nullptr;
  if (j.tipo(cu) != TipoJson::Objeto) return "cuidado: no es un objeto";
  static const char* const CLAVES[] = {"agua", "pausa", "trabajo", "animo", "extranar", "extranarDias", "resumen"};
  if (!soloEstas(j, cu, CLAVES))
    return "cuidado: claves válidas: agua, pausa, trabajo, animo, extranar, extranarDias y resumen";
  Cuidado n;
  auto entero = [&](const char* k, int lo, int hi, int* v) {
    const int t = j.buscar(cu, k);
    if (t < 0) return true;
    const float f = j.numero(t, -1);
    if (j.tipo(t) != TipoJson::Numero || f != static_cast<float>(static_cast<int>(f))) return false;
    *v = static_cast<int>(f);
    return *v == 0 || (*v >= lo && *v <= hi);
  };
  int agua = 0, pausa = 0, dias = 2;
  if (!entero("agua", 1, 8, &agua)) return "cuidado: agua cada 1 a 8 horas (0 = nunca)";
  if (!entero("pausa", 30, 240, &pausa)) return "cuidado: pausa de 30 a 240 minutos (0 = nunca)";
  if (!entero("extranarDias", 1, 7, &dias) || dias == 0) return "cuidado: extranarDias de 1 a 7";
  n.agua = static_cast<uint8_t>(agua), n.pausa = static_cast<uint8_t>(pausa);
  static const char* const BOOLEANOS[] = {"animo", "extranar", "resumen"};
  for (const char* k : BOOLEANOS) {
    const int t = j.buscar(cu, k);
    if (t >= 0 && !esBool(j, t)) return "cuidado: animo, extranar y resumen son true o false";
  }
  n.animo = j.booleano(j.buscar(cu, "animo"), true);
  const int tr = j.buscar(cu, "trabajo");
  if (tr >= 0) {
    static const char* const CT[] = {"desde", "hasta", "dias"};
    if (j.tipo(tr) != TipoJson::Objeto || !soloEstas(j, tr, CT)) return "cuidado: trabajo es {desde, hasta, dias}";
    char t[12];
    const int de = j.buscar(tr, "desde"), ha = j.buscar(tr, "hasta"), di = j.buscar(tr, "dias");
    if (de >= 0 && (j.texto(de, t, sizeof t) <= 0 || !hora(t, &n.desde))) return "cuidado: trabajo.desde es HH:MM";
    if (ha >= 0 && (j.texto(ha, t, sizeof t) <= 0 || !hora(t, &n.hasta))) return "cuidado: trabajo.hasta es HH:MM";
    if (n.desde >= n.hasta) return "cuidado: trabajo.desde antes que trabajo.hasta";
    if (di >= 0) {
      const int k = j.texto(di, t, sizeof t);
      if (k < 0 || k > 7 || !leerDias(t, &n.dias)) return "cuidado: trabajo.dias son letras de L M X J V S D";
    }
  }
  *c = n;
  return nullptr;
}

namespace {
// Compara sin distinguir mayúsculas (ASCII y las tildes de 2 bytes).
bool igualesSinMayusculas(const char* a, const char* b) {
  auto bajar = [](uint8_t c) { return (c >= 'A' && c <= 'Z') ? static_cast<uint8_t>(c + 32) : c; };
  for (; *a && *b; a++, b++) {
    uint8_t x = static_cast<uint8_t>(*a), y = static_cast<uint8_t>(*b);
    // ASCII: A-Z → a-z; las tildes (segundo byte de 0xC3 0x81 Á … 0xC3 0xA1 á) difieren en 0x20.
    x = bajar(x), y = bajar(y);
    if (x != y) {
      const bool tildes = static_cast<uint8_t>(a[0]) >= 0x80 && static_cast<uint8_t>(b[0]) >= 0x80 &&
                          ((x | 0x20) == (y | 0x20));
      if (!tildes) return false;
    }
  }
  return *a == *b;
}
}  // namespace

const char* leerAvisos(const Json& j, int raiz, const Config& c, Avisos* a) {
  const int av = j.buscar(raiz, "avisos");
  if (av < 0) return nullptr;
  if (j.tipo(av) != TipoJson::Arreglo) return "avisos: es una lista";
  if (j.cantidad(av) > MAX_AVISOS) return "avisos: hasta 8";
  static const char* const CLAVES[] = {"tipo", "app", "contacto", "expresion", "comportamiento", "decir", "sonido"};
  Avisos n;
  for (int i = 0; i < j.cantidad(av); i++) {
    const int o = j.elemento(av, i);
    if (j.tipo(o) != TipoJson::Objeto || !soloEstas(j, o, CLAVES))
      return "avisos: claves válidas: tipo, app, contacto, expresion, comportamiento, decir y sonido";
    Aviso& x = n.lista[n.n];
    const int ti = j.buscar(o, "tipo");
    if (ti < 0 || !(j.textoIgual(ti, "notificacion") || j.textoIgual(ti, "llamada")))
      return "avisos: tipo es notificacion o llamada";
    x.llamada = j.textoIgual(ti, "llamada");
    static const char* const TEXTOS[] = {"app", "contacto"};
    for (const char* k : TEXTOS) {
      const int t = j.buscar(o, k);
      if (t < 0) continue;
      char* d = iguales(k, "app") ? x.app : x.contacto;
      char buf[LARGO_APP + 8];
      const int l = j.tipo(t) == TipoJson::Texto ? j.texto(t, buf, sizeof buf) : -1;
      if (l < 0 || l >= LARGO_APP) return "avisos: app y contacto de hasta 24 bytes";
      copiarTexto(d, LARGO_APP, buf);
    }
    char e[LARGO_FRASE];
    const int ex = j.buscar(o, "expresion");
    if (ex >= 0) {
      const Config::Cara cara = j.texto(ex, e, sizeof e) > 0 ? c.resolverCara(e) : Config::Cara();
      if (!cara.existe()) return "avisos: una expresion que no existe";
      x.caraVida = cara.vida, x.caraDibujo = cara.dibujo;
    }
    const int co = j.buscar(o, "comportamiento");
    if (co >= 0) {
      const int k = j.texto(co, e, sizeof e) > 0 ? c.vida.indiceComportamiento(e) : -1;
      if (k < 0) return "avisos: un comportamiento que no existe";
      x.comportamiento = static_cast<int8_t>(k);
    }
    const int de = j.buscar(o, "decir");
    if (de >= 0) {
      char buf[LARGO_DECIR_AVISO + 8];
      const int l = j.tipo(de) == TipoJson::Texto ? j.texto(de, buf, sizeof buf) : -1;
      if (l < 1 || l >= LARGO_DECIR_AVISO || !utf8Valido(buf)) return "avisos: decir de 1 a 40 bytes";
      copiarTexto(x.decir, sizeof x.decir, buf);
    }
    const int so = j.buscar(o, "sonido");
    if (so >= 0 && !esBool(j, so)) return "avisos: sonido es true o false";
    x.sonido = j.booleano(so, true);
    n.n++;
  }
  *a = n;
  return nullptr;
}

namespace {
const char* const ICONOS_HABITO[] = {"gota", "libro", "pesa", "zen", "pastilla", "cama", "fruta", "estrella"};
static_assert(sizeof ICONOS_HABITO / sizeof ICONOS_HABITO[0] == static_cast<int>(IconoHabito::Cantidad), "iconos");
}  // namespace

const char* leerHabitos(const Json& j, int raiz, Habitos* h) {
  const int ha = j.buscar(raiz, "habitos");
  if (ha < 0) return nullptr;
  if (j.tipo(ha) != TipoJson::Arreglo) return "habitos: es una lista";
  if (j.cantidad(ha) > MAX_HABITOS) return "habitos: hasta 4";
  static const char* const CLAVES[] = {"nombre", "icono", "hora", "dias"};
  Habitos n;
  for (int i = 0; i < j.cantidad(ha); i++) {
    const int o = j.elemento(ha, i);
    if (j.tipo(o) != TipoJson::Objeto || !soloEstas(j, o, CLAVES)) return "habitos: claves válidas: nombre, icono, hora y dias";
    Habito& x = n.lista[n.n];
    char buf[LARGO_HABITO + 8];
    const int no = j.buscar(o, "nombre");
    const int l = no >= 0 && j.tipo(no) == TipoJson::Texto ? j.texto(no, buf, sizeof buf) : -1;
    if (l < 1 || l >= LARGO_HABITO || !utf8Valido(buf)) return "habitos: nombre de 1 a 16 bytes";
    copiarTexto(x.nombre, sizeof x.nombre, buf);
    const int ic = j.buscar(o, "icono");
    if (ic >= 0) {
      int k = -1;
      for (int c = 0; c < static_cast<int>(IconoHabito::Cantidad); c++)
        if (j.textoIgual(ic, ICONOS_HABITO[c])) k = c;
      if (k < 0) return "habitos: icono es gota, libro, pesa, zen, pastilla, cama, fruta o estrella";
      x.icono = static_cast<IconoHabito>(k);
    }
    const int ho = j.buscar(o, "hora");
    if (ho >= 0) {
      char t[8];
      if (j.tipo(ho) != TipoJson::Texto || j.texto(ho, t, sizeof t) <= 0 || !hora(t, &x.minuto)) return "habitos: hora es HH:MM";
      x.conHora = true;
    }
    const int di = j.buscar(o, "dias");
    if (di >= 0) {
      char t[12];
      const int k = j.tipo(di) == TipoJson::Texto ? j.texto(di, t, sizeof t) : -1;
      if (k < 0 || k > 7 || !leerDias(t, &x.dias)) return "habitos: dias son letras de L M X J V S D, sin repetir (vacío = todos)";
    }
    n.n++;
  }
  *h = n;
  return nullptr;
}

const char* leerPresencia(const Json& j, int raiz, Presencia* p) {
  const int pr = j.buscar(raiz, "presencia");
  if (pr < 0) return nullptr;
  if (j.tipo(pr) != TipoJson::Objeto) return "presencia: no es un objeto";
  static const char* const CLAVES[] = {"preguntar", "texto"};
  if (!soloEstas(j, pr, CLAVES)) return "presencia: claves válidas: preguntar y texto";
  Presencia n;
  const int pg = j.buscar(pr, "preguntar");
  if (pg >= 0 && !esBool(j, pg)) return "presencia: preguntar es true o false";
  n.preguntar = j.booleano(pg, true);
  const int te = j.buscar(pr, "texto");
  if (te >= 0) {
    char buf[LARGO_PRESENCIA + 8];
    const int l = j.tipo(te) == TipoJson::Texto ? j.texto(te, buf, sizeof buf) : -1;
    if (l < 0 || l >= LARGO_PRESENCIA || !utf8Valido(buf)) return "presencia: texto de hasta 40 bytes";
    copiarTexto(n.texto, sizeof n.texto, buf);
  }
  *p = n;
  return nullptr;
}

const char* leerFechas(const Json& j, int raiz, FechasTuyas* f) {
  const int fe = j.buscar(raiz, "fechas");
  if (fe < 0) return nullptr;
  if (j.tipo(fe) != TipoJson::Arreglo) return "fechas: es una lista";
  if (j.cantidad(fe) > MAX_FECHAS) return "fechas: hasta 6";
  static const char* const CLAVES[] = {"fecha", "texto"};
  FechasTuyas n;
  for (int i = 0; i < j.cantidad(fe); i++) {
    const int o = j.elemento(fe, i);
    if (j.tipo(o) != TipoJson::Objeto || !soloEstas(j, o, CLAVES)) return "fechas: claves válidas: fecha y texto";
    FechaTuya& x = n.lista[n.n];
    char t[8];
    const int fc = j.buscar(o, "fecha");
    auto d = [](char c) { return c >= '0' && c <= '9'; };
    if (fc < 0 || j.tipo(fc) != TipoJson::Texto || j.texto(fc, t, sizeof t) != 5 || !d(t[0]) || !d(t[1]) || t[2] != '-' ||
        !d(t[3]) || !d(t[4]))
      return "fechas: fecha es MM-DD (obligatoria)";
    const int mes = (t[0] - '0') * 10 + (t[1] - '0'), dia = (t[3] - '0') * 10 + (t[4] - '0');
    static const int DIAS[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (mes < 1 || mes > 12 || dia < 1 || dia > DIAS[mes - 1]) return "fechas: fecha es MM-DD (obligatoria)";
    x.fecha = static_cast<int16_t>(mes * 100 + dia);
    char buf[LARGO_FECHA + 8];
    const int te = j.buscar(o, "texto");
    const int l = te >= 0 && j.tipo(te) == TipoJson::Texto ? j.texto(te, buf, sizeof buf) : -1;
    if (l < 1 || l >= LARGO_FECHA || !utf8Valido(buf)) return "fechas: texto de 1 a 40 bytes (obligatorio)";
    copiarTexto(x.texto, sizeof x.texto, buf);
    n.n++;
  }
  *f = n;
  return nullptr;
}

const char* nombreIconoHabito(IconoHabito i) {
  return static_cast<int>(i) < static_cast<int>(IconoHabito::Cantidad) ? ICONOS_HABITO[static_cast<int>(i)] : "";
}

int buscarAviso(const Avisos& a, bool llamada, const char* app, const char* contacto) {
  for (int i = 0; i < a.n; i++) {
    const Aviso& x = a.lista[i];
    if (x.llamada != llamada) continue;
    if (x.app[0] && !igualesSinMayusculas(x.app, app ? app : "")) continue;
    if (x.contacto[0] && !igualesSinMayusculas(x.contacto, contacto ? contacto : "")) continue;
    return i;
  }
  return -1;
}

size_t completarFrase(const char* p, const char* dueno, const char* robot, char* salida, size_t cap, const char* clima,
                      const char* app, const char* contacto, Plantillas extra) {
  if (cap == 0) return 0;
  // Se arma entero y se corta al final, sin partir una letra.
  char buf[256];
  size_t m = 0;
  auto agregar = [&](const char* s, size_t k) {
    for (size_t i = 0; i < k && m < sizeof buf - 1; i++) buf[m++] = s[i];
  };
  struct Marca {
    const char* clave;
    const char* valor;
  };
  const Marca marcas[] = {{"{dueno}", dueno}, {"{robot}", robot}, {"{clima}", clima}, {"{app}", app}, {"{contacto}", contacto}};
  for (const char* c = p; *c;) {
    bool hecho = false;
    if (*c == '{') {
      // Las de la v1.9 ({de}, {titulo}…): "{" + clave + "}".
      Marca otra = {nullptr, nullptr};
      size_t lo = 0;
      for (int e = 0; e < extra.n && !otra.clave; e++) {
        const size_t l = largo(extra.lista[e].clave);
        if (iguales(c + 1, extra.lista[e].clave, l) && c[1 + l] == '}') otra = {c, extra.lista[e].valor}, lo = l + 2;
      }
      constexpr int N_MARCAS = static_cast<int>(sizeof marcas / sizeof marcas[0]);
      for (int mi = otra.clave ? -1 : 0; mi < N_MARCAS; mi++) {
        const Marca& k = mi < 0 ? otra : marcas[mi];
        const size_t lk = mi < 0 ? lo : largo(k.clave);
        if (mi >= 0 && !iguales(c, k.clave, lk)) continue;
        if (k.valor && k.valor[0]) {
          agregar(k.valor, largo(k.valor));
        } else {
          // Vacío: se lleva lo de antes ("¡Hola, {dueno}!" → "¡Hola!") y, si
          // quedó al principio ("{dueno}, ¿me cargás?"), lo de después.
          while (m > 0 && (buf[m - 1] == ' ' || buf[m - 1] == ',' || buf[m - 1] == ':')) m--;
          if (m == 0)
            while (c[lk] == ',' || c[lk] == ' ' || c[lk] == ':') c++;
        }
        c += lk;
        hecho = true;
        break;
      }
    }
    if (hecho) continue;
    agregar(c, 1);
    c++;
  }
  buf[m] = 0;
  const size_t n = cortarUtf8(buf, m, cap - 1);
  copiarTexto(salida, cap, buf, n);
  return n;
}

const char* presetCercano(const Personalidad& p) {
  const char* mejor = PRESETS[0];
  float dMejor = 1e9f;
  for (const char* n : PRESETS) {
    Personalidad q;
    if (!presetPersonalidad(n, &q)) continue;
    float d = 0;
    for (int i = 0; i < N_RASGOS; i++) d += (p[i] - q[i]) * (p[i] - q[i]);
    if (d < dMejor) dMejor = d, mejor = n;
  }
  return mejor;
}

bool elegirFrase(const Habla& h, Situacion s, const char* preset, const char* dueno, const char* robot, Azar& azar,
                 char* salida, size_t cap, const char* clima, Plantillas extra) {
  const bool hayDueno = dueno && dueno[0];
  // Sin dueño, las que empiezan con {dueno}; y las que usan una plantilla de
  // la v1.9 que vino vacía ("Un mail de {de}." sin de quién).
  auto sirve = [&](const char* t) {
    if (!hayDueno && iguales(t, "{dueno}", 7)) return false;
    for (const char* c = t; *c; c++) {
      if (*c != '{') continue;
      for (int e = 0; e < extra.n; e++) {
        const Plantilla& x = extra.lista[e];
        const size_t l = largo(x.clave);
        if ((!x.valor || !x.valor[0]) && iguales(c + 1, x.clave, l) && c[1 + l] == '}') return false;
      }
    }
    return true;
  };
  // Las propias del usuario: si tiene para esta situación, solo esas (son su voz).
  int propias = 0;
  for (int i = 0; i < h.nPropias; i++)
    if (h.propias[i].situacion == s && sirve(h.propias[i].texto)) propias++;
  if (propias > 0) {
    int k = static_cast<int>(azar.entre(0, static_cast<uint32_t>(propias - 1)));
    for (int i = 0; i < h.nPropias; i++) {
      if (h.propias[i].situacion != s || !sirve(h.propias[i].texto)) continue;
      if (k-- == 0) {
        completarFrase(h.propias[i].texto, dueno, robot, salida, cap, clima, nullptr, nullptr, extra);
        return true;
      }
    }
  }
  const SituacionBase* b = base(s);
  if (!b) return false;
  // Peso 1 las de todos, 2 las de su preset; las de otros presets, nada.
  auto peso = [&](const FraseBase& f) {
    if (!sirve(f.texto)) return 0;
    if (!f.preset[0]) return 1;
    return preset && iguales(f.preset, preset) ? 2 : 0;
  };
  int total = 0;
  for (int i = 0; i < b->cantidad; i++) total += peso(FRASES_BASE[b->primera + i]);
  if (total == 0) return false;
  int k = static_cast<int>(azar.entre(0, static_cast<uint32_t>(total - 1)));
  for (int i = 0; i < b->cantidad; i++) {
    const FraseBase& f = FRASES_BASE[b->primera + i];
    k -= peso(f);
    if (k < 0) {
      completarFrase(f.texto, dueno, robot, salida, cap, clima, nullptr, nullptr, extra);
      return true;
    }
  }
  return false;
}

bool Hablador::quiere(Situacion s, uint32_t segundos, uint32_t dia, uint8_t frecuencia, Azar& azar) {
  const SituacionBase* b = base(s);
  if (!b) return false;
  const int i = static_cast<int>(s);
  // Un día nuevo (o, sin calendario, 24 h de reloj): vuelve a cero el tope.
  if (dia ? dia != dia_ : segundos - diaDesde_ >= 86400u) {
    dia_ = dia, diaDesde_ = segundos, hoy_ = 0;
  }
  const uint16_t ahora = static_cast<uint16_t>(segundos / 60u + 1u);
  auto paso = [&](uint16_t desde) { return static_cast<uint16_t>(ahora - desde); };  // minutos
  if (!b->siempre) {
    if (frecuencia == 0) return false;
    const float f = frecuencia / 50.0f;  // 0,5 → ×1
    if (hoy_ >= static_cast<int>(FRASES_POR_DIA * f + 0.5f)) return false;
    if (ultimaCualquiera_ && paso(ultimaCualquiera_) < static_cast<uint32_t>(FRASES_SEPARACION_S / 60 / f)) return false;
    if (ultima_[i] && paso(ultima_[i]) < static_cast<uint32_t>(b->enfriamiento / 60)) return false;
    if (azar.real() >= minf(1, b->probabilidad / 100.0f * f)) return false;
    hoy_++;
    ultimaCualquiera_ = ahora ? ahora : 1;
  }
  ultima_[i] = ahora ? ahora : 1;
  return true;
}

bool situacionSiempre(Situacion s) {
  const SituacionBase* b = base(s);
  return b && b->siempre;
}

bool textoUtf8Valido(const char* s, size_t n) {
  char t[256];
  if (n >= sizeof t) return false;
  copiarTexto(t, sizeof t, s, n);
  return largo(t) == n && utf8Valido(t);
}

uint32_t duracionGlobo(const char* texto) {
  const uint32_t ms = 2500 + 60 * static_cast<uint32_t>(largo(texto));
  return ms < 3000 ? 3000 : ms > 10000 ? 10000 : ms;
}

}  // namespace sintetica
