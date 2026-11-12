#include "sprites.h"

#include "qoi.h"

namespace sintetica {

namespace {

inline uint32_t be32(const uint8_t* b) {
  return static_cast<uint32_t>(b[0]) << 24 | static_cast<uint32_t>(b[1]) << 16 | static_cast<uint32_t>(b[2]) << 8 | b[3];
}

struct Lector {
  LeerSprite leer;
  void* contexto;
  int sprite;
};

int leerQoi(void* c, uint32_t off, uint8_t* d, size_t n) {
  const Lector* l = static_cast<const Lector*>(c);
  return l->leer(l->contexto, l->sprite, off, d, n);
}

bool leerTodo(const Lector& l, uint32_t off, uint8_t* d, size_t n) {
  size_t k = 0;
  while (k < n) {
    const int m = l.leer(l.contexto, l.sprite, off + static_cast<uint32_t>(k), d + k, n - k);
    if (m <= 0) return false;
    k += static_cast<size_t>(m);
  }
  return true;
}

// Para decodificar (solo al cargar una cara): una fila del archivo y lo que se
// va sumando de cada píxel de la fila achicada (R·A, G·A, B·A y cuántos).
DecodificadorQoi g_qoi;
uint8_t g_fila[LADO_SPRITE * 4];
uint32_t g_suma[LADO_SPRITE][4];

// Decodifica el sprite achicándolo a ancho × alto (promediando lo que cae en
// cada píxel, con la transparencia premultiplicada) en `destino` (RGB565).
bool decodificar(const Lector& l, uint16_t ow, uint16_t oh, int ancho, int alto, uint16_t* destino) {
  if (!g_qoi.empezar(leerQoi, const_cast<Lector*>(&l)) || g_qoi.ancho() != ow || g_qoi.alto() != oh) return false;
  int filaDestino = 0;
  auto volcar = [&](int dy) {
    uint16_t* d = destino + dy * ancho;
    for (int dx = 0; dx < ancho; dx++) {
      uint32_t* s = g_suma[dx];
      const uint32_t n = s[3] ? s[3] * 255u : 1u;
      const uint32_t r = s[0] / n, g = s[1] / n, b = s[2] / n;
      d[dx] = static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
      s[0] = s[1] = s[2] = s[3] = 0;
    }
  };
  for (int dx = 0; dx < ancho; dx++) g_suma[dx][0] = g_suma[dx][1] = g_suma[dx][2] = g_suma[dx][3] = 0;
  for (uint32_t sy = 0; sy < oh; sy++) {
    if (!g_qoi.siguienteFilaRgba(g_fila)) return false;
    const int dy = static_cast<int>(sy * static_cast<uint32_t>(alto) / oh);
    if (dy != filaDestino) volcar(filaDestino), filaDestino = dy;
    for (uint32_t sx = 0; sx < ow; sx++) {
      const uint8_t* p = g_fila + sx * 4;
      uint32_t* s = g_suma[sx * static_cast<uint32_t>(ancho) / ow];
      s[0] += static_cast<uint32_t>(p[0]) * p[3];
      s[1] += static_cast<uint32_t>(p[1]) * p[3];
      s[2] += static_cast<uint32_t>(p[2]) * p[3];
      s[3]++;
    }
  }
  volcar(filaDestino);
  return true;
}

}  // namespace

bool cabeceraSprite(const uint8_t* b, size_t n, uint32_t* ancho, uint32_t* alto) {
  if (n < 14 || b[0] != 'q' || b[1] != 'o' || b[2] != 'i' || b[3] != 'f' || (b[12] != 3 && b[12] != 4)) return false;
  *ancho = be32(b + 4), *alto = be32(b + 8);
  return *ancho >= 1 && *alto >= 1 && *ancho <= static_cast<uint32_t>(LADO_SPRITE) && *alto <= static_cast<uint32_t>(LADO_SPRITE);
}

bool CacheSprites::cargar(const CaraImagen& cara, float unidad, LeerSprite leer, void* contexto) {
  listo_ = false;
  cara_ = cara;
  usados_ = 0;
  for (Parte& p : partes_) p = Parte();
  if (!leer || unidad <= 0) return false;
  // Las cabeceras: el tamaño de cada parte.
  for (int i = 0; i < N_PARTES_SPRITE; i++) {
    if (cara.sprite[i] < 0) continue;
    const Lector l{leer, contexto, cara.sprite[i]};
    uint8_t cab[14];
    uint32_t w = 0, h = 0;
    if (!leerTodo(l, 0, cab, sizeof cab) || !cabeceraSprite(cab, sizeof cab, &w, &h)) {
      if (i == static_cast<int>(ParteSprite::Ojo)) return false;
      continue;
    }
    partes_[i].anchoOriginal = static_cast<uint16_t>(w), partes_[i].altoOriginal = static_cast<uint16_t>(h);
  }
  const Parte& ojo = partes_[static_cast<int>(ParteSprite::Ojo)];
  escala_ = cara.tamano / 10000.0f / ojo.anchoOriginal;
  // Lo que mide cada una en la pantalla (sin agrandar nunca el archivo).
  float total = 0;
  float anchos[N_PARTES_SPRITE] = {};
  for (int i = 0; i < N_PARTES_SPRITE; i++) {
    const Parte& p = partes_[i];
    if (!p.anchoOriginal) continue;
    const float unidades = (i == static_cast<int>(ParteSprite::Boca) && cara.bocaTamano > 0) ? cara.bocaTamano / 10000.0f
                                                                                              : p.anchoOriginal * escala_;
    anchos[i] = minf(p.anchoOriginal, maxf(1, unidades * unidad * 1.05f));
    total += anchos[i] * anchos[i] * p.altoOriginal / p.anchoOriginal;
  }
  // Si no entran, se achican todas por igual.
  float f = total > CACHE_SPRITES_PX ? raiz(CACHE_SPRITES_PX / total) : 1.0f;
  for (int intento = 0; intento < 40; intento++) {
    uint32_t suma = 0;
    for (int i = 0; i < N_PARTES_SPRITE; i++) {
      Parte& p = partes_[i];
      if (!p.anchoOriginal) continue;
      const int w = maxi(1, mini(p.anchoOriginal, static_cast<int>(anchos[i] * f + 0.5f)));
      const int h = maxi(1, mini(p.altoOriginal, static_cast<int>(static_cast<float>(w) * p.altoOriginal / p.anchoOriginal + 0.5f)));
      p.ancho = static_cast<uint16_t>(w), p.alto = static_cast<uint16_t>(h);
      // La misma imagen en dos partes (ojo y ojo_der iguales): se guarda una vez.
      int igual = -1;
      for (int k = 0; k < i && igual < 0; k++)
        if (cara.sprite[k] == cara.sprite[i] && partes_[k].ancho == p.ancho && partes_[k].alto == p.alto) igual = k;
      p.inicio = igual >= 0 ? partes_[igual].inicio : suma;
      if (igual < 0) suma += static_cast<uint32_t>(w * h);
    }
    usados_ = suma;
    if (suma <= static_cast<uint32_t>(CACHE_SPRITES_PX)) break;
    f *= 0.92f;
  }
  if (usados_ > static_cast<uint32_t>(CACHE_SPRITES_PX)) return false;
  for (int i = 0; i < N_PARTES_SPRITE; i++) {
    Parte& p = partes_[i];
    if (!p.ancho) continue;
    bool compartida = false;
    for (int k = 0; k < i; k++)
      if (partes_[k].ancho && cara.sprite[k] == cara.sprite[i] && partes_[k].inicio == p.inicio) compartida = true;
    if (compartida) continue;
    const Lector l{leer, contexto, cara.sprite[i]};
    if (!decodificar(l, p.anchoOriginal, p.altoOriginal, p.ancho, p.alto, px_ + p.inicio)) {
      if (i == static_cast<int>(ParteSprite::Ojo)) return false;
      p = Parte();
    }
  }
  listo_ = true;
  return true;
}

namespace {

// Lo que hace falta para dibujar las partes en una franja.
struct Pincel {
  Lienzo& l;
  const VistaRostro& v;
  const MarcoRostro& m;
  const CacheSprites& s;
  int kr, kg, kb;          // brillo de cada canal (256 = igual)
  float glitch, semilla;

