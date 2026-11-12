#include "globo.h"

#include "fuentes.h"

namespace sintetica {

namespace {
float anchoTramo(const Fuente& f, const char* s, int a, int b) {
  char t[LARGO_TEXTO_AJUSTADO];
  copiarTexto(t, sizeof t, s + a, static_cast<size_t>(b - a));
  return anchoTexto(f, t);
}

constexpr uint32_t FONDO_GLOBO = paleta::TINTA;
constexpr uint32_t LETRA_GLOBO = paleta::PANTALLA;
constexpr float RELLENO_X = 10, RELLENO_Y = 6;
}  // namespace

bool partirLineas(const Fuente& f, const char* s, const float* anchos, int max, bool cortar, TextoAjustado* a) {
  const int n = static_cast<int>(largo(s));
  int pos = 0;
  a->n = 0;
  while (true) {
    while (pos < n && s[pos] == ' ') pos++;
    if (pos >= n) break;
    if (a->n >= max) return false;
    const float w = anchos[a->n];
    int fin = pos, k = pos;
    while (k < n) {
      int e = k;
      while (e < n && s[e] != ' ') e++;
      if (anchoTramo(f, s, pos, e) > w) break;
      fin = e;
      k = e;
      while (k < n && s[k] == ' ') k++;
    }
    if (fin == pos) {
      // Una palabra que no entra: se corta por letras (sin partir una tilde).
      if (!cortar) return false;
      int e = pos;
      while (e < n) {
        int sig = e + 1;
        while (sig < n && (static_cast<uint8_t>(s[sig]) & 0xC0) == 0x80) sig++;
        if (e > pos && anchoTramo(f, s, pos, sig) > w) break;
        e = sig;
      }
      fin = e;
    }
    a->ini[a->n] = static_cast<uint8_t>(pos);
    a->fin[a->n] = static_cast<uint8_t>(fin);
    a->n++;
    pos = fin;
  }
  return a->n > 0;
}

void mayusculasUtf8(char* s) {
  for (; *s; s++) {
    const uint8_t c = static_cast<uint8_t>(*s);
    if (c >= 'a' && c <= 'z') *s = static_cast<char>(c - 32);
    else if (c == 0xC3 && s[1] && static_cast<uint8_t>(s[1]) >= 0xA0 && static_cast<uint8_t>(s[1]) <= 0xBE &&
             static_cast<uint8_t>(s[1]) != 0xB7)
      s[1] = static_cast<char>(static_cast<uint8_t>(s[1]) - 0x20), s++;
  }
}

void ajustarTexto(const char* s, float cy, float anchoMax, int maxLineas, bool redonda, float W, float H, float margen,
                  TextoAjustado* a) {
  struct Intento {
    const Fuente* f;
    int max;
    bool cortar, mayus;
  };
  if (maxLineas > MAX_LINEAS_TEXTO) maxLineas = MAX_LINEAS_TEXTO;
  const Intento intentos[] = {{&fuentes::VOZ_20, maxLineas, false, false},
                              {&fuentes::VOZ_20, maxLineas, true, false},
                              {&fuentes::MONO_13, maxLineas + 1, true, true},
                              {&fuentes::MONO_9, MAX_LINEAS_TEXTO, true, true}};
  for (const Intento& in : intentos) {
    copiarTexto(a->texto, sizeof a->texto, s);
    if (in.mayus) mayusculasUtf8(a->texto);
    a->fuente = in.f;
    a->lh = static_cast<float>(in.f->ascenso + in.f->descenso);
    const int maxN = in.max > MAX_LINEAS_TEXTO ? MAX_LINEAS_TEXTO : in.max;
    for (int n = 1; n <= maxN; n++) {
      float anchos[MAX_LINEAS_TEXTO];
      a->top = cy - n * a->lh / 2;
      for (int i = 0; i < n; i++) {
        anchos[i] = anchoMax;
        if (!redonda) continue;
        const float y0 = a->top + i * a->lh, y1 = y0 + a->lh, r = W / 2 - 6;
        const float dy = maxf(absf(y0 - H / 2), absf(y1 - H / 2));
        anchos[i] = minf(anchoMax, 2 * raiz(maxf(0, r * r - dy * dy)) - 2 * margen);
      }
      if (partirLineas(*in.f, a->texto, anchos, n, in.cortar, a)) {
        a->ancho = 0;
        for (int i = 0; i < a->n; i++) a->ancho = maxf(a->ancho, anchoTramo(*in.f, a->texto, a->ini[i], a->fin[i]));
        return;
      }
    }
  }
  // Ni así: lo que entre en una línea de la más chica.
  a->n = 1, a->ini[0] = 0, a->fin[0] = static_cast<uint8_t>(largo(a->texto));
  a->top = cy - a->lh / 2;
  a->ancho = anchoMax;
}

void dibujarAjustado(Lienzo& l, const TextoAjustado& a, float cx, uint32_t color) {
  for (int i = 0; i < a.n; i++) {
    char linea[LARGO_TEXTO_AJUSTADO];
    copiarTexto(linea, sizeof linea, a.texto + a.ini[i], static_cast<size_t>(a.fin[i] - a.ini[i]));
    dibujarTexto(l, *a.fuente, linea, cx - anchoTexto(*a.fuente, linea) / 2, a.top + i * a.lh + a.fuente->ascenso, color);
  }
}

void dibujarGlobo(Lienzo& l, const char* s, float cx, float cy, float anchoMax, int maxLineas, bool redonda, float W,
                  float H, float colaX, float colaY, float alfa) {
  TextoAjustado a;
  ajustarTexto(s, cy, anchoMax - 2 * RELLENO_X, maxLineas, redonda, W, H, RELLENO_X + 4, &a);
  const float w = a.ancho + 2 * RELLENO_X, h = a.n * a.lh + 2 * RELLENO_Y;
  const float r = minf(14, h / 2);
  l.opacidad(alfa);
  // La colita: un triángulo desde el borde de arriba hacia la boca.
  const float arriba = cy - h / 2 + 1;
  const float bx = limitar(colaX, cx - w / 2 + r + 6, cx + w / 2 - r - 6);
  const float puntaY = maxf(colaY, arriba - 14);
  const Punto cola[3] = {{bx - 7, arriba + 2}, {bx + 7, arriba + 2}, {mezclar(bx, colaX, 0.5f), puntaY}};
  if (puntaY < arriba - 2) l.poligono(cola, 3, FONDO_GLOBO);
  l.pildora(cx, cy, w, h, r, FONDO_GLOBO);
  dibujarAjustado(l, a, cx, LETRA_GLOBO);
}

}  // namespace sintetica
