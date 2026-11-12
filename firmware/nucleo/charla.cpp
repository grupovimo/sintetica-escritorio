// La charla sin IA (charla.h). Todo en la pila: una frase de hasta 200 bytes,
// 32 palabras, sin memoria dinámica ni libc.
#include "charla.h"

#include "charla_base.h"

namespace sintetica {

namespace {

// ---------------------------------------------------------------- texto

constexpr int MAX_PALABRAS = 32;
constexpr int LARGO_BUFFER = 320;
constexpr int MAX_LETRAS = 24;  // una palabra más larga se compara cortada

// Marcas de cada palabra.
constexpr uint8_t RELLENO = 1;    // "che", "el", "que": no cuentan para ver cuánto se entendió
constexpr uint8_t HORA = 2;       // es parte de una hora o un día ("a las 7", "mañana")
constexpr uint8_t ROBOT = 4;      // el nombre del robot ("Toto"): nunca es el del dueño

// Bits de "necesita" (los de charla_base.h). N_CONFIRMA: preguntó "¿Te llamás
// Zoe?" y espera un sí o un no (solo en EstadoCharla::espera).
constexpr uint8_t N_HORA = 1, N_TEXTO = 2, N_JUEGO = 4, N_HABITO = 8, N_QUIEN = 16, N_CONFIRMA = 32;
// El grupo de respuestas de una intención con la pregunta antes de hacer algo
// (los otros: 0 respuestas, 1 falta, 2 sin).
constexpr int G_CONFIRMAR = 3;
constexpr int PENA_DUDOSO = 10;  // un patrón con "~" pierde contra otro igual de bueno
constexpr int UN_ERROR = 70;     // la calidad de una palabra con un error de tipeo (o una raíz con uno)
// Marcas de una intención.
constexpr uint8_t M_SIGUE = 1, M_IA = 2, M_MANANA = 4, M_MAYUSCULA = 8;

struct Palabra {
  uint16_t ini = 0;     // en Frase::buf
  uint8_t n = 0;
  uint8_t oIni = 0, oFin = 0;  // dónde estaba en el texto original
  uint8_t marcas = 0;
};

struct Frase {
  char buf[LARGO_BUFFER];
  uint16_t usado = 0;
  Palabra p[MAX_PALABRAS];
  int n = 0;
  const char* original = nullptr;
  const char* w(int i) const { return buf + p[i].ini; }
  bool es(int i, const char* s) const {
    if (i < 0 || i >= n) return false;
    const size_t l = largo(s);
    return l == p[i].n && iguales(w(i), s, l);
  }
  // ¿Empieza con esa raíz?
  bool empieza(int i, const char* s) const {
    if (i < 0 || i >= n) return false;
    const size_t l = largo(s);
    return p[i].n >= l && iguales(w(i), s, l);
  }
};

bool esLetra(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool esDigito(char c) { return c >= '0' && c <= '9'; }

// Una letra de un texto UTF-8 plegada a ASCII en minúscula (0 = separador).
// Avanza *i lo que ocupa.
char plegar(const char* s, size_t n, size_t* i) {
  const uint8_t c = static_cast<uint8_t>(s[*i]);
  if (c < 0x80) {
    (*i)++;
    if (c >= 'A' && c <= 'Z') return static_cast<char>(c + 32);
    if (esLetra(static_cast<char>(c)) || esDigito(static_cast<char>(c))) return static_cast<char>(c);
    if (c == '+') return '+';
    if (c == ':' || c == '.') return ':';
    return 0;
  }
  // Cuántos bytes ocupa.
  size_t k = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
  if (*i + k > n) k = n - *i;
  char r = 0;
  if (c == 0xC3 && k == 2) {
    const uint8_t d = static_cast<uint8_t>(s[*i + 1]) & 0xDF;  // mayúscula y minúscula juntas (0x80-0x9F)
    if (d >= 0x80 && d <= 0x85) r = 'a';
    else if (d == 0x87) r = 'c';
    else if (d >= 0x88 && d <= 0x8B) r = 'e';
    else if (d >= 0x8C && d <= 0x8F) r = 'i';
    else if (d == 0x91) r = 'n';
    else if (d >= 0x92 && d <= 0x96) r = 'o';
    else if (d >= 0x99 && d <= 0x9C) r = 'u';
    else if (d == 0x9D) r = 'y';
  }
  *i += k;
  return r;
}

bool buscarEn(const char* const* lista, int n, const char* w, size_t l) {
  for (int i = 0; i < n; i++)
    if (largo(lista[i]) == l && iguales(lista[i], w, l)) return true;
  return false;
}

void agregarPalabra(Frase& f, const char* w, size_t l, uint8_t oIni, uint8_t oFin) {
  if (f.n >= MAX_PALABRAS || !l || f.usado + l + 1 > LARGO_BUFFER) return;
  Palabra& p = f.p[f.n++];
  p.ini = f.usado;
  p.n = static_cast<uint8_t>(l > 255 ? 255 : l);
  p.oIni = oIni, p.oFin = oFin;
  copiarBytes(f.buf + f.usado, w, p.n);
  f.usado = static_cast<uint16_t>(f.usado + p.n);
  f.buf[f.usado++] = 0;
}

// Una palabra cruda: se expande si es una abreviatura (que puede ser de
// varias palabras: "finde" → "fin de semana").
void sumarCruda(Frase& f, const char* w, size_t l, uint8_t oIni, uint8_t oFin) {
  for (const auto& a : ABREVIATURAS_CHARLA) {
    if (largo(a.de) != l || !iguales(a.de, w, l)) continue;
    const char* s = a.a;
    while (*s) {
      size_t k = 0;
      while (s[k] && s[k] != ' ') k++;
      agregarPalabra(f, s, k, oIni, oFin);
      s += k;
      while (*s == ' ') s++;
    }
    return;
  }
  agregarPalabra(f, w, l, oIni, oFin);
}

void partir(const char* texto, size_t n, Frase& f) {
  if (n > MAX_ENTRADA_CHARLA) n = MAX_ENTRADA_CHARLA;
  f.original = texto;
  char w[MAX_LETRAS + 8];
  size_t lw = 0;
  size_t oIni = 0, oFin = 0;
  auto cerrar = [&]() {
    if (lw) sumarCruda(f, w, lw, static_cast<uint8_t>(oIni), static_cast<uint8_t>(oFin));
    lw = 0;
  };
  for (size_t i = 0; i < n;) {
    const size_t antes = i;
    char c = plegar(texto, n, &i);
    if (c == '+') {
      cerrar();
      sumarCruda(f, "mas", 3, static_cast<uint8_t>(antes), static_cast<uint8_t>(i));
      continue;
    }
    // ":" o "." solo entre dígitos ("7:30"); si no, separan.
    if (c == ':') {
      const bool entre = lw && esDigito(w[lw - 1]) && i < n && esDigito(texto[i]);
      if (!entre) {
        cerrar();
        continue;
      }
    }
    if (!c) {
      cerrar();
      continue;
    }
    // Entre letras y números se parte ("7hs", "10min").
    if (lw && c != ':' && w[lw - 1] != ':' && esDigito(c) != esDigito(w[lw - 1])) cerrar();
    // Tres o más letras iguales seguidas son una ("holaaa").
    if (lw >= 2 && esLetra(c) && w[lw - 1] == c && w[lw - 2] == c) {
      oFin = i;
      continue;
    }
    if (lw >= 2 && esLetra(c) && w[lw - 1] == c && lw >= 1 && i < n) {
      // Dos iguales: se deja (llamar, perro); la tercera ya la sacó lo de arriba.
    }
    if (!lw) oIni = antes;
    if (lw < MAX_LETRAS) w[lw++] = c;
    oFin = i;
  }
  cerrar();
  // "Holaa" → "hola": una vocal doble al final se achica (las palabras del
  // castellano casi nunca terminan así; "bebee" sí, pero no importa).
  for (int k = 0; k < f.n; k++) {
    Palabra& p = f.p[k];
    char* s = f.buf + p.ini;
    if (p.n >= 3 && s[p.n - 1] == s[p.n - 2] && (s[p.n - 1] == 'a' || s[p.n - 1] == 'o' || s[p.n - 1] == 'i' || s[p.n - 1] == 'u'))
      s[--p.n] = 0;
    if (buscarEn(RELLENO_CHARLA, CANTIDAD_RELLENO_CHARLA, s, p.n)) p.marcas |= RELLENO;
  }
}

// ---------------------------------------------------------------- parecido

int distancia(const char* a, size_t la, const char* b, size_t lb, int tope) {
  if (la > MAX_LETRAS) la = MAX_LETRAS;
  if (lb > MAX_LETRAS) lb = MAX_LETRAS;
  const int dl = static_cast<int>(la) - static_cast<int>(lb);
  if (dl > tope || -dl > tope) return tope + 1;
  // Damerau-Levenshtein (con transposiciones de dos letras), tres filas.
  int f0[MAX_LETRAS + 1], f1[MAX_LETRAS + 1], f2[MAX_LETRAS + 1];
  int *pp = f0, *p = f1, *c = f2;
  for (size_t j = 0; j <= lb; j++) p[j] = static_cast<int>(j);
  for (size_t i = 1; i <= la; i++) {
    c[0] = static_cast<int>(i);
    int minFila = c[0];
    for (size_t j = 1; j <= lb; j++) {
      const int costo = a[i - 1] == b[j - 1] ? 0 : 1;
      int v = mini(mini(p[j] + 1, c[j - 1] + 1), p[j - 1] + costo);
      if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) v = mini(v, pp[j - 2] + 1);
      c[j] = v;
      if (v < minFila) minFila = v;
    }
    if (minFila > tope) return tope + 1;
    int* t = pp;
    pp = p, p = c, c = t;
  }
  return p[lb];
}

}  // namespace

int parecidoCharla(const char* a, size_t la, bool raiz, const char* w, size_t lw) {
  if (!la || !lw) return 0;
  // La primera letra casi nunca se equivoca (o se dan vuelta las dos primeras).
  const bool primera = a[0] == w[0] || (la > 1 && lw > 1 && a[0] == w[1] && a[1] == w[0]);
  if (raiz) {
    if (lw >= la && iguales(w, a, la)) return 92;
    if (primera && la >= 5 && lw >= la && distancia(a, la, w, la, 1) <= 1) return 70;
    return 0;
  }
  if (la == lw && iguales(a, w, la)) return 100;
  if (!primera || la < 5 || lw < 4) return 0;  // las cortas, exactas ("hola" no es "hora")
  const int tope = la >= 8 ? 2 : 1;
  const int d = distancia(a, la, w, lw, tope);
  if (d > tope) return 0;
  return d == 1 ? 78 : 66;
}

namespace {

// ---------------------------------------------------------------- patrones

struct Coincidencia {
  int puntaje = -1000;
  int calidad = 0;   // 0-100, sin mirar cuánto de la frase cubre
  int hechas = 0;    // tokens que encontró
  int primera = -1, ultima = -1;
  uint32_t cubiertas = 0;  // máscara de palabras
  bool dudoso = false;     // el patrón empieza con "~": si cambia algo, pregunta antes
  int minimo = 0;          // la peor palabra que encontró (100 exacta, 78 un error, 66 dos)
  int cobertura = 0;       // cuánto de la frase explica (0-100)
};

// Lo mejor que da esta alternativa (de un token del patrón) con la palabra i.
int calidadToken(const char* tok, size_t lt, const Frase& f, int i) {
  int mejor = 0;
  const char* w = f.w(i);
  const size_t lw = f.p[i].n;
  const bool relleno = (f.p[i].marcas & RELLENO) != 0;
  size_t k = 0;
  while (k < lt) {
    size_t e = k;
    while (e < lt && tok[e] != '|') e++;
    size_t la = e - k;
    bool raiz = la && tok[k + la - 1] == '*';
    if (raiz) la--;
    int q = parecidoCharla(tok + k, la, raiz, w, lw);
    // Con el relleno, solo exacto: "ahora" no es "hora".
    if (relleno && q < 100 && !(raiz && q == 92)) q = 0;
    if (q > mejor) mejor = q;
    k = e + 1;
  }
  return mejor;
}

size_t largoAlternativa(const char* tok, size_t lt) {
  size_t mejor = 0, k = 0;
  while (k < lt) {
    size_t e = k;
    while (e < lt && tok[e] != '|') e++;
    if (e - k > mejor) mejor = e - k;
    k = e + 1;
  }
  return mejor;
}

// Prueba un patrón ("pone*|programa* alarma") sobre la frase. `extra`: las
// palabras que esta intención explica aunque no estén en el patrón (la hora,
// el juego). `textoLibre`: lo que viene después del disparador también cuenta
// (el texto de un recordatorio, un gusto).
bool probarPatron(const char* p, const Frase& f, uint32_t extra, bool textoLibre, Coincidencia* c) {
  const bool dudoso = *p == '~';
  if (dudoso) p++;
  int cursor = 0, sumaQ = 0, sumaPeso = 0, req = 0, hechas = 0, minimo = 100;
  int primera = -1, ultima = -1;
  uint32_t mascara = 0;
  const char* t = p;
  while (*t) {
    size_t lt = 0;
    while (t[lt] && t[lt] != ' ') lt++;
    const bool opcional = lt && t[lt - 1] == '?';
    const size_t lTok = opcional ? lt - 1 : lt;
    // La mejor palabra en las próximas cuatro (gana la más cercana si empatan).
    int mejorQ = 0, mejorI = -1;
    for (int i = cursor; i < f.n && (hechas == 0 || i < cursor + 4); i++) {
      const int q = calidadToken(t, lTok, f, i);
      if (q > mejorQ) mejorQ = q, mejorI = i;
      if (q == 100) break;
    }
    if (mejorI >= 0) {
      const int peso = 10 + static_cast<int>(largoAlternativa(t, lTok));
      sumaQ += mejorQ * peso;
      sumaPeso += peso;
      if (mejorQ < minimo) minimo = mejorQ;
      mascara |= 1u << mejorI;
      if (primera < 0) primera = mejorI;
      ultima = mejorI;
      cursor = mejorI + 1;
      hechas++;
      if (!opcional) req++;
    } else if (!opcional) {
      return false;
    }
    t += lt;
    while (*t == ' ') t++;
  }
  if (!hechas) return false;
  const int calidad = sumaQ / sumaPeso;
  // Cuánto de lo que dijiste explica (sin contar el relleno).
  uint32_t contenido = 0;
  for (int i = 0; i < f.n; i++)
    if (!(f.p[i].marcas & (RELLENO | HORA))) contenido |= 1u << i;
  uint32_t explicadas = mascara | extra;
  if (textoLibre)
    for (int i = ultima + 1; i < f.n; i++) explicadas |= 1u << i;
  const int total = __builtin_popcount(contenido);
  const int cubiertas = __builtin_popcount(contenido & explicadas);
  const int cobertura = total ? cubiertas * 100 / total : 100;
  const int huecos = (ultima - primera + 1) - hechas;
  const int puntaje = calidad * (55 + 45 * cobertura / 100) / 100 + 4 * mini(req, 5) - 3 * huecos - (dudoso ? PENA_DUDOSO : 0);
  if (puntaje > c->puntaje) {
    c->dudoso = dudoso;
    c->minimo = minimo;
    c->cobertura = cobertura;
    c->puntaje = puntaje;
    c->calidad = calidad;
    c->hechas = hechas;
    c->primera = primera, c->ultima = ultima;
    c->cubiertas = mascara;
  }
  return true;
}

// El mejor de un grupo (juegos, modos, toques): índice o -1, y sus palabras.
// Acá no importa cuánto de la frase cubre: que esté, y bien escrito.
int buscarGrupo(const GrupoCharla* g, int n, const Frase& f, uint32_t* mascara) {
  int mejor = -1, mejorP = 0;
  uint32_t m = 0;
  for (int k = 0; k < n; k++) {
    for (int j = 0; j < g[k].nPatrones; j++) {
      Coincidencia c;
      if (!probarPatron(PATRONES_CHARLA[g[k].patron + j], f, 0, false, &c) || c.calidad < 75) continue;
      const int p = c.calidad * c.hechas;
      if (p > mejorP) mejorP = p, mejor = k, m = c.cubiertas;
    }
  }
  if (mascara) *mascara = m;
  return mejor;
}

// ---------------------------------------------------------------- números y horas

struct NumeroEscrito {
  const char* w;
  int v;
};
const NumeroEscrito NUMEROS[] = {
    {"cero", 0},       {"un", 1},          {"uno", 1},          {"una", 1},          {"dos", 2},
    {"tres", 3},       {"cuatro", 4},      {"cinco", 5},        {"seis", 6},         {"siete", 7},
    {"ocho", 8},       {"nueve", 9},       {"diez", 10},        {"once", 11},        {"doce", 12},
    {"trece", 13},     {"catorce", 14},    {"quince", 15},      {"dieciseis", 16},   {"diecisiete", 17},
    {"dieciocho", 18}, {"diecinueve", 19}, {"veinte", 20},      {"veintiuno", 21},   {"veintiun", 21},
    {"veintidos", 22}, {"veintitres", 23}, {"veinticuatro", 24}, {"veinticinco", 25}, {"veintiseis", 26},
    {"veintisiete", 27}, {"veintiocho", 28}, {"veintinueve", 29}, {"treinta", 30},   {"cuarenta", 40},
    {"cincuenta", 50}, {"sesenta", 60},    {"cien", 100},       {"ciento", 100},     {"mil", 1000},
};

// Un número de una palabra (-1 si no es).
int32_t valorPalabra(const Frase& f, int i) {
  if (i < 0 || i >= f.n) return -1;
  const char* w = f.w(i);
  const size_t l = f.p[i].n;
  if (esDigito(w[0])) {
    int32_t v = 0;
    for (size_t k = 0; k < l; k++) {
      if (!esDigito(w[k])) return -1;
      if (v < 100000) v = v * 10 + (w[k] - '0');
    }
    return v;
  }
  for (const auto& x : NUMEROS)
    if (largo(x.w) == l && iguales(x.w, w, l)) return x.v;
  return -1;
}

// Un número desde la palabra i ("treinta y cinco"): cuántas palabras ocupa (0 = no hay).
int leerNumero(const Frase& f, int i, int32_t* v) {
  const int32_t a = valorPalabra(f, i);
  if (a < 0) return 0;
  *v = a;
  if ((a == 20 || a == 30 || a == 40 || a == 50) && f.es(i + 1, "y")) {
    const int32_t b = valorPalabra(f, i + 2);
    if (b >= 1 && b <= 9) {
      *v = a + b;
      return 3;
    }
  }
  return 1;
}

// "7:30" en una palabra.
bool horaConDosPuntos(const Frase& f, int i, int* h, int* m) {
  if (i < 0 || i >= f.n) return false;
  const char* w = f.w(i);
  const size_t l = f.p[i].n;
  size_t k = 0;
  int a = 0, b = 0;
  while (k < l && esDigito(w[k])) a = a * 10 + (w[k++] - '0');
  if (k == 0 || k > 2 || k >= l || w[k] != ':') return false;
  const size_t inicioB = ++k;
  while (k < l && esDigito(w[k])) b = b * 10 + (w[k++] - '0');
  if (k != l || k - inicioB != 2 || a > 24 || b > 59) return false;
  *h = a, *m = b;
  return true;
}

const char* const DIAS_SEMANA[] = {"lunes", "martes", "miercoles", "jueves", "viernes", "sabado", "domingo"};
int diaDeSemana(const Frase& f, int i) {
  for (int d = 0; d < 7; d++)
    if (f.es(i, DIAS_SEMANA[d])) return d;
  // "sabados", "domingos".
  if (f.es(i, "sabados")) return 5;
  if (f.es(i, "domingos")) return 6;
  return -1;
}

enum Periodo : int8_t { SinPeriodo = -1, Manana, Mediodia, Tarde, Noche, Madrugada };

struct HoraDicha {
  int8_t h = -1, m = 0;
  int8_t periodo = SinPeriodo;
  bool am = false, pm = false;
  bool resuelta = false;  // ya es la hora del día (vino de antes)
  int16_t en = -1;      // minutos (relativo)
  int8_t dia = -1;      // 0 hoy, 1 mañana, 2 pasado
  int8_t semana = -1;
  uint8_t dias = 0;
};

int periodoDe(const Frase& f, int i) {
  if (f.es(i, "manana") || f.es(i, "mananas")) return Manana;
  if (f.es(i, "mediodia")) return Mediodia;
  if (f.es(i, "tarde") || f.es(i, "tardes")) return Tarde;
  if (f.es(i, "noche") || f.es(i, "noches")) return Noche;
  if (f.es(i, "madrugada")) return Madrugada;
  return SinPeriodo;
}

void marcar(Frase& f, int desde, int hasta) {
  for (int k = desde; k < hasta && k < f.n; k++)
    if (k >= 0) f.p[k].marcas |= HORA;
}

// Lo que dice después de la hora: "y media", "menos cuarto", "de la tarde", "hs".
int colaDeHora(Frase& f, int i, HoraDicha& h) {
  int j = i;
  int32_t v;
  // "las siete treinta", "a las 7 45" (como lo pasa la voz): los minutos solos.
  if (h.m == 0) {
    const int k = leerNumero(f, j, &v);
    if (k && v >= 10 && v <= 59 && !f.empieza(j + k, "minuto") && !f.empieza(j + k, "hora") && !f.empieza(j + k, "segundo") &&
        !f.es(j + k, "min"))
      h.m = static_cast<int8_t>(v), j += k;
  }
  if (f.es(j, "y")) {
    if (f.es(j + 1, "media")) h.m = 30, j += 2;
    else if (f.es(j + 1, "cuarto")) h.m = 15, j += 2;
    else if (int k = leerNumero(f, j + 1, &v); k && v >= 1 && v <= 59) {
      h.m = static_cast<int8_t>(v), j += 1 + k;
      if (f.empieza(j, "minuto")) j++;
    }
  } else if (f.es(j, "menos")) {
    int resta = 0;
    if (f.es(j + 1, "cuarto")) resta = 15, j += 2;
    else if (int k = leerNumero(f, j + 1, &v); k && v >= 1 && v <= 30) resta = static_cast<int>(v), j += 1 + k;
    if (resta) {
      int total = h.h * 60 + h.m - resta;
      if (total < 0) total += 1440;
      h.h = static_cast<int8_t>(total / 60), h.m = static_cast<int8_t>(total % 60);
    }
  }
  if (f.es(j, "en") && f.es(j + 1, "punto")) j += 2;
  if (f.es(j, "hs") || f.es(j, "horas") || f.es(j, "hora")) j++;
  if (f.es(j, "am")) h.am = true, j++;
  else if (f.es(j, "pm")) h.pm = true, j++;
  // "de la tarde", "a la noche", "de la mañana", "de noche".
  int k = j;
  if (f.es(k, "de") || f.es(k, "a") || f.es(k, "por")) {
    int q = k + 1;
    if (f.es(q, "la")) q++;
    const int p = periodoDe(f, q);
    if (p != SinPeriodo) h.periodo = static_cast<int8_t>(p), j = q + 1;
  }
  return j;
}

void leerHoras(Frase& f, HoraDicha& h) {
  for (int i = 0; i < f.n; i++) {
    if (f.p[i].marcas & HORA) continue;
    int32_t v;
    // Días.
    if (f.es(i, "pasado") && f.es(i + 1, "manana")) {
      h.dia = 2, marcar(f, i, i + 2), i++;
      continue;
    }
    if (f.es(i, "hoy")) {
      if (h.dia < 0) h.dia = 0;
      marcar(f, i, i + 1);
      continue;
    }
    if (f.es(i, "manana") && !(f.es(i - 1, "la") && (f.es(i - 2, "de") || f.es(i - 2, "a") || f.es(i - 2, "por")))) {
      // "todas las mañanas" es repetir, no mañana.
      h.dia = 1, marcar(f, i, i + 1);
      continue;
    }
    if ((f.es(i, "todos") && f.es(i + 1, "los") && f.es(i + 2, "dias")) ||
        (f.es(i, "todas") && f.es(i + 1, "las") && (f.es(i + 2, "mananas") || f.es(i + 2, "noches") || f.es(i + 2, "tardes")))) {
      h.dias = 0x7F, marcar(f, i, i + 3), i += 2;
      continue;
    }
    if (f.es(i, "cada") && f.es(i + 1, "dia")) {
      h.dias = 0x7F, marcar(f, i, i + 2), i++;
      continue;
    }
    if (f.es(i, "de") && f.es(i + 1, "lunes") && f.es(i + 2, "a") && f.es(i + 3, "viernes")) {
      h.dias = 0x1F, marcar(f, i, i + 4), i += 3;
      continue;
    }
    if ((f.es(i, "dias") && f.es(i + 1, "de") && f.es(i + 2, "semana")) || (f.es(i, "entre") && f.es(i + 1, "semana"))) {
      h.dias = 0x1F, marcar(f, i, i + 3), i += 2;
      continue;
    }
    if ((f.es(i, "fin") || f.es(i, "fines")) && f.es(i + 1, "de") && f.es(i + 2, "semana")) {
      h.dias = 0x60, marcar(f, i, i + 3), i += 2;
      continue;
    }
    const int d = diaDeSemana(f, i);
    if (d >= 0) {
      // "los lunes", "cada lunes": repetir; "el lunes": ese día.
      if (f.es(i - 1, "los") || f.es(i - 1, "cada")) h.dias = static_cast<uint8_t>(h.dias | (1u << d));
      else h.semana = static_cast<int8_t>(d);
      int desde = i;
      if (f.es(desde - 1, "proximo")) desde--;
      if (f.es(desde - 1, "el") || f.es(desde - 1, "este") || f.es(desde - 1, "los") || f.es(desde - 1, "cada")) desde--;
      marcar(f, desde, i + 1);
      continue;
    }
    // Relativo: "en 10 minutos", "en media hora", "dentro de una hora", "en un rato".
    int j = i;
    if (f.es(j, "en") || f.es(j, "dentro")) {
      j++;
      if (f.es(j, "de")) j++;
      if (f.es(j, "un") && (f.es(j + 1, "rato") || f.es(j + 1, "ratito"))) {
        h.en = f.es(j + 1, "rato") ? 10 : 5;
        marcar(f, i, j + 2);
        i = j + 1;
        continue;
      }
      if (f.es(j, "media") && f.es(j + 1, "hora")) {
        h.en = 30, marcar(f, i, j + 2), i = j + 1;
        continue;
      }
    }
    // "10 minutos", "una hora y media", "2 horas" (con o sin "en").
    if (int k = leerNumero(f, j, &v); k && v > 0 && v <= 1440) {
      const int u = j + k;
      int factor = 0;
      if (f.empieza(u, "minuto") || f.es(u, "min") || f.es(u, "mins")) factor = 1;
      else if (f.empieza(u, "hora")) factor = 60;
      else if (f.empieza(u, "segundo")) factor = -1;
      if (factor) {
        int total = factor > 0 ? static_cast<int>(v) * factor : (static_cast<int>(v) + 59) / 60;
        int fin = u + 1;
        if (factor == 60 && f.es(fin, "y") && f.es(fin + 1, "media")) total += 30, fin += 2;
        else if (factor == 60 && f.es(fin, "y") && f.es(fin + 1, "cuarto")) total += 15, fin += 2;
        h.en = static_cast<int16_t>(total > 1440 ? 1440 : total);
        marcar(f, i, fin);
        i = fin - 1;
        continue;
      }
    }
    // Absoluta: "a las 7", "a la una", "para las 8 y media", "al mediodía", "7:30", "7 y media".
    int a = i;
    if (f.es(a, "a") || f.es(a, "para") || f.es(a, "tipo") || f.es(a, "como")) a++;
    if (f.es(a, "mediodia") || (f.es(a, "al") && f.es(a + 1, "mediodia"))) {
      const int q = f.es(a, "al") ? a + 1 : a;
      h.h = 12, h.m = 0, h.periodo = Mediodia;
      marcar(f, i, q + 1);
      i = q;
      continue;
    }
    if (f.es(a, "medianoche") || (f.es(a, "la") && f.es(a + 1, "medianoche"))) {
      const int q = f.es(a, "la") ? a + 1 : a;
      h.h = 0, h.m = 0;
      marcar(f, i, q + 1);
      i = q;
      continue;
    }
    if (f.es(a, "las") || f.es(a, "la")) {
      int hh = -1, mm = 0;
      int k = 0;
      if (horaConDosPuntos(f, a + 1, &hh, &mm)) k = 1;
      else if ((k = leerNumero(f, a + 1, &v)) && v <= 24) hh = static_cast<int>(v);
      else k = 0;
      if (k && hh >= 0 && (f.es(a, "las") || hh == 1)) {
        h.h = static_cast<int8_t>(hh), h.m = static_cast<int8_t>(mm);
        const int fin = colaDeHora(f, a + 1 + k, h);
        marcar(f, i, fin);
        i = fin - 1;
        continue;
      }
    }
    // Sin "a las": "7:30" o "7 y media", "8 menos cuarto", "7 hs", "7 de la tarde".
    {
      int hh = -1, mm = 0;
      if (horaConDosPuntos(f, i, &hh, &mm)) {
        h.h = static_cast<int8_t>(hh), h.m = static_cast<int8_t>(mm);
        const int fin = colaDeHora(f, i + 1, h);
        marcar(f, i, fin);
        i = fin - 1;
        continue;
      }
      if (int k = leerNumero(f, i, &v); k && v <= 24 && !(f.p[i].marcas & HORA)) {
        const int u = i + k;
        const bool cola = (f.es(u, "y") && (f.es(u + 1, "media") || f.es(u + 1, "cuarto"))) ||
                          (f.es(u, "menos") && f.es(u + 1, "cuarto")) || f.es(u, "hs") || f.es(u, "am") ||
                          f.es(u, "pm") || ((f.es(u, "de") || f.es(u, "a")) && f.es(u + 1, "la") && periodoDe(f, u + 2) >= 0);
        if (cola) {
          h.h = static_cast<int8_t>(v), h.m = 0;
          const int fin = colaDeHora(f, u, h);
          marcar(f, i, fin);
          i = fin - 1;
          continue;
        }
      }
    }
    // Solo el momento del día: "a la noche", "esta tarde", "de noche".
    if ((f.es(i, "a") || f.es(i, "de") || f.es(i, "por") || f.es(i, "esta")) && h.periodo == SinPeriodo) {
      int q = i + 1;
      if (f.es(q, "la")) q++;
      const int p = periodoDe(f, q);
      if (p != SinPeriodo && !(p == Manana && q == i + 1)) {
        h.periodo = static_cast<int8_t>(p);
        marcar(f, i, q + 1);
        i = q;
        continue;
      }
    }
  }
}

// ---------------------------------------------------------------- fechas

struct Fecha {
  int anio, mes, dia, semana;  // semana: 0 lunes
};
Fecha fechaDe(uint32_t epoch) {
  // Howard Hinnant, días → fecha civil.
  const int32_t z = static_cast<int32_t>(epoch / 86400u) + 719468;
  const int32_t era = z / 146097;
  const int32_t doe = z - era * 146097;
  const int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int32_t mp = (5 * doy + 2) / 153;
  const int d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  const int m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  const int y = static_cast<int>(yoe + era * 400 + (m <= 2 ? 1 : 0));
  return {y, m, d, static_cast<int>((epoch / 86400u + 3) % 7)};
}

const char* const NOMBRES_DIA[] = {"lunes", "martes", "mi\xC3\xA9rcoles", "jueves", "viernes", ("s\xC3\xA1" "bado"), "domingo"};
const char* const NOMBRES_MES[] = {"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio",
                                   "agosto", "septiembre", "octubre", "noviembre", "diciembre"};

// ---------------------------------------------------------------- escribir

struct Escritor {
  char* s;
  size_t cap, n = 0;
  Escritor(char* d, size_t c) : s(d), cap(c) {
    if (cap) s[0] = 0;
  }
  void texto(const char* t, size_t k) {
    for (size_t i = 0; i < k && n + 1 < cap; i++) s[n++] = t[i];
    if (cap) s[n] = 0;
  }
  void texto(const char* t) { texto(t, largo(t)); }
  void numero(int32_t v) {
    char b[12];
    int k = 0;
    bool neg = v < 0;
    uint32_t u = neg ? static_cast<uint32_t>(-v) : static_cast<uint32_t>(v);
    do b[k++] = static_cast<char>('0' + u % 10), u /= 10;
    while (u && k < 11);
    if (neg) texto("-");
    while (k) texto(&b[--k], 1);
  }
  void dos(int v) {
    char b[2] = {static_cast<char>('0' + v / 10 % 10), static_cast<char>('0' + v % 10)};
    texto(b, 2);
  }
  void hora(int minuto) {
    numero(minuto / 60);
    texto(":");
    dos(minuto % 60);
  }
};

// Corta en `max` bytes sin partir una letra; devuelve el largo.
size_t cortarLetra(const char* s, size_t n, size_t max) {
  if (n <= max) return n;
  size_t k = max;
  while (k > 0 && (static_cast<uint8_t>(s[k]) & 0xC0) == 0x80) k--;
  return k;
}

// ---------------------------------------------------------------- respuestas

bool presetIgual(const char* a, const char* b) { return a && b && a[0] && iguales(a, b); }

struct Valores {
  Plantilla lista[24];
  int n = 0;
  void poner(const char* clave, const char* valor) {
    if (n < 24) lista[n++] = {clave, valor};
  }
  const char* de(const char* clave, size_t l) const {
    for (int i = 0; i < n; i++)
      if (largo(lista[i].clave) == l && iguales(lista[i].clave, clave, l)) return lista[i].valor;
    return nullptr;
  }
};

// ¿Sirve esta respuesta? No, si usa una plantilla que vino vacía (salvo
// {dueno} en el medio, que se borra con su coma). `datos`: cuántas de las que
// usa vinieron llenas.
bool sirve(const char* t, const Valores& v, const char* dueno, const char* robot, const char* clima, int* datos) {
  int llenas = 0;
  for (const char* c = t; *c; c++) {
    if (*c != '{') continue;
    size_t l = 0;
    while (c[1 + l] && c[1 + l] != '}') l++;
    const char* val;
    if (l == 5 && iguales(c + 1, "dueno", 5)) {
      if ((!dueno || !dueno[0]) && c == t) return false;
      continue;
    }
    if (l == 5 && iguales(c + 1, "robot", 5)) val = robot;
    else if (l == 5 && iguales(c + 1, "clima", 5)) val = clima;
    else val = v.de(c + 1, l);
    if (!val || !val[0]) return false;
    llenas++;
  }
  if (datos) *datos = llenas;
  return true;
}

// Elige del grupo [primera, primera + cantidad): las de su preset pesan doble;
// las de "@hora", "@texto"... (en "falta") son para cuando falta eso. -1 si
// ninguna sirve.
int elegir(int primera, int cantidad, const char* preset, const char* falta, const Valores& v, const ContextoCharla& ctx,
           const char* clima, uint16_t evitar, Azar& azar, int* datos) {
  auto peso = [&](int i) {
    const RespuestaCharlaBase& r = RESPUESTAS_CHARLA[i];
    if (r.preset[0] == '@') {
      if (!falta || !iguales(r.preset + 1, falta)) return 0;
    } else if (r.preset[0] && !presetIgual(r.preset, preset)) {
      return 0;
    }
    if (!sirve(r.texto, v, ctx.dueno, ctx.robot, clima, nullptr)) return 0;
    return r.preset[0] && r.preset[0] != '@' ? 2 : 1;
  };
  int validas = 0;
  for (int i = primera; i < primera + cantidad; i++)
    if (peso(i)) validas++;
  if (!validas) return -1;
  int total = 0;
  for (int i = primera; i < primera + cantidad; i++)
    if (!(validas > 1 && i == evitar)) total += peso(i);
  int x = static_cast<int>(azar.siguiente() % static_cast<uint32_t>(total));
  for (int i = primera; i < primera + cantidad; i++) {
    if (validas > 1 && i == evitar) continue;
    x -= peso(i);
    if (x < 0) {
      sirve(RESPUESTAS_CHARLA[i].texto, v, ctx.dueno, ctx.robot, clima, datos);
      return i;
    }
  }
  return -1;
}

uint32_t hashTexto(const char* s, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) h = (h ^ static_cast<uint8_t>(s[i])) * 16777619u;
  return h;
}

// ---------------------------------------------------------------- el texto libre

bool palabraInutilAlPrincipio(const Frase& f, int i) {
  return f.es(i, "que") || f.es(i, "de") || f.es(i, "a") || f.es(i, "y") || f.es(i, "para") || f.es(i, "por") ||
         f.es(i, "como") || f.es(i, "es") || f.es(i, "sea") || f.es(i, "me") || f.es(i, "mas") || f.es(i, "mucho") ||
         f.es(i, "muchisimo") || f.es(i, "bastante");
}

bool esRellenoBase(const Frase& f, int i) { return buscarEn(RELLENO_CHARLA, CANTIDAD_RELLENO_CHARLA, f.w(i), f.p[i].n); }

// ¿La palabra i es una de "tu|que|cuando"?
bool enLista(const char* lista, const Frase& f, int i) {
  const char* w = f.w(i);
  const size_t l = f.p[i].n;
  while (*lista) {
    size_t k = 0;
    while (lista[k] && lista[k] != '|') k++;
    if (k == l && iguales(lista, w, l)) return true;
    lista += k;
    if (*lista == '|') lista++;
  }
  return false;
}

// ¿Lo que viene después del disparador sirve de texto para esta intención?
// "me gusta tu voz" no es un gusto; "decime de Zoe", "me llamo la atención" y
// "decime Toto" no son un nombre (no empieza con "de", "la", un número ni es
// el del robot). Si no dijo nada, sirve: lo pregunta.
bool textoSirve(const IntencionCharla& it, const Frase& f, int ultima) {
  int i = ultima + 1;
  while (i < f.n && (f.p[i].marcas & HORA)) i++;
  if (i >= f.n) return true;
  if (it.noEmpieza[0] && enLista(it.noEmpieza, f, i)) return false;
  if (static_cast<AccionCharla>(it.accion) == AccionCharla::Dueno)
    return !esDigito(f.w(i)[0]) && !(f.p[i].marcas & ROBOT) && !esRellenoBase(f, i);
  return true;
}
bool palabraInutilAlFinal(const Frase& f, int i) {
  return f.es(i, "por") || f.es(i, "favor") || f.es(i, "che") || f.es(i, "dale") || f.es(i, "gracias") ||
         f.es(i, "bueno") || f.es(i, "eh") || f.es(i, "y") || f.es(i, "a") || f.es(i, "de") || f.es(i, "que");
}

// Arma el texto con las palabras de la máscara (con lo original: tildes,
// mayúsculas), cortado en `cap - 1` bytes sin partir una palabra.
size_t textoDe(const Frase& f, uint32_t mascara, char* salida, size_t cap, int maxPalabras) {
  int desde = 0, hasta = f.n - 1;
  while (desde <= hasta && (!(mascara & (1u << desde)) || palabraInutilAlPrincipio(f, desde))) desde++;
  while (hasta >= desde && (!(mascara & (1u << hasta)) || palabraInutilAlFinal(f, hasta))) hasta--;
  Escritor e(salida, cap);
  int puestas = 0;
  int finAnterior = -1;
  for (int i = desde; i <= hasta && puestas < maxPalabras; i++) {
    if (!(mascara & (1u << i))) continue;
    const Palabra& p = f.p[i];
    if (p.oIni == finAnterior || (finAnterior >= 0 && p.oIni < finAnterior)) continue;  // una abreviatura expandida
    const size_t sep = e.n ? 1 : 0;
    const size_t k = static_cast<size_t>(p.oFin - p.oIni);
    if (e.n + sep + k + 1 > cap) break;
    if (sep) e.texto(" ");
    e.texto(f.original + p.oIni, k);
    finAnterior = p.oFin;
    puestas++;
  }
  return e.n;
}

}  // namespace

