#include "emparejamiento.h"

#include "formato.h"
#include "fuentes.h"
#include "qr.h"
#include "texto.h"

namespace sintetica {

namespace {
Qr g_qr;
char g_qrContenido[48];  // el último texto codificado: no se recodifica en cada franja
bool g_qrOk = false;
}

namespace {
void dibujarQr(Lienzo& l, int panelX, int panelY, int panel, const char* contenido) {
  l.pildora(panelX + panel / 2.0f, panelY + panel / 2.0f, panel, panel, 8, paleta::TINTA);
  if (!iguales(contenido, g_qrContenido)) {
    copiarTexto(g_qrContenido, sizeof g_qrContenido, contenido);
    g_qrOk = g_qr.codificar(contenido);
  }
  if (!g_qrOk) return;
  const int lado = g_qr.lado();
  const int modulo = maxi(1, (panel - 16) / lado);  // deja al menos 2 módulos de margen
  const int x0 = panelX + (panel - lado * modulo) / 2;
  const int y0 = panelY + (panel - lado * modulo) / 2;
  for (int y = 0; y < lado; y++)
    for (int x = 0; x < lado; x++)
      if (g_qr.oscuro(x, y)) l.rectangulo(x0 + x * modulo, y0 + y * modulo, modulo, modulo, paleta::PANTALLA);
}

void textoUnidad(char* destino, size_t cap, const char* serie) {
  copiarTexto(destino, cap, "UNIDAD #");
  size_t n = largo(destino);
  copiarTexto(destino + n, cap - n, (serie && serie[0]) ? serie : "—");
}

void centrado(Lienzo& l, const Fuente& f, const char* texto, float cx, float base, uint32_t color) {
  dibujarTexto(l, f, texto, cx - anchoTexto(f, texto) / 2, base, color);
}
}  // namespace

void dibujarEmparejamiento(Lienzo& l, const char* serie, const char* contenidoQr, const char* estado, bool redonda) {
  l.limpiar(paleta::PANTALLA);
  char unidad[32];
  textoUnidad(unidad, sizeof unidad, serie);
  if (redonda) {
    // Diseño propio para el broche, con las mismas piezas que el del manual.
    const float cx = l.ancho() / 2.0f;
    const int panel = l.ancho() / 2;
    dibujarQr(l, static_cast<int>(cx) - panel / 2, l.alto() / 2 - panel / 2 - 4, panel, contenidoQr);
    centrado(l, fuentes::MONO_13, unidad, cx, l.alto() * 0.20f, paleta::NARANJA);
    centrado(l, fuentes::VOZ_20, "Escaneame", cx, l.alto() * 0.84f, paleta::TINTA);
    centrado(l, fuentes::MONO_9, estado, cx, l.alto() * 0.92f, paleta::TINTA_SUAVE);
    return;
  }
  // Medidas del manual, para la pantalla de 320 × 172.
  constexpr int PANEL_X = 20, PANEL_Y = 22, PANEL = 128;
  dibujarQr(l, PANEL_X, PANEL_Y, PANEL, contenidoQr);

  constexpr float TEXTO_X = 164;
  dibujarTexto(l, fuentes::MONO_13, unidad, TEXTO_X, 44, paleta::NARANJA);
  dibujarTexto(l, fuentes::VOZ_20, "Escaneame", TEXTO_X, 82, paleta::TINTA);
  dibujarTexto(l, fuentes::VOZ_20, "con la app.", TEXTO_X, 106, paleta::TINTA);
  dibujarTexto(l, fuentes::MONO_9, estado, TEXTO_X, 142, paleta::TINTA_SUAVE);
}

}  // namespace sintetica
