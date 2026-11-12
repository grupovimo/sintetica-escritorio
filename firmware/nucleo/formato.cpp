#include "formato.h"

#include "formato_vida.h"
#include "rostro.h"

#include "caras_base.h"

namespace sintetica {

namespace {

const char* const NOMBRES_EVENTO[] = {
    "tacto", "doble-toque", "levantado", "sacudida", "boca-abajo", "vinculado", "app-abierta", "despertado",
    "caricia", "muchos-toques", "sacudida-suave", "sacudida-fuerte", "inclinado", "caida", "voz", "nombre",
    "ruido", "musica", "silencio", "notificacion", "llamada", "bateria-baja", "bateria-critica", "enchufado",
    "desenchufado", "cargado", "sin-wifi", "cambio-de-cara", "bicho-cerca", "volvio", "hito",
    "recordatorio", "pomodoro", "despertador", "juego", "animo", "cara-detectada", "oscuridad", "mano-cerca",
    "toque-recibido", "devolver-toque", "agenda", "llegaste", "te-fuiste", "habito", "crecio"};
static_assert(sizeof NOMBRES_EVENTO / sizeof NOMBRES_EVENTO[0] == static_cast<int>(Evento::Cantidad), "eventos");

// Memoria de trabajo compartida (no reentrante: el núcleo corre en un solo hilo).
TokenJson g_tokens[MAX_TOKENS_JSON];

ResultadoConfig error(ErrorConfig e, const char* detalle) {
  ResultadoConfig r;
  r.error = e;
  r.detalle = detalle;
  return r;
}

bool esNumero(const Json& j, int i) { return j.valido(i) && j.tipo(i) == TipoJson::Numero; }

// Lee un número obligatorio dentro de [lo, hi].
bool numeroEn(const Json& j, int obj, const char* clave, float lo, float hi, float* salida) {
  int i = j.buscar(obj, clave);
  if (!esNumero(j, i)) return false;
  float v = j.numero(i, 0);
  if (v < lo || v > hi) return false;
  *salida = v;
  return true;
}

// Lee un número opcional; si está mal, falla.
bool numeroOpcional(const Json& j, int obj, const char* clave, float lo, float hi, float* salida) {
  int i = j.buscar(obj, clave);
  if (i < 0) return true;
  return numeroEn(j, obj, clave, lo, hi, salida);
}

bool leerPunto(const Json& j, int arr, Punto* p) {
  if (!j.valido(arr) || j.tipo(arr) != TipoJson::Arreglo || j.cantidad(arr) != 2) return false;
  int a = j.elemento(arr, 0), b = j.elemento(arr, 1);
  if (!esNumero(j, a) || !esNumero(j, b)) return false;
  p->x = j.numero(a, 0);
  p->y = j.numero(b, 0);
  return absf(p->x) <= 2 && absf(p->y) <= 2;
}

bool leerPuntos(const Json& j, int obj, int minimo, int maximo, Forma* f) {
  int arr = j.buscar(obj, "puntos");
  if (!j.valido(arr) || j.tipo(arr) != TipoJson::Arreglo) return false;
  int n = j.cantidad(arr);
  if (n < minimo || n > maximo) return false;
  for (int k = 0; k < n; k++)
    if (!leerPunto(j, j.elemento(arr, k), &f->puntos[k])) return false;
  f->nPuntos = static_cast<uint8_t>(n);
  return true;
}

ResultadoConfig leerForma(const Json& j, int obj, Forma* f) {
  const auto MAL = [](const char* d) { return error(ErrorConfig::Forma, d); };
  if (!j.valido(obj) || j.tipo(obj) != TipoJson::Objeto) return MAL("forma: no es un objeto");
  *f = Forma();
  int t = j.buscar(obj, "forma");
  char tipo[16];
  if (j.texto(t, tipo, sizeof tipo) < 0) return MAL("forma: falta \"forma\"");

  // Atributos comunes.
  int c = j.buscar(obj, "color");
  if (c >= 0) {
    char col[LARGO_NOMBRE];
    if (j.texto(c, col, sizeof col) < 0 || !interpretarColor(col, &f->color, &f->acento))
      return MAL("forma: color desconocido");
  }
  f->fijo = j.booleano(j.buscar(obj, "fijo"), false);
  f->progreso = j.booleano(j.buscar(obj, "progreso"), false);
  float audio = -1;
  if (!numeroOpcional(j, obj, "audio", 0, 7, &audio)) return MAL("forma: audio fuera de rango");
  f->audio = static_cast<int8_t>(audio);

  const bool centro = numeroEn(j, obj, "x", -2, 2, &f->x) && numeroEn(j, obj, "y", -2, 2, &f->y);

  if (iguales(tipo, "pildora") || iguales(tipo, "corte")) {
    f->tipo = iguales(tipo, "pildora") ? TipoForma::Pildora : TipoForma::Corte;
    if (!centro || !numeroEn(j, obj, "w", 0, 2, &f->w) || !numeroEn(j, obj, "h", 0, 2, &f->h))
      return MAL("pildora/corte: faltan x, y, w o h");
    if (f->tipo == TipoForma::Pildora) {
      if (!numeroOpcional(j, obj, "r", 0, 2, &f->r)) return MAL("pildora: r fuera de rango");
      f->relleno = j.booleano(j.buscar(obj, "relleno"), true);
      if (!numeroOpcional(j, obj, "trazo", 0, 2, &f->trazo)) return MAL("pildora: trazo");
      if (!f->relleno && f->trazo <= 0) return MAL("pildora: sin relleno necesita trazo");
    } else {
      if (!numeroOpcional(j, obj, "izq", 0, 2, &f->izq) ||
          !numeroOpcional(j, obj, "der", 0, 2, &f->der))
        return MAL("corte: izq/der fuera de rango");
    }
  } else if (iguales(tipo, "circulo")) {
    f->tipo = TipoForma::Circulo;
    if (!centro || !numeroEn(j, obj, "r", 0, 2, &f->r)) return MAL("circulo: faltan x, y o r");
  } else if (iguales(tipo, "arco")) {
    f->tipo = TipoForma::Arco;
    if (!leerPuntos(j, obj, 3, 3, f)) return MAL("arco: necesita 3 puntos");
    if (!numeroEn(j, obj, "trazo", 0, 2, &f->trazo)) return MAL("arco: falta trazo");
  } else if (iguales(tipo, "corazon")) {
    f->tipo = TipoForma::Corazon;
    if (!centro) return MAL("corazon: faltan x o y");
    if (!numeroOpcional(j, obj, "tamano", 0, 2, &f->tamano)) return MAL("corazon: tamano");
  } else if (iguales(tipo, "espiral")) {
    f->tipo = TipoForma::Espiral;
    if (!centro || !numeroEn(j, obj, "r0", 0, 2, &f->r0) ||
        !numeroEn(j, obj, "paso", 0, 2, &f->paso) || !numeroEn(j, obj, "giro", 0, 40, &f->giro) ||
        !numeroEn(j, obj, "trazo", 0, 2, &f->trazo))
      return MAL("espiral: faltan campos");
  } else if (iguales(tipo, "poligono")) {
    f->tipo = TipoForma::Poligono;
    if (!leerPuntos(j, obj, 3, MAX_PUNTOS, f)) return MAL("poligono: de 3 a 12 puntos");
  } else if (iguales(tipo, "franjas")) {
    f->tipo = TipoForma::Franjas;
    int arr = j.buscar(obj, "bandas");
    if (!j.valido(arr) || j.tipo(arr) != TipoJson::Arreglo || j.cantidad(arr) > MAX_BANDAS)
      return MAL("franjas: hasta 6 bandas");
    for (int k = 0; k < j.cantidad(arr); k++) {
      int b = j.elemento(arr, k);
      Banda& bd = f->bandas[k];
      if (!numeroEn(j, b, "y", -2, 2, &bd.y) || !numeroEn(j, b, "h", 0, 2, &bd.h) ||
          !numeroEn(j, b, "dx", -2, 2, &bd.dx))
        return MAL("franjas: banda incompleta");
    }
    f->nPuntos = static_cast<uint8_t>(j.cantidad(arr));
  } else {
    return MAL("forma desconocida");
  }
  return {};
}

ResultadoConfig leerExpresion(const Json& j, int obj, Expresion* e) {
  if (!j.valido(obj) || j.tipo(obj) != TipoJson::Objeto)
    return error(ErrorConfig::Campo, "expresion: no es un objeto");
  e->parpadea = j.booleano(j.buscar(obj, "parpadea"), true);
  int arr = j.buscar(obj, "formas");
  if (!j.valido(arr) || j.tipo(arr) != TipoJson::Arreglo || j.cantidad(arr) < 1)
    return error(ErrorConfig::Forma, "expresion: falta \"formas\"");
  if (j.cantidad(arr) > MAX_FORMAS) return error(ErrorConfig::Demasiadas, "expresion: más de 12 formas");
  e->nFormas = 0;
  for (int k = 0; k < j.cantidad(arr); k++) {
    ResultadoConfig r = leerForma(j, j.elemento(arr, k), &e->formas[k]);
    if (!r.ok()) return r;
    e->nFormas++;
  }
  return {};
}

}  // namespace

bool nombreValido(const char* s) {
  size_t n = largo(s);
  if (n == 0 || n > 24 || s[0] < 'a' || s[0] > 'z') return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}

namespace {

// Guarda la expresión en `c`: reemplaza la del mismo nombre o la agrega.
ResultadoConfig guardarExpresion(Config& c, const Expresion& e) {
  int i = c.indiceExpresion(e.nombre);
  if (i < 0) {
    if (c.nExpresiones >= MAX_EXPRESIONES) return error(ErrorConfig::Demasiadas, "más de 24 expresiones");
    i = c.nExpresiones++;
  }
  c.expresiones[i] = e;
  return {};
}

bool leerHora(const char* s, int16_t* minutos) {
  auto d = [](char c) { return c >= '0' && c <= '9'; };
  if (!d(s[0]) || !d(s[1]) || s[2] != ':' || !d(s[3]) || !d(s[4])) return false;
  int h = (s[0] - '0') * 10 + (s[1] - '0');
  int m = (s[3] - '0') * 10 + (s[4] - '0');
  if (h > 23 || m > 59) return false;
  *minutos = static_cast<int16_t>(h * 60 + m);
  return true;
}

// "bateria<15", "humor>0.5" o un nombre de evento.
bool leerCondicion(const char* s, Condicion* c) {
  if (eventoPorNombre(s, &c->evento)) {
    c->tipo = Condicion::Tipo::Evento;
    return true;
  }
  const char* op = s;
  while (*op && *op != '<' && *op != '>') op++;
  if (!*op) return false;
  size_t n = static_cast<size_t>(op - s);
  char var[16];
  copiarTexto(var, sizeof var, s, n);
  if (iguales(var, "bateria")) c->variable = Variable::Bateria;
  else if (iguales(var, "humor")) c->variable = Variable::Humor;
  else return false;
  c->tipo = *op == '<' ? Condicion::Tipo::Menor : Condicion::Tipo::Mayor;
  // Reusar el lector de números del JSON: el resto es un número JSON válido.
  const char* num = op + 1;
  Json jn;
  TokenJson t[1];
  if (jn.parsear(num, largo(num), t, 1) != 1 || jn.tipo(0) != TipoJson::Numero) return false;
  c->valor = jn.numero(0, 0);
  return true;
}

bool leerAccion(const char* s, Accion* a) {
  // "estado.animo+0.2"
  if (iguales(s, "estado.", 7)) {
    const char* op = s + 7;
    while (*op && *op != '+' && *op != '-') op++;
    if (!*op || op == s + 7) return false;
    VarVida v;
    if (!varVidaPorNombre(s, static_cast<size_t>(op - s), &v) || static_cast<int>(v) >= N_ESTADO) return false;
    Json jn;
    TokenJson t[1];
    if (jn.parsear(op + 1, largo(op + 1), t, 1) != 1 || jn.tipo(0) != TipoJson::Numero) return false;
    const float d = jn.numero(0, 0);
    if (d < 0 || d > 1) return false;
    a->tipo = Accion::Tipo::Estado;
    a->variable = static_cast<int8_t>(v);
    const float delta = *op == '-' ? -d : d;
    a->cent = static_cast<int8_t>(delta * 100 + (delta < 0 ? -0.5f : 0.5f));
    copiarTexto(a->argumento, sizeof a->argumento, s + 7);
    return true;
  }
  const char* dos = s;
  while (*dos && *dos != ':') dos++;
  if (!*dos || !dos[1]) return false;
  char tipo[16];
  copiarTexto(tipo, sizeof tipo, s, static_cast<size_t>(dos - s));
  if (iguales(tipo, "expresion")) a->tipo = Accion::Tipo::Expresion;
  else if (iguales(tipo, "sonido")) a->tipo = Accion::Tipo::Sonido;
  else if (iguales(tipo, "led")) a->tipo = Accion::Tipo::Led;
  else if (iguales(tipo, "animacion")) a->tipo = Accion::Tipo::Animacion;
  else if (iguales(tipo, "reflejo")) a->tipo = Accion::Tipo::Reflejo;
  else if (iguales(tipo, "comportamiento")) a->tipo = Accion::Tipo::Comportamiento;
  else if (iguales(tipo, "particula")) a->tipo = Accion::Tipo::Particula;
  else if (iguales(tipo, "decir")) a->tipo = Accion::Tipo::Decir;
  else return false;
  return copiarTexto(a->argumento, sizeof a->argumento, dos + 1) > 0;
}

// Una animación: {"duracion", "repetir", "sobre", "pistas": [{"forma", "centro", "claves"}]}.
ResultadoConfig leerAnimacion(const Json& j, int obj, const Config& c, Animacion* a) {
  const auto MAL = [](const char* d) { return error(ErrorConfig::Campo, d); };
  if (!j.valido(obj) || j.tipo(obj) != TipoJson::Objeto) return MAL("animacion: no es un objeto");
  float dur = 0;
  if (!numeroEn(j, obj, "duracion", 1, MAX_DURACION_MS, &dur)) return MAL("animacion: duracion en ms, de 1 a 60000");
  a->duracion = static_cast<uint16_t>(dur);
  a->repetir = j.booleano(j.buscar(obj, "repetir"), false);
  a->sobre = -1;
  int sobre = j.buscar(obj, "sobre");
  if (sobre >= 0) {
    char n[LARGO_NOMBRE];
    if (j.texto(sobre, n, sizeof n) <= 0) return MAL("animacion: sobre es el nombre de una expresión");
    const int idx = c.indiceExpresion(n);
    if (idx < 0) return MAL("animacion: sobre una expresión que no existe");
    a->sobre = static_cast<int8_t>(idx);
  }
  int pistas = j.buscar(obj, "pistas");
  if (!j.valido(pistas) || j.tipo(pistas) != TipoJson::Arreglo || j.cantidad(pistas) < 1)
    return MAL("animacion: falta \"pistas\"");
  if (j.cantidad(pistas) > MAX_PISTAS) return error(ErrorConfig::Demasiadas, "animacion: más de 6 pistas");
  a->nPistas = 0;
  for (int k = 0; k < j.cantidad(pistas); k++) {
    const int po = j.elemento(pistas, k);
    if (!j.valido(po) || j.tipo(po) != TipoJson::Objeto) return MAL("pista: no es un objeto");
    Pista& p = a->pistas[k];
    p = Pista();
    ResultadoConfig r = leerForma(j, j.buscar(po, "forma"), &p.forma);
    if (!r.ok()) return r;
    if (p.forma.tipo == TipoForma::Franjas) return error(ErrorConfig::Forma, "pista: las franjas no se animan");
    int centro = j.buscar(po, "centro");
    if (centro >= 0) {
      Punto q;
      if (!leerPunto(j, centro, &q)) return MAL("pista: centro es [x, y]");
      p.hayCentro = true, p.cx = q.x, p.cy = q.y;
    }
    int claves = j.buscar(po, "claves");
    if (!j.valido(claves) || j.tipo(claves) != TipoJson::Arreglo || j.cantidad(claves) < 1 ||
        j.cantidad(claves) > MAX_CLAVES)
      return error(ErrorConfig::Demasiadas, "pista: de 1 a 8 claves");
    float anterior = -1;
    for (int m = 0; m < j.cantidad(claves); m++) {
      const int co = j.elemento(claves, m);
      if (!j.valido(co) || j.tipo(co) != TipoJson::Objeto) return MAL("clave: no es un objeto");
      Clave& cl = p.claves[m];
      cl = Clave();
      float t = 0;
      if (!numeroEn(j, co, "t", 0, dur, &t)) return MAL("clave: t en ms, dentro de la duracion");
      if (t < anterior) return MAL("clave: los t van en orden");
      anterior = t;
      cl.t = static_cast<uint16_t>(t);
      if (!numeroOpcional(j, co, "x", -4, 4, &cl.x) || !numeroOpcional(j, co, "y", -4, 4, &cl.y) ||
          !numeroOpcional(j, co, "escala", 0, 8, &cl.escala) ||
          !numeroOpcional(j, co, "rotacion", -3600, 3600, &cl.rotacion) ||
          !numeroOpcional(j, co, "opacidad", 0, 1, &cl.opacidad))
        return MAL("clave: x, y, escala, rotacion u opacidad fuera de rango");
      int col = j.buscar(co, "color");
      if (col >= 0) {
        char t2[LARGO_NOMBRE];
        bool acento;
        if (j.texto(col, t2, sizeof t2) < 0 || !interpretarColor(t2, &cl.color, &acento))
          return MAL("clave: color desconocido");
        cl.hayColor = true;
      }
      int cur = j.buscar(co, "curva");
      if (cur >= 0) {
        char t3[16];
        if (j.texto(cur, t3, sizeof t3) < 0 || !curvaPorNombre(t3, &cl.curva))
          return MAL("clave: curva lineal, suave, entrada, salida, rebote o paso");
      }
      p.nClaves++;
    }
    a->nPistas++;
  }
  return {};
}

ResultadoConfig guardarAnimacion(Config& c, const Animacion& a) {
  int i = c.indiceAnimacion(a.nombre);
  if (i < 0) {
    if (c.nAnimaciones >= MAX_ANIMACIONES) return error(ErrorConfig::Demasiadas, "más de 6 animaciones");
    i = c.nAnimaciones++;
  }
  c.animaciones[i] = a;
  return {};
}

// Resuelve el argumento de una acción contra la config (y valida que exista).
const char* resolverAccion(const Config& c, Accion& a) {
  switch (a.tipo) {
    case Accion::Tipo::Expresion:
    case Accion::Tipo::Reflejo:
      if (!c.resolverCara(a.argumento).existe()) return "regla: expresión que no existe";
      a.indice = static_cast<int8_t>(c.vida.indiceCara(a.argumento));
      return nullptr;
    case Accion::Tipo::Animacion:
      return c.indiceAnimacion(a.argumento) < 0 ? "regla: animación que no existe" : nullptr;
    case Accion::Tipo::Comportamiento: {
      const int i = c.vida.indiceComportamiento(a.argumento);
      if (i < 0) return "regla: comportamiento que no existe";
      a.indice = static_cast<int8_t>(i);
      return nullptr;
    }
    case Accion::Tipo::Particula: {
      TipoParticula t;
      if (!particulaPorNombre(a.argumento, &t)) return "regla: partícula desconocida";
      a.indice = static_cast<int8_t>(t);
      return nullptr;
    }
    case Accion::Tipo::Decir: {
      Situacion s;
      if (!situacionPorNombre(a.argumento, largo(a.argumento), &s)) return "regla: decir:<situación> (una de frases.json)";
      a.indice = static_cast<int8_t>(s);
      return nullptr;
    }
    default:
      return nullptr;
  }
}

// Lee un arreglo de reglas en `destino` (hasta `max`).
ResultadoConfig leerReglas(const Json& j, int reg, const Config& c, Regla* destino, int max, int* n) {
  if (j.tipo(reg) != TipoJson::Arreglo) return error(ErrorConfig::Regla, "reglas: no es un arreglo");
  if (j.cantidad(reg) > max) return error(ErrorConfig::Demasiadas, "reglas: más de 24");
  *n = j.cantidad(reg);
  for (int i = 0; i < *n; i++) {
    int r = j.elemento(reg, i);
    Regla& regla = destino[i];
    regla = Regla();
    char si[LARGO_NOMBRE];
    if (j.texto(j.buscar(r, "si"), si, sizeof si) <= 0 || !leerCondicion(si, &regla.si))
      return error(ErrorConfig::Regla, "regla: \"si\" desconocido");
    const int y = j.buscar(r, "y");
    if (y >= 0) {
      char cond[64];
      if (j.texto(y, cond, sizeof cond) <= 0) return error(ErrorConfig::Regla, "regla: \"y\" inválido");
      regla.nY = static_cast<uint8_t>(leerCondiciones(cond, regla.y, MAX_Y));
      if (!regla.nY) return error(ErrorConfig::Regla, "regla: \"y\" inválido (ej. \"afecto>0.6,noche\")");
    }
    int ent = j.buscar(r, "entonces");
    if (!j.valido(ent) || j.tipo(ent) != TipoJson::Arreglo || j.cantidad(ent) < 1 ||
        j.cantidad(ent) > MAX_ACCIONES)
      return error(ErrorConfig::Regla, "regla: \"entonces\" de 1 a 6 acciones");
    for (int a = 0; a < j.cantidad(ent); a++) {
      char acc[48];
      if (j.texto(j.elemento(ent, a), acc, sizeof acc) <= 0 || !leerAccion(acc, &regla.entonces[a]))
        return error(ErrorConfig::Regla, "regla: acción inválida");
      const char* mal = resolverAccion(c, regla.entonces[a]);
      if (mal) return error(ErrorConfig::Regla, mal);
      regla.nAcciones++;
    }
  }
  return {};
}

}  // namespace

const char* nombreEvento(Evento e) {
  auto i = static_cast<int>(e);
  return i < static_cast<int>(Evento::Cantidad) ? NOMBRES_EVENTO[i] : "?";
}

Evento eventoPadre(Evento e) {
  switch (e) {
    case Evento::SacudidaSuave:
    case Evento::SacudidaFuerte:
      return Evento::Sacudida;
    case Evento::Caricia:
    case Evento::MuchosToques:
      return Evento::Tacto;
    case Evento::Caida:
      return Evento::Levantado;
    case Evento::Nombre:
      return Evento::Voz;
    case Evento::Crecio:
      return Evento::Hito;
    default:
      return Evento::Cantidad;
  }
}

bool eventoPorNombre(const char* nombreOriginal, Evento* salida) {
  // "sacudida.fuerte", "nombre_escuchado" y "toque" también valen.
  char nombre[32];
  copiarTexto(nombre, sizeof nombre, nombreOriginal);
  for (char* c = nombre; *c; c++)
    if (*c == '_' || *c == '.') *c = '-';
  if (iguales(nombre, "toque")) copiarTexto(nombre, sizeof nombre, "tacto");
  if (iguales(nombre, "nombre-escuchado")) copiarTexto(nombre, sizeof nombre, "nombre");
  if (iguales(nombre, "ruido-fuerte")) copiarTexto(nombre, sizeof nombre, "ruido");
  if (iguales(nombre, "silencio-largo")) copiarTexto(nombre, sizeof nombre, "silencio");
  if (iguales(nombre, "cargando")) copiarTexto(nombre, sizeof nombre, "enchufado");
  for (int i = 0; i < static_cast<int>(Evento::Cantidad); i++) {
    if (iguales(nombre, NOMBRES_EVENTO[i])) {
      *salida = static_cast<Evento>(i);
      return true;
    }
  }
  return false;
}

int Config::indiceAnimacion(const char* n) const {
  for (int i = 0; i < nAnimaciones; i++)
    if (iguales(animaciones[i].nombre, n)) return i;
  return -1;
}

bool curvaPorNombre(const char* n, Curva* c) {
  static const char* const nombres[] = {"lineal", "suave", "entrada", "salida", "rebote", "paso"};
  for (int i = 0; i < 6; i++) {
    if (iguales(n, nombres[i])) {
      *c = static_cast<Curva>(i);
      return true;
    }
  }
  return false;
}

float aplicarCurva(Curva c, float t) {
  t = limitar(t, 0, 1);
  switch (c) {
    case Curva::Lineal: return t;
    case Curva::Suave: return suave(t);
    case Curva::Entrada: return t * t * t;
    case Curva::Salida: {
      const float u = 1 - t;
      return 1 - u * u * u;
    }
    case Curva::Rebote: {
      // "Back out": se pasa un poco y vuelve.
      constexpr float K = 1.70158f;
      const float u = t - 1;
      return 1 + (K + 1) * u * u * u + K * u * u;
    }
    case Curva::Paso: return t < 1 ? 0.0f : 1.0f;
  }
  return t;
}

Cuadro cuadroDePista(const Pista& p, uint32_t t) {
  Cuadro q;
  if (p.nClaves == 0) return q;
  const Clave* a = &p.claves[0];
  const Clave* b = a;
  float k = 0;
  if (t <= p.claves[0].t) {
    b = a;
  } else if (t >= p.claves[p.nClaves - 1].t) {
    a = b = &p.claves[p.nClaves - 1];
  } else {
    for (int i = 0; i + 1 < p.nClaves; i++) {
      if (t >= p.claves[i].t && t < p.claves[i + 1].t) {
        a = &p.claves[i], b = &p.claves[i + 1];
        const float largoTramo = static_cast<float>(b->t - a->t);
        k = largoTramo > 0 ? aplicarCurva(a->curva, (t - a->t) / largoTramo) : 1.0f;
        break;
      }
    }
  }
  q.x = mezclar(a->x, b->x, k);
  q.y = mezclar(a->y, b->y, k);
  q.escala = mezclar(a->escala, b->escala, k);
  q.rotacion = mezclar(a->rotacion, b->rotacion, k);
  q.opacidad = limitar(mezclar(a->opacidad, b->opacidad, k), 0, 1);
  if (a->hayColor || b->hayColor) {
    const uint32_t ca = a->hayColor ? a->color : b->color, cb = b->hayColor ? b->color : a->color;
    auto canal = [&](int sh) {
      const float va = static_cast<float>((ca >> sh) & 0xFF), vb = static_cast<float>((cb >> sh) & 0xFF);
      return static_cast<uint32_t>(limitar(mezclar(va, vb, limitar(k, 0, 1)), 0, 255) + 0.5f) << sh;
    };
    q.hayColor = true;
    q.color = canal(16) | canal(8) | canal(0);
  }
  return q;
}

int Config::indiceExpresion(const char* n) const {
  for (int i = 0; i < nExpresiones; i++)
    if (iguales(expresiones[i].nombre, n)) return i;
  return -1;
}

bool interpretarColor(const char* s, uint32_t* rgb, bool* acento) {
  struct Token {
    const char* nombre;
    uint32_t valor;
  };
  static const Token tokens[] = {
      {"naranja", paleta::NARANJA},       {"naranja-profundo", paleta::NARANJA_PROFUNDO},
      {"pantalla", paleta::PANTALLA},     {"tinta", paleta::TINTA},
      {"tinta-suave", paleta::TINTA_SUAVE}, {"linea", paleta::LINEA},
  };
  for (const auto& t : tokens) {
    if (iguales(s, t.nombre)) {
      *rgb = t.valor;
      *acento = t.valor == paleta::NARANJA;
      return true;
    }
  }
  if (s[0] != '#' || largo(s) != 7) return false;
  uint32_t v = 0;
  for (int i = 1; i < 7; i++) {
    char c = s[i];
    int d = (c >= '0' && c <= '9') ? c - '0'
            : (c >= 'a' && c <= 'f') ? c - 'a' + 10
            : (c >= 'A' && c <= 'F') ? c - 'A' + 10
                                     : -1;
    if (d < 0) return false;
    v = v * 16 + static_cast<uint32_t>(d);
  }
  *rgb = v;
  *acento = false;
  return true;
}

ResultadoConfig aplicarExpresion(Config& c, const char* texto, size_t n) {
  Json j;
  if (j.parsear(texto, n, g_tokens, MAX_TOKENS_JSON) <= 0)
    return error(ErrorConfig::JsonInvalido, "expresion: JSON inválido");
  static Expresion e;
  e = Expresion();
  if (j.texto(j.buscar(0, "nombre"), e.nombre, sizeof e.nombre) < 0 || !nombreValido(e.nombre))
    return error(ErrorConfig::Campo, "expresion: nombre inválido");
  ResultadoConfig r = leerExpresion(j, 0, &e);
  if (!r.ok()) return r;
  return guardarExpresion(c, e);
}

ResultadoConfig cargarBase(Config& c) {
  // Sin temporal: `c = Config()` arma ~110 KB en la pila y en la placa la
  // pila de la tarea principal es de 8 KB.
  new (static_cast<void*>(&c), EnSuLugar{}) Config();  // ver formato.h
  copiarTexto(c.nombre, sizeof c.nombre, "bicho");
  for (int i = 0; i < CANTIDAD_CARAS_BASE; i++) {
    ResultadoConfig r = aplicarExpresion(c, CARAS_BASE[i], largo(CARAS_BASE[i]));
    if (!r.ok()) return r;
  }
  // El motor de vida: expresiones paramétricas, comportamientos y reglas base
  // (shared/formato/vida).
  ResultadoConfig r = cargarVidaBase(c);
  if (!r.ok()) return r;
  const char* reglas = reglasVidaBase();
  Json j;
  if (j.parsear(reglas, largo(reglas), g_tokens, MAX_TOKENS_JSON) <= 0) return error(ErrorConfig::JsonInvalido, "reglas base");
  int n = 0;
  r = leerReglas(j, 0, c, c.reglas, MAX_REGLAS, &n);
  if (!r.ok()) return r;
  c.nReglas = static_cast<uint8_t>(n);
  return {};
}

namespace {
// Aplica la config de usuario sobre `c` (que es memoria de trabajo: si falla,
// queda a medio aplicar y el que llama la descarta).
ResultadoConfig aplicarSobre(Config& c, const char* texto, size_t n) {
  if (n > MAX_CONFIG_BYTES) return error(ErrorConfig::Demasiadas, "config: más de 16 KB");
  Json j;
  if (j.parsear(texto, n, g_tokens, MAX_TOKENS_JSON) <= 0 || j.tipo(0) != TipoJson::Objeto)
    return error(ErrorConfig::JsonInvalido, "config: JSON inválido");

  float version = 0;
  if (!numeroEn(j, 0, "version", 1, 2, &version) || (version != 1 && version != 2))
    return error(ErrorConfig::Version, "config: version debe ser 1 o 2");
  c.version = static_cast<int>(version);

  int nom = j.buscar(0, "nombre");
  if (nom >= 0) {
    int len = j.texto(nom, c.nombre, sizeof c.nombre);
    if (len <= 0) return error(ErrorConfig::Campo, "config: nombre inválido");
  }

  int cara = j.buscar(0, "cara");
  if (cara >= 0) {
    if (j.tipo(cara) != TipoJson::Objeto) return error(ErrorConfig::Campo, "cara: no es un objeto");
    int col = j.buscar(cara, "color");
    if (col >= 0) {
      char hex[LARGO_NOMBRE];
      bool acento;
      if (j.texto(col, hex, sizeof hex) != 7 || hex[0] != '#' ||
          !interpretarColor(hex, &c.colorOjos, &acento))
        return error(ErrorConfig::Campo, "cara: color debe ser #RRGGBB");
      c.hayColorOjos = true;
    }
    if (!numeroOpcional(j, cara, "tamano", 0.5f, 1.5f, &c.tamano))
      return error(ErrorConfig::Campo, "cara: tamano entre 0.5 y 1.5");
  }

  int per = j.buscar(0, "personalidad");
  if (per >= 0) {
    if (j.tipo(per) != TipoJson::Objeto) return error(ErrorConfig::Campo, "personalidad: no es un objeto");
    if (!numeroOpcional(j, per, "energia", 0, 1, &c.energia) ||
        !numeroOpcional(j, per, "timidez", 0, 1, &c.timidez))
      return error(ErrorConfig::Campo, "personalidad: valores entre 0 y 1");
    c.vida.personalidad.energia = c.energia;
    c.vida.personalidad.timidez = c.timidez;
    int s = j.buscar(per, "sueno");
    if (s >= 0) {
      char h[16];
      if (j.texto(s, h, sizeof h) != 11 || h[5] != '-' || !leerHora(h, &c.suenoDesde) ||
          !leerHora(h + 6, &c.suenoHasta) || c.suenoDesde == c.suenoHasta)
        return error(ErrorConfig::Campo, "personalidad: sueno es HH:MM-HH:MM");
      // La v1 decía cuándo duerme; la v2, el horario en que está despierto.
      c.vida.duerme = c.suenoDesde;
      c.vida.despierta = c.suenoHasta;
    }
  }

  int exs = j.buscar(0, "expresiones");
  if (exs >= 0) {
    if (j.tipo(exs) != TipoJson::Objeto) return error(ErrorConfig::Campo, "expresiones: no es un objeto");
    static Expresion e;
    int k = exs + 1;
    for (int i = 0; i < j.cantidad(exs); i++) {
      e = Expresion();
      if (j.texto(k, e.nombre, sizeof e.nombre) < 0 || !nombreValido(e.nombre))
        return error(ErrorConfig::Campo, "expresiones: nombre inválido");
      if (esCaraParametrica(j, k + 1)) {
        // v2: una expresión paramétrica (ojo, izq, der, gx, ...).
        ResultadoConfig r = leerCaraVida(j, k + 1, c, e.nombre, true);
        if (!r.ok()) return r;
        k = j.saltar(k + 1);
        continue;
      }
      e.propia = true;
      ResultadoConfig r = leerExpresion(j, k + 1, &e);
      if (!r.ok()) return r;
      r = guardarExpresion(c, e);
      if (!r.ok()) return r;
      k = j.saltar(k + 1);
    }
  }

  {
    ResultadoConfig r = leerVidaConfig(j, 0, c);
    if (!r.ok()) return r;
  }
  // La cara de siempre ("cara": {"expresion": …}), cuando ya están todas las expresiones.
  if (cara >= 0) {
    const int ex = j.buscar(cara, "expresion");
    if (ex >= 0) {
      char n[LARGO_NOMBRE];
      const int i = j.texto(ex, n, sizeof n) > 0 ? c.vida.indiceCara(n) : -1;
      if (i < 0) return error(ErrorConfig::Campo, "cara: expresion es el nombre de una expresión paramétrica o de imagen que exista");
      c.vida.reposo = static_cast<int8_t>(i);
    }
  }

  int ans = j.buscar(0, "animaciones");
  if (ans >= 0) {
    if (j.tipo(ans) != TipoJson::Objeto) return error(ErrorConfig::Campo, "animaciones: no es un objeto");
    static Animacion a;
    int k = ans + 1;
    for (int i = 0; i < j.cantidad(ans); i++) {
      a = Animacion();
      if (j.texto(k, a.nombre, sizeof a.nombre) < 0 || !nombreValido(a.nombre))
        return error(ErrorConfig::Campo, "animaciones: nombre inválido");
      ResultadoConfig r = leerAnimacion(j, k + 1, c, &a);
      if (!r.ok()) return r;
      r = guardarAnimacion(c, a);
      if (!r.ok()) return r;
      k = j.saltar(k + 1);
    }
  }

  int reg = j.buscar(0, "reglas");
  if (reg >= 0) {
    // Las reglas del usuario van primero: ganan sobre las base.
    static Regla propias[MAX_REGLAS_USUARIO];
    int nPropias = 0;
    ResultadoConfig rr = leerReglas(j, reg, c, propias, MAX_REGLAS_USUARIO, &nPropias);
    if (!rr.ok()) return rr;
    int total = nPropias + c.nReglas;
    if (total > MAX_REGLAS) total = MAX_REGLAS;
    for (int i = total - 1; i >= nPropias; i--) c.reglas[i] = c.reglas[i - nPropias];
    for (int i = 0; i < nPropias; i++) c.reglas[i] = propias[i];
    c.nReglas = static_cast<uint8_t>(total);
  }

  int lista = j.buscar(0, "lista");
  if (lista >= 0) {
    if (j.tipo(lista) != TipoJson::Arreglo || j.cantidad(lista) > MAX_LISTA)
      return error(ErrorConfig::Campo, "lista: hasta 16 elementos");
    c.nLista = 0;
    for (int i = 0; i < j.cantidad(lista); i++) {
      char t[40];
      if (j.texto(j.elemento(lista, i), t, sizeof t) <= 0) return error(ErrorConfig::Campo, "lista: cada elemento es un texto");
      ElementoLista& el = c.lista[c.nLista];
      if (t[0] == 'c' && t[1] == 'a' && t[2] == 'r' && t[3] == 'a' && t[4] == ':') {
        const Config::Cara cara = c.resolverCara(t + 5);
        if (!cara.existe()) return error(ErrorConfig::Campo, "lista: una cara que no existe");
        el.tipo = cara.dibujo >= 0 ? ElementoLista::Tipo::Cara : ElementoLista::Tipo::CaraVida;
        el.indice = static_cast<uint8_t>(cara.dibujo >= 0 ? cara.dibujo : cara.vida);
      } else if (iguales(t, "animacion:", 10) && t[10]) {
        int idx = c.indiceAnimacion(t + 10);
        if (idx < 0) return error(ErrorConfig::Campo, "lista: una animación que no existe");
        el.tipo = ElementoLista::Tipo::Animacion;
        el.indice = static_cast<uint8_t>(idx);
      } else if (iguales(t, "imagen:", 7) && t[7] >= '0' && t[7] < '0' + MAX_IMAGENES && t[8] == 0) {
        el.tipo = ElementoLista::Tipo::Imagen;
        el.indice = static_cast<uint8_t>(t[7] - '0');
      } else {
        return error(ErrorConfig::Campo, "lista: usá cara:<nombre>, animacion:<nombre> o imagen:<0-7>");
      }
      c.nLista++;
    }
  }
  float rotar = c.rotarMinutos;
  if (!numeroOpcional(j, 0, "rotar", 0, 1440, &rotar)) return error(ErrorConfig::Campo, "rotar: minutos, de 0 a 1440");
  c.rotarMinutos = static_cast<uint16_t>(rotar);

  // Que hable solo, el despertador y dónde está (para el clima de la nube).
  if (const char* m = leerHabla(j, 0, &c.habla)) return error(ErrorConfig::Campo, m);
  if (const char* m = leerDespertador(j, 0, &c.despertador)) return error(ErrorConfig::Campo, m);
  if (const char* m = validarUbicacion(j, 0)) return error(ErrorConfig::Campo, m);
  if (const char* m = leerCuidado(j, 0, &c.cuidado)) return error(ErrorConfig::Campo, m);
  if (const char* m = leerAvisos(j, 0, c, &c.avisos)) return error(ErrorConfig::Campo, m);
  if (const char* m = leerHabitos(j, 0, &c.habitos)) return error(ErrorConfig::Campo, m);
  if (const char* m = leerPresencia(j, 0, &c.presencia)) return error(ErrorConfig::Campo, m);
  if (const char* m = leerFechas(j, 0, &c.fechas)) return error(ErrorConfig::Campo, m);

  int modo = j.buscar(0, "modo");
  if (modo >= 0) {
    static const char* const modos[] = {"mascota", "reloj", "pomodoro", "clima", "notificaciones", "galeria"};
    bool ok = false;
    for (int i = 0; i < 6; i++) {
      if (j.textoIgual(modo, modos[i])) {
        c.modo = static_cast<Modo>(i);
        ok = true;
      }
    }
    if (!ok) return error(ErrorConfig::Campo, "modo desconocido");
  }
  return {};
}
}  // namespace

#ifdef SINTETICA_HERRAMIENTAS
ResultadoConfig aplicarConfig(Config& destino, const char* texto, size_t n) {
  static Config g_copia;  // 64 KB: solo en la PC
  g_copia = destino;
  ResultadoConfig r = aplicarSobre(g_copia, texto, n);
  if (r.ok()) destino = g_copia;
  return r;
}
#endif

ResultadoConfig cargarConfig(Config& destino, const char* texto, size_t n) {
  ResultadoConfig r = cargarBase(destino);
  if (r.ok()) r = aplicarSobre(destino, texto, n);
  if (!r.ok()) {
    const ResultadoConfig mal = r;
    cargarBase(destino);
    return mal;
  }
  return r;
}

}  // namespace sintetica