// ---------------------------------------------------------------- lo público

const char* nombreAccionCharla(AccionCharla a) {
  const int i = static_cast<int>(a);
  return i >= 0 && i < CANTIDAD_ACCIONES_CHARLA ? ACCIONES_CHARLA[i] : "";
}

int buscarIntencionCharla(const char* nombre) {
  for (int i = 0; i < CANTIDAD_INTENCIONES_CHARLA; i++)
    if (iguales(INTENCIONES_CHARLA[i].nombre, nombre)) return i;
  return -1;
}

const char* nombreIntencionCharla(int i) {
  return i >= 0 && i < CANTIDAD_INTENCIONES_CHARLA ? INTENCIONES_CHARLA[i].nombre : "";
}

const char* RespuestaCharla::nombre() const { return nombreIntencionCharla(intencion); }

size_t normalizarCharla(const char* texto, size_t n, char* salida, size_t cap) {
  Frase f;
  partir(texto, n, f);
  Escritor e(salida, cap);
  for (int i = 0; i < f.n; i++) {
    if (i) e.texto(" ");
    e.texto(f.w(i), f.p[i].n);
  }
  return e.n;
}

namespace {

bool mismoGusto(const char* a, size_t la, const char* b, size_t lb) {
  char x[LARGO_GUSTO * 2], y[LARGO_GUSTO * 2];
  const size_t nx = normalizarCharla(a, la, x, sizeof x), ny = normalizarCharla(b, lb, y, sizeof y);
  return nx == ny && iguales(x, y, nx);
}

}  // namespace