  // Corrimiento del glitch de la fila y (en píxeles), como las bandas de la SDF.
  float corrimiento(int y) const {
    if (glitch <= 0.01f) return 0;
    const float py = -(y + 0.5f - v.origenY) / v.unidad - m.dy;
    const float banda = piso(py * 14.0f + semilla * 7.0f);
    const float corr = (hashReal(banda + semilla * 13.0f) - 0.5f) * glitch * 0.22f *
                       (hashReal(banda * 1.7f + semilla) >= 0.55f ? 1.0f : 0.0f);
    return corr * v.unidad;
  }

  // Una parte centrada en (cx, cy) del rostro, de w × h unidades, girada `giro`
  // (como el tilt del ojo), espejada o no, con una opacidad.
  void parte(const CacheSprites::Parte& p, float cx, float cy, float w, float h, float giro, bool espejo, float alfa) const {
    if (!p.hay() || w <= 1e-4f || h <= 1e-4f || alfa <= 0.004f) return;
    const float ct = coseno(giro), st = seno(giro);
    const float pw = p.ancho, ph = p.alto;
    // Del píxel (sin el glitch) a la coordenada del buffer: es afín.
    auto mapa = [&](float px, float py, float* u, float* q) {
      const float x0 = (px + 0.5f - v.origenX) / v.unidad - m.dx, y0 = -(py + 0.5f - v.origenY) / v.unidad - m.dy;
      const float x1 = (m.cRot * x0 + m.sRot * y0) / m.ex, y1 = (-m.sRot * x0 + m.cRot * y0) / m.ey;
      const float qx = x1 - cx, qy = y1 - cy;
      float nx = (ct * qx + st * qy) / w;
      const float ny = (-st * qx + ct * qy) / h;
      if (espejo) nx = -nx;
      *u = (nx + 0.5f) * pw - 0.5f;
      *q = (0.5f - ny) * ph - 0.5f;
    };
    float u0, v0, u1, v1, u2, v2;
    mapa(0, 0, &u0, &v0);
    mapa(1, 0, &u1, &v1);
    mapa(0, 1, &u2, &v2);
    const float ux = u1 - u0, vx = v1 - v0, uy = u2 - u0, vy = v2 - v0;
    const float det = ux * vy - uy * vx;
    if (absf(det) < 1e-9f) return;
    // La caja en la pantalla: las cuatro esquinas del buffer (con un píxel de borde).
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    const float esq[4][2] = {{-1, -1}, {pw, -1}, {-1, ph}, {pw, ph}};
    for (const auto& e : esq) {
      const float du = e[0] - u0, dv = e[1] - v0;
      const float px = (vy * du - uy * dv) / det, py = (-vx * du + ux * dv) / det;
      x0 = minf(x0, px), x1 = maxf(x1, px), y0 = minf(y0, py), y1 = maxf(y1, py);
    }
    const float extra = glitch > 0.01f ? 0.12f * glitch * v.unidad + 1 : 0;
    const int f0 = l.franja(), f1 = f0 + l.filasFranja();
    const int ya = maxi(f0, static_cast<int>(piso(y0)) - 1), yb = mini(f1, static_cast<int>(techo(y1)) + 1);
    const int xa = maxi(0, static_cast<int>(piso(x0 - extra)) - 1), xb = mini(l.ancho(), static_cast<int>(techo(x1 + extra)) + 1);
    if (ya >= yb || xa >= xb) return;
    const int iw = p.ancho, ih = p.alto;
    const uint16_t* src = s.pixeles() + p.inicio;
    const int a = static_cast<int>(limitar(alfa, 0, 1) * 256.0f);
    const int ar = (kr * a) >> 8, ag = (kg * a) >> 8, ab = (kb * a) >> 8;
    const int32_t dU = static_cast<int32_t>(ux * 65536.0f), dV = static_cast<int32_t>(vx * 65536.0f);
    uint16_t* dst = l.pixeles();
    const int ancho = l.ancho();
    auto tap = [&](int x, int y) -> uint32_t {
      return (x >= 0 && y >= 0 && x < iw && y < ih) ? src[y * iw + x] : 0u;
    };
    for (int y = ya; y < yb; y++) {
      const float xg = xa + corrimiento(y);
      int32_t U = static_cast<int32_t>((u0 + ux * xg + uy * y) * 65536.0f);
      int32_t V = static_cast<int32_t>((v0 + vx * xg + vy * y) * 65536.0f);
      uint16_t* fila = dst + (y - f0) * ancho;
      for (int x = xa; x < xb; x++, U += dU, V += dV) {
        const int iu = U >> 16, iv = V >> 16;
        if (iu < -1 || iv < -1 || iu >= iw || iv >= ih) continue;
        const int fx = (U >> 8) & 0xFF, fy = (V >> 8) & 0xFF;
        const uint32_t c00 = tap(iu, iv), c10 = tap(iu + 1, iv), c01 = tap(iu, iv + 1), c11 = tap(iu + 1, iv + 1);
        if (!(c00 | c10 | c01 | c11)) continue;
        const int w00 = (256 - fx) * (256 - fy), w10 = fx * (256 - fy), w01 = (256 - fx) * fy, w11 = fx * fy;
        auto canal = [&](int sh, uint32_t mask) {
          return static_cast<int>((((c00 >> sh) & mask) * w00 + ((c10 >> sh) & mask) * w10 + ((c01 >> sh) & mask) * w01 +
                                   ((c11 >> sh) & mask) * w11) >> 16);
        };
        const int r = (canal(11, 0x1F) * ar) >> 8, g = (canal(5, 0x3F) * ag) >> 8, b = (canal(0, 0x1F) * ab) >> 8;
        const uint16_t d = fila[x];
        // Aclarar: el máximo por canal (sobre la pantalla casi negra, como mezclar con la transparencia).
        const int dr = d >> 11, dg = (d >> 5) & 0x3F, db = d & 0x1F;
        fila[x] = static_cast<uint16_t>((r > dr ? r : dr) << 11 | (g > dg ? g : dg) << 5 | (b > db ? b : db));
      }
    }
  }
};

}  // namespace

void dibujarSprites(Lienzo& l, const VistaRostro& v, const CuadroRostro& c, const CacheSprites& s, float opacidad) {
  if (!s.listo() || opacidad <= 0.001f) return;
  const MarcoRostro m = marcoRostro(c);
  const Rostro& r = c.r;
  const CaraImagen& ci = s.cara();
  const float k = s.escala();
  const float brillo = limitar((1.0f - r[param::DIM] * 0.55f) * c.brillo * limitar(opacidad, 0, 1), 0, 1);
  const float hue = r[param::HUE];
  Pincel pin{l, v, m, s, 0, 0, 0, c.glitch, c.semillaGlitch};
  pin.kr = static_cast<int>(brillo * 256.0f);
  pin.kg = pin.kb = static_cast<int>(brillo * (1.0f - 0.6f * hue) * 256.0f);
  using P = ParteSprite;
  float cxs[2] = {0, 0};
  for (int lado = 0; lado < 2; lado++) {
    const int b = lado * param::POR_OJO;
    const float side = lado == 0 ? -1.0f : 1.0f;
    float cx = side * r[param::SEP] + r[param::GX];
    float cy = r[param::EY] + r[param::GY];
    const float orb = r[param::ORBIT];
    if (orb > 0.001f) {
      const float a = c.faseOrbita + lado * MEDIA_VUELTA, rad = r[param::SEP] * 0.9f;
      cx = cx * (1 - orb) + coseno(a) * rad * orb;
      cy = cy * (1 - orb) + seno(a) * rad * 0.55f * orb;
    }
    cxs[lado] = cx;
    // El tamaño sigue a los del rostro: una cara que agranda los ojos también los agranda.
    const float latido = 1 + r[param::BEAT] * 0.16f * c.latido;
    const float sq = r[b + param::SQUASH];
    const float fw = limitar(r[b + param::W] / 0.105f * latido / raiz(sq), 0.4f, 2.0f);
    const float fh = limitar(r[b + param::H] / 0.23f * latido * c.pulso * sq, 0.4f, 2.0f);
    // Los párpados: cuánto del ojo se ve (el parpadeo los lleva a 0,5 y 0,5).
    const float lt = r[b + param::LT], lb = r[b + param::LB];
    const float abierto = limitar(1 - lt - lb, 0, 1) * (1 - 0.6f * r[b + param::SMILE]);
    const float giro = r[b + param::TILT] * side;
    P abierta = P::Ojo;
    bool espejoA = false;
    if (lado == 1) {
      if (s.parte(P::OjoDer).hay()) abierta = P::OjoDer;
      else espejoA = ci.espejar;
    }
    P cerrada = P::Cantidad;
    bool espejoC = false;
    if (lado == 1 && s.parte(P::OjoDerCerrado).hay()) cerrada = P::OjoDerCerrado;
    else if (s.parte(P::OjoCerrado).hay()) cerrada = P::OjoCerrado, espejoC = lado == 1 && ci.espejar;
    const CacheSprites::Parte& pa = s.parte(abierta);
    const float W = pa.anchoOriginal * k * fw, H = pa.altoOriginal * k * fh;
    // Con ojo_cerrado: el abierto se aplasta un poco y se funde con el cerrado.
    // Sin él, el abierto se aplasta hasta ser una rayita.
    const bool hayCerrado = cerrada != P::Cantidad;
    const float alfa = hayCerrado ? suaveEntre(0.22f, 0.5f, abierto) : 1.0f;
    const float alto = maxf(abierto, hayCerrado ? 0.2f : 0.06f);
    pin.parte(pa, cx, cy + (lb - lt) * H * 0.5f, W, H * alto, giro, espejoA, alfa);
    if (hayCerrado && alfa < 0.996f) {
      const CacheSprites::Parte& pc = s.parte(cerrada);
      pin.parte(pc, cx, cy, pc.anchoOriginal * k * fw, pc.altoOriginal * k * fh, giro, espejoC, 1 - alfa);
    }
  }
  const CacheSprites::Parte& pb = s.parte(P::Boca);
  if (pb.hay()) {
    const float base = ci.bocaTamano > 0 ? ci.bocaTamano / 10000.0f : pb.anchoOriginal * k;
    const float fm = limitar(r[param::MW] / 0.07f, 0.5f, 2.0f);
    const float w = base * fm;
    const float h = base * pb.altoOriginal / pb.anchoOriginal * (1 + r[param::MO] * 6) * c.pulso;
    pin.parte(pb, (cxs[0] + cxs[1]) * 0.5f, ci.bocaY / 10000.0f + r[param::EY] + r[param::GY] * 0.5f, w, h, 0, false, 1);
  }
}

}  // namespace sintetica
