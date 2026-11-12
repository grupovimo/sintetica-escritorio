#include "qoi.h"

namespace sintetica {

namespace {
inline uint32_t be32(const uint8_t* b) {
  return static_cast<uint32_t>(b[0]) << 24 | static_cast<uint32_t>(b[1]) << 16 | static_cast<uint32_t>(b[2]) << 8 | b[3];
}
inline uint16_t a565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
}  // namespace

bool DecodificadorQoi::leerCabecera(const uint8_t* b, size_t n, uint32_t* ancho, uint32_t* alto) {
  if (n < 14 || b[0] != 'q' || b[1] != 'o' || b[2] != 'i' || b[3] != 'f') return false;
  if (b[12] != 3 && b[12] != 4) return false;
  *ancho = be32(b + 4);
  *alto = be32(b + 8);
  return *ancho > 0 && *alto > 0 && *ancho <= 4096 && *alto <= 4096;
}

bool DecodificadorQoi::empezar(Leer leer, void* contexto) {
  leer_ = leer;
  contexto_ = contexto;
  uint8_t cab[14];
  uint32_t leidos = 0;  // la lectura puede venir de a partes
  while (leer_ && leidos < sizeof cab) {
    int n = leer_(contexto_, leidos, cab + leidos, sizeof cab - leidos);
    if (n <= 0) return false;
    leidos += static_cast<uint32_t>(n);
  }
  if (!leer_ || !leerCabecera(cab, sizeof cab, &ancho_, &alto_)) return false;
  canales_ = cab[12];
  posicion_ = 14;
  enBuf_ = leidoBuf_ = 0;
  llenarBytes(indice_, 0, sizeof indice_);
  r_ = g_ = b_ = 0;
  a_ = 255;
  racha_ = 0;
  fila_ = 0;
  return true;
}

bool DecodificadorQoi::byte(uint8_t* b) {
  if (leidoBuf_ >= enBuf_) {
    int n = leer_(contexto_, posicion_, buf_, sizeof buf_);
    if (n <= 0) return false;
    posicion_ += static_cast<uint32_t>(n);
    enBuf_ = static_cast<uint16_t>(n);
    leidoBuf_ = 0;
  }
  *b = buf_[leidoBuf_++];
  return true;
}

bool DecodificadorQoi::pixel(uint8_t* r, uint8_t* g, uint8_t* bl) {
  if (racha_ > 0) {
    racha_--;
  } else {
    uint8_t c;
    if (!byte(&c)) return false;
    if (c == 0xFE) {
      if (!byte(&r_) || !byte(&g_) || !byte(&b_)) return false;
    } else if (c == 0xFF) {
      if (!byte(&r_) || !byte(&g_) || !byte(&b_) || !byte(&a_)) return false;
    } else if ((c & 0xC0) == 0x00) {
      r_ = indice_[c][0], g_ = indice_[c][1], b_ = indice_[c][2], a_ = indice_[c][3];
    } else if ((c & 0xC0) == 0x40) {
      r_ = static_cast<uint8_t>(r_ + ((c >> 4) & 3) - 2);
      g_ = static_cast<uint8_t>(g_ + ((c >> 2) & 3) - 2);
      b_ = static_cast<uint8_t>(b_ + (c & 3) - 2);
    } else if ((c & 0xC0) == 0x80) {
      uint8_t c2;
      if (!byte(&c2)) return false;
      const int vg = (c & 0x3F) - 32;
      r_ = static_cast<uint8_t>(r_ + vg - 8 + ((c2 >> 4) & 0x0F));
      g_ = static_cast<uint8_t>(g_ + vg);
      b_ = static_cast<uint8_t>(b_ + vg - 8 + (c2 & 0x0F));
    } else {
      racha_ = c & 0x3F;
    }
    const int h = (r_ * 3 + g_ * 5 + b_ * 7 + a_ * 11) % 64;
    indice_[h][0] = r_, indice_[h][1] = g_, indice_[h][2] = b_, indice_[h][3] = a_;
  }
  *r = r_, *g = g_, *bl = b_;
  return true;
}

bool DecodificadorQoi::siguienteFila(uint16_t* rgb565) {
  if (fila_ >= alto_) return false;
  for (uint32_t x = 0; x < ancho_; x++) {
    uint8_t r, g, b;
    if (!pixel(&r, &g, &b)) return false;
    if (rgb565) rgb565[x] = a565(r, g, b);
  }
  fila_++;
  return true;
}

bool DecodificadorQoi::siguienteFilaRgba(uint8_t* rgba) {
  if (fila_ >= alto_) return false;
  for (uint32_t x = 0; x < ancho_; x++) {
    uint8_t r, g, b;
    if (!pixel(&r, &g, &b)) return false;
    rgba[x * 4] = r, rgba[x * 4 + 1] = g, rgba[x * 4 + 2] = b, rgba[x * 4 + 3] = a_;
  }
  fila_++;
  return true;
}

bool DecodificadorQoi::saltarFilas(uint32_t n) {
  for (uint32_t i = 0; i < n; i++)
    if (!siguienteFila(nullptr)) return false;
  return true;
}

namespace animacion {

namespace {
inline uint16_t le16(const uint8_t* b) { return static_cast<uint16_t>(b[0] | b[1] << 8); }
inline uint32_t le32(const uint8_t* b) {
  return static_cast<uint32_t>(b[0]) | static_cast<uint32_t>(b[1]) << 8 | static_cast<uint32_t>(b[2]) << 16 |
         static_cast<uint32_t>(b[3]) << 24;
}
}  // namespace

bool leerCabecera(const uint8_t* b, size_t n, int* cuadros, uint32_t* ancho, uint32_t* alto) {
  if (n < CABECERA || !es(b, n) || b[4] != 1) return false;
  *cuadros = b[5];
  *ancho = le16(b + 6);
  *alto = le16(b + 8);
  return *cuadros >= 1 && *cuadros <= MAX_CUADROS && *ancho > 0 && *alto > 0;
}

Cuadro leerCuadro(const uint8_t* b) {
  Cuadro c;
  c.ms = le16(b);
  c.desplazamiento = le32(b + 2);
  c.largo = le32(b + 6);
  return c;
}

}  // namespace animacion

}  // namespace sintetica