size_t sacarGusto(char* lista, size_t cap, const char* gusto) {
  if (!cap) return 0;
  char nueva[LARGO_LISTA_GUSTOS + MAX_GUSTOS];
  Escritor e(nueva, sizeof nueva);
  const size_t lg = largo(gusto);
  const char* s = lista;
  while (*s) {
    size_t k = 0;
    while (s[k] && s[k] != '\n') k++;
    // Sale si es el mismo o si lo contiene ("la pizza" saca "pizza" y al revés).
    bool sale = mismoGusto(s, k, gusto, lg);
    if (!sale) {
      char x[LARGO_GUSTO * 2], y[LARGO_GUSTO * 2];
      const size_t nx = normalizarCharla(s, k, x, sizeof x), ny = normalizarCharla(gusto, lg, y, sizeof y);
      for (size_t i = 0; ny && i + ny <= nx && !sale; i++)
        if (iguales(x + i, y, ny) && (i == 0 || x[i - 1] == ' ') && (i + ny == nx || x[i + ny] == ' ')) sale = true;
      for (size_t i = 0; nx && i + nx <= ny && !sale; i++)
        if (iguales(y + i, x, nx) && (i == 0 || y[i - 1] == ' ') && (i + nx == ny || y[i + nx] == ' ')) sale = true;
    }
    if (!sale && k) {
      if (e.n) e.texto("\n");
      e.texto(s, k);
    }
    s += k;
    if (*s == '\n') s++;
  }
  return copiarTexto(lista, cap, nueva, e.n);
}

