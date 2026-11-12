#include "json.h"

namespace sintetica {

namespace {
constexpr int PROFUNDIDAD_MAXIMA = 16;

bool esDigito(char c) { return c >= '0' && c <= '9'; }

int hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
}  // namespace

void Json::espacios() {
  while (pos_ < largo_) {
    char c = s_[pos_];
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') pos_++;
    else break;
  }
}

int Json::nuevo(TipoJson t, int ini) {
  if (n_ >= max_) return static_cast<int>(ErrorJson::SinLugar);
  TokenJson& k = toks_[n_];
  k.tipo = t;
  k.hijos = 0;
  k.ini = static_cast<PosJson>(ini);
  k.fin = static_cast<PosJson>(ini);
  k.siguiente = static_cast<PosJson>(n_ + 1);
  return n_++;
}

int Json::parsear(const char* texto, size_t largo, TokenJson* tokens, int maxTokens) {
  if (largo > static_cast<size_t>(MAX_TEXTO_JSON)) return static_cast<int>(ErrorJson::SinLugar);
  s_ = texto;
  largo_ = static_cast<int32_t>(largo);
  pos_ = 0;
  toks_ = tokens;
  max_ = maxTokens;
  n_ = 0;
  errorEn_ = -1;
  espacios();
  if (pos_ >= largo_) return static_cast<int>(ErrorJson::Vacio);
  int r = valor(0);
  if (r < 0) {
    errorEn_ = pos_;
    n_ = 0;
    return r;
  }
  espacios();
  if (pos_ != largo_) {
    errorEn_ = pos_;
    n_ = 0;
    return static_cast<int>(ErrorJson::Sintaxis);
  }
  return n_;
}

int Json::valor(int profundidad) {
  if (profundidad > PROFUNDIDAD_MAXIMA) return static_cast<int>(ErrorJson::MuyProfundo);
  espacios();
  if (pos_ >= largo_) return static_cast<int>(ErrorJson::Sintaxis);
  const char c = s_[pos_];
  const auto SINTAXIS = static_cast<int>(ErrorJson::Sintaxis);

  if (c == '{' || c == '[') {
    const bool esObjeto = c == '{';
    const char cierre = esObjeto ? '}' : ']';
    int yo = nuevo(esObjeto ? TipoJson::Objeto : TipoJson::Arreglo, pos_);
    if (yo < 0) return yo;
    pos_++;
    espacios();
    if (pos_ < largo_ && s_[pos_] == cierre) {
      pos_++;
    } else {
      for (;;) {
        if (esObjeto) {
          espacios();
          if (pos_ >= largo_ || s_[pos_] != '"') return SINTAXIS;
          int k = valor(profundidad + 1);  // la clave
          if (k < 0) return k;
          espacios();
          if (pos_ >= largo_ || s_[pos_] != ':') return SINTAXIS;
          pos_++;
        }
        int v = valor(profundidad + 1);
        if (v < 0) return v;
        toks_[yo].hijos++;
        espacios();
        if (pos_ >= largo_) return SINTAXIS;
        if (s_[pos_] == ',') {
          pos_++;
          continue;
        }
        if (s_[pos_] == cierre) {
          pos_++;
          break;
        }
        return SINTAXIS;
      }
    }
    toks_[yo].fin = static_cast<PosJson>(pos_);
    toks_[yo].siguiente = static_cast<PosJson>(n_);
    return yo;
  }

  if (c == '"') {
    pos_++;
    int yo = nuevo(TipoJson::Texto, pos_);
    if (yo < 0) return yo;
    while (pos_ < largo_ && s_[pos_] != '"') {
      unsigned char ch = static_cast<unsigned char>(s_[pos_]);
      if (ch < 0x20) return SINTAXIS;
      if (ch == '\\') {
        pos_++;
        if (pos_ >= largo_) return SINTAXIS;
        char e = s_[pos_];
        if (e == 'u') {
          for (int i = 1; i <= 4; i++)
            if (pos_ + i >= largo_ || hex(s_[pos_ + i]) < 0) return SINTAXIS;
          pos_ += 4;
        } else if (e != '"' && e != '\\' && e != '/' && e != 'b' && e != 'f' && e != 'n' &&
                   e != 'r' && e != 't') {
          return SINTAXIS;
        }
      }
      pos_++;
    }
    if (pos_ >= largo_) return SINTAXIS;
    toks_[yo].fin = static_cast<PosJson>(pos_);
    pos_++;
    return yo;
  }

  if (c == '-' || esDigito(c)) {
    int yo = nuevo(TipoJson::Numero, pos_);
    if (yo < 0) return yo;
    if (s_[pos_] == '-') pos_++;
    if (pos_ >= largo_ || !esDigito(s_[pos_])) return SINTAXIS;
    while (pos_ < largo_ && esDigito(s_[pos_])) pos_++;
    if (pos_ < largo_ && s_[pos_] == '.') {
      pos_++;
      if (pos_ >= largo_ || !esDigito(s_[pos_])) return SINTAXIS;
      while (pos_ < largo_ && esDigito(s_[pos_])) pos_++;
    }
    if (pos_ < largo_ && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
      pos_++;
      if (pos_ < largo_ && (s_[pos_] == '+' || s_[pos_] == '-')) pos_++;
      if (pos_ >= largo_ || !esDigito(s_[pos_])) return SINTAXIS;
      while (pos_ < largo_ && esDigito(s_[pos_])) pos_++;
    }
    toks_[yo].fin = static_cast<PosJson>(pos_);
    return yo;
  }

  struct Literal {
    const char* p;
    TipoJson t;
  };
  static const Literal literales[] = {
      {"true", TipoJson::Verdadero}, {"false", TipoJson::Falso}, {"null", TipoJson::Nulo}};
  for (const auto& lit : literales) {
    size_t n = largo(lit.p);
    if (pos_ + static_cast<int32_t>(n) <= largo_) {
      bool ok = true;
      for (size_t i = 0; i < n; i++) ok = ok && s_[pos_ + i] == lit.p[i];
      if (ok) {
        int yo = nuevo(lit.t, pos_);
        if (yo < 0) return yo;
        pos_ += static_cast<int32_t>(n);
        toks_[yo].fin = static_cast<PosJson>(pos_);
        return yo;
      }
    }
  }
  return SINTAXIS;
}

int Json::buscar(int objeto, const char* clave) const {
  if (!valido(objeto) || toks_[objeto].tipo != TipoJson::Objeto) return -1;
  int k = objeto + 1;
  for (int i = 0; i < toks_[objeto].hijos; i++) {
    if (textoIgual(k, clave)) return k + 1;
    k = toks_[k + 1].siguiente;
  }
  return -1;
}

int Json::elemento(int arreglo, int idx) const {
  if (!valido(arreglo) || toks_[arreglo].tipo != TipoJson::Arreglo) return -1;
  if (idx < 0 || idx >= toks_[arreglo].hijos) return -1;
  int k = arreglo + 1;
  for (int i = 0; i < idx; i++) k = toks_[k].siguiente;
  return k;
}

float Json::numero(int i, float porDefecto) const {
  if (!valido(i) || toks_[i].tipo != TipoJson::Numero) return porDefecto;
  const char* p = s_ + toks_[i].ini;
  const char* fin = s_ + toks_[i].fin;
  bool negativo = false;
  if (*p == '-') negativo = true, p++;
  double v = 0;
  while (p < fin && esDigito(*p)) v = v * 10 + (*p++ - '0');
  if (p < fin && *p == '.') {
    p++;
    double f = 0.1;
    while (p < fin && esDigito(*p)) v += (*p++ - '0') * f, f *= 0.1;
  }
  if (p < fin && (*p == 'e' || *p == 'E')) {
    p++;
    bool expNeg = false;
    if (*p == '+' || *p == '-') expNeg = *p++ == '-';
    int e = 0;
    while (p < fin && esDigito(*p)) e = e * 10 + (*p++ - '0');
    if (e > 38) e = 38;
    while (e--) v = expNeg ? v / 10 : v * 10;
  }
  return static_cast<float>(negativo ? -v : v);
}

bool Json::booleano(int i, bool porDefecto) const {
  if (!valido(i)) return porDefecto;
  if (toks_[i].tipo == TipoJson::Verdadero) return true;
  if (toks_[i].tipo == TipoJson::Falso) return false;
  return porDefecto;
}

int Json::texto(int i, char* destino, size_t cap) const {
  if (!valido(i) || toks_[i].tipo != TipoJson::Texto || cap == 0) return -1;
  size_t n = 0;
  auto poner = [&](char c) {
    if (n + 1 < cap) destino[n++] = c;
  };
  for (int32_t p = toks_[i].ini; p < toks_[i].fin; p++) {
    char c = s_[p];
    if (c != '\\') {
      poner(c);
      continue;
    }
    char e = s_[++p];
    switch (e) {
      case 'n': poner('\n'); break;
      case 't': poner('\t'); break;
      case 'r': poner('\r'); break;
      case 'b': poner('\b'); break;
      case 'f': poner('\f'); break;
      case 'u': {
        uint32_t cp = 0;
        for (int k = 1; k <= 4; k++) cp = cp * 16 + hex(s_[p + k]);
        p += 4;
        // Un par sustituto (😀, como escapa Python los emoji) es una
        // sola letra de 4 bytes; suelto, queda como estaba (3 bytes).
        if (cp >= 0xD800 && cp < 0xDC00 && p + 6 < toks_[i].fin && s_[p + 1] == '\\' && s_[p + 2] == 'u') {
          uint32_t bajo = 0;
          for (int k = 3; k <= 6; k++) bajo = bajo * 16 + hex(s_[p + k]);
          if (bajo >= 0xDC00 && bajo < 0xE000) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (bajo - 0xDC00);
            p += 6;
          }
        }
        if (cp >= 0x10000) {
          poner(static_cast<char>(0xF0 | (cp >> 18)));
          poner(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
          poner(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
          poner(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x80) {
          poner(static_cast<char>(cp));
        } else if (cp < 0x800) {
          poner(static_cast<char>(0xC0 | (cp >> 6)));
          poner(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
          poner(static_cast<char>(0xE0 | (cp >> 12)));
          poner(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
          poner(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        break;
      }
      default: poner(e); break;
    }
  }
  destino[n] = 0;
  return static_cast<int>(n);
}

bool Json::textoIgual(int i, const char* s) const {
  if (!valido(i) || toks_[i].tipo != TipoJson::Texto) return false;
  int32_t n = toks_[i].fin - toks_[i].ini;
  for (int32_t k = 0; k < n; k++) {
    if (s[k] == 0 || s[k] != s_[toks_[i].ini + k]) return false;
  }
  return s[n] == 0;
}

}  // namespace sintetica
