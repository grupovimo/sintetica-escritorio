#include "cara.h"

namespace sintetica {

namespace {

constexpr int MAX_MUESTRAS = 160;
Punto g_muestras[MAX_MUESTRAS];

// Una transformación afín en coordenadas normalizadas,
// x' = ax·x + kx·y + bx, y' = ky·x + ay·y + by, y después a píxeles con la
// vista. Las caras solo escalan por eje (kx = ky = 0); las animaciones además
// giran (y entonces la escala es pareja).
struct Transformacion {
  float ax = 1, bx = 0, ay = 1, by = 0;
  float kx = 0, ky = 0;
  Vista v;
  float e = 0;  // la escala que usa esta forma: la de los ojos o la de los indicadores
  Punto aPixel(float x, float y) const {
    return {v.origenX + e * (ax * x + kx * y + bx), v.origenY + e * (ky * x + ay * y + by)};
  }
  Punto aPixel(Punto p) const { return aPixel(p.x, p.y); }
  float anchoPx(float w) const { return w * ax * e; }
  float altoPx(float h) const { return h * ay * e; }
  bool gira() const { return kx != 0 || ky != 0; }
  float escalaPareja() const { return raiz(ax * ax + ky * ky); }
};

// El contorno de una píldora (rectángulo redondeado) como polígono, en
// coordenadas normalizadas: para dibujarla girada.
int contornoPildora(float cx, float cy, float w, float h, float r, Punto* p) {
  constexpr int PASOS = 8;  // por esquina
  const float mw = w / 2, mh = h / 2;
  r = limitar(r, 0, minf(mw, mh));
  const float esquinas[4][3] = {{mw - r, mh - r, 0},
                                {-(mw - r), mh - r, MEDIA_VUELTA / 2},
                                {-(mw - r), -(mh - r), MEDIA_VUELTA},
                                {mw - r, -(mh - r), MEDIA_VUELTA * 1.5f}};
  int n = 0;
  for (const auto& q : esquinas) {
    for (int i = 0; i <= PASOS; i++) {
      const float a = q[2] + (MEDIA_VUELTA / 2) * i / PASOS;
      p[n++] = {cx + q[0] + r * coseno(a), cy + q[1] + r * seno(a)};
    }
  }
  return n;
}

// Corazón del manual: dos cúbicas (offsets en px de la pantalla de referencia
// respecto del centro del ojo).
const Punto CORAZON[7] = {{0, 21.4f},   {-31.2f, 0.6f}, {-23.4f, -28.0f}, {0, -11.1f},
                          {23.4f, -28.0f}, {31.2f, 0.6f},  {0, 21.4f}};

Punto cubica(Punto a, Punto b, Punto c, Punto d, float t) {
  float u = 1 - t;
  float k0 = u * u * u, k1 = 3 * u * u * t, k2 = 3 * u * t * t, k3 = t * t * t;
  return {k0 * a.x + k1 * b.x + k2 * c.x + k3 * d.x, k0 * a.y + k1 * b.y + k2 * c.y + k3 * d.y};
}

Punto cuadratica(Punto a, Punto b, Punto c, float t) {
  float u = 1 - t;
  return {u * u * a.x + 2 * u * t * b.x + t * t * c.x, u * u * a.y + 2 * u * t * b.y + t * t * c.y};
}

uint32_t colorDe(const Forma& f, const EstadoCara& e) {
  return (f.acento && e.hayColorOjos) ? e.colorOjos : f.color;
}

void dibujarForma(Lienzo& l, const Forma& f, const Transformacion& t, const EstadoCara& e) {
  const uint32_t color = colorDe(f, e);
  switch (f.tipo) {
    case TipoForma::Pildora: {
      float w = f.w, x = f.x, h = f.h;
      if (f.progreso) {
        float izq = f.x - f.w / 2;
        w = f.w * limitar(e.progreso, 0, 1);
        x = izq + w / 2;
      }
      if (f.audio >= 0) h = f.h * (0.35f + 0.65f * limitar(e.audio[f.audio], 0, 1));
      if (t.gira()) {
        const float r = f.r < 0 ? minf(w, h) / 2 : minf(f.r, minf(w, h) / 2);
        if (f.relleno) {
          int n = contornoPildora(x, f.y, w, h, r, g_muestras);
          for (int i = 0; i < n; i++) g_muestras[i] = t.aPixel(g_muestras[i]);
          l.poligono(g_muestras, n, color);
        } else {
          // El trazo va por dentro: el contorno se achica medio trazo.
          const float m = f.trazo / 2;
          int n = contornoPildora(x, f.y, maxf(w - f.trazo, 0), maxf(h - f.trazo, 0), maxf(r - m, 0), g_muestras);
          for (int i = 0; i < n; i++) g_muestras[i] = t.aPixel(g_muestras[i]);
          g_muestras[n] = g_muestras[0];
          l.trazo(g_muestras, n + 1, f.trazo * t.e * t.escalaPareja(), color);
        }
        break;
      }
      Punto c = t.aPixel(x, f.y);
      float wp = t.anchoPx(w), hp = t.altoPx(h);
      float r = f.r < 0 ? minf(wp, hp) / 2 : minf(f.r * t.ax * t.e, minf(wp, hp) / 2);
      if (f.relleno) l.pildora(c.x, c.y, wp, hp, r, color);
      else l.pildoraContorno(c.x, c.y, wp, hp, r, f.trazo * t.e, color);
      break;
    }
    case TipoForma::Circulo: {
      Punto c = t.aPixel(f.x, f.y);
      float wp = t.anchoPx(2 * f.r), hp = t.altoPx(2 * f.r);
      if (t.gira()) wp = hp = 2 * f.r * t.e * t.escalaPareja();
      l.pildora(c.x, c.y, wp, hp, minf(wp, hp) / 2, color);
      break;
    }
    case TipoForma::Corte: {
      // Borde de arriba inclinado y medio círculo abajo.
      const float x0 = f.x - f.w / 2, x1 = f.x + f.w / 2;
      const float arriba = f.y - f.h / 2, abajo = f.y + f.h / 2;
      const float r = f.w / 2;
      int n = 0;
      g_muestras[n++] = t.aPixel(x0, arriba + f.izq);
      g_muestras[n++] = t.aPixel(x1, arriba + f.der);
      constexpr int PASOS = 24;
      for (int i = 0; i <= PASOS; i++) {
        float a = MEDIA_VUELTA * i / PASOS;  // 0 = derecha, π/2 = abajo, π = izquierda
        g_muestras[n++] = t.aPixel(f.x + r * coseno(a), abajo - r + r * seno(a));
      }
      l.poligono(g_muestras, n, color);
      break;
    }
    case TipoForma::Corazon: {
      const float k = f.tamano / 160.0f;
      int n = 0;
      constexpr int PASOS = 20;
      for (int c = 0; c < 2; c++) {
        const Punto* p = &CORAZON[c * 3];
        for (int i = 0; i < PASOS; i++) {
          Punto q = cubica(p[0], p[1], p[2], p[3], static_cast<float>(i) / PASOS);
          g_muestras[n++] = t.aPixel(f.x + q.x * k, f.y + q.y * k);
        }
      }
      l.poligono(g_muestras, n, color);
      break;
    }
    case TipoForma::Arco: {
      constexpr int PASOS = 24;
      for (int i = 0; i <= PASOS; i++)
        g_muestras[i] = t.aPixel(cuadratica(f.puntos[0], f.puntos[1], f.puntos[2], static_cast<float>(i) / PASOS));
      l.trazo(g_muestras, PASOS + 1, f.trazo * t.e * (t.gira() ? t.escalaPareja() : 1), color);
      break;
    }
    case TipoForma::Espiral: {
      int n = 0;
      const float paso = f.giro / (MAX_MUESTRAS - 1);
      for (int i = 0; i < MAX_MUESTRAS; i++) {
        float a = i * paso;
        float r = f.r0 + f.paso * a;
        g_muestras[n++] = t.aPixel(f.x + r * coseno(a), f.y + r * seno(a));
      }
      l.trazo(g_muestras, n, f.trazo * t.e * (t.gira() ? t.escalaPareja() : 1), color);
      break;
    }
    case TipoForma::Poligono: {
      for (int i = 0; i < f.nPuntos; i++) g_muestras[i] = t.aPixel(f.puntos[i]);
      l.poligono(g_muestras, f.nPuntos, color);
      break;
    }
    case TipoForma::Franjas: {
      for (int i = 0; i < f.nPuntos; i++) {
        const Banda& b = f.bandas[i];
        // Las franjas son de la pantalla, no de los ojos: sin transformación.
        int y0 = static_cast<int>(t.v.origenY + b.y * t.v.escala + 0.5f);
        int y1 = static_cast<int>(t.v.origenY + (b.y + b.h) * t.v.escala + 0.5f);
        int dx = static_cast<int>(b.dx * t.v.escala + (b.dx >= 0 ? 0.5f : -0.5f));
        l.correrFilas(y0, y1, dx, paleta::PANTALLA);
      }
      break;
    }
  }
}

}  // namespace

void cajaDeForma(const Forma& f, float* x0, float* y0, float* x1, float* y1) {
  switch (f.tipo) {
    case TipoForma::Pildora:
    case TipoForma::Corte:
      *x0 = f.x - f.w / 2, *x1 = f.x + f.w / 2, *y0 = f.y - f.h / 2, *y1 = f.y + f.h / 2;
      return;
    case TipoForma::Circulo:
      *x0 = f.x - f.r, *x1 = f.x + f.r, *y0 = f.y - f.r, *y1 = f.y + f.r;
      return;
    case TipoForma::Corazon: {
      const float k = f.tamano / 160.0f;
      *x0 = f.x - 24 * k, *x1 = f.x + 24 * k, *y0 = f.y - 18 * k, *y1 = f.y + 21.4f * k;
      return;
    }
    case TipoForma::Espiral: {
      const float r = f.r0 + f.paso * f.giro;
      *x0 = f.x - r, *x1 = f.x + r, *y0 = f.y - r, *y1 = f.y + r;
      return;
    }
    case TipoForma::Arco:
    case TipoForma::Poligono: {
      *x0 = *x1 = f.puntos[0].x;
      *y0 = *y1 = f.puntos[0].y;
      for (int i = 1; i < f.nPuntos; i++) {
        *x0 = minf(*x0, f.puntos[i].x), *x1 = maxf(*x1, f.puntos[i].x);
        *y0 = minf(*y0, f.puntos[i].y), *y1 = maxf(*y1, f.puntos[i].y);
      }
      return;
    }
    case TipoForma::Franjas:
      *x0 = *x1 = *y0 = *y1 = 0;
      return;
  }
}

void dibujarExpresion(Lienzo& l, const Vista& v, const Expresion& e, const EstadoCara& estado) {
  // Agrupar las formas que no son fijas en ojo izquierdo y derecho, y calcular
  // la caja de cada grupo: el parpadeo y la respiración escalan cada ojo
  // alrededor de su propio centro.
  struct Grupo {
    float x0 = 9, y0 = 9, x1 = -9, y1 = -9;
    bool usado = false;
  } grupos[2];
  int grupoDe[MAX_FORMAS];
  for (int i = 0; i < e.nFormas; i++) {
    const Forma& f = e.formas[i];
    grupoDe[i] = -1;
    if (f.fijo || f.tipo == TipoForma::Franjas) continue;
    float x0, y0, x1, y1;
    cajaDeForma(f, &x0, &y0, &x1, &y1);
    int g = (x0 + x1) / 2 < 0 ? 0 : 1;
    grupoDe[i] = g;
    Grupo& gr = grupos[g];
    gr.usado = true;
    gr.x0 = minf(gr.x0, x0), gr.y0 = minf(gr.y0, y0);
    gr.x1 = maxf(gr.x1, x1), gr.y1 = maxf(gr.y1, y1);
  }

  Transformacion fija;
  fija.v = v;
  fija.e = v.escalaFijos;
  Transformacion ojo[2];
  for (int g = 0; g < 2; g++) {
    Transformacion& t = ojo[g];
    t.v = v;
    t.e = v.escala;
    if (!grupos[g].usado) continue;
    const float cx = (grupos[g].x0 + grupos[g].x1) / 2;
    const float cy = (grupos[g].y0 + grupos[g].y1) / 2;
    const float alto = grupos[g].y1 - grupos[g].y0;
    float ky = mezclar(1.0f, alto > ALTO_CERRADO ? ALTO_CERRADO / alto : 1.0f, limitar(estado.cierre, 0, 1));
    float kx = 1.0f;
    const float resp = estado.respiracion * AMPLITUD_RESPIRACION;
    ky *= (1 + resp) * estado.tamano;
    kx *= (1 - resp * 0.5f) * estado.tamano;
    t.ax = kx;
    t.bx = cx - cx * kx + estado.miradaX;
    t.ay = ky;
    t.by = cy - cy * ky + estado.miradaY;
  }

  // Los indicadores van en grupos (las formas fijas seguidas: la batería, los
  // puntos, las barras, la zZ). Cada grupo conserva su tamaño a la escala de
  // los ojos y solo su posición usa la escala de los indicadores: en la
  // pantalla redonda se acercan al centro sin achicarse. En la rectangular las
  // dos escalas son iguales y no cambia nada.
  for (int i = 0; i < e.nFormas;) {
    const Forma& f = e.formas[i];
    if (grupoDe[i] >= 0 || f.tipo == TipoForma::Franjas) {
      dibujarForma(l, f, grupoDe[i] < 0 ? fija : ojo[grupoDe[i]], estado);
      i++;
      continue;
    }
    int fin = i;
    float x0 = 9, y0 = 9, x1 = -9, y1 = -9;
    while (fin < e.nFormas && grupoDe[fin] < 0 && e.formas[fin].tipo != TipoForma::Franjas) {
      float a = 0, b = 0, c = 0, d = 0;
      cajaDeForma(e.formas[fin], &a, &b, &c, &d);
      x0 = minf(x0, a), y0 = minf(y0, b), x1 = maxf(x1, c), y1 = maxf(y1, d);
      fin++;
    }
    Transformacion t;
    t.v = v;
    t.e = v.escala;
    const float k = v.escalaFijos / v.escala - 1;
    t.bx = k * (x0 + x1) / 2;
    t.by = k * (y0 + y1) / 2;
    for (; i < fin; i++) dibujarForma(l, e.formas[i], t, estado);
  }
  (void)fija;
}

void dibujarAnimacion(Lienzo& l, const Vista& v, const Animacion& a, uint32_t t, const EstadoCara& estado) {
  if (t > a.duracion) t = a.duracion;
  for (int i = 0; i < a.nPistas; i++) {
    const Pista& p = a.pistas[i];
    const Cuadro q = cuadroDePista(p, t);
    if (q.opacidad <= 0.002f || q.escala <= 0) continue;
    float cx = p.cx, cy = p.cy;
    if (!p.hayCentro) {
      float x0, y0, x1, y1;
      cajaDeForma(p.forma, &x0, &y0, &x1, &y1);
      cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    }
    // p' = c + R·(p − c)·s + (x, y). Los grados van en sentido horario (y crece hacia abajo).
    const float rad = q.rotacion * (MEDIA_VUELTA / 180.0f);
    const float co = coseno(rad) * q.escala, se = seno(rad) * q.escala;
    Transformacion tr;
    tr.v = v;
    tr.e = v.escala;
    tr.ax = co, tr.kx = -se, tr.ky = se, tr.ay = co;
    tr.bx = cx - (co * cx - se * cy) + q.x;
    tr.by = cy - (se * cx + co * cy) + q.y;
    // Sin giro, la píldora se dibuja con su forma exacta.
    if (absf(tr.kx) < 1e-6f && absf(tr.ky) < 1e-6f) tr.kx = tr.ky = 0;
    if (tr.ax < 0 || tr.ay < 0) tr.kx = tr.kx == 0 ? 1e-6f : tr.kx;  // 180°: por el camino del polígono
    Forma f = p.forma;
    if (q.hayColor) f.color = q.color, f.acento = false;
    l.opacidad(q.opacidad);
    dibujarForma(l, f, tr, estado);
    l.opacidad(1);
  }
}

}  // namespace sintetica
