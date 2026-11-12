// Dibuja una expresión en el lienzo con el estado vivo del momento: cuánto
// está cerrado el parpadeo, hacia dónde mira, la respiración, el color y el
// tamaño que eligió el dueño.
#pragma once

#include "formato.h"
#include "lienzo.h"

namespace sintetica {

// Cómo se pasa de coordenadas normalizadas a píxeles en una pantalla.
struct Vista {
  float escala;       // píxeles por unidad normalizada, para los ojos
  float origenX;      // píxel del centro (0, 0)
  float origenY;
  float escalaFijos;  // para los indicadores (zZ, batería, puntos, barras)
  bool redonda;

  // Rectangular (la de referencia del manual, 320 × 172, y cualquier otra):
  // escala = ancho / 2. Redonda (el broche, 240 × 240): los ojos un 50% más
  // grandes para llenar el círculo, y los indicadores más chicos para que
  // queden adentro.
  static Vista para(int ancho, int alto, bool redonda = false) {
    const float base = ancho / 2.0f;
    if (!redonda) return {base, base, alto / 2.0f, base, false};
    return {base * 1.5f, base, alto / 2.0f, base * 0.8f, true};
  }
};

struct EstadoCara {
  float cierre = 0;          // 0 abierto, 1 cerrado (parpadeo)
  float miradaX = 0;         // corrimiento de los ojos, en unidades normalizadas
  float miradaY = 0;
  float respiracion = 0;     // -1..1, se aplica como una escala muy chica
  float tamano = 1;
  bool hayColorOjos = false;
  uint32_t colorOjos = paleta::NARANJA;
  float audio[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  float progreso = 0.6176f;  // el valor del manual (84 de 136)
};

// El alto al que llega un ojo cerrado: 8 px en la pantalla de referencia.
constexpr float ALTO_CERRADO = 8.0f / 160.0f;
constexpr float AMPLITUD_RESPIRACION = 0.015f;

void dibujarExpresion(Lienzo& lienzo, const Vista& vista, const Expresion& e, const EstadoCara& estado);

// Un cuadro de una animación, t en ms (se recorta a la duración). Usa la
// escala de los ojos de la vista.
void dibujarAnimacion(Lienzo& lienzo, const Vista& vista, const Animacion& a, uint32_t t, const EstadoCara& estado);

// Caja de una forma en coordenadas normalizadas (para agrupar los ojos).
void cajaDeForma(const Forma& f, float* x0, float* y0, float* x1, float* y1);

}  // namespace sintetica
