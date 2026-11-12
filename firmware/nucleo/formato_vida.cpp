// La parte de la config que es del motor de vida (v2): expresiones
// paramétricas, comportamientos, personalidad, horario, dueño y límites.
#include "formato_vida.h"

#include "rostro.h"
#include "vida_base.h"

namespace sintetica {

namespace {

ResultadoConfig falla(const char* detalle, ErrorConfig e = ErrorConfig::Campo) {
  ResultadoConfig r;
  r.error = e;
  r.detalle = detalle;
  return r;
}

const char* const NOMBRES_VAR[static_cast<int>(VarVida::Cantidad)] = {
    "energia", "animo", "afecto", "aburrimiento", "curiosidad", "sueno", "estres",
    "rasgo.energia", "rasgo.sociabilidad", "rasgo.picardia", "rasgo.timidez", "rasgo.curiosidad", "rasgo.malGenio",
    "bateria", "hora", "humor", "ignorado", "ausencia", "rencor", "dormido", "noche", "cargando",
    "etapa", "dias", "solo", "bajon", "lluvia", "esperando"};

const char* const NOMBRES_RASGO[N_RASGOS] = {"energia", "sociabilidad", "picardia", "timidez", "curiosidad", "malGenio"};

const char* const NOMBRES_MOVIMIENTO[static_cast<int>(Movimiento::Cantidad)] = {
    "ninguno", "rebote", "saltitos", "risa", "mirada-lado", "salto", "encogerse", "dormir", "temblor", "resoplido",
    "lagrima", "achicarse", "picaro", "latido", "bamboleo", "escuchar", "hablar", "pensar", "duda", "acercarse",
    "suspiro", "quieto", "cabeceo", "carga", "orbita", "buscar", "caer", "glitch", "estirarse"};

const char* const NOMBRES_PARPADEO[static_cast<int>(Parpadeo::Cantidad)] = {"normal", "menos", "mas", "no",
                                                                            "lento", "rapido", "doble"};

const char* const NOMBRES_PARTES[N_PARTES_SPRITE] = {"ojo", "ojo_cerrado", "ojo_der", "ojo_der_cerrado", "boca"};

const char* const NOMBRES_MIRADA[static_cast<int>(Mirada::Cantidad)] = {
    "libre", "frente", "arriba", "abajo", "izquierda", "derecha", "lado", "alrededor", "perdida", "reojo", "estimulo",
    "rem", "ovejas", "particula", "puerta"};

template <int N>
int buscarNombre(const char* const (&tabla)[N], const char* n) {
  for (int i = 0; i < N; i++)
    if (iguales(tabla[i], n)) return i;
  return -1;
}

bool leerNumero(const char* s, size_t n, float* v) {
  Json j;
  TokenJson t[1];
  if (j.parsear(s, n, t, 1) != 1 || j.tipo(0) != TipoJson::Numero) return false;
  *v = j.numero(0, 0);
  return true;
}

// "MM-DD" → mes·100 + día.
bool leerFecha(const char* s, int16_t* salida) {
  auto d = [](char c) { return c >= '0' && c <= '9'; };
  if (largo(s) != 5 || !d(s[0]) || !d(s[1]) || s[2] != '-' || !d(s[3]) || !d(s[4])) return false;
  const int m = (s[0] - '0') * 10 + (s[1] - '0'), dia = (s[3] - '0') * 10 + (s[4] - '0');
  if (m < 1 || m > 12 || dia < 1 || dia > 31) return false;
  *salida = static_cast<int16_t>(m * 100 + dia);
  return true;
}

bool leerHoraVida(const char* s, int16_t* minutos) {
  auto d = [](char c) { return c >= '0' && c <= '9'; };
  if (largo(s) != 5 || !d(s[0]) || !d(s[1]) || s[2] != ':' || !d(s[3]) || !d(s[4])) return false;
  const int h = (s[0] - '0') * 10 + (s[1] - '0'), m = (s[3] - '0') * 10 + (s[4] - '0');
  if (h > 23 || m > 59) return false;
  *minutos = static_cast<int16_t>(h * 60 + m);
  return true;
}

// Los parámetros de un objeto (los de un ojo o los globales) a ajustes.
// lado: -1 = globales, 0 = izquierdo, 1 = derecho, 2 = los dos.
ResultadoConfig leerParametros(const Json& j, int obj, int lado, CaraVida* c) {
  int k = obj + 1;
  for (int i = 0; i < j.cantidad(obj); i++, k = j.saltar(k + 1)) {
    char clave[16];
    if (j.texto(k, clave, sizeof clave) <= 0) return falla("expresion: clave inválida");
    bool esOjo = false;
    const int p = parametroPorNombre(clave, &esOjo);
    if (lado < 0) {
      // En el nivel de arriba hay también otras cosas.
      if (iguales(clave, "ojo") || iguales(clave, "izq") || iguales(clave, "der") || iguales(clave, "movimiento") ||
          iguales(clave, "particula") || iguales(clave, "parpadeo") || iguales(clave, "extiende") ||
          iguales(clave, "nombre") || iguales(clave, "numero") || iguales(clave, "disparador") ||
          iguales(clave, "sprites") || iguales(clave, "tamano") || iguales(clave, "boca_tamano") ||
          iguales(clave, "boca_y") || iguales(clave, "espejar_der"))
        continue;
      if (p < 0 || esOjo) return falla("expresion: parámetro desconocido (los del ojo van en \"ojo\", \"izq\" o \"der\")");
    } else if (p < 0 || !esOjo) {
      return falla("expresion: parámetro de ojo desconocido");
    }
    if (j.tipo(k + 1) != TipoJson::Numero) return falla("expresion: los parámetros son números");
    const float v = j.numero(k + 1, 0);
    const InfoParametro& info = infoParametro(p);
    if (v < info.minimo - 1e-6f || v > info.maximo + 1e-6f) return falla("expresion: parámetro fuera de los límites de la marca");
    for (int l = 0; l < 2; l++) {
      if (lado >= 0 && lado != 2 && lado != l) continue;
      const int idx = lado < 0 ? p : p + l * param::POR_OJO;
      int a = 0;
      while (a < c->nAjustes && c->ajustes[a].param != idx) a++;
      if (a == c->nAjustes) {
        if (c->nAjustes >= MAX_AJUSTES) return falla("expresion: demasiados parámetros", ErrorConfig::Demasiadas);
        c->nAjustes++;
      }
      c->ajustes[a] = Ajuste::de(idx, v);
      if (lado < 0) break;
    }
  }
  return {};
}

bool tieneClave(const Json& j, int obj, const char* clave) { return j.buscar(obj, clave) >= 0; }

int leerTextoEn(const Json& j, int obj, const char* clave, char* d, size_t cap) {
  const int t = j.buscar(obj, clave);
  if (t < 0) return 0;
  return j.texto(t, d, cap) > 0 ? 1 : -1;
}

// "dura": 1.5 o [1, 2] (segundos) → ms.
bool leerRango(const Json& j, int obj, const char* clave, float lo, float hi, float escala, float* a, float* b) {
  const int t = j.buscar(obj, clave);
  if (t < 0) return true;
  if (j.tipo(t) == TipoJson::Numero) {
    const float v = j.numero(t, 0);
    if (v < lo || v > hi) return false;
    *a = *b = v * escala;
    return true;
  }
  if (j.tipo(t) != TipoJson::Arreglo || j.cantidad(t) != 2) return false;
  const int x = j.elemento(t, 0), y = j.elemento(t, 1);
  if (j.tipo(x) != TipoJson::Numero || j.tipo(y) != TipoJson::Numero) return false;
  const float va = j.numero(x, 0), vb = j.numero(y, 0);
  if (va < lo || vb > hi || vb < va) return false;
  *a = va * escala, *b = vb * escala;
  return true;
}

// "animo+0.1", "estado.estres-0.4".
bool leerEmpujon(const char* s, int8_t* var, float* delta) {
  const char* op = s;
  while (*op && *op != '+' && *op != '-') op++;
  if (!*op || op == s) return false;
  VarVida v;
  if (!varVidaPorNombre(s, static_cast<size_t>(op - s), &v) || static_cast<int>(v) >= N_ESTADO) return false;
  float n;
  if (!leerNumero(op + 1, largo(op + 1), &n) || n < 0 || n > 1) return false;
  *var = static_cast<int8_t>(v);
  *delta = *op == '-' ? -n : n;
  return true;
}

ResultadoConfig leerPaso(const Json& j, int obj, const Config& c, Paso* p) {
  if (!j.valido(obj) || j.tipo(obj) != TipoJson::Objeto) return falla("comportamiento: cada paso es un objeto");
  char t[LARGO_VIDA];
  int r = leerTextoEn(j, obj, "cara", t, sizeof t);
  if (r < 0) return falla("paso: cara inválida");
  if (r > 0) {
    const int i = c.vida.indiceCara(t);
    if (i < 0) return falla("paso: una cara paramétrica que no existe");
    p->cara = static_cast<int8_t>(i);
  }
  float a = p->duraMin, b = p->duraMax;
  if (!leerRango(j, obj, "dura", 0.05f, 60, 1000, &a, &b)) return falla("paso: dura en segundos (0,05 a 60) o [min, max]");
  p->duraMin = static_cast<uint16_t>(a), p->duraMax = static_cast<uint16_t>(b);
  a = p->vecesMin, b = p->vecesMax;
  if (!leerRango(j, obj, "veces", 1, 20, 1, &a, &b)) return falla("paso: veces de 1 a 20 o [min, max]");
  p->vecesMin = static_cast<uint8_t>(a), p->vecesMax = static_cast<uint8_t>(b);
  if ((r = leerTextoEn(j, obj, "mirada", t, sizeof t)) != 0) {
    Mirada m;
    if (r < 0 || !miradaPorNombre(t, &m)) return falla("paso: mirada desconocida");
    p->mirada = m;
  }
  if ((r = leerTextoEn(j, obj, "parpadeo", t, sizeof t)) != 0) {
    Parpadeo m;
    if (r < 0 || !parpadeoPorNombre(t, &m)) return falla("paso: parpadeo desconocido");
    p->parpadeo = m;
  }
  if ((r = leerTextoEn(j, obj, "particula", t, sizeof t)) != 0) {
    TipoParticula tp;
    if (r < 0 || !particulaPorNombre(t, &tp)) return falla("paso: partícula desconocida");
    p->particula = static_cast<int8_t>(tp);
  }
  float n = p->cuantas;
  const int cu = j.buscar(obj, "cuantas");
  if (cu >= 0) {
    if (j.tipo(cu) != TipoJson::Numero || (n = j.numero(cu, 1)) < 1 || n > MAX_PARTICULAS) return falla("paso: cuantas de 1 a 6");
    p->cuantas = static_cast<uint8_t>(n);
  }
  const int pe = j.buscar(obj, "peso");
  if (pe >= 0) {
    if (j.tipo(pe) != TipoJson::Numero || j.numero(pe, 1) < 0 || j.numero(pe, 1) > 1) return falla("paso: peso de 0 a 1");
    p->pesoQ = static_cast<uint8_t>(j.numero(pe, 1) * 255 + 0.5f);
  }
  if ((r = leerTextoEn(j, obj, "sonido", p->sonido, sizeof p->sonido)) < 0) return falla("paso: sonido de hasta 8 letras");
  char e[32];
  if ((r = leerTextoEn(j, obj, "decir", e, sizeof e)) != 0) {
    Situacion s;
    if (r < 0 || !situacionPorNombre(e, largo(e), &s)) return falla("paso: decir es una situación de frases.json");
    p->decir = static_cast<int8_t>(s);
  }
  if ((r = leerTextoEn(j, obj, "dentro", e, sizeof e)) != 0) {
    TipoParticula tp;
    if (r > 0 && iguales(e, "sueno")) p->dentro = DENTRO_SUENO;
    else if (r < 0 || !particulaPorNombre(e, &tp)) return falla("paso: dentro es una partícula o \"sueno\"");
    else p->dentro = static_cast<int8_t>(tp);
  }
  if ((r = leerTextoEn(j, obj, "estado", e, sizeof e)) != 0) {
    float delta = 0;
    if (r < 0 || !leerEmpujon(e, &p->estadoVar, &delta)) return falla("paso: estado es \"animo+0.1\"");
    p->estadoCent = static_cast<int8_t>(delta * 100 + (delta < 0 ? -0.5f : 0.5f));
  }
  return {};
}

ResultadoConfig leerComportamiento(const Json& j, int obj, const Config& c, Comportamiento* b) {
  if (!j.valido(obj) || j.tipo(obj) != TipoJson::Objeto) return falla("comportamiento: no es un objeto");
  char t[64];
  int r;
  if ((r = leerTextoEn(j, obj, "tipo", t, sizeof t)) != 0) {
    if (iguales(t, "ocioso")) b->tipo = TipoComportamiento::Ocioso;
    else if (iguales(t, "reaccion")) b->tipo = TipoComportamiento::Reaccion;
    else if (iguales(t, "rutina")) b->tipo = TipoComportamiento::Rutina;
    else return falla("comportamiento: tipo es ocioso, reaccion o rutina");
  }
  if ((r = leerTextoEn(j, obj, "cuando", t, sizeof t)) != 0) {
    b->nCuando = static_cast<uint8_t>(r < 0 ? 0 : leerCondiciones(t, b->cuando, MAX_CUANDO));
    if (b->nCuando == 0) return falla("comportamiento: \"cuando\" inválido (ej. \"sueno>0.5,noche\")");
  }
  if ((r = leerTextoEn(j, obj, "necesidad", t, sizeof t)) != 0) {
    VarVida v;
    if (r < 0 || !varVidaPorNombre(t, largo(t), &v)) return falla("comportamiento: necesidad desconocida");
    b->necesidad = static_cast<int8_t>(v);
  }
  const int ba = j.buscar(obj, "base");
  if (ba >= 0) {
    if (j.tipo(ba) != TipoJson::Numero || j.numero(ba, 0) < 0 || j.numero(ba, 0) > 1) return falla("comportamiento: base de 0 a 1");
    b->base = j.numero(ba, 0.3f);
  }
  const int pr = j.buscar(obj, "probabilidad");
  if (pr >= 0) {
    if (j.tipo(pr) != TipoJson::Numero || j.numero(pr, 1) <= 0 || j.numero(pr, 1) > 1) return falla("comportamiento: probabilidad de 0 a 1");
    b->probabilidad = j.numero(pr, 1);
  }
  const int en = j.buscar(obj, "enfriamiento");
  if (en >= 0) {
    if (j.tipo(en) != TipoJson::Numero || j.numero(en, 0) < 0 || j.numero(en, 0) > 86400) return falla("comportamiento: enfriamiento en segundos (hasta un día)");
    b->enfriamiento = static_cast<uint16_t>(minf(j.numero(en, 0), 65535));
  }
  b->interrumpible = j.booleano(j.buscar(obj, "interrumpible"), b->interrumpible);
  const int af = j.buscar(obj, "afinidad");
  if (af >= 0) {
    if (j.tipo(af) != TipoJson::Objeto) return falla("comportamiento: afinidad es un objeto de rasgos");
    int k = af + 1;
    for (int i = 0; i < j.cantidad(af); i++, k = j.saltar(k + 1)) {
      char n[20];
      j.texto(k, n, sizeof n);
      const int ri = buscarNombre(NOMBRES_RASGO, n);
      const float v = j.numero(k + 1, 0);
      if (ri < 0 || j.tipo(k + 1) != TipoJson::Numero || v < -1 || v > 1) return falla("comportamiento: afinidad por rasgo, de -1 a 1");
      b->afinidad[ri] = static_cast<int8_t>(v * 100);
    }
  }
  const int ps = j.buscar(obj, "pasos");
  if (!j.valido(ps) || j.tipo(ps) != TipoJson::Arreglo || j.cantidad(ps) < 1 || j.cantidad(ps) > MAX_PASOS)
    return falla("comportamiento: de 1 a 6 pasos");
  b->nPasos = 0;
  for (int i = 0; i < j.cantidad(ps); i++) {
    b->pasos[i] = Paso();
    ResultadoConfig rp = leerPaso(j, j.elemento(ps, i), c, &b->pasos[i]);
    if (!rp.ok()) return rp;
    b->nPasos++;
  }
  return {};
}

ResultadoConfig guardarComportamiento(DatosVida& v, const Comportamiento& b) {
  int i = v.indiceComportamiento(b.nombre);
  if (i < 0) {
    if (v.nComportamientos >= MAX_COMPORTAMIENTOS) return falla("demasiados comportamientos", ErrorConfig::Demasiadas);
    i = v.nComportamientos++;
  }
  v.comportamientos[i] = b;
  return {};
}

ResultadoConfig guardarCara(DatosVida& v, const CaraVida& c) {
  int i = v.indiceCara(c.nombre);
  if (i < 0) {
    if (v.nCaras >= MAX_CARAS_VIDA) return falla("demasiadas expresiones paramétricas", ErrorConfig::Demasiadas);
    i = v.nCaras++;
  }
  v.caras[i] = c;
  return {};
}

TokenJson g_tok[260];  // los presets y cada dato base (el de la config del usuario usa otros)

bool leerPersonalidadObj(const Json& j, int obj, Personalidad* p) {
  for (int i = 0; i < N_RASGOS; i++) {
    const int t = j.buscar(obj, NOMBRES_RASGO[i]);
    if (t < 0) continue;
    const float v = j.numero(t, -1);
    if (j.tipo(t) != TipoJson::Numero || v < 0 || v > 1) return false;
    (*p)[i] = v;
  }
  return true;
}

// Que el objeto no tenga claves fuera de `claves` (additionalProperties: false).
template <int N>
bool soloClaves(const Json& j, int obj, const char* const (&claves)[N]) {
  int k = obj + 1;
  for (int i = 0; i < j.cantidad(obj); i++, k = j.saltar(k + 1)) {
    char clave[16];
    if (j.texto(k, clave, sizeof clave) <= 0 || buscarNombre(claves, clave) < 0) return false;
  }
  return true;
}

bool esBooleano(const Json& j, int t) { return j.tipo(t) == TipoJson::Verdadero || j.tipo(t) == TipoJson::Falso; }

// Un número de 0 a 1 en centésimos (lo que no es 0 queda en 1 como mínimo:
// 0 es "silencio" y tiene que ser exacto).
bool leerCentesimos(const Json& j, int obj, const char* clave, uint8_t* salida) {
  const int t = j.buscar(obj, clave);
  if (t < 0) return true;
  if (j.tipo(t) != TipoJson::Numero) return false;
  const float v = j.numero(t, -1);
  if (v < 0 || v > 1) return false;
  const int c = static_cast<int>(v * 100 + 0.5f);
  *salida = static_cast<uint8_t>(v > 0 && c == 0 ? 1 : c);
  return true;
}

// Un entero en [lo, hi] (opcional).
bool leerEntero(const Json& j, int obj, const char* clave, int lo, int hi, uint8_t* salida) {
  const int t = j.buscar(obj, clave);
  if (t < 0) return true;
  if (j.tipo(t) != TipoJson::Numero) return false;
  const float v = j.numero(t, -1);
  if (v < lo || v > hi || v != static_cast<float>(static_cast<int>(v))) return false;
  *salida = static_cast<uint8_t>(v);
  return true;
}

const char* const CLAVES_RECORDATORIO[] = {"hora", "dias", "texto", "expresion", "sonido", "activo"};
const char* const CLAVES_VOZ[] = {"volumen", "tipo", "palabra", "conversar", "microfono"};
const char* const CLAVES_POMODORO[] = {"trabajo", "descanso"};
const char* const CLAVES_PROTECTOR[] = {"imagen", "espera"};
const char* const CLAVES_GALERIA[] = {"segundos"};
const char* const TIPOS_VOZ[] = {"grave", "media", "aguda", "robot"};

ResultadoConfig leerRecordatorio(const Json& j, int obj, const Config& c, Recordatorio* r) {
  if (!j.valido(obj) || j.tipo(obj) != TipoJson::Objeto) return falla("recordatorios: cada uno es un objeto");
  if (!soloClaves(j, obj, CLAVES_RECORDATORIO))
    return falla("recordatorios: claves válidas: hora, dias, texto, expresion, sonido y activo");
  char h[8];
  int16_t minuto = 0;
  if (leerTextoEn(j, obj, "hora", h, sizeof h) <= 0 || !leerHoraVida(h, &minuto))
    return falla("recordatorios: hora es HH:MM (obligatoria)");
  r->minuto = static_cast<uint16_t>(minuto);
  const int di = j.buscar(obj, "dias");
  if (di >= 0) {
    char d[12];
    const int n = j.texto(di, d, sizeof d);
    if (n < 0 || n > 7 || !leerDias(d, &r->dias))
      return falla("recordatorios: dias son letras de L M X J V S D, sin repetir (vacío = todos)");
  }
  const int te = j.buscar(obj, "texto");
  char t[LARGO_RECORDATORIO + 8];
  const int nt = te >= 0 ? j.texto(te, t, sizeof t) : -1;
  if (nt < 1 || nt >= LARGO_RECORDATORIO) return falla("recordatorios: texto de 1 a 40 bytes (obligatorio; una tilde ocupa 2)");
  copiarTexto(r->texto, sizeof r->texto, t);
  char e[LARGO_NOMBRE];
  const int re = leerTextoEn(j, obj, "expresion", e, sizeof e);
  if (re < 0) return falla("recordatorios: expresion es el nombre de una expresión");
  const Config::Cara cara = c.resolverCara(re > 0 ? e : "sorpresa");
  if (re > 0 && !cara.existe()) return falla("recordatorios: una expresion que no existe");
  r->caraVida = cara.vida, r->caraDibujo = cara.dibujo;
  const int so = j.buscar(obj, "sonido"), ac = j.buscar(obj, "activo");
  if ((so >= 0 && !esBooleano(j, so)) || (ac >= 0 && !esBooleano(j, ac)))
    return falla("recordatorios: sonido y activo son true o false");
  r->sonido = j.booleano(so, true);
  r->activo = j.booleano(ac, true);
  return {};
}

// recordatorios, voz, pomodoro y protector.
ResultadoConfig leerAvisos(const Json& j, int raiz, Config& c) {
  const int re = j.buscar(raiz, "recordatorios");
  if (re >= 0) {
    if (j.tipo(re) != TipoJson::Arreglo) return falla("recordatorios: es una lista");
    if (j.cantidad(re) > MAX_RECORDATORIOS) return falla("recordatorios: hasta 12", ErrorConfig::Demasiadas);
    c.nRecordatorios = 0;
    for (int i = 0; i < j.cantidad(re); i++) {
      Recordatorio& r = c.recordatorios[i];
      r = Recordatorio();
      ResultadoConfig rr = leerRecordatorio(j, j.elemento(re, i), c, &r);
      if (!rr.ok()) {
        c.nRecordatorios = 0;
        return rr;
      }
      c.nRecordatorios++;
    }
  }
  const int vo = j.buscar(raiz, "voz");
  if (vo >= 0) {
    if (j.tipo(vo) != TipoJson::Objeto) return falla("voz: no es un objeto");
    if (!soloClaves(j, vo, CLAVES_VOZ)) return falla("voz: claves válidas: volumen, tipo, palabra, conversar y microfono");
    Voz v;
    if (!leerCentesimos(j, vo, "volumen", &v.volumen)) return falla("voz: volumen de 0 a 1");
    if (!leerCentesimos(j, vo, "microfono", &v.microfono)) return falla("voz: microfono de 0 a 1");
    char t[12];
    const int r = leerTextoEn(j, vo, "tipo", t, sizeof t);
    if (r != 0) {
      const int i = r > 0 ? buscarNombre(TIPOS_VOZ, t) : -1;
      if (i < 0) return falla("voz: tipo es grave, media, aguda o robot");
      v.tipo = static_cast<Voz::Tipo>(i);
    }
    const int pa = j.buscar(vo, "palabra");
    if (pa >= 0) {
      char p[LARGO_PALABRA + 8];
      const int n = j.texto(pa, p, sizeof p);
      if (n < 1 || n >= LARGO_PALABRA) return falla("voz: palabra de 1 a 16 bytes (una tilde ocupa 2)");
      copiarTexto(v.palabra, sizeof v.palabra, p);
    }
    const int co = j.buscar(vo, "conversar");
    if (co >= 0 && !esBooleano(j, co)) return falla("voz: conversar es true o false");
    v.conversar = j.booleano(co, false);
    c.voz = v;
  }
  const int po = j.buscar(raiz, "pomodoro");
  if (po >= 0) {
    if (j.tipo(po) != TipoJson::Objeto) return falla("pomodoro: no es un objeto");
    if (!soloClaves(j, po, CLAVES_POMODORO)) return falla("pomodoro: claves válidas: trabajo y descanso");
    TramosPomodoro p;
    if (!leerEntero(j, po, "trabajo", 1, 120, &p.trabajo)) return falla("pomodoro: trabajo en minutos, de 1 a 120");
    if (!leerEntero(j, po, "descanso", 1, 60, &p.descanso)) return falla("pomodoro: descanso en minutos, de 1 a 60");
    c.pomodoro = p;
  }
  const int pr = j.buscar(raiz, "protector");
  if (pr >= 0) {
    if (j.tipo(pr) != TipoJson::Objeto) return falla("protector: no es un objeto");
    if (!soloClaves(j, pr, CLAVES_PROTECTOR)) return falla("protector: claves válidas: imagen y espera");
    uint8_t imagen = 255;
    if (j.buscar(pr, "imagen") < 0 || !leerEntero(j, pr, "imagen", 0, MAX_IMAGENES - 1, &imagen))
      return falla("protector: imagen es el espacio de una imagen, de 0 a 7 (obligatorio)");
    const int es = j.buscar(pr, "espera");
    float espera = 10;
    if (es >= 0) {
      espera = j.numero(es, -1);
      if (j.tipo(es) != TipoJson::Numero || espera < 3 || espera > 3600 || espera != static_cast<float>(static_cast<int>(espera)))
        return falla("protector: espera en segundos, de 3 a 3600");
    }
    c.protector.imagen = static_cast<int8_t>(imagen);
    c.protector.espera = static_cast<uint16_t>(espera);
  }
  const int ga = j.buscar(raiz, "galeria");
  if (ga >= 0) {
    if (j.tipo(ga) != TipoJson::Objeto) return falla("galeria: no es un objeto");
    if (!soloClaves(j, ga, CLAVES_GALERIA)) return falla("galeria: la clave válida es segundos");
    const int se = j.buscar(ga, "segundos");
    float segundos = 30;
    if (se >= 0) {
      segundos = j.numero(se, -1);
      if (j.tipo(se) != TipoJson::Numero || segundos < 5 || segundos > 3600 || segundos != static_cast<float>(static_cast<int>(segundos)))
        return falla("galeria: segundos, de 5 a 3600");
    }
    c.galeriaSegundos = static_cast<uint16_t>(segundos);
  }
  return {};
}

}  // namespace

bool leerDias(const char* s, uint8_t* mascara) {
  static const char LETRAS[] = "LMXJVSD";
  uint8_t m = 0;
  for (; *s; s++) {
    int i = 0;
    while (LETRAS[i] && LETRAS[i] != *s) i++;
    if (!LETRAS[i] || (m & (1u << i))) return false;
    m = static_cast<uint8_t>(m | (1u << i));
  }
  *mascara = m ? m : 0x7F;
  return true;
}

const char* nombreVarVida(VarVida v) {
  const int i = static_cast<int>(v);
  return i < static_cast<int>(VarVida::Cantidad) ? NOMBRES_VAR[i] : "";
}

bool varVidaPorNombre(const char* n, size_t largoN, VarVida* v) {
  char t[32];
  copiarTexto(t, sizeof t, n, largoN);
  const char* s = t;
  if (iguales(s, "estado.", 7)) s += 7;
  for (int i = 0; i < static_cast<int>(VarVida::Cantidad); i++)
    if (iguales(s, NOMBRES_VAR[i])) {
      *v = static_cast<VarVida>(i);
      return true;
    }
  return false;
}

int leerCondiciones(const char* s, CondVida* salida, int max) {
  int n = 0;
  while (*s) {
    const char* fin = s;
    while (*fin && *fin != ',') fin++;
    if (n >= max || fin == s) return 0;
    const char* op = s;
    while (op < fin && *op != '<' && *op != '>') op++;
    CondVida& c = salida[n];
    if (op == fin) {
      VarVida v;
      if (!varVidaPorNombre(s, static_cast<size_t>(fin - s), &v)) return 0;
      c.var = v, c.op = CondVida::Op::Verdad, c.valor = 0;
    } else {
      VarVida v;
      float num;
      if (!varVidaPorNombre(s, static_cast<size_t>(op - s), &v) || !leerNumero(op + 1, static_cast<size_t>(fin - op - 1), &num))
        return 0;
      c.var = v, c.op = *op == '<' ? CondVida::Op::Menor : CondVida::Op::Mayor, c.valor = num;
    }
    n++;
    s = *fin ? fin + 1 : fin;
  }
  return n;
}

const char* nombreParteSprite(int p) { return p >= 0 && p < N_PARTES_SPRITE ? NOMBRES_PARTES[p] : ""; }

const char* nombreRasgo(int i) { return i >= 0 && i < N_RASGOS ? NOMBRES_RASGO[i] : ""; }

bool presetPersonalidad(const char* nombre, Personalidad* p) {
  Json j;
  if (j.parsear(PERSONALIDADES_BASE, largo(PERSONALIDADES_BASE), g_tok, 260) <= 0) return false;
  const int o = j.buscar(0, nombre);
  if (o < 0 || j.tipo(o) != TipoJson::Objeto) return false;
  Personalidad q;
  if (!leerPersonalidadObj(j, o, &q)) return false;
  *p = q;
  return true;
}

const char* nombreMovimiento(Movimiento m) {
  const int i = static_cast<int>(m);
  return i < static_cast<int>(Movimiento::Cantidad) ? NOMBRES_MOVIMIENTO[i] : "";
}
bool movimientoPorNombre(const char* n, Movimiento* m) {
  const int i = buscarNombre(NOMBRES_MOVIMIENTO, n);
  if (i < 0) return false;
  *m = static_cast<Movimiento>(i);
  return true;
}
const char* nombreParpadeo(Parpadeo p) {
  const int i = static_cast<int>(p);
  return i < static_cast<int>(Parpadeo::Cantidad) ? NOMBRES_PARPADEO[i] : "";
}
bool parpadeoPorNombre(const char* n, Parpadeo* p) {
  const int i = buscarNombre(NOMBRES_PARPADEO, n);
  if (i < 0) return false;
  *p = static_cast<Parpadeo>(i);
  return true;
}
const char* nombreMirada(Mirada m) {
  const int i = static_cast<int>(m);
  return i < static_cast<int>(Mirada::Cantidad) ? NOMBRES_MIRADA[i] : "";
}
bool miradaPorNombre(const char* n, Mirada* m) {
  const int i = buscarNombre(NOMBRES_MIRADA, n);
  if (i < 0) return false;
  *m = static_cast<Mirada>(i);
  return true;
}

void CaraVida::aplicar(Rostro& r) const {
  for (int i = 0; i < nAjustes; i++) r[ajustes[i].param] = ajustes[i].valor();
}

int DatosVida::indiceCara(const char* n) const {
  for (int i = 0; i < nCaras; i++)
    if (iguales(caras[i].nombre, n)) return i;
  return -1;
}

int DatosVida::indiceComportamiento(const char* n) const {
  for (int i = 0; i < nComportamientos; i++)
    if (iguales(comportamientos[i].nombre, n)) return i;
  return -1;
}

Config::Cara Config::resolverCara(const char* n) const {
  Cara c;
  const int d = indiceExpresion(n), v = vida.indiceCara(n);
  if (d >= 0 && (expresiones[d].propia || v < 0)) {
    c.dibujo = static_cast<int8_t>(d);
    return c;
  }
  c.vida = static_cast<int8_t>(v);
  return c;
}

bool esCaraParametrica(const Json& j, int obj) {
  if (!j.valido(obj) || j.tipo(obj) != TipoJson::Objeto) return false;
  if (tieneClave(j, obj, "formas")) return false;
  return true;
}

// Lo de una cara de imagen: "sprites" ({"ojo": 0, …}, de 0 a 11) y la
// geometría ("tamano", "boca_tamano", "boca_y", "espejar_der"). *hay: si es
// una cara de imagen (tiene "sprites" o lo heredó).
ResultadoConfig leerImagen(const Json& j, int obj, CaraImagen* im, bool* hay) {
  const int sp = j.buscar(obj, "sprites");
  if (sp >= 0) {
    if (j.tipo(sp) != TipoJson::Objeto) return falla("expresion: sprites es un objeto {\"ojo\": 0, \"boca\": 2, …}");
    CaraImagen nueva;
    nueva.espejar = im->espejar, nueva.tamano = im->tamano, nueva.bocaTamano = im->bocaTamano, nueva.bocaY = im->bocaY;
    int k = sp + 1;
    for (int i = 0; i < j.cantidad(sp); i++, k = j.saltar(k + 1)) {
      char clave[20];
      const int parte = j.texto(k, clave, sizeof clave) > 0 ? buscarNombre(NOMBRES_PARTES, clave) : -1;
      if (parte < 0) return falla("expresion: las partes de sprites son ojo, ojo_cerrado, ojo_der, ojo_der_cerrado y boca");
      const float v = j.numero(k + 1, -1);
      if (j.tipo(k + 1) != TipoJson::Numero || v < 0 || v > MAX_SPRITES - 1 || v != static_cast<float>(static_cast<int>(v)))
        return falla("expresion: cada parte de sprites es el número de un sprite, de 0 a 11");
      nueva.sprite[parte] = static_cast<int8_t>(v);
    }
    if (!nueva.tiene(ParteSprite::Ojo)) return falla("expresion: una cara de imagen necesita al menos sprites.ojo");
    *im = nueva;
    *hay = true;
  }
  struct Medida {
    const char* clave;
    float lo, hi;
    int16_t* destino;
  } medidas[] = {{"tamano", 0.08f, 0.8f, &im->tamano}, {"boca_tamano", 0.04f, 0.9f, &im->bocaTamano}, {"boca_y", -0.7f, 0.3f, &im->bocaY}};
  for (const Medida& m : medidas) {
    const int t = j.buscar(obj, m.clave);
    if (t < 0) continue;
    if (!*hay) return falla("expresion: tamano, boca_tamano, boca_y y espejar_der van solo en una cara de imagen (con sprites)");
    const float v = j.numero(t, -9);
    if (j.tipo(t) != TipoJson::Numero || v < m.lo - 1e-6f || v > m.hi + 1e-6f)
      return falla("expresion: tamano de 0,08 a 0,8; boca_tamano de 0,04 a 0,9; boca_y de -0,7 a 0,3");
    *m.destino = static_cast<int16_t>(v * 10000.0f + (v < 0 ? -0.5f : 0.5f));
  }
  const int es = j.buscar(obj, "espejar_der");
  if (es >= 0) {
    if (!*hay) return falla("expresion: tamano, boca_tamano, boca_y y espejar_der van solo en una cara de imagen (con sprites)");
    if (j.tipo(es) != TipoJson::Verdadero && j.tipo(es) != TipoJson::Falso) return falla("expresion: espejar_der es true o false");
    im->espejar = j.booleano(es, true);
  }
  return {};
}

ResultadoConfig leerCaraVida(const Json& j, int obj, Config& c, const char* nombre, bool propia) {
  if (!j.valido(obj) || j.tipo(obj) != TipoJson::Objeto) return falla("expresion: no es un objeto");
  static CaraVida cv;
  cv = CaraVida();
  copiarTexto(cv.nombre, sizeof cv.nombre, nombre);
  cv.propia = propia;
  // Si ya existe (una base con el mismo nombre), los ajustes nuevos van encima.
  char ext[LARGO_VIDA];
  int r = leerTextoEn(j, obj, "extiende", ext, sizeof ext);
  if (r < 0) return falla("expresion: extiende es el nombre de otra expresión");
  const int desde = r > 0 ? c.vida.indiceCara(ext) : c.vida.indiceCara(nombre);
  if (r > 0 && desde < 0) return falla("expresion: extiende una expresión que no existe");
  CaraImagen im;
  bool hayImagen = false;
  if (desde >= 0) {
    const CaraVida& b = c.vida.caras[desde];
    cv.nAjustes = b.nAjustes;
    for (int i = 0; i < b.nAjustes; i++) cv.ajustes[i] = b.ajustes[i];
    cv.movimiento = b.movimiento, cv.particula = b.particula, cv.parpadeo = b.parpadeo;
    if (b.imagen >= 0) im = c.vida.imagenes[b.imagen], hayImagen = true;
  }
  ResultadoConfig rr = leerImagen(j, obj, &im, &hayImagen);
  if (!rr.ok()) return rr;
  rr = leerParametros(j, obj, -1, &cv);
  if (!rr.ok()) return rr;
  const char* lados[3] = {"ojo", "izq", "der"};
  const int codigos[3] = {2, 0, 1};
  for (int i = 0; i < 3; i++) {
    const int o = j.buscar(obj, lados[i]);
    if (o < 0) continue;
    if (j.tipo(o) != TipoJson::Objeto) return falla("expresion: ojo, izq y der son objetos");
    rr = leerParametros(j, o, codigos[i], &cv);
    if (!rr.ok()) return rr;
  }
  char t[LARGO_VIDA];
  if ((r = leerTextoEn(j, obj, "movimiento", t, sizeof t)) != 0)
    if (r < 0 || !movimientoPorNombre(t, &cv.movimiento)) return falla("expresion: movimiento desconocido");
  if ((r = leerTextoEn(j, obj, "parpadeo", t, sizeof t)) != 0)
    if (r < 0 || !parpadeoPorNombre(t, &cv.parpadeo)) return falla("expresion: parpadeo desconocido");
  if ((r = leerTextoEn(j, obj, "particula", t, sizeof t)) != 0) {
    TipoParticula tp;
    if (r < 0 || (!iguales(t, "ninguna") && !particulaPorNombre(t, &tp))) return falla("expresion: partícula desconocida");
    cv.particula = iguales(t, "ninguna") ? -1 : static_cast<int8_t>(tp);
  }
  if (hayImagen) {
    // Si ya era de imagen (la misma, reemplazada), usa su lugar.
    const int ya = c.vida.indiceCara(nombre);
    int slot = ya >= 0 ? c.vida.caras[ya].imagen : -1;
    if (slot < 0) {
      if (c.vida.nImagenes >= MAX_CARAS_IMAGEN) return falla("expresiones: hasta 8 caras de imagen", ErrorConfig::Demasiadas);
      slot = c.vida.nImagenes++;
    }
    c.vida.imagenes[slot] = im;
    cv.imagen = static_cast<int8_t>(slot);
  }
  return guardarCara(c.vida, cv);
}

ResultadoConfig leerVidaConfig(const Json& j, int raiz, Config& c) {
  DatosVida& v = c.vida;
  const int se = j.buscar(raiz, "semilla");
  if (se >= 0) {
    const float s = j.numero(se, -1);
    if (j.tipo(se) != TipoJson::Numero || s < 0 || s > 4294967295.0f) return falla("semilla: entero positivo");
    v.semilla = static_cast<uint32_t>(s);
  }
  const int per = j.buscar(raiz, "personalidad");
  if (per >= 0) {
    if (j.tipo(per) != TipoJson::Objeto) return falla("personalidad: no es un objeto");
    char p[20];
    const int r = leerTextoEn(j, per, "preset", p, sizeof p);
    if (r != 0 && (r < 0 || !presetPersonalidad(p, &v.personalidad))) return falla("personalidad: preset desconocido");
    if (!leerPersonalidadObj(j, per, &v.personalidad)) return falla("personalidad: valores entre 0 y 1");
  }
  const int ho = j.buscar(raiz, "horario");
  if (ho >= 0) {
    if (j.tipo(ho) != TipoJson::Objeto) return falla("horario: no es un objeto");
    char h[8];
    int r = leerTextoEn(j, ho, "despierta", h, sizeof h);
    if (r < 0 || (r > 0 && !leerHoraVida(h, &v.despierta))) return falla("horario: despierta es HH:MM");
    r = leerTextoEn(j, ho, "duerme", h, sizeof h);
    if (r < 0 || (r > 0 && !leerHoraVida(h, &v.duerme))) return falla("horario: duerme es HH:MM");
    if (v.despierta == v.duerme) return falla("horario: despierta y duerme no pueden ser iguales");
  }
  const int du = j.buscar(raiz, "dueno");
  if (du >= 0) {
    if (j.tipo(du) != TipoJson::Objeto) return falla("dueno: no es un objeto");
    int r = leerTextoEn(j, du, "nombre", v.dueno, sizeof v.dueno);
    if (r < 0) return falla("dueno: nombre de 1 a 24 bytes");
    char f[8];
    r = leerTextoEn(j, du, "cumple", f, sizeof f);
    if (r < 0 || (r > 0 && !leerFecha(f, &v.cumpleDueno))) return falla("dueno: cumple es MM-DD");
  }
  char f[8];
  int r = leerTextoEn(j, raiz, "cumple", f, sizeof f);
  if (r < 0 || (r > 0 && !leerFecha(f, &v.cumpleBicho))) return falla("cumple: MM-DD (la fecha de armado)");
  const int li = j.buscar(raiz, "limites_marca");
  if (li >= 0) {
    if (j.tipo(li) != TipoJson::Objeto) return falla("limites_marca: no es un objeto");
    const int b = j.buscar(li, "brillo_max");
    if (b >= 0) {
      const float x = j.numero(b, -1);
      if (j.tipo(b) != TipoJson::Numero || x < 0.2f || x > 1) return falla("limites_marca: brillo_max de 0,2 a 1");
      v.brilloMax = x;
    }
    char col[LARGO_NOMBRE];
    r = leerTextoEn(j, li, "color", col, sizeof col);
    if (r != 0) {
      bool acento;
      if (r < 0 || !interpretarColor(col, &c.colorOjos, &acento)) return falla("limites_marca: color #RRGGBB");
      c.hayColorOjos = true;
    }
  }
  const int co = j.buscar(raiz, "comportamientos");
  if (co >= 0) {
    if (j.tipo(co) != TipoJson::Objeto) return falla("comportamientos: no es un objeto");
    static Comportamiento b;
    int k = co + 1;
    for (int i = 0; i < j.cantidad(co); i++, k = j.saltar(k + 1)) {
      b = Comportamiento();
      if (j.texto(k, b.nombre, sizeof b.nombre) <= 0 || !nombreValido(b.nombre))
        return falla("comportamientos: nombre inválido");
      b.propio = true;
      ResultadoConfig rr = leerComportamiento(j, k + 1, c, &b);
      if (!rr.ok()) return rr;
      rr = guardarComportamiento(v, b);
      if (!rr.ok()) return rr;
    }
  }
  return leerAvisos(j, raiz, c);
}

ResultadoConfig cargarVidaBase(Config& c) {
  new (static_cast<void*>(&c.vida), EnSuLugar{}) DatosVida();  // sin temporal (~24 KB): ver formato.h
  Json j;
  for (const DatoVidaBase& d : CARAS_VIDA_BASE) {
    if (j.parsear(d.json, largo(d.json), g_tok, 260) <= 0) return falla("vida base: expresión inválida");
    ResultadoConfig r = leerCaraVida(j, 0, c, d.nombre, false);
    if (!r.ok()) return r;
  }
  static Comportamiento b;
  for (const DatoVidaBase& d : COMPORTAMIENTOS_BASE) {
    if (j.parsear(d.json, largo(d.json), g_tok, 260) <= 0) return falla("vida base: comportamiento inválido");
    b = Comportamiento();
    copiarTexto(b.nombre, sizeof b.nombre, d.nombre);
    ResultadoConfig r = leerComportamiento(j, 0, c, &b);
    if (!r.ok()) return r;
    r = guardarComportamiento(c.vida, b);
    if (!r.ok()) return r;
  }
  return {};
}

const char* reglasVidaBase() { return REGLAS_VIDA_BASE; }

}  // namespace sintetica