size_t sumarGusto(char* lista, size_t cap, const char* gusto) {
  if (!cap) return 0;
  size_t lg = cortarLetra(gusto, largo(gusto), LARGO_GUSTO - 1);
  if (!lg) return largo(lista);
  char g[LARGO_GUSTO];
  copiarTexto(g, sizeof g, gusto, lg);
  sacarGusto(lista, cap, g);
  // Cuántos hay: si ya son MAX_GUSTOS, se va el primero (el más viejo).
  int n = 0;
  for (const char* s = lista; *s; s++)
    if (*s == '\n') n++;
  if (lista[0]) n++;
  const char* s = lista;
  while (n >= MAX_GUSTOS && *s) {
    while (*s && *s != '\n') s++;
    if (*s == '\n') s++;
    n--;
  }
  char nueva[LARGO_LISTA_GUSTOS + MAX_GUSTOS];
  Escritor e(nueva, sizeof nueva);
  e.texto(s);
  if (e.n) e.texto("\n");
  e.texto(g, lg);
  return copiarTexto(lista, cap, nueva, e.n);
}

void charlar(const char* texto, size_t n, const ContextoCharla& ctx, EstadoCharla& estado, RespuestaCharla* r) {
  *r = RespuestaCharla();
  if (!texto) n = 0;
  Frase f;
  partir(texto ? texto : "", n, f);
  // El nombre del robot (y el del dueño) son relleno: "Toto, ¿qué hora es?".
  const char* const nombres[] = {ctx.robot, ctx.dueno};
  for (const char* nombre : nombres) {
    if (!nombre || !nombre[0]) continue;
    const uint8_t marca = nombre == ctx.robot ? (RELLENO | ROBOT) : RELLENO;
    char nn[LARGO_GUSTO * 2];
    const size_t ln = normalizarCharla(nombre, largo(nombre), nn, sizeof nn);
    for (int i = 0; i < f.n; i++) {
      const size_t l = f.p[i].n;
      for (size_t k = 0; k + l <= ln; k++)
        if (iguales(nn + k, f.w(i), l) && (k == 0 || nn[k - 1] == ' ') && (k + l == ln || nn[k + l] == ' '))
          f.p[i].marcas |= marca;
    }
  }

  // Las entidades: horas y días, números, juegos, modos, toques y hábitos.
  HoraDicha hd;
  leerHoras(f, hd);
  EntidadesCharla e;
  uint32_t mJuego = 0, mModo = 0, mToque = 0, mHabito = 0;
  e.juego = static_cast<int8_t>(buscarGrupo(JUEGOS_CHARLA, CANTIDAD_JUEGOS_CHARLA, f, &mJuego));
  e.modo = static_cast<int8_t>(buscarGrupo(MODOS_CHARLA, CANTIDAD_MODOS_CHARLA, f, &mModo));
  e.toque = static_cast<int8_t>(buscarGrupo(TOQUES_CHARLA, CANTIDAD_TOQUES_CHARLA, f, &mToque));
  {
    int mejor = 0;
    for (int h = 0; h < ctx.nHabitos && h < MAX_HABITOS; h++) {
      const char* nombre = ctx.habitos[h];
      if (!nombre || !nombre[0]) continue;
      Frase fh;
      partir(nombre, largo(nombre), fh);
      int iguales_ = 0, contenido = 0;
      uint32_t m = 0;
      for (int k = 0; k < fh.n; k++) {
        if ((fh.p[k].marcas & RELLENO) || fh.p[k].n < 3) continue;
        contenido++;
        for (int i = 0; i < f.n; i++) {
          if (f.p[i].marcas & RELLENO) continue;
          const char* a = fh.w(k);
          const char* w = f.w(i);
          const size_t la = fh.p[k].n, lw = f.p[i].n;
          // Igual, parecida o con la misma raíz ("tomar" y "tomé", "leer" y "leí").
          size_t comun = 0;
          while (comun < la && comun < lw && a[comun] == w[comun]) comun++;
          if (parecidoCharla(a, la, false, w, lw) >= 78 || (comun >= 3 && comun + 2 >= la) || (comun >= 2 && la <= 4 && lw <= 4)) {
            iguales_++;
            m |= 1u << i;
            break;
          }
        }
      }
      if (contenido && iguales_ * 2 >= contenido && iguales_ > mejor) mejor = iguales_, e.habito = static_cast<int8_t>(h), mHabito = m;
    }
  }
  // Un número suelto (para las cuentas y "las 7" a secas).
  int32_t numeroSuelto = -1;
  int iNumero = -1;
  for (int i = 0; i < f.n; i++) {
    int32_t v;
    if (!(f.p[i].marcas & HORA) && leerNumero(f, i, &v) && !f.es(i, "un") && !f.es(i, "una") && !f.es(i, "uno")) {
      numeroSuelto = v, iNumero = i;
      break;
    }
  }
  if (numeroSuelto >= 0) e.hayNumero = true, e.numero = numeroSuelto;

  // Cada intención: su mejor patrón.
  int mejor = -1;
  Coincidencia mc;
  for (int k = 0; k < CANTIDAD_INTENCIONES_CHARLA; k++) {
    const IntencionCharla& it = INTENCIONES_CHARLA[k];
    const AccionCharla a = static_cast<AccionCharla>(it.accion);
    uint32_t extra = 0;
    if (a == AccionCharla::Jugar) extra |= mJuego;
    if (a == AccionCharla::Modo) extra |= mModo;
    if (a == AccionCharla::Toque) extra |= mToque;
    if (a == AccionCharla::Habito) extra |= mHabito;
    if (iguales(it.nombre, "cuenta"))
      for (int i = 0; i < f.n; i++) {
        int32_t v;
        if (leerNumero(f, i, &v) || f.es(i, "mas") || f.es(i, "menos") || f.es(i, "por") || f.es(i, "dividido") ||
            f.es(i, "sobre") || f.es(i, "x"))
          extra |= 1u << i;
      }
    const bool libre = (it.necesita & (N_TEXTO | N_QUIEN)) != 0;
    Coincidencia c;
    for (int j = 0; j < it.nPatrones; j++) probarPatron(PATRONES_CHARLA[it.patron + j], f, extra, libre, &c);
    if (c.puntaje > mc.puntaje && (!libre || textoSirve(it, f, c.ultima))) mc = c, mejor = k;
  }

  // El contexto: si preguntó algo hace poco ("¿a qué hora?") o si esto sigue
  // lo anterior ("¿y mañana?", "otro").
  const bool vigente = estado.ultima >= 0 && estado.ultima < CANTIDAD_INTENCIONES_CHARLA &&
                       ctx.segundos - estado.cuando <= static_cast<uint32_t>(CHARLA_CONTEXTO_S);
  bool seguimos = false;
  const bool horaNueva = hd.h >= 0 || hd.en >= 0 || hd.periodo != SinPeriodo;
  if (vigente && estado.espera && (mejor < 0 || mc.puntaje < 90 || mejor == estado.ultima)) {
    const uint8_t es = estado.espera;
    const bool sirveHora = (es & N_HORA) && (horaNueva || hd.dia >= 0 || hd.semana >= 0 || (numeroSuelto >= 0 && numeroSuelto <= 24));
    const bool sirveTexto = (es & (N_TEXTO | N_QUIEN)) && f.n > 0;
    const bool sirveJuego = (es & N_JUEGO) && e.juego >= 0;
    const bool sirveHabito = (es & N_HABITO) && e.habito >= 0;
    if (sirveHora || sirveTexto || sirveJuego || sirveHabito) seguimos = true;
  }
  if (!seguimos && vigente && (INTENCIONES_CHARLA[estado.ultima].marcas & M_SIGUE) && (mejor < 0 || mc.puntaje < 85)) {
    const bool conY = f.es(0, "y") || f.es(0, "mejor") || f.es(0, "no") || f.es(0, "tambien");
    bool otro = false;
    for (int i = 0; i < f.n; i++)
      if (f.es(i, "otro") || f.es(i, "otra") || (f.es(i, "de") && f.es(i + 1, "nuevo")) || f.es(i, "mas")) otro = true;
    if ((conY && (horaNueva || hd.dia >= 0 || hd.semana >= 0)) || (otro && f.n <= 4)) seguimos = true;
  }

  // ¿Contesta un "¿Te llamás Zoe?"? Con "sí", hace lo que preguntó (con lo
  // que ya sabía); con "no", nada. Con otra cosa, sigue como siempre.
  bool confirmado = false, cancelado = false;
  const int iSi = buscarIntencionCharla("si"), iNo = buscarIntencionCharla("no");
  if (!seguimos && vigente && (estado.espera & N_CONFIRMA)) {
    if (mejor >= 0 && mejor == iSi && mc.puntaje >= CHARLA_UMBRAL) confirmado = true;
    else if ((mejor >= 0 && mejor == iNo && mc.puntaje >= CHARLA_UMBRAL) || (f.es(0, "no") && mc.puntaje < CHARLA_UMBRAL_ACCION))
      cancelado = iNo >= 0;
  }

  int it = -1;
  int confianza = 0;
  if (seguimos || confirmado) {
    it = estado.ultima;
    confianza = seguimos ? 80 : 100;
  } else if (cancelado) {
    it = iNo;
    confianza = 100;
  } else if (mejor >= 0 && mc.puntaje >= CHARLA_UMBRAL) {
    it = mejor;
    confianza = mc.puntaje;
  }

  // Lo que se sabía de antes (si seguimos la charla): lo nuevo gana.
  const EntidadesCharla antes = estado.e;
  Azar azar;
  azar.sembrar(ctx.semilla ^ hashTexto(texto ? texto : "", n) ^ (static_cast<uint32_t>(estado.respuesta) << 16));
  const char* preset = ctx.preset;

  if (it < 0) {
    // No entendió: con gracia, y que pruebe la IA si hay.
    Valores v;
    const int k = elegir(NO_ENTENDI_PRIMERA, NO_ENTENDI_CANTIDAD, preset, nullptr, v, ctx, nullptr, estado.respuesta, azar, nullptr);
    if (k >= 0) completarFrase(RESPUESTAS_CHARLA[k].texto, ctx.dueno, ctx.robot, r->frase, sizeof r->frase);
    r->noEntendi = true;
    r->confianza = static_cast<uint8_t>(maxi(0, mini(100, mc.puntaje)));
    r->e = e;
    estado.ultima = -1;
    estado.espera = 0;
    estado.respuesta = static_cast<uint16_t>(k < 0 ? 0xFFFF : k);
    estado.cuando = ctx.segundos;
    return;
  }

  const IntencionCharla& in = INTENCIONES_CHARLA[it];
  AccionCharla accion = static_cast<AccionCharla>(in.accion);
  r->intencion = static_cast<int16_t>(it);
  r->expresion = in.expresion;
  r->comportamiento = in.comportamiento;
  r->confianza = static_cast<uint8_t>(maxi(0, mini(100, confianza)));

  // El texto libre (el recordatorio, el gusto, el nombre, a quién).
  const bool quiereTexto = (in.necesita & N_TEXTO) || accion == AccionCharla::Musica || accion == AccionCharla::Recordatorio;
  const bool quiereQuien = (in.necesita & N_QUIEN) != 0;
  if (seguimos && (estado.espera & (N_TEXTO | N_QUIEN)) && !e.texto[0]) {
    // Contestó lo que faltaba: todo lo que dijo (sin la hora).
    uint32_t m = 0;
    for (int i = 0; i < f.n; i++)
      if (!(f.p[i].marcas & HORA) && !(quiereQuien && (mToque & (1u << i)))) m |= 1u << i;
    textoDe(f, m, e.texto, sizeof e.texto, quiereQuien ? 3 : 12);
  } else if ((quiereTexto || quiereQuien) && !seguimos && !confirmado) {
    uint32_t m = 0;
    int puestas = 0;
    for (int i = mc.ultima + 1; i < f.n; i++) {
      if (f.p[i].marcas & HORA) continue;
      if (quiereQuien && ((mToque & (1u << i)) || (f.p[i].marcas & RELLENO))) continue;
      // Un nombre: hasta tres palabras, hasta el primer "y", "de", "que" ("me llamo Juan y tengo 30").
      if (accion == AccionCharla::Dueno && (puestas >= 3 || (puestas && esRellenoBase(f, i)))) break;
      m |= 1u << i;
      puestas++;
    }
    // Si no dijo nada después ("comprar pan, recordámelo"), lo de antes.
    if (!m && quiereTexto)
      for (int i = 0; i < mc.primera; i++)
        if (!(f.p[i].marcas & HORA)) m |= 1u << i;
    textoDe(f, m, e.texto, sizeof e.texto, quiereQuien ? 3 : 12);
  }
  // Los gustos y el nombre son cortos.
  if (accion == AccionCharla::Gusto || accion == AccionCharla::OlvidarGusto || accion == AccionCharla::Dueno || quiereQuien) {
    const size_t k = largo(e.texto);
    size_t c = cortarLetra(e.texto, k, LARGO_GUSTO - 1);
    if (c < k)
      while (c > 0 && e.texto[c] != ' ') c--;
    e.texto[c] = 0;
  }
  if ((in.marcas & M_MAYUSCULA) && e.texto[0] >= 'a' && e.texto[0] <= 'z') e.texto[0] = static_cast<char>(e.texto[0] - 32);

  // La hora: se resuelve con el contexto (la de ahora y si es para despertarse).
  HoraDicha h = hd;
  if ((in.necesita & N_HORA) && h.h < 0 && h.en < 0 && h.periodo == SinPeriodo && numeroSuelto >= 1 && numeroSuelto <= 24 &&
      iNumero >= 0) {
    h.h = static_cast<int8_t>(numeroSuelto), h.m = 0;  // "despertame 7"
    e.hayNumero = false;
  }
  if (seguimos) {
    // Lo de antes, si ahora no lo dijo.
    if (h.h < 0 && h.en < 0 && h.periodo == SinPeriodo) {
      if (antes.enMinutos >= 0 && antes.minuto < 0) h.en = antes.enMinutos;
      else if (antes.minuto >= 0) h.h = static_cast<int8_t>(antes.minuto / 60), h.m = static_cast<int8_t>(antes.minuto % 60), h.resuelta = true;
    }
    if (h.dia < 0 && h.semana < 0) h.dia = antes.dia, h.semana = antes.semana;
    if (!h.dias) h.dias = antes.dias;
    if (!e.texto[0]) copiarTexto(e.texto, sizeof e.texto, antes.texto);
    if (e.juego < 0) e.juego = antes.juego;
    if (e.habito < 0) e.habito = antes.habito;
    if (e.toque < 0) e.toque = antes.toque;
  }
  const int ahora = ctx.minuto;
  const Fecha hoy = ctx.epoch ? fechaDe(ctx.epoch) : Fecha{0, 0, 0, -1};
  if (h.semana >= 0 && h.dia < 0 && hoy.semana >= 0) h.dia = static_cast<int8_t>((h.semana - hoy.semana + 7) % 7);
  e.dia = h.dia;
  e.semana = h.semana;
  e.dias = h.dias;
  if (h.h >= 0 || h.periodo != SinPeriodo) {
    int hh = h.h, mm = h.m;
    if (h.resuelta) {
      // Ya resuelta (vino de la frase anterior).
    } else if (hh < 0) {
      // Solo el momento del día.
      static const int8_t DEFECTO[] = {8, 12, 17, 21, 6};
      hh = DEFECTO[h.periodo], mm = 0;
    } else if (hh == 24) {
      hh = 0;
    } else if (h.periodo == Tarde || h.periodo == Noche || h.pm) {
      if (hh < 12) hh += 12;
      if (h.periodo == Noche && hh == 24) hh = 0;
      if (h.periodo == Noche && h.h == 12) hh = 0;
    } else if (h.periodo == Manana || h.periodo == Madrugada || h.am) {
      if (hh == 12) hh = 0;
    } else if (h.periodo == SinPeriodo && hh >= 1 && hh <= 12) {
      // Sin decir si es de mañana o de tarde: para despertarse, de mañana; si
      // no, la próxima que viene (y de 1 a 6, de tarde: nadie quiere un
      // recordatorio a las 3 de la mañana).
      if (in.marcas & M_MANANA) {
        // Tal cual (las 12 son el mediodía).
      } else if (ahora >= 0 && h.dia <= 0 && !h.dias) {
        const int a = hh % 12 * 60 + mm, b = a + 720;
        if (a > ahora && hh > 6) hh = hh % 12;
        else if (b > ahora) hh = hh % 12 + 12;
        else hh = hh <= 6 ? hh % 12 + 12 : hh % 12;
      } else if (hh <= 6) {
        hh += 12;
      }
    }
    e.minuto = static_cast<int16_t>((hh % 24) * 60 + mm);
    // Si ya pasó hoy (y no dijo el día), es mañana.
    if (e.dia < 0 && !e.dias && ahora >= 0 && e.minuto <= ahora) e.dia = 1;
    if (e.dia == 0 && ahora >= 0 && e.minuto <= ahora && (accion == AccionCharla::Alarma || accion == AccionCharla::Recordatorio))
      e.dia = 1;
  } else if (h.en >= 0) {
    e.enMinutos = h.en;
    if (ahora >= 0) {
      const int total = ahora + h.en;
      e.minuto = static_cast<int16_t>(total % 1440);
      e.dia = static_cast<int8_t>(total / 1440);
    }
  }
  if (e.dia < 0 && e.minuto >= 0 && !e.dias && ahora >= 0) e.dia = 0;

  // Con el "sí", lo que había entendido antes de preguntar.
  if (confirmado) e = antes;

  // El dato (juego, modo, toque...).
  int8_t dato = in.dato;
  if (accion == AccionCharla::Jugar && dato < 0) dato = e.juego;
  if (accion == AccionCharla::Toque) dato = e.toque >= 0 ? e.toque : (dato >= 0 ? dato : 0);
  if (accion == AccionCharla::Habito) {
    if (e.habito < 0 && ctx.nHabitos == 1) e.habito = 0;
    dato = e.habito;
  }
  if (accion == AccionCharla::Jugar) e.juego = dato;
  if (accion == AccionCharla::Recordatorio && !e.texto[0] && in.texto[0]) copiarTexto(e.texto, sizeof e.texto, in.texto);

  // ¿Puede? ¿Le falta algo?
  int grupo = 0;  // 0 respuestas, 1 falta, 2 sin
  uint8_t falta = 0;
  if ((in.requiere == 1 && !ctx.alarmaSonando) || (in.requiere == 2 && ctx.nHabitos <= 0)) grupo = 2;
  if (!grupo) {
    if ((in.necesita & N_HORA) && e.minuto < 0 && e.enMinutos < 0) falta |= N_HORA;
    if ((in.necesita & N_TEXTO) && !e.texto[0]) falta |= N_TEXTO;
    if ((in.necesita & N_QUIEN) && !e.texto[0]) falta |= N_QUIEN;
    if ((in.necesita & N_JUEGO) && e.juego < 0) falta |= N_JUEGO;
    if ((in.necesita & N_HABITO) && e.habito < 0) falta |= N_HABITO;
    if (falta) grupo = 1;
  }
  // Lo que cambia algo (una alarma, el nombre, un gusto) sin estar seguro (poco
  // puntaje o un patrón dudoso): pregunta antes. Con "sí", lo hace.
  // Seguro: mucho puntaje, o explica toda la frase con a lo sumo un error por
  // palabra ("desperrtame a las 7" sí; "mañana me levanto a las 8", no).
  const bool seguro = !mc.dudoso && (confianza >= CHARLA_UMBRAL_ACCION || (mc.cobertura == 100 && mc.minimo >= UN_ERROR));
  if (!grupo && !seguimos && !confirmado && in.grupo[G_CONFIRMAR][1] && !seguro) grupo = G_CONFIRMAR;

  // Los valores de las plantillas.
  char bHora[8], bFecha[40], bDia[16], bCuando[48], bCuandoDia[20], bALas[8], bMinutos[8], bHabitos[80], bGustos[LARGO_LISTA_GUSTOS + 16],
      bDias[8], bResultado[20];
  bHora[0] = bFecha[0] = bDia[0] = bCuando[0] = bCuandoDia[0] = bALas[0] = bMinutos[0] = bHabitos[0] = bGustos[0] = bDias[0] = bResultado[0] = 0;
  if (ahora >= 0) Escritor(bHora, sizeof bHora).hora(ahora);
  if (hoy.semana >= 0) {
    Escritor x(bFecha, sizeof bFecha);
    x.texto(NOMBRES_DIA[hoy.semana]);
    x.texto(" ");
    x.numero(hoy.dia);
    x.texto(" de ");
    x.texto(NOMBRES_MES[(hoy.mes - 1) % 12]);
    copiarTexto(bDia, sizeof bDia, NOMBRES_DIA[hoy.semana]);
  }
  if (e.minuto >= 0) Escritor(bALas, sizeof bALas).hora(e.minuto);
  {
    Escritor x(bCuando, sizeof bCuando);
    if (e.enMinutos >= 0 && hd.h < 0) {
      x.texto("en ");
      if (e.enMinutos < 60 || e.enMinutos % 60) {
        x.numero(e.enMinutos);
        x.texto(e.enMinutos == 1 ? " minuto" : " minutos");
      } else {
        x.numero(e.enMinutos / 60);
        x.texto(e.enMinutos == 60 ? " hora" : " horas");
      }
    } else if (e.minuto >= 0) {
      if (e.dias == 0x7F) x.texto("todos los d\xC3\xAD" "as ");
      else if (e.dias == 0x1F) x.texto("de lunes a viernes ");
      else if (e.dias == 0x60) x.texto("los fines de semana ");
      else if (e.dias) {
        for (int d = 0; d < 7; d++)
          if (e.dias & (1u << d)) {
            x.texto("los ");
            x.texto(NOMBRES_DIA[d]);
            x.texto(" ");
            break;
          }
      } else if (e.dia == 1) x.texto("ma\xC3\xB1" "ana ");
      else if (e.dia == 2) x.texto("pasado ma\xC3\xB1" "ana ");
      else if (e.dia > 2 && e.semana >= 0) {
        x.texto("el ");
        x.texto(NOMBRES_DIA[e.semana]);
        x.texto(" ");
      }
      x.texto(e.minuto / 60 == 1 ? "a la " : "a las ");
      x.hora(e.minuto);
    }
  }
  const int diaPedido = e.dia < 0 ? 0 : e.dia;
  copiarTexto(bCuandoDia, sizeof bCuandoDia, diaPedido == 0 ? "Hoy" : diaPedido == 1 ? "Ma\xC3\xB1" "ana" : "");
  if (e.enMinutos >= 0) Escritor(bMinutos, sizeof bMinutos).numero(e.enMinutos);
  else if (e.hayNumero) Escritor(bMinutos, sizeof bMinutos).numero(e.numero);
  {
    // "agua, leer y meditar".
    Escritor x(bHabitos, sizeof bHabitos);
    int total = 0;
    for (int i = 0; i < ctx.nHabitos && i < MAX_HABITOS; i++)
      if (ctx.habitos[i] && ctx.habitos[i][0]) total++;
    int puestos = 0;
    for (int i = 0; i < ctx.nHabitos && i < MAX_HABITOS; i++) {
      if (!ctx.habitos[i] || !ctx.habitos[i][0]) continue;
      if (puestos) x.texto(puestos == total - 1 ? " y " : ", ");
      x.texto(ctx.habitos[i]);
      puestos++;
    }
  }
  if (ctx.gustos && ctx.gustos[0]) {
    Escritor x(bGustos, sizeof bGustos);
    int total = 1;
    for (const char* s = ctx.gustos; *s; s++)
      if (*s == '\n') total++;
    int puestos = 0;
    const char* s = ctx.gustos;
    while (*s) {
      size_t k = 0;
      while (s[k] && s[k] != '\n') k++;
      if (k) {
        if (puestos) x.texto(puestos == total - 1 ? " y " : ", ");
        x.texto(s, k);
        puestos++;
      }
      s += k;
      if (*s == '\n') s++;
    }
  }
  if (ctx.diasJuntos >= 0) Escritor(bDias, sizeof bDias).numero(ctx.diasJuntos);
  if (iguales(in.nombre, "cuenta")) {
    // "cuánto es 2 más 2": el primer número, la operación y el segundo.
    int32_t a = 0, b = 0;
    int op = -1;
    bool hayA = false, hayB = false;
    for (int i = 0; i < f.n; i++) {
      int32_t v;
      const int k = leerNumero(f, i, &v);
      if (k && !f.es(i, "un") && !f.es(i, "una")) {
        if (!hayA) hayA = true, a = v;
        else if (op >= 0 && !hayB) hayB = true, b = v;
        i += k - 1;
        continue;
      }
      if (hayA && op < 0) {
        if (f.es(i, "mas")) op = 0;
        else if (f.es(i, "menos")) op = 1;
        else if (f.es(i, "por") || f.es(i, "x")) op = 2;
        else if (f.es(i, "dividido") || f.es(i, "sobre")) op = 3;
      }
    }
    if (hayA && hayB && op >= 0) {
      Escritor x(bResultado, sizeof bResultado);
      if (op == 0) x.numero(a + b);
      else if (op == 1) x.numero(a - b);
      else if (op == 2) x.numero(a * b);
      else if (b != 0) {
        // Con un decimal si hace falta ("2,5").
        const int32_t d10 = (a * 10 + (a >= 0 ? b / 2 : -b / 2)) / b;
        x.numero(d10 / 10);
        if (d10 % 10) {
          x.texto(",");
          x.numero(d10 % 10 < 0 ? -(d10 % 10) : d10 % 10);
        }
      }
    }
  }
  const char* clima = diaPedido == 1 ? ctx.climaManana : diaPedido == 0 ? ctx.clima : nullptr;
  const char* agenda = diaPedido == 1 ? ctx.agendaManana : diaPedido == 0 ? ctx.agenda : nullptr;
  Valores v;
  v.poner("hora", bHora);
  v.poner("fecha", bFecha);
  v.poner("dia", bDia);
  v.poner("cuando", bCuando);
  v.poner("cuando_dia", bCuandoDia);
  v.poner("a_las", bALas);
  v.poner("minutos", bMinutos);
  v.poner("texto", e.texto);
  v.poner("quien", e.texto);
  v.poner("toque", dato >= 0 && dato < CANTIDAD_TOQUES_CHARLA && accion == AccionCharla::Toque ? TOQUES_CHARLA[dato].decir : "");
  v.poner("juego", e.juego >= 0 && e.juego < CANTIDAD_JUEGOS_CHARLA ? JUEGOS_CHARLA[e.juego].decir : "");
  v.poner("habito", e.habito >= 0 && e.habito < ctx.nHabitos && ctx.habitos[e.habito] ? ctx.habitos[e.habito] : "");
  v.poner("habitos", bHabitos);
  v.poner("gustos", bGustos);
  v.poner("agenda", agenda ? agenda : "");
  v.poner("musica", ctx.musica ? ctx.musica : "");
  v.poner("dias", bDias);
  v.poner("resultado", bResultado);

  // La respuesta: la del grupo; si ninguna sirve (le falta un dato), la de "sin".
  int datos = 0;
  // Lo que falta, en el orden en que lo pregunta.
  const char* queFalta = (falta & N_TEXTO) ? "texto" : (falta & N_QUIEN) ? "quien" : (falta & N_JUEGO) ? "juego"
                         : (falta & N_HABITO) ? "habito" : (falta & N_HORA) ? "hora" : nullptr;
  int k = cancelado ? elegir(CANCELADO_PRIMERA, CANCELADO_CANTIDAD, preset, nullptr, v, ctx, clima, estado.respuesta, azar, &datos)
                    : elegir(in.grupo[grupo][0], in.grupo[grupo][1], preset, queFalta, v, ctx, clima, estado.respuesta, azar, &datos);
  if (k < 0 && grupo == G_CONFIRMAR) {
    // Sin con qué preguntar (le falta un dato a la pregunta): como siempre.
    grupo = 0;
    k = elegir(in.grupo[0][0], in.grupo[0][1], preset, nullptr, v, ctx, clima, estado.respuesta, azar, &datos);
  }
  if (k < 0 && grupo == 0) {
    grupo = 2;
    k = elegir(in.grupo[2][0], in.grupo[2][1], preset, nullptr, v, ctx, clima, estado.respuesta, azar, &datos);
  }
  if (k < 0) k = elegir(NO_ENTENDI_PRIMERA, NO_ENTENDI_CANTIDAD, preset, nullptr, v, ctx, clima, estado.respuesta, azar, &datos);
  if (k >= 0) completarFrase(RESPUESTAS_CHARLA[k].texto, ctx.dueno, ctx.robot, r->frase, sizeof r->frase, clima, nullptr, nullptr,
                             {v.lista, v.n});
  // La acción, solo si contestó con lo que pidió.
  if (grupo != 0) accion = AccionCharla::Ninguna;
  if (grupo == 1) r->falta = true;
  if (grupo == G_CONFIRMAR) {
    r->falta = r->confirma = true;
    r->comportamiento = "";
  }
  if (grupo == 2 && (accion == AccionCharla::Ninguna)) r->comportamiento = "";
  r->accion = accion;
  r->dato = accion == AccionCharla::Ninguna ? -1 : dato;
  r->e = e;
  // Las que contesta mejor la IA (una pregunta difícil): si no usó un dato, que pruebe ella.
  if ((in.marcas & M_IA) && (grupo != 0 || datos == 0)) r->noEntendi = true;

  estado.ultima = static_cast<int16_t>(it);
  estado.espera = grupo == G_CONFIRMAR ? N_CONFIRMA : falta;
  estado.respuesta = static_cast<uint16_t>(k < 0 ? 0xFFFF : k);
  estado.cuando = ctx.segundos;
  estado.e = e;
}

}  // namespace sintetica
