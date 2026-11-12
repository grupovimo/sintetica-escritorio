#include "rostro.h"

#include "sprites.h"

namespace sintetica {

namespace {

using G = GrupoResorte;

// El orden tiene que coincidir con param::Ojo y param::Global. Defectos: los
// del v0 (BASE_EYE y BASE_G).
const InfoParametro INFO_OJO[param::POR_OJO] = {
    {"w", 0.03f, 0.2f, 0.105f, G::Forma},
    {"h", 0.03f, 0.32f, 0.23f, G::Forma},
    {"r", 0.0f, 1.0f, 1.0f, G::Forma},
    {"tilt", -0.6f, 0.6f, 0.0f, G::Forma},
    {"lt", 0.0f, 0.5f, 0.0f, G::Parpados},
    {"lta", -0.8f, 0.8f, 0.0f, G::Parpados},
    {"lb", 0.0f, 0.5f, 0.0f, G::Parpados},
    {"lba", -0.8f, 0.8f, 0.0f, G::Parpados},
    {"smile", 0.0f, 1.0f, 0.0f, G::Forma},
    {"heart", 0.0f, 1.0f, 0.0f, G::Forma},
    {"star", 0.0f, 1.0f, 0.0f, G::Forma},
    {"cross", 0.0f, 1.0f, 0.0f, G::Forma},
    {"ring", 0.0f, 1.0f, 0.0f, G::Forma},
    {"pupil", 0.0f, 1.0f, 0.0f, G::Forma},
    {"squash", 0.7f, 1.3f, 1.0f, G::Forma},
};
const InfoParametro INFO_GLOBAL[N_PARAMETROS - param::GX] = {
    {"gx", -0.3f, 0.3f, 0.0f, G::Mirada},
    {"gy", -0.25f, 0.25f, 0.0f, G::Mirada},
    {"ey", -0.25f, 0.25f, 0.0f, G::Cara},
    {"sep", 0.15f, 0.45f, 0.3f, G::Forma},
    {"ftilt", -0.6f, 0.6f, 0.0f, G::Cara},
    {"fscale", 0.7f, 1.3f, 1.0f, G::Cara},
    {"spin", 0.0f, 1.0f, 0.0f, G::Forma},
    {"orbit", 0.0f, 1.0f, 0.0f, G::Forma},
    {"pulse", 0.0f, 1.0f, 0.0f, G::Forma},
    {"beat", 0.0f, 1.0f, 0.0f, G::Forma},
    {"glitch", 0.0f, 1.0f, 0.0f, G::Parpados},
    {"dim", 0.0f, 1.0f, 0.0f, G::Color},
    {"hue", 0.0f, 1.0f, 0.0f, G::Color},
    {"mo", 0.0f, 0.2f, 0.0f, G::Forma},
    {"mw", 0.02f, 0.2f, 0.07f, G::Forma},
    {"mcurve", -1.0f, 1.0f, 0.0f, G::Forma},
    {"blush", 0.0f, 1.0f, 0.0f, G::Color},
    {"shake", 0.0f, 1.0f, 0.0f, G::Cara},
};

const char* const NOMBRES_PARTICULAS[static_cast<int>(TipoParticula::Cantidad)] = {
    "zzz", "corazon", "lagrima", "gota", "chispa", "pregunta", "exclamacion", "nota", "estrella", "burbuja", "oveja", "reloj", "garabato"};

uint32_t g_finos = 0;

struct V2 {
  float x, y;
};
inline float largo2(float x, float y) { return raiz(x * x + y * y); }
inline float dot2(float x, float y) { return x * x + y * y; }

// rot(a)·q con la convención de GLSL del v0: mat2(c, -s, s, c).
inline V2 rotar(float c, float s, V2 q) { return {c * q.x + s * q.y, -s * q.x + c * q.y}; }

float sdRB(V2 p, float bx, float by, float r) {
  const float qx = absf(p.x) - bx + r, qy = absf(p.y) - by + r;
  return largo2(maxf(qx, 0), maxf(qy, 0)) + minf(maxf(qx, qy), 0.0f) - r;
}

// El punto de la caja redondeada más cerca de p (p afuera, a distancia d).
V2 cercaRB(V2 p, float bx, float by, float r, float d) {
  const float qx = absf(p.x) - bx + r, qy = absf(p.y) - by + r;
  float nx, ny;
  if (qx > 0 && qy > 0) {
    const float l = largo2(qx, qy);
    nx = qx / l, ny = qy / l;
  } else if (qx > qy) {
    nx = 1, ny = 0;
  } else {
    nx = 0, ny = 1;
  }
  return {p.x - signo(p.x) * nx * d, p.y - signo(p.y) * ny * d};
}

float distSegmento(V2 p, float ax, float ay, float bx, float by) {
  const float vx = bx - ax, vy = by - ay;
  const float l2 = vx * vx + vy * vy;
  const float t = l2 > 0 ? limitar(((p.x - ax) * vx + (p.y - ay) * vy) / l2, 0, 1) : 0;
  return largo2(p.x - ax - t * vx, p.y - ay - t * vy);
}

float sdHeart(V2 p) {
  p.x = absf(p.x);
  if (p.y + p.x > 1.0f) return raiz(dot2(p.x - 0.25f, p.y - 0.75f)) - 0.35355339f;
  const float m = 0.5f * maxf(p.x + p.y, 0.0f);
  return raiz(minf(dot2(p.x, p.y - 1.0f), dot2(p.x - m, p.y - m))) * signo(p.x - p.y);
}

// Estrella de 5 puntas (Inigo Quilez), radio r y factor de las puntas rf.
float sdStar5(V2 p, float r, float rf) {
  const float k1x = 0.809016994f, k1y = -0.587785252f;
  const float k2x = -k1x, k2y = k1y;
  p.x = absf(p.x);
  float d = 2.0f * maxf(k1x * p.x + k1y * p.y, 0.0f);
  p.x -= d * k1x, p.y -= d * k1y;
  d = 2.0f * maxf(k2x * p.x + k2y * p.y, 0.0f);
  p.x -= d * k2x, p.y -= d * k2y;
  p.x = absf(p.x);
  p.y -= r;
  const float bax = -k1y * rf, bay = k1x * rf - 1.0f;  // vec2(-k1.y, k1.x)*rf - vec2(0, 1)
  const float h = limitar((p.x * bax + p.y * bay) / dot2(bax, bay), 0.0f, r);
  const float ex = p.x - bax * h, ey = p.y - bay * h;
  return largo2(ex, ey) * signo(p.y * bax - p.x * bay);
}

float sdSegmento(V2 p, V2 a, V2 b) {
  const float bx = b.x - a.x, by = b.y - a.y, px = p.x - a.x, py = p.y - a.y;
  const float l = dot2(bx, by);
  const float t = l > 0 ? limitar((px * bx + py * by) / l, 0.0f, 1.0f) : 0.0f;
  return largo2(px - bx * t, py - by * t);
}

// Una X (KO): dos barras redondeadas cruzadas.
float sdCruz(V2 p, float r) {
  const float t = r * 0.22f;
  const float a = sdSegmento(p, {-r * 0.8f, -r * 0.8f}, {r * 0.8f, r * 0.8f});
  const float b = sdSegmento(p, {-r * 0.8f, r * 0.8f}, {r * 0.8f, -r * 0.8f});
  return minf(a, b) - t;
}

float sdRombo(V2 p, float bx, float by) {
  p.x = absf(p.x), p.y = absf(p.y);
  const float ndot = bx * (bx - 2 * p.x) - by * (by - 2 * p.y);
  const float h = limitar(ndot / dot2(bx, by), -1.0f, 1.0f);
  const float dx = p.x - 0.5f * bx * (1 - h), dy = p.y - 0.5f * by * (1 + h);
  return largo2(dx, dy) * signo(p.x * by + p.y * bx - bx * by);
}

// Todo lo de un ojo que no depende del píxel.
struct Ojo {
  float cx, cy, w, h, rr, side;
  float ct, st;              // rot(tilt·side)
  float nTx, nTy, kT;        // párpado de arriba: normal y altura
  float nBx, nBy, kB;
  float smile, heart, star, cross, ring, pupil;
  float cerrado;             // 0 abierto, 1 línea
  float Rr, linY, finY;      // la línea del ojo cerrado
  float pupX, pupY, pupR;    // la pupila (en el sistema del ojo)
  // Los bordes rectos que dejan los párpados (el corte de cada uno con el
  // ojo, recortado por el otro): afuera, el resplandor mide hasta ahí.
  bool hayT, hayB;
  float t0x, t0y, t1x, t1y, b0x, b0y, b1x, b1y;
};

struct Prep {
  Ojo ojo[2];
  float spin, faseSpin;
  float bocaX, bocaY, mw, mo, mcurve;
  float cRot, sRot;          // rot(-ftilt)
  float ex, ey;              // escalas de x e y (con fscale)
  float dx, dy;              // corrimiento (deriva y temblor)
  float glitch, semilla;
  float radio;               // radio del contenido, en unidades del rostro
  float blush;
  float progreso;
};

// Dónde corta la recta de un párpado (por (0, k), normal n) al ojo sin
// párpados: false si no lo corta (el párpado está abierto del todo).
bool cuerda(const Ojo& o, float nx, float ny, float k, float* ax, float* ay, float* bx, float* by) {
  const float dx = ny, dy = -nx;  // a lo largo de la recta
  auto f = [&](float t) { return sdRB({t * dx, k + t * dy}, o.w, o.h, o.rr); };
  if (f(0) >= -1e-5f) return false;
  const float lejos = 2.0f * (o.w + o.h) + 0.1f;
  float extremos[2];
  for (int lado = 0; lado < 2; lado++) {
    float adentro = 0, afuera = lado ? lejos : -lejos;
    for (int i = 0; i < 24; i++) {
      const float m = 0.5f * (adentro + afuera);
      (f(m) < 0 ? adentro : afuera) = m;
    }
    extremos[lado] = 0.5f * (adentro + afuera);
  }
  *ax = extremos[0] * dx, *ay = k + extremos[0] * dy;
  *bx = extremos[1] * dx, *by = k + extremos[1] * dy;
  return true;
}

// Recorta el segmento a b al semiplano n·(p - (0, k)) <= 0.
bool recortar(float nx, float ny, float k, float* ax, float* ay, float* bx, float* by) {
  const float da = *ax * nx + (*ay - k) * ny, db = *bx * nx + (*by - k) * ny;
  if (da > 0 && db > 0) return false;
  if (da > 0 || db > 0) {
    const float t = da / (da - db);
    const float x = *ax + (*bx - *ax) * t, y = *ay + (*by - *ay) * t;
    if (da > 0) *ax = x, *ay = y;
    else *bx = x, *by = y;
  }
  return true;
}

void cuerdas(Ojo* o) {
  o->hayT = cuerda(*o, o->nTx, o->nTy, o->kT, &o->t0x, &o->t0y, &o->t1x, &o->t1y) &&
            recortar(o->nBx, o->nBy, o->kB, &o->t0x, &o->t0y, &o->t1x, &o->t1y);
  o->hayB = cuerda(*o, o->nBx, o->nBy, o->kB, &o->b0x, &o->b0y, &o->b1x, &o->b1y) &&
            recortar(o->nTx, o->nTy, o->kT, &o->b0x, &o->b0y, &o->b1x, &o->b1y);
}

void prepararOjo(const CuadroRostro& c, int lado, Ojo* o) {
  const Rostro& r = c.r;
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
  const float s = 1 + r[param::BEAT] * 0.16f * c.latido;
  const float sq = r[b + param::SQUASH];
  o->cx = cx, o->cy = cy, o->side = side;
  o->w = r[b + param::W] * s / raiz(sq);
  o->h = r[b + param::H] * s * c.pulso * sq;
  o->rr = r[b + param::R] * minf(o->w, o->h);
  const float tilt = r[b + param::TILT] * side;
  o->ct = coseno(tilt), o->st = seno(tilt);
  const float lt = r[b + param::LT], lb = r[b + param::LB];
  const float a2 = r[b + param::LTA] * side, b2 = r[b + param::LBA] * side;
  o->nTx = -seno(a2), o->nTy = coseno(a2), o->kT = o->h * (1 - 2 * lt);
  o->nBx = seno(b2), o->nBy = -coseno(b2), o->kB = -o->h * (1 - 2 * lb);
  o->smile = r[b + param::SMILE];
  o->heart = r[b + param::HEART];
  o->star = r[b + param::STAR];
  o->cross = r[b + param::CROSS];
  o->ring = r[b + param::RING];
  o->pupil = r[b + param::PUPIL];
  o->cerrado = suaveEntre(0.86f, 0.99f, lt + lb);
  o->Rr = minf(40.0f, o->w / maxf(o->smile * 0.75f, 0.001f));
  o->linY = -o->Rr + o->smile * 0.02f;
  o->finY = o->Rr - raiz(maxf(o->Rr * o->Rr - o->w * o->w * 0.9025f, 0.0f)) - o->smile * 0.02f;
  cuerdas(o);
  // La pupila mira para donde mira la cara.
  o->pupR = 0.55f * minf(o->w, o->h) * o->pupil;
  o->pupX = limitar(r[param::GX] * 0.6f, -o->w * 0.4f, o->w * 0.4f);
  o->pupY = limitar(r[param::GY] * 0.6f, -o->h * 0.4f, o->h * 0.4f);
}

}  // namespace

MarcoRostro marcoRostro(const CuadroRostro& c) {
  const Rostro& r = c.r;
  MarcoRostro m;
  const float ft = r[param::FTILT] + r[param::SPIN] * 0.12f * seno(c.tiempo * 2.5f);
  m.cRot = coseno(-ft), m.sRot = seno(-ft);
  const float fs = r[param::FSCALE];
  m.ex = maxf(0.2f, c.escalaX * fs), m.ey = maxf(0.2f, c.escalaY * fs);
  const float sh = r[param::SHAKE];
  m.dx = c.derivaX + sh * 0.018f * seno(c.tiempo * 61.0f);
  m.dy = c.derivaY + sh * 0.011f * seno(c.tiempo * 47.0f + 1.3f);
  return m;
}

namespace {

void preparar(const CuadroRostro& c, Prep* p) {
  const Rostro& r = c.r;
  prepararOjo(c, 0, &p->ojo[0]);
  prepararOjo(c, 1, &p->ojo[1]);
  p->spin = r[param::SPIN];
  p->faseSpin = c.faseSpin;
  p->mo = r[param::MO], p->mw = r[param::MW], p->mcurve = r[param::MCURVE];
  p->bocaX = (p->ojo[0].cx + p->ojo[1].cx) * 0.5f;
  p->bocaY = -0.42f + r[param::EY] + r[param::GY] * 0.5f;
  const MarcoRostro m = marcoRostro(c);
  p->cRot = m.cRot, p->sRot = m.sRot, p->ex = m.ex, p->ey = m.ey, p->dx = m.dx, p->dy = m.dy;
  p->glitch = c.glitch;
  p->semilla = c.semillaGlitch;
  p->blush = r[param::BLUSH];
  p->progreso = limitar(c.progreso, 0, 1);
  float rad = 0;
  for (const Ojo& o : p->ojo) rad = maxf(rad, largo2(o.cx, o.cy) + maxf(o.w, o.h) * 2.0f + 0.05f);
  rad = maxf(rad, largo2(p->bocaX, p->bocaY) + p->mw + 0.1f);
  p->radio = rad;
}

// El ojo del v0, con las mezclas que agrega la especificación (estrella, cruz,
// anillo). *dPupila: la distancia a la pupila (si tiene).
float ojo(V2 p, const Ojo& o, const Prep& pr, float* dPupila, float* anguloAnillo) {
  V2 q = {p.x - o.cx, p.y - o.cy};
  if (pr.spin > 0.001f) {
    const float a = pr.spin * (5.5f * largo2(q.x, q.y)) + pr.faseSpin * pr.spin;
    q = rotar(coseno(a), seno(a), q);
  }
  q = rotar(o.ct, o.st, q);
  const float w = o.w, h = o.h;
  float d = sdRB(q, w, h, o.rr);
  if (o.heart > 0.001f) {
    const float s = maxf(w, h) * 1.9f;
    d = mezclar(d, sdHeart({q.x / s, q.y / s + 0.55f}) * s, o.heart);
  }
  if (o.star > 0.001f) d = mezclar(d, sdStar5(q, maxf(w, h) * 1.15f, 0.42f), o.star);
  if (o.cross > 0.001f) d = mezclar(d, sdCruz(q, maxf(w, h) * 0.9f), o.cross);
  if (o.ring > 0.001f) {
    const float R = maxf(w, h) * 0.78f;
    d = mezclar(d, absf(largo2(q.x, q.y) - R) - 0.03f, o.ring);
    if (anguloAnillo) *anguloAnillo = arcotangente2(q.x, q.y);
  }
  const float dT = (q.x - 0) * o.nTx + (q.y - o.kT) * o.nTy;
  const float dB = (q.x - 0) * o.nBx + (q.y - o.kB) * o.nBy;
  float dl = maxf(d, maxf(dT, dB));
  if (dl > 0 && (o.hayT || o.hayB) && o.heart + o.star + o.cross + o.ring < 0.001f) {
    // Afuera del ojo con párpados (que es convexo), el máximo se queda corto
    // al costado de las esquinas y sus curvas de nivel (el resplandor) se ven
    // como cajas. La distancia de verdad: hasta el ojo sin párpados si ese
    // punto más cercano queda entre los párpados; si no, hasta el borde recto
    // que deja alguno. Adentro y en el borde no cambia nada.
    bool listo = false;
    if (d > 0) {
      const V2 c = cercaRB(q, w, h, o.rr, d);
      listo = c.x * o.nTx + (c.y - o.kT) * o.nTy <= 1e-4f && c.x * o.nBx + (c.y - o.kB) * o.nBy <= 1e-4f;
    }
    if (!listo) {
      float e = 9.0f;
      if (o.hayT) e = minf(e, distSegmento(q, o.t0x, o.t0y, o.t1x, o.t1y));
      if (o.hayB) e = minf(e, distSegmento(q, o.b0x, o.b0y, o.b1x, o.b1y));
      if (e < 9.0f) dl = maxf(dl, e);
    }
  }
  if (o.smile > 0.001f) {
    const float R = w * 0.68f;
    float qcx = q.x, qcy = q.y - (-h - R + o.smile * h * 1.5f);
    qcy -= limitar(qcy, -2.0f, 0.0f);
    dl = maxf(dl, -(largo2(qcx, qcy) - R));
  }
  if (dPupila && o.pupil > 0.01f) *dPupila = largo2(q.x - o.pupX, q.y - o.pupY) - o.pupR;
  if (o.cerrado <= 0.0f) return dl;
  const float qay = q.y - o.linY;
  // Un arco con las puntas redondas: en el tramo del arco, la distancia al
  // círculo; fuera de él, a la punta (con un corte recto, el resplandor de
  // las puntas se veía cuadrado).
  const float lq = largo2(q.x, qay);
  const bool enArco = qay > 0 && absf(q.x) * o.Rr <= w * 0.95f * lq;
  float dLine = enArco ? absf(lq - o.Rr) - 0.022f : 9.0f;
  dLine = minf(dLine, largo2(absf(q.x) - w * 0.95f, q.y + o.finY) - 0.022f);
  return minf(dl + o.cerrado * 0.03f, mezclar(9.0f, dLine, o.cerrado));
}

struct Muestra {
  float d;
  float pupila;  // cobertura de la pupila (0 a 1)
  float apagado; // el anillo, la parte que todavía no se llenó
};

float boca(V2 p, const Prep& pr) {
  float d = 9.0f;
  const float qx = p.x - pr.bocaX, qy = p.y - pr.bocaY;
  if (pr.mo > 0.002f) {
    const float ax = pr.mw, ay = pr.mo;
    d = (largo2(qx / ax, qy / ay) - 1.0f) * minf(ax, ay);
  }
  const float mc = pr.mcurve;
  if (absf(mc) > 0.02f) {
    // Una línea curva: sonrisa (las puntas arriba) o puchero.
    const float k = 0.05f * mc / (pr.mw * pr.mw);
    const float xc = limitar(qx, -pr.mw, pr.mw);
    const float f = k * xc * xc - 0.05f * mc * 0.5f;
    const float dfx = 2 * k * xc;
    const float dy = (qy - f) / raiz(1 + dfx * dfx);
    const float grosor = 0.02f * minf(1.0f, absf(mc) * 4.0f);
    d = minf(d, largo2(qx - xc, dy) - grosor);
  }
  return d;
}

Muestra cara(V2 p, const Prep& pr, float aa) {
  Muestra m{9.0f, 0, 0};
  for (int i = 0; i < 2; i++) {
    float dp = 9.0f, ang = 0;
    const Ojo& o = pr.ojo[i];
    const float d = ojo(p, o, pr, &dp, o.ring > 0.5f ? &ang : nullptr);
    if (d < m.d) {
      m.d = d;
      m.pupila = o.pupil > 0.01f ? (1.0f - suaveEntre(-aa, aa, dp)) * o.pupil : 0.0f;
      if (o.ring > 0.5f) {
        // El anillo se llena en el sentido del reloj desde arriba.
        float f = ang / VUELTA;
        if (f < 0) f += 1;
        m.apagado = suaveEntre(pr.progreso - 0.01f, pr.progreso + 0.01f, f) * o.ring;
      }
    }
  }
  const float db = boca(p, pr);
  if (db < m.d) m.d = db, m.pupila = 0;
  return m;
}

// Del píxel a las coordenadas del rostro (sin el glitch).
inline V2 aRostro(float px, float py, const VistaRostro& v, const Prep& pr) {
  V2 p = {(px - v.origenX) / v.unidad - pr.dx, -(py - v.origenY) / v.unidad - pr.dy};
  p = rotar(pr.cRot, pr.sRot, p);
  p.x /= pr.ex, p.y /= pr.ey;
  return p;
}

struct Rgb {
  float r, g, b;
};
inline Rgb rgbDe(uint32_t c) {
  return {((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f};
}
inline uint16_t a565f(float r, float g, float b) {
  const int R = static_cast<int>(limitar(r, 0, 1) * 31.0f + 0.5f);
  const int Gc = static_cast<int>(limitar(g, 0, 1) * 63.0f + 0.5f);
  const int B = static_cast<int>(limitar(b, 0, 1) * 31.0f + 0.5f);
  return static_cast<uint16_t>(R << 11 | Gc << 5 | B);
}

void dibujarParticula(Lienzo& l, const VistaRostro& v, const Particula& pt, uint32_t colorOjos, float brillo);

// El resplandor del v0 (0,22·e^(-16 d)), llevado a cero justo en ALCANCE_HALO
// para que no quede un escalón donde se deja de calcular.
constexpr float ALCANCE_HALO = 0.2f;
const float E_ALCANCE = 0.0407622f;  // e^(-16 · 0,2)
inline float brillo(float d, bool halo) {
  if (!halo || d >= ALCANCE_HALO) return 0.0f;
  return 0.22f * (exponencial(-maxf(d, 0.0f) * 16.0f) - E_ALCANCE) / (1.0f - E_ALCANCE);
}

}  // namespace

const InfoParametro& infoParametro(int i) {
  if (i < param::GX) return INFO_OJO[i % param::POR_OJO];
  return INFO_GLOBAL[i - param::GX];
}

int parametroPorNombre(const char* n, bool* esOjo) {
  for (int i = 0; i < param::POR_OJO; i++)
    if (iguales(n, INFO_OJO[i].nombre)) {
      if (esOjo) *esOjo = true;
      return i;
    }
  for (int i = 0; i < N_PARAMETROS - param::GX; i++)
    if (iguales(n, INFO_GLOBAL[i].nombre)) {
      if (esOjo) *esOjo = false;
      return param::GX + i;
    }
  return -1;
}

Rostro::Rostro() {
  for (int i = 0; i < N_PARAMETROS; i++) v[i] = infoParametro(i).defecto;
}

void Rostro::limitar() {
  for (int i = 0; i < N_PARAMETROS; i++) {
    const InfoParametro& p = infoParametro(i);
    v[i] = sintetica::limitar(v[i], p.minimo, p.maximo);
  }
}

const char* nombreParticula(TipoParticula t) {
  const int i = static_cast<int>(t);
  return i < static_cast<int>(TipoParticula::Cantidad) ? NOMBRES_PARTICULAS[i] : "";
}

bool particulaPorNombre(const char* n, TipoParticula* t) {
  for (int i = 0; i < static_cast<int>(TipoParticula::Cantidad); i++)
    if (iguales(n, NOMBRES_PARTICULAS[i])) {
      *t = static_cast<TipoParticula>(i);
      return true;
    }
  return false;
}

uint32_t pixelesFinosUltimoDibujo() { return g_finos; }

float distanciaRostro(const CuadroRostro& c, float x, float y) {
  Prep pr;
  preparar(c, &pr);
  return cara({x, y}, pr, 0.01f).d;
}

void dibujarRostro(Lienzo& l, const VistaRostro& v, const CuadroRostro& c, const CacheSprites* sprites, float opSprites,
                   float ocultarOjos) {
  Prep pr;
  preparar(c, &pr);
  g_finos = l.franja() == 0 ? 0 : g_finos;
  const bool conSprites = sprites && sprites->listo() && opSprites > 0.001f;
  ocultarOjos = limitar(ocultarOjos, 0, 1);

  const Rgb fondo = rgbDe(paleta::PANTALLA);
  const float hue = c.r[param::HUE];
  // Con una cara de imagen, los ojos de la SDF se apagan (el fundido).
  const float apagar = (1.0f - c.r[param::DIM] * 0.55f) * c.brillo * (1.0f - ocultarOjos);
  const Rgb base = rgbDe(c.colorOjos), rojo = rgbDe(c.colorRojo);
  const Rgb ojoC = {mezclar(base.r, rojo.r, hue) * apagar, mezclar(base.g, rojo.g, hue) * apagar,
                    mezclar(base.b, rojo.b, hue) * apagar};
  const Rgb bg = {fondo.r * c.brillo, fondo.g * c.brillo, fondo.b * c.brillo};
  const uint16_t bg565 = a565f(bg.r, bg.g, bg.b), ojo565 = a565f(ojoC.r, ojoC.g, ojoC.b);
  {
    uint16_t* px = l.pixeles();
    const int n = l.ancho() * l.filasFranja();
    for (int i = 0; i < n; i++) px[i] = bg565;
  }

  // El ancho del antialias: 1,3 px (el de v0: smoothstep(0, 2,6/m)).
  const float aa = 1.3f / v.unidad;
  const float minEsc = minf(pr.ex, pr.ey);
  const float halo = v.halo ? ALCANCE_HALO : 0.0f;
  // La caja del contenido en píxeles (un cuadrado alrededor del centro).
  const float radPx = (pr.radio + halo) * v.unidad * maxf(pr.ex, pr.ey) + 3;
  const float cxp = v.origenX + pr.dx * v.unidad, cyp = v.origenY - pr.dy * v.unidad;
  const int y0 = l.franja(), y1 = y0 + l.filasFranja();
  const int bx0 = maxi(0, static_cast<int>(piso((cxp - radPx) / 4)) * 4);
  const int bx1 = mini(l.ancho(), static_cast<int>(techo((cxp + radPx) / 4)) * 4);
  const int by0 = maxi(y0 - (y0 % 4), static_cast<int>(piso((cyp - radPx) / 4)) * 4);
  const int by1 = mini(y1, static_cast<int>(techo((cyp + radPx) / 4)) * 4);
  const bool hayGlitch = pr.glitch > 0.01f;
  const bool porBloques = !hayGlitch && pr.spin < 0.01f;
  bool detalles = false;  // pupila o anillo: el relleno no es parejo
  for (const Ojo& o : pr.ojo) detalles = detalles || o.pupil > 0.01f || o.ring > 0.5f;
  // Medio diagonal de un bloque de 4 × 4, en unidades del rostro.
  const float margen = 2.0f * (2.83f / v.unidad) / minEsc + aa;
  uint16_t* px = l.pixeles();
  const int ancho = l.ancho();

  auto pixel = [&](int x, int y) {
    const V2 p = aRostro(x + 0.5f, y + 0.5f, v, pr);
    float rr, gg, bb, dg;
    float pup = 0, apag = 0;
    if (hayGlitch) {
      const float g = pr.glitch;
      const float banda = piso(p.y * 14.0f + pr.semilla * 7.0f);
      const float corr = (hashReal(banda + pr.semilla * 13.0f) - 0.5f) * g * 0.22f *
                         (hashReal(banda * 1.7f + pr.semilla) >= 0.55f ? 1.0f : 0.0f);
      const V2 pp = {p.x + corr, p.y};
      const float dr = cara({pp.x + g * 0.035f, pp.y}, pr, aa).d;
      const Muestra mg = cara(pp, pr, aa);
      const float db = cara({pp.x - g * 0.035f, pp.y}, pr, aa).d;
      dg = mg.d;
      rr = 1 - suaveEntre(0, 2 * aa, dr);
      gg = 1 - suaveEntre(0, 2 * aa, dg);
      bb = 1 - suaveEntre(0, 2 * aa, db);
    } else {
      const Muestra m = cara(p, pr, aa);
      dg = m.d;
      rr = gg = bb = 1 - suaveEntre(0, 2 * aa, dg);
      pup = m.pupila;
      apag = m.apagado;
    }
    g_finos++;
    const float k = 1.0f - 0.7f * apag;  // el anillo: lo que falta cargar, apagado
    rr *= k, gg *= k, bb *= k;
    const float glow = brillo(dg, v.halo);
    Rgb col = {bg.r + ojoC.r * glow, bg.g + ojoC.g * glow, bg.b + ojoC.b * glow};
    col.r = mezclar(col.r, ojoC.r, rr);
    col.g = mezclar(col.g, ojoC.g, gg);
    col.b = mezclar(col.b, ojoC.b, bb);
    if (pup > 0.002f) {
      col.r = mezclar(col.r, bg.r, pup);
      col.g = mezclar(col.g, bg.g, pup);
      col.b = mezclar(col.b, bg.b, pup);
    }
    px[(y - y0) * ancho + x] = a565f(col.r, col.g, col.b);
  };

  // El resplandor fuera de la forma cambia suave: se calcula en las esquinas
  // de cada bloque y se interpola (una SDF cada 16 píxeles).
  auto resplandor = [&](float d) { return brillo(d, v.halo); };
  auto colorHalo = [&](float glow) {
    return a565f(bg.r + ojoC.r * glow, bg.g + ojoC.g * glow, bg.b + ojoC.b * glow);
  };

  // Con la cara de imagen entera, la SDF no se calcula (los sprites la reemplazan).
  const int byFin = ocultarOjos >= 0.999f ? by0 : by1;
  for (int by = by0; by < byFin; by += 4) {
    for (int bx = bx0; bx < bx1; bx += 4) {
      const int ya = maxi(by, y0), yb = mini(by + 4, y1);
      const int xb = mini(bx + 4, ancho);
      if (ya >= yb) continue;
      if (porBloques) {
        // La SDF en el centro del bloque decide: lejos afuera, fondo; bien
        // adentro, relleno parejo; cerca del borde, píxel por píxel.
        const V2 p = aRostro(bx + 2.0f, by + 2.0f, v, pr);
        const float d = cara(p, pr, aa).d;
        if (d > margen + halo) continue;
        if (d < -margen && !detalles) {
          for (int y = ya; y < yb; y++)
            for (int x = bx; x < xb; x++) px[(y - y0) * ancho + x] = ojo565;
          continue;
        }
        if (d > margen) {
          // Solo resplandor: interpolar entre las cuatro esquinas.
          const float g00 = resplandor(cara(aRostro(bx, by, v, pr), pr, aa).d);
          const float g10 = resplandor(cara(aRostro(bx + 4.0f, by, v, pr), pr, aa).d);
          const float g01 = resplandor(cara(aRostro(bx, by + 4.0f, v, pr), pr, aa).d);
          const float g11 = resplandor(cara(aRostro(bx + 4.0f, by + 4.0f, v, pr), pr, aa).d);
          for (int y = ya; y < yb; y++) {
            const float fy = (y - by + 0.5f) / 4.0f;
            const float gi = mezclar(g00, g01, fy), gd = mezclar(g10, g11, fy);
            for (int x = bx; x < xb; x++) px[(y - y0) * ancho + x] = colorHalo(mezclar(gi, gd, (x - bx + 0.5f) / 4.0f));
          }
          continue;
        }
      }
      for (int y = ya; y < yb; y++)
        for (int x = bx; x < xb; x++) pixel(x, y);
    }
  }

  // Cachetes: dos manchas suaves debajo de los ojos.
  if (pr.blush > 0.01f) {
    for (const Ojo& o : pr.ojo) {
      const float ccx = o.cx + o.side * 0.03f, ccy = o.cy - o.h - 0.07f;
      for (int y = y0; y < y1; y++) {
        for (int x = 0; x < ancho; x++) {
          const V2 p = aRostro(x + 0.5f, y + 0.5f, v, pr);
          const float ex = (p.x - ccx) / 0.08f, ey = (p.y - ccy) / 0.032f;
          if (ex * ex + ey * ey > 2.0f) continue;
          const float d = (raiz(ex * ex + ey * ey) - 1.0f) * 0.032f;
          const float cob = (1 - suaveEntre(-0.02f, 0.02f, d)) * 0.55f * pr.blush;
          l.cubrir(x, y, cob, paleta::NARANJA_PROFUNDO);
        }
      }
    }
  }

  if (conSprites) dibujarSprites(l, v, c, *sprites, opSprites);

  for (const Particula& pt : c.particulas)
    if (pt.viva) dibujarParticula(l, v, pt, c.colorOjos, c.brillo);
}

namespace {

uint32_t escalarColor(uint32_t c, float k) {
  const Rgb x = rgbDe(c);
  auto b = [](float v) { return static_cast<uint32_t>(limitar(v, 0, 1) * 255 + 0.5f); };
  return b(x.r * k) << 16 | b(x.g * k) << 8 | b(x.b * k);
}

float sdParticula(TipoParticula t, V2 q, float a, float prog) {
  switch (t) {
    case TipoParticula::Zzz: {
      const float d1 = sdSegmento(q, {-a, a}, {a, a});
      const float d2 = sdSegmento(q, {a, a}, {-a, -a});
      const float d3 = sdSegmento(q, {-a, -a}, {a, -a});
      return minf(d1, minf(d2, d3)) - a * 0.2f;
    }
    case TipoParticula::Corazon: {
      const float s = a * 1.7f;
      return sdHeart({q.x / s, q.y / s + 0.5f}) * s;
    }
    case TipoParticula::Lagrima:
    case TipoParticula::Gota: {
      const float d1 = largo2(q.x, q.y + 0.3f * a) - 0.55f * a;
      const float t = limitar((q.y + 0.3f * a) / (1.1f * a), 0.0f, 1.0f);
      const float d2 = maxf(absf(q.x) - 0.55f * a * (1 - t), maxf(q.y - 0.8f * a, -0.3f * a - q.y));
      return minf(d1, d2);
    }
    case TipoParticula::Chispa:
      return minf(sdRombo(q, a, a * 0.22f), sdRombo(q, a * 0.22f, a));
    case TipoParticula::Pregunta: {
      const float cy = 0.35f * a, r = 0.32f * a;
      float arco = absf(largo2(q.x, q.y - cy) - r) - 0.1f * a;
      if (q.x < 0 && q.y < cy) arco = 9.0f;
      const float tallo = sdSegmento(q, {0, cy - r}, {0, -0.2f * a}) - 0.1f * a;
      const float punto = largo2(q.x, q.y + 0.6f * a) - 0.12f * a;
      return minf(arco, minf(tallo, punto));
    }
    case TipoParticula::Exclamacion: {
      const float tallo = sdSegmento(q, {0, 0.75f * a}, {0, -0.15f * a}) - 0.13f * a;
      return minf(tallo, largo2(q.x, q.y + 0.6f * a) - 0.13f * a);
    }
    case TipoParticula::Nota: {
      const float cabeza = largo2((q.x + 0.22f * a) / 1.25f, q.y + 0.45f * a) - 0.26f * a;
      const float tallo = sdSegmento(q, {0.08f * a, -0.45f * a}, {0.08f * a, 0.7f * a}) - 0.07f * a;
      const float bandera = sdSegmento(q, {0.08f * a, 0.7f * a}, {0.45f * a, 0.38f * a}) - 0.07f * a;
      return minf(cabeza, minf(tallo, bandera));
    }
    case TipoParticula::Estrella:
      return sdStar5(q, a, 0.5f);
    case TipoParticula::Burbuja:
      return minf(absf(largo2(q.x, q.y) - a * 0.8f) - 0.08f * a, largo2(q.x + 0.85f * a, q.y + 0.95f * a) - 0.16f * a);
    case TipoParticula::Reloj: {
      // Un relojito: el aro y dos agujas (v1.9, el apuro de la agenda).
      const float aro = absf(largo2(q.x, q.y) - 0.62f * a) - 0.1f * a;
      const float h = sdSegmento(q, {0, 0}, {0, 0.42f * a}) - 0.08f * a;
      return minf(aro, minf(h, sdSegmento(q, {0, 0}, {0.3f * a, 0}) - 0.08f * a));
    }
    case TipoParticula::Garabato: {
      // Un dibujito que se va haciendo (v3, juega solo): una espiral que crece
      // con la vida de la partícula (termina al 70 %).
      const float hasta = limitar(prog / 0.7f, 0, 1) * 12.0f;
      float d = 9.0f;
      V2 antes = {0.12f * a, 0};
      for (int k = 1; k <= 12 && k - 1 < hasta; k++) {
        const float f = minf(static_cast<float>(k), hasta) / 12.0f;
        const float r = a * (0.12f + 0.88f * f), ang = f * 3.0f * MEDIA_VUELTA;
        const V2 p = {r * coseno(ang), r * seno(ang) * 0.8f};
        d = minf(d, sdSegmento(q, antes, p));
        antes = p;
      }
      return d - 0.07f * a;
    }
    case TipoParticula::Oveja: {
      // Lana (cuatro bultos), la cabeza adelante y dos patitas.
      float d = largo2(q.x + 0.3f * a, q.y) - 0.38f * a;
      d = minf(d, largo2(q.x, q.y - 0.18f * a) - 0.4f * a);
      d = minf(d, largo2(q.x + 0.05f * a, q.y + 0.12f * a) - 0.38f * a);
      d = minf(d, largo2(q.x - 0.28f * a, q.y - 0.02f * a) - 0.34f * a);
      d = minf(d, largo2(q.x - 0.7f * a, q.y - 0.12f * a) - 0.2f * a);
      d = minf(d, sdSegmento(q, {-0.25f * a, -0.3f * a}, {-0.25f * a, -0.7f * a}) - 0.07f * a);
      return minf(d, sdSegmento(q, {0.25f * a, -0.3f * a}, {0.25f * a, -0.7f * a}) - 0.07f * a);
    }
    default:
      return 9.0f;
  }
}

void dibujarParticula(Lienzo& l, const VistaRostro& v, const Particula& pt, uint32_t colorOjos, float brillo) {
  const float vida = pt.vida > 0 ? pt.edad / pt.vida : 1;
  float op = vida < 0.15f ? vida / 0.15f : (vida > 0.7f ? (1 - vida) / 0.3f : 1.0f);
  op = limitar(op, 0, 1);
  if (op <= 0.01f) return;
  uint32_t color;
  switch (pt.tipo) {
    case TipoParticula::Corazon:
    case TipoParticula::Chispa:
    case TipoParticula::Estrella:
    case TipoParticula::Garabato:
      color = colorOjos;
      break;
    case TipoParticula::Lagrima:
      color = paleta::TINTA_SUAVE;
      break;
    default:
      color = paleta::TINTA;
  }
  color = escalarColor(color, brillo);
  const float a = pt.tamano;
  const float cx = v.origenX + pt.x * v.unidad, cy = v.origenY - pt.y * v.unidad;
  const float rad = a * 1.6f * v.unidad + 2;
  const int x0 = maxi(0, static_cast<int>(cx - rad)), x1 = mini(l.ancho(), static_cast<int>(cx + rad) + 1);
  const int y0 = maxi(l.franja(), static_cast<int>(cy - rad));
  const int y1 = mini(l.franja() + l.filasFranja(), static_cast<int>(cy + rad) + 1);
  const float aa = 1.0f / v.unidad;
  // Un balanceo chiquito.
  const float giro = 0.25f * seno(pt.fase + pt.edad * 3.0f);
  const float cg = coseno(giro), sg = seno(giro);
  l.opacidad(op);
  for (int y = y0; y < y1; y++) {
    for (int x = x0; x < x1; x++) {
      V2 q = {(x + 0.5f - cx) / v.unidad, -(y + 0.5f - cy) / v.unidad};
      q = rotar(cg, sg, q);
      const float d = sdParticula(pt.tipo, q, a, vida);
      if (d > aa) continue;
      l.cubrir(x, y, 1 - suaveEntre(-aa, aa, d), color);
    }
  }
  l.opacidad(1);
}

}  // namespace

}  // namespace sintetica
