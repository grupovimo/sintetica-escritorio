#include "lienzo.h"

namespace sintetica {

namespace {
constexpr int SUBFILAS = 8;  // muestras verticales por píxel al rellenar polígonos

float g_fila[MAX_ANCHO + 1];
// Máscara para combinar los segmentos de un trazo (cobertura máxima, sin
// dibujar dos veces las uniones). Alcanza para trazos de hasta 160 × 160 px;
// uno más grande se dibuja segmento por segmento.
#ifdef ARDUINO
constexpr int LADO_MASCARA = 112;  // en la placa la RAM es poca: un trazo más grande va de a segmentos
#else
constexpr int LADO_MASCARA = 160;
#endif
uint8_t g_mascara[LADO_MASCARA * LADO_MASCARA];

// Distancia con signo a un rectángulo redondeado centrado en el origen.
float distanciaPildora(float px, float py, float mw, float mh, float r) {
  float qx = absf(px) - (mw - r);
  float qy = absf(py) - (mh - r);
  float ax = maxf(qx, 0), ay = maxf(qy, 0);
  return raiz(ax * ax + ay * ay) + minf(maxf(qx, qy), 0.0f) - r;
}

float distanciaSegmento(float px, float py, Punto a, Punto b) {
  float bx = b.x - a.x, by = b.y - a.y;
  float wx = px - a.x, wy = py - a.y;
  float l2 = bx * bx + by * by;
  float t = l2 > 0 ? limitar((wx * bx + wy * by) / l2, 0, 1) : 0;
  float dx = wx - bx * t, dy = wy - by * t;
  return raiz(dx * dx + dy * dy);
}
}  // namespace

void Lienzo::iniciar(uint16_t* pixeles, int ancho, int alto, int filasFranja) {
  px_ = pixeles;
  ancho_ = mini(ancho, MAX_ANCHO);
  alto_ = mini(alto, MAX_ALTO);
  filas_ = filasFranja > 0 ? mini(filasFranja, alto_) : alto_;
  y0_ = 0;
}

void Lienzo::empezarFranja(int y0) { y0_ = maxi(0, mini(y0, alto_ - 1)); }

void Lienzo::limpiar(uint32_t rgb) {
  const uint16_t c = a565(rgb);
  const int n = ancho_ * filasFranja();
  for (int i = 0; i < n; i++) px_[i] = c;
}

void Lienzo::mezclarPixel(int x, int y, float cobertura, uint32_t rgb) {
  cobertura *= alfa_;
  if (cobertura <= 0.002f || x < 0 || x >= ancho_ || y < yIni() || y >= yFin()) return;
  uint16_t& d = px_[(y - y0_) * ancho_ + x];
  if (cobertura >= 0.998f) {
    d = a565(rgb);
    return;
  }
  uint32_t f = de565(d);
  int a = static_cast<int>(cobertura * 256 + 0.5f);
  auto canal = [&](int sh) {
    int cf = (f >> sh) & 0xFF, cn = (rgb >> sh) & 0xFF;
    return static_cast<uint32_t>((cf * (256 - a) + cn * a) >> 8) << sh;
  };
  d = a565(canal(16) | canal(8) | canal(0));
}

void Lienzo::pildora(float cx, float cy, float w, float h, float r, uint32_t rgb) {
  if (w <= 0 || h <= 0) return;
  const float mw = w / 2, mh = h / 2;
  r = limitar(r, 0, minf(mw, mh));
  int x0 = maxi(0, static_cast<int>(piso(cx - mw - 1))), x1 = mini(ancho_, static_cast<int>(techo(cx + mw + 1)));
  int y0 = maxi(yIni(), static_cast<int>(piso(cy - mh - 1))), y1 = mini(yFin(), static_cast<int>(techo(cy + mh + 1)));
  for (int y = y0; y < y1; y++) {
    for (int x = x0; x < x1; x++) {
      float d = distanciaPildora(x + 0.5f - cx, y + 0.5f - cy, mw, mh, r);
      // Formas más finas que un píxel: la cobertura no puede pasar del grosor.
      float cob = limitar(0.5f - d, 0, 1);
      if (h < 1) cob *= h;
      if (w < 1) cob *= w;
      mezclarPixel(x, y, cob, rgb);
    }
  }
}

void Lienzo::pildoraContorno(float cx, float cy, float w, float h, float r, float t, uint32_t rgb) {
  if (w <= 0 || h <= 0 || t <= 0) return;
  const float mw = w / 2, mh = h / 2;
  r = limitar(r, 0, minf(mw, mh));
  int x0 = maxi(0, static_cast<int>(piso(cx - mw - 1))), x1 = mini(ancho_, static_cast<int>(techo(cx + mw + 1)));
  int y0 = maxi(yIni(), static_cast<int>(piso(cy - mh - 1))), y1 = mini(yFin(), static_cast<int>(techo(cy + mh + 1)));
  for (int y = y0; y < y1; y++) {
    for (int x = x0; x < x1; x++) {
      float d = distanciaPildora(x + 0.5f - cx, y + 0.5f - cy, mw, mh, r);
      // Anillo centrado en d = -t/2 (el trazo va por dentro del borde).
      float anillo = absf(d + t / 2) - t / 2;
      mezclarPixel(x, y, limitar(0.5f - anillo, 0, 1), rgb);
    }
  }
}

void Lienzo::poligono(const Punto* p, int n, uint32_t rgb) {
  if (n < 3) return;
  float minX = p[0].x, maxX = p[0].x, minY = p[0].y, maxY = p[0].y;
  for (int i = 1; i < n; i++) {
    minX = minf(minX, p[i].x), maxX = maxf(maxX, p[i].x);
    minY = minf(minY, p[i].y), maxY = maxf(maxY, p[i].y);
  }
  int x0 = maxi(0, static_cast<int>(piso(minX))), x1 = mini(ancho_, static_cast<int>(techo(maxX)) + 1);
  int y0 = maxi(yIni(), static_cast<int>(piso(minY))), y1 = mini(yFin(), static_cast<int>(techo(maxY)) + 1);
  if (x0 >= x1 || y0 >= y1) return;

  constexpr int MAX_CRUCES = 64;
  float cruces[MAX_CRUCES];
  const float peso = 1.0f / SUBFILAS;

  for (int y = y0; y < y1; y++) {
    for (int x = x0; x <= x1 && x <= ancho_; x++) g_fila[x] = 0;
    for (int s = 0; s < SUBFILAS; s++) {
      const float sy = y + (s + 0.5f) / SUBFILAS;
      int nc = 0;
      for (int i = 0, j = n - 1; i < n; j = i++) {
        const Punto a = p[j], b = p[i];
        if ((a.y <= sy && b.y > sy) || (b.y <= sy && a.y > sy)) {
          if (nc < MAX_CRUCES) cruces[nc++] = a.x + (sy - a.y) / (b.y - a.y) * (b.x - a.x);
        }
      }
      // Ordenar (n chico: inserción).
      for (int i = 1; i < nc; i++) {
        float v = cruces[i];
        int k = i - 1;
        while (k >= 0 && cruces[k] > v) cruces[k + 1] = cruces[k], k--;
        cruces[k + 1] = v;
      }
      for (int i = 0; i + 1 < nc; i += 2) {
        float xa = limitar(cruces[i], static_cast<float>(x0), static_cast<float>(x1));
        float xb = limitar(cruces[i + 1], static_cast<float>(x0), static_cast<float>(x1));
        if (xb <= xa) continue;
        int ia = static_cast<int>(piso(xa)), ib = static_cast<int>(piso(xb));
        if (ia == ib) {
          g_fila[ia] += (xb - xa) * peso;
        } else {
          g_fila[ia] += (ia + 1 - xa) * peso;
          for (int k = ia + 1; k < ib; k++) g_fila[k] += peso;
          if (ib < ancho_) g_fila[ib] += (xb - ib) * peso;
        }
      }
    }
    for (int x = x0; x < x1; x++) mezclarPixel(x, y, minf(g_fila[x], 1.0f), rgb);
  }
}

void Lienzo::trazo(const Punto* p, int n, float grosor, uint32_t rgb) {
  if (n < 1 || grosor <= 0) return;
  const float radio = grosor / 2;
  const int segmentos = n == 1 ? 1 : n - 1;
  float minX = p[0].x, maxX = p[0].x, minY = p[0].y, maxY = p[0].y;
  for (int i = 1; i < n; i++) {
    minX = minf(minX, p[i].x), maxX = maxf(maxX, p[i].x);
    minY = minf(minY, p[i].y), maxY = maxf(maxY, p[i].y);
  }
  const int bx0 = maxi(0, static_cast<int>(piso(minX - radio - 1)));
  const int bx1 = mini(ancho_, static_cast<int>(techo(maxX + radio + 1)));
  const int by0 = maxi(yIni(), static_cast<int>(piso(minY - radio - 1)));
  const int by1 = mini(yFin(), static_cast<int>(techo(maxY + radio + 1)));
  if (bx0 >= bx1 || by0 >= by1) return;
  const bool conMascara = bx1 - bx0 <= LADO_MASCARA && by1 - by0 <= LADO_MASCARA;
  const int mw = bx1 - bx0;

  for (int i = 0; i < segmentos; i++) {
    Punto a = p[i], b = n == 1 ? p[i] : p[i + 1];
    int x0 = maxi(bx0, static_cast<int>(piso(minf(a.x, b.x) - radio - 1)));
    int x1 = mini(bx1, static_cast<int>(techo(maxf(a.x, b.x) + radio + 1)));
    int y0 = maxi(by0, static_cast<int>(piso(minf(a.y, b.y) - radio - 1)));
    int y1 = mini(by1, static_cast<int>(techo(maxf(a.y, b.y) + radio + 1)));
    for (int y = y0; y < y1; y++) {
      for (int x = x0; x < x1; x++) {
        float d = distanciaSegmento(x + 0.5f, y + 0.5f, a, b);
        float cob = limitar(radio + 0.5f - d, 0, 1);
        if (!conMascara) {
          mezclarPixel(x, y, cob, rgb);
          continue;
        }
        uint8_t v = static_cast<uint8_t>(cob * 255 + 0.5f);
        uint8_t& m = g_mascara[(y - by0) * mw + (x - bx0)];
        if (v > m) m = v;
      }
    }
  }
  if (!conMascara) return;
  for (int y = by0; y < by1; y++) {
    for (int x = bx0; x < bx1; x++) {
      uint8_t& m = g_mascara[(y - by0) * mw + (x - bx0)];
      if (m) {
        mezclarPixel(x, y, m / 255.0f, rgb);
        m = 0;
      }
    }
  }
}

void Lienzo::fila(int y, const uint16_t* rgb565) {
  if (y < yIni() || y >= yFin()) return;
  copiarBytes(px_ + (y - y0_) * ancho_, rgb565, sizeof(uint16_t) * static_cast<size_t>(ancho_));
}

void Lienzo::filaMezclada(int y, const uint16_t* rgb565, float alfa) {
  if (y < yIni() || y >= yFin()) return;
  const int a = static_cast<int>(limitar(alfa, 0, 1) * 256 + 0.5f);
  uint16_t* d = px_ + (y - y0_) * ancho_;
  for (int x = 0; x < ancho_; x++) {
    const uint32_t f = de565(d[x]), n = de565(rgb565[x]);
    auto canal = [&](int sh) {
      const int cf = (f >> sh) & 0xFF, cn = (n >> sh) & 0xFF;
      return static_cast<uint32_t>((cf * (256 - a) + cn * a) >> 8) << sh;
    };
    d[x] = a565(canal(16) | canal(8) | canal(0));
  }
}

void Lienzo::rectangulo(int x, int y, int w, int h, uint32_t rgb) {
  const uint16_t c = a565(rgb);
  for (int j = maxi(yIni(), y); j < mini(yFin(), y + h); j++)
    for (int i = maxi(0, x); i < mini(ancho_, x + w); i++) px_[(j - y0_) * ancho_ + i] = c;
}

void Lienzo::correrFilas(int y0, int y1, int dx, uint32_t fondo) {
  y0 = maxi(yIni(), y0), y1 = mini(yFin(), y1);
  if (dx == 0 || y0 >= y1) return;
  const uint16_t f = a565(fondo);
  for (int y = y0; y < y1; y++) {
    uint16_t* fila = px_ + (y - y0_) * ancho_;
    if (dx > 0) {
      for (int x = ancho_ - 1; x >= 0; x--) fila[x] = x - dx >= 0 ? fila[x - dx] : f;
    } else {
      for (int x = 0; x < ancho_; x++) fila[x] = x - dx < ancho_ ? fila[x - dx] : f;
    }
  }
}

}  // namespace sintetica
