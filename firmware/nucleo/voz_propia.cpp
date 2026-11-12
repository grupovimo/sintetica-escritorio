// La voz propia (voz_propia.h): un fonema por letra, con enteros.
#include "voz_propia.h"

namespace sintetica {

namespace {

// Los dos formantes de cada sonido (resonadores de dos polos a 16 kHz, en
// Q14: b1 = 2 r cos θ, b2 = r², la ganancia que deja el pico en 1). F1/F2: a
// 750/1300, e 500/1850, i 300/2300, o 500/950, u 330/800, nasal 280/1200,
// líquida 380/1500; anchos de banda 90 y 140 Hz.
constexpr int16_t FORMANTES[7][6] = {
    {30808, 15815, 165, 27815, 15508, 428},  // a
    {31575, 15815, 111, 23830, 15508, 582},  // e
    {31971, 15815, 67, 19736, 15508, 688},   // i
    {31575, 15815, 111, 29687, 15508, 320},  // o
    {31924, 15815, 74, 30319, 15508, 271},   // u
    {32000, 15815, 63, 28405, 15508, 398},   // m, n, ñ
    {31836, 15815, 85, 26507, 15508, 487},   // l, r, y
};
constexpr int NASAL = 5, LIQUIDA = 6;

// Cuánto sube o baja cada sílaba (porcentaje), según la letra: el balbuceo.
constexpr int8_t SALTOS[8] = {0, 12, -8, 18, -5, 9, -13, 5};

constexpr uint32_t PASO_POR_HZ = 268435;  // 2^32 / 16000

struct Letra {
  char c = 0;       // minúscula ASCII; ' ' espacio, ',' pausa corta, '.' pausa larga; 0 = nada
  bool tilde = false;
  uint8_t bytes = 1;
};

Letra leerLetra(const char* s, size_t n, size_t i) {
  Letra l;
  const uint8_t c = static_cast<uint8_t>(s[i]);
  if (c < 0x80) {
    if (c >= 'A' && c <= 'Z') l.c = static_cast<char>(c + 32);
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) l.c = static_cast<char>(c);
    else if (c == ' ' || c == '\n' || c == '-' || c == '"' || c == '(' || c == ')') l.c = ' ';
    else if (c == ',' || c == ';' || c == ':') l.c = ',';
    else if (c == '.' || c == '!' || c == '?') l.c = '.';
    return l;
  }
  l.bytes = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
  if (i + l.bytes > n) l.bytes = static_cast<uint8_t>(n - i);
  if (c == 0xC3 && l.bytes == 2) {
    const uint8_t d = static_cast<uint8_t>(s[i + 1]) & 0xDF;
    switch (d) {
      case 0x81: l.c = 'a', l.tilde = true; break;
      case 0x89: l.c = 'e', l.tilde = true; break;
      case 0x8D: l.c = 'i', l.tilde = true; break;
      case 0x93: l.c = 'o', l.tilde = true; break;
      case 0x9A: l.c = 'u', l.tilde = true; break;
      case 0x9C: l.c = 'u'; break;
      case 0x91: l.c = 'n'; break;
      default:
        if (d >= 0x80 && d <= 0x85) l.c = 'a';
        else if (d >= 0x88 && d <= 0x8B) l.c = 'e';
        else if (d >= 0x8C && d <= 0x8F) l.c = 'i';
        else if (d >= 0x92 && d <= 0x96) l.c = 'o';
        else if (d >= 0x99 && d <= 0x9B) l.c = 'u';
        break;
    }
  } else if (c == 0xC2 && l.bytes == 2) {
    l.c = ' ';  // ¡ ¿ « »
  } else if (c == 0xE2 && l.bytes == 3 && static_cast<uint8_t>(s[i + 1]) == 0x80 && static_cast<uint8_t>(s[i + 2]) == 0xA6) {
    l.c = '.';  // …
  }
  return l;
}

int vocalDe(char c) {
  switch (c) {
    case 'a': return 0;
    case 'e': return 1;
    case 'i': return 2;
    case 'o': return 3;
    case 'u': case 'w': return 4;
    default: return -1;
  }
}

bool esLetra(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }

int limitarI(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

}  // namespace

ParamsVoz paramsVoz(int tipo, int volumen, const Personalidad& p, float energia, float animo) {
  static const uint16_t TONOS[] = {120, 185, 280, 150};
  ParamsVoz v;
  tipo = limitarI(tipo, 0, 3);
  v.robot = tipo == 3;
  float tono = TONOS[tipo];
  tono *= 0.92f + 0.16f * limitar(animo, 0, 1);
  tono *= 1.0f + 0.12f * (p.energia - 0.5f) - 0.12f * (p.malGenio - 0.5f);
  v.tono = static_cast<uint16_t>(limitarI(static_cast<int>(tono), 70, 520));
  float vel = 100.0f * (0.85f + 0.3f * limitar(energia, 0, 1)) * (1.0f + 0.35f * (p.energia - 0.5f));
  v.velocidad = static_cast<uint8_t>(limitarI(static_cast<int>(vel), 60, 170));
  const float var = v.robot ? 10.0f : 25.0f + 55.0f * p.picardia + 20.0f * limitar(energia, 0, 1);
  v.variacion = static_cast<uint8_t>(limitarI(static_cast<int>(var), 0, 100));
  v.volumen = static_cast<uint8_t>(limitarI(static_cast<int>(volumen * (1.0f - 0.3f * p.timidez)), 0, 100));
  return v;
}

void VozPropia::empezar(const char* texto, const ParamsVoz& p) {
  *this = VozPropia();
  if (!texto || !texto[0]) return;
  texto_ = texto;
  p_ = p;
  if (p_.velocidad < 50) p_.velocidad = 50;
  if (p_.velocidad > 200) p_.velocidad = 200;
  size_t n = largo(texto);
  largo_ = static_cast<uint8_t>(n > 255 ? 255 : n);
  for (size_t i = 0; i < largo_;) {
    const Letra l = leerLetra(texto_, largo_, i);
    if (vocalDe(l.c) >= 0 || (l.c >= '0' && l.c <= '9')) silabas_++;
    if (texto_[i] == '?') pregunta_ = true;
    if (texto_[i] == '!') exclama_ = true;
    i += l.bytes ? l.bytes : 1;
  }
  paso_ = p_.tono * PASO_POR_HZ;
  if (!siguiente()) texto_ = nullptr;
}

void VozPropia::segmento(Tipo t, uint32_t ms, int formante, int amplitud) {
  tipo_ = t;
  total_ = resta_ = maxi(1, static_cast<int>(ms * 100u / p_.velocidad)) * (MUESTRAS_POR_SEGUNDO_VOZ / 1000);
  if (formante >= 0)
    for (int k = 0; k < 6; k++) c_[k] = FORMANTES[formante][k];
  amplitud_ = static_cast<int16_t>(amplitud);
}

bool VozPropia::siguiente() {
  while (pos_ < largo_) {
    const Letra l = leerLetra(texto_, largo_, pos_);
    pos_ = static_cast<uint8_t>(pos_ + (l.bytes ? l.bytes : 1));
    const char c = l.c;
    if (!c) continue;
    if (c == ' ') return segmento(Silencio, 35, -1, 0), true;
    if (c == ',') return segmento(Silencio, 150, -1, 0), true;
    if (c == '.') return segmento(Silencio, 240, -1, 0), true;
    const int v = (c >= '0' && c <= '9') ? 0 : vocalDe(c);
    if (v >= 0) {
      // Una sílaba: el tono baja de a poco en la frase, sube al final de una
      // pregunta, y salta según la letra (el balbuceo).
      silaba_++;
      const int s = silaba_, total = silabas_ ? silabas_ : 1;
      int contorno = 108 - 16 * s / total;
      if (pregunta_ && s >= total - 1) contorno += s == total ? 30 : 15;
      if (exclama_) contorno += 10;
      if (l.tilde) contorno += 6;
      const int var = p_.robot ? p_.variacion / 5 : p_.variacion;
      const int salto = SALTOS[(static_cast<int>(c) * 7 + s * 3) & 7] * var / 100;
      const int hz = limitarI(p_.tono * contorno / 100 * (100 + salto) / 100, 60, 900);
      paso_ = static_cast<uint32_t>(hz) * PASO_POR_HZ;
      // Al final de una palabra, la vocal dura más.
      bool finDePalabra = pos_ >= largo_;
      if (!finDePalabra) finDePalabra = !esLetra(leerLetra(texto_, largo_, pos_).c);
      segmento(Vocal, (l.tilde ? 100 : 75) + (finDePalabra ? 20 : 0), v, 256);
      return true;
    }
    switch (c) {
      case 'h': continue;  // muda
      case 'm': case 'n': segmento(Sonora, 35, NASAL, 150); return true;
      case 'l': case 'r': case 'y': segmento(Sonora, 30, LIQUIDA, 170); return true;
      case 's': case 'z': case 'f': segmento(Ruido, 45, -1, 110); return true;
      case 'j': case 'x': segmento(Ruido, 45, -1, 140); return true;
      case 'c': {
        const char d = pos_ < largo_ ? leerLetra(texto_, largo_, pos_).c : 0;
        if (d == 'e' || d == 'i') segmento(Ruido, 45, -1, 110);
        else if (d == 'h') segmento(Ruido, 40, -1, 150);
        else segmento(Explosiva, 22, -1, 200);
        return true;
      }
      case 'b': case 'd': case 'g': case 'v': segmento(Explosiva, 20, -1, 150); return true;
      default: segmento(Explosiva, 22, -1, 200); return true;  // p t k q y lo que quede
    }
  }
  return false;
}

size_t VozPropia::generar(int16_t* m, size_t n) {
  size_t k = 0;
  while (k < n && texto_) {
    if (!resta_ && !siguiente()) {
      texto_ = nullptr;
      break;
    }
    const uint32_t hecho = total_ - resta_;
    // Rampas de 4 ms al entrar y 6 ms al salir (en las vocales, el último
    // tercio cae: es lo que hace sonar cada sílaba como un "bip").
    int env = 256;
    if (hecho < 64) env = static_cast<int>(hecho) * 4;
    const uint32_t salida = tipo_ == Vocal ? total_ / 3 : 96;
    if (resta_ < salida) env = mini(env, static_cast<int>(resta_ * 256 / salida));
    env = env * amplitud_ >> 8;
    int32_t x = 0;
    switch (tipo_) {
      case Silencio: break;
      case Vocal:
      case Sonora: {
        const int32_t fuente = p_.robot ? ((fase_ & 0x80000000u) ? 6000 : -6000)
                                        : ((static_cast<int32_t>(fase_ >> 16) - 32768) >> 2);
        fase_ += paso_;
        int32_t ya = (c_[0] * ya1_ - c_[1] * ya2_ + c_[2] * fuente) >> 14;
        int32_t yb = (c_[3] * yb1_ - c_[4] * yb2_ + c_[5] * fuente) >> 14;
        ya = limitarI(ya, -30000, 30000), yb = limitarI(yb, -30000, 30000);
        ya2_ = ya1_, ya1_ = ya;
        yb2_ = yb1_, yb1_ = yb;
        x = tipo_ == Vocal ? ya + (yb * 5 >> 3) + (fuente >> 3) : ya + (fuente >> 4);
        break;
      }
      case Ruido:
      case Explosiva: {
        ruido_ ^= ruido_ << 13;
        ruido_ ^= ruido_ >> 17;
        ruido_ ^= ruido_ << 5;
        const int32_t r = static_cast<int32_t>(ruido_) >> 19;  // ±4096
        if (tipo_ == Ruido) {
          // Ruido agudo (la "s"): la diferencia entre dos muestras.
          x = (r - anteriorRuido_) * 3 >> 1;
          anteriorRuido_ = r;
        } else if (hecho * 100 > total_ * 55) {
          // La explosiva: un silencio (la oclusión) y un golpe que se apaga.
          x = r * static_cast<int32_t>(resta_) / static_cast<int32_t>(total_ - total_ * 55 / 100 + 1) * 2;
        }
        break;
      }
    }
    // Sin sonido, los resonadores se apagan solos (sin clics al volver).
    if (tipo_ != Vocal && tipo_ != Sonora) {
      ya1_ = ya1_ * 15 >> 4, ya2_ = ya2_ * 15 >> 4, yb1_ = yb1_ * 15 >> 4, yb2_ = yb2_ * 15 >> 4;
    }
    int32_t y = x * 4 * env >> 8;
    y = y * p_.volumen / 100;
    m[k++] = static_cast<int16_t>(limitarI(y, -30000, 30000));
    resta_--;
  }
  return k;
}

uint32_t VozPropia::duracion(const char* texto, const ParamsVoz& p) {
  VozPropia v;
  v.empezar(texto, p);
  uint32_t total = 0;
  while (v.texto_) {
    total += v.resta_;
    v.resta_ = 0;
    if (!v.siguiente()) break;
  }
  return total;
}

}  // namespace sintetica
