#include "clima.h"

namespace sintetica {

namespace {
constexpr uint32_t SOL = paleta::NARANJA;
constexpr uint32_t NUBE = paleta::TINTA;
constexpr uint32_t NUBE_ATRAS = paleta::TINTA_SUAVE;
constexpr uint32_t AGUA = 0x8FB8E8;
constexpr uint32_t RAYO = 0xFFC94A;

void disco(Lienzo& l, float x, float y, float r, uint32_t c) { l.pildora(x, y, 2 * r, 2 * r, r, c); }

// Una nube: tres bultos y la base redondeada. (cx, cy) es el centro; s, el ancho.
void nube(Lienzo& l, float cx, float cy, float s, uint32_t c) {
  disco(l, cx - s * 0.2f, cy - s * 0.02f, s * 0.2f, c);
  disco(l, cx + s * 0.06f, cy - s * 0.12f, s * 0.26f, c);
  disco(l, cx + s * 0.28f, cy + s * 0.02f, s * 0.17f, c);
  l.pildora(cx + s * 0.02f, cy + s * 0.1f, s * 0.86f, s * 0.3f, s * 0.15f, c);
}

void sol(Lienzo& l, float cx, float cy, float r, float t) {
  disco(l, cx, cy, r, SOL);
  const float giro = t * 0.25f;
  for (int i = 0; i < 8; i++) {
    const float a = giro + i * (VUELTA / 8);
    const float c = coseno(a), s = seno(a);
    const Punto p[2] = {{cx + c * r * 1.35f, cy + s * r * 1.35f}, {cx + c * r * 1.75f, cy + s * r * 1.75f}};
    l.trazo(p, 2, maxf(1.5f, r * 0.2f), SOL);
  }
}

void luna(Lienzo& l, float cx, float cy, float r) {
  disco(l, cx, cy, r, NUBE);
  disco(l, cx + r * 0.45f, cy - r * 0.3f, r * 0.85f, paleta::PANTALLA);
}

}  // namespace

bool leerClima(const uint8_t* d, size_t n, DatosClima* c) {
  if (n < 7) return false;
  DatosClima x;
  x.decimas = static_cast<int16_t>(static_cast<uint16_t>(d[0] | d[1] << 8));
  x.codigo = d[2];
  x.maxima = static_cast<int8_t>(d[3]);
  x.minima = static_cast<int8_t>(d[4]);
  x.dia = d[5] & 1;
  const size_t nc = d[6];
  if (nc >= LARGO_CIUDAD || 7 + nc != n) return false;
  if (x.decimas < -900 || x.decimas > 700) return false;
  copiarTexto(x.ciudad, sizeof x.ciudad, reinterpret_cast<const char*>(d + 7), nc);
  x.hay = true;
  *c = x;
  return true;
}

Cielo cieloDe(uint8_t w) {
  if (w == 0) return Cielo::Despejado;
  if (w <= 2) return Cielo::Parcial;
  if (w == 3) return Cielo::Nublado;
  if (w == 45 || w == 48) return Cielo::Niebla;
  if (w >= 51 && w <= 57) return Cielo::Llovizna;
  if ((w >= 61 && w <= 67) || (w >= 80 && w <= 82)) return Cielo::Lluvia;
  if ((w >= 71 && w <= 77) || w == 85 || w == 86) return Cielo::Nieve;
  if (w >= 95) return Cielo::Tormenta;
  return Cielo::Nublado;
}

const char* descripcionCielo(Cielo c) {
  static const char* const D[] = {"despejado", "algo nublado", "nublado", "niebla", "llovizna", "lluvia", "nieve", "tormenta"};
  return D[static_cast<int>(c)];
}

size_t textoTemperatura(int g, char* s, size_t cap) {
  char t[8];
  size_t n = 0;
  if (g < 0) t[n++] = '-', g = -g;
  if (g >= 100) t[n++] = static_cast<char>('0' + g / 100 % 10);
  if (g >= 10) t[n++] = static_cast<char>('0' + g / 10 % 10);
  t[n++] = static_cast<char>('0' + g % 10);
  t[n++] = static_cast<char>(0xC2), t[n++] = static_cast<char>(0xB0);  // °
  return copiarTexto(s, cap, t, n);
}

size_t textoClima(const DatosClima& c, char* s, size_t cap) {
  if (!c.hay) return copiarTexto(s, cap, "");
  char t[64];
  size_t n = textoTemperatura(c.temperatura(), t, sizeof t);
  const Cielo cielo = cieloDe(c.codigo);
  // "18° y nublado"; "y despejado" de noche suena raro: "18° y despejado" igual sirve.
  n += copiarTexto(t + n, sizeof t - n, " y ");
  n += copiarTexto(t + n, sizeof t - n, descripcionCielo(cielo));
  return copiarTexto(s, cap, t, n);
}

const char* caraClima(const DatosClima& c) {
  if (!c.hay) return nullptr;
  const Cielo s = cieloDe(c.codigo);
  if (s == Cielo::Tormenta) return "miedo";
  if (c.temperatura() < 8) return "frio";
  switch (s) {
    case Cielo::Lluvia:
    case Cielo::Llovizna: return "triste";
    case Cielo::Nieve: return "emocionado";
    case Cielo::Niebla: return "confundido";
    case Cielo::Nublado: return "pensando";
    default: break;
  }
  if (c.temperatura() > 30) return "cansado";
  return c.dia ? "contento" : nullptr;
}

Situacion situacionClima(const DatosClima& c) {
  if (!c.hay) return Situacion::Cantidad;
  const Cielo s = cieloDe(c.codigo);
  if (s == Cielo::Lluvia || s == Cielo::Llovizna || s == Cielo::Tormenta) return Situacion::ClimaLluvia;
  if (c.temperatura() < 8) return Situacion::ClimaFrio;
  if (c.temperatura() > 30) return Situacion::ClimaCalor;
  if (c.dia && (s == Cielo::Despejado || s == Cielo::Parcial)) return Situacion::ClimaLindo;
  return Situacion::Cantidad;
}

void dibujarIconoClima(Lienzo& l, Cielo cielo, bool dia, float cx, float cy, float tam, float t, float alfa) {
  const float s = tam;
  l.opacidad(alfa);
  switch (cielo) {
    case Cielo::Despejado:
      if (dia) sol(l, cx, cy, s * 0.24f, t);
      else luna(l, cx, cy, s * 0.3f);
      return;
    case Cielo::Parcial:
      if (dia) sol(l, cx + s * 0.16f, cy - s * 0.16f, s * 0.18f, t);
      else luna(l, cx + s * 0.18f, cy - s * 0.18f, s * 0.2f);
      nube(l, cx - s * 0.06f, cy + s * 0.1f, s * 0.78f, NUBE);
      return;
    case Cielo::Nublado:
      nube(l, cx + s * 0.14f, cy - s * 0.12f, s * 0.6f, NUBE_ATRAS);
      nube(l, cx - s * 0.06f, cy + s * 0.08f, s * 0.8f, NUBE);
      return;
    case Cielo::Niebla:
      for (int i = 0; i < 3; i++) {
        const float dx = seno(t * 0.8f + i * 1.7f) * s * 0.08f;
        l.pildora(cx + dx, cy - s * 0.22f + i * s * 0.22f, s * (0.86f - 0.12f * (i & 1)), s * 0.11f, s * 0.055f,
                  i == 1 ? NUBE : NUBE_ATRAS);
      }
      return;
    case Cielo::Llovizna:
    case Cielo::Lluvia:
    case Cielo::Tormenta:
    case Cielo::Nieve: {
      nube(l, cx, cy - s * 0.12f, s * 0.82f, cielo == Cielo::Tormenta ? NUBE_ATRAS : NUBE);
      const int gotas = cielo == Cielo::Llovizna ? 2 : 3;
      for (int i = 0; i < gotas; i++) {
        // Caen en un ciclo de 0,9 s, desfasadas.
        const float f = fraccion(t / 0.9f + i * 0.37f);
        const float x = cx - s * 0.24f + i * s * (gotas == 2 ? 0.48f : 0.24f);
        const float y = cy + s * 0.12f + f * s * 0.3f;
        l.opacidad(alfa * (1 - f * 0.8f));
        if (cielo == Cielo::Nieve) {
          disco(l, x, y, s * 0.045f, NUBE);
        } else if (cielo != Cielo::Tormenta || i != 1) {
          const Punto p[2] = {{x + s * 0.03f, y - s * 0.06f}, {x - s * 0.02f, y + s * 0.04f}};
          l.trazo(p, 2, maxf(1.5f, s * 0.05f), AGUA);
        }
        l.opacidad(alfa);
      }
      if (cielo == Cielo::Tormenta) {
        // El rayo titila.
        const float brillo = fraccion(t * 0.6f) < 0.12f ? 0.35f : 1.0f;
        const Punto r[] = {{cx + s * 0.04f, cy + s * 0.02f}, {cx - s * 0.1f, cy + s * 0.24f}, {cx, cy + s * 0.24f},
                           {cx - s * 0.06f, cy + s * 0.44f}, {cx + s * 0.12f, cy + s * 0.18f}, {cx + s * 0.02f, cy + s * 0.18f},
                           {cx + s * 0.1f, cy + s * 0.02f}};
        l.opacidad(alfa * brillo);
        l.poligono(r, 7, RAYO);
        l.opacidad(alfa);
      }
      return;
    }
  }
}

}  // namespace sintetica
