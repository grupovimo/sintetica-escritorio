// Texto que entra en la pantalla (en varias líneas, achicando la letra si hace
// falta) y el globo de diálogo del bicho: lo que dice (comando Decir, sus
// frases, el despertador) sale de su cara en un globo claro.
#pragma once

#include "lienzo.h"
#include "texto.h"

namespace sintetica {

constexpr int MAX_LINEAS_TEXTO = 4;
constexpr int LARGO_TEXTO_AJUSTADO = 64;

struct TextoAjustado {
  const Fuente* fuente = nullptr;
  char texto[LARGO_TEXTO_AJUSTADO] = {};  // ya en mayúsculas si la fuente es mono
  int n = 0;
  uint8_t ini[MAX_LINEAS_TEXTO] = {}, fin[MAX_LINEAS_TEXTO] = {};
  float top = 0, lh = 0;  // dónde empieza la primera línea y el alto de cada una
  float ancho = 0;        // el de la línea más larga
};

// Parte `s` en hasta `max` líneas, la i-ésima de `anchos[i]` píxeles como
// mucho. Sin `cortar`, solo entre palabras. false si no entra.
bool partirLineas(const Fuente& f, const char* s, const float* anchos, int max, bool cortar, TextoAjustado* a);

// Las fuentes mono solo tienen mayúsculas (con tildes y Ñ).
void mayusculasUtf8(char* s);

// Ajusta el texto centrado en cy: primero en la voz del bicho (Fraunces) en
// hasta maxLineas, entre palabras y si no cortando; si no entra, en mono
// mayúscula más chica. En la redonda, cada línea mide lo que deja el círculo
// a esa altura (menos `margen` a cada lado).
void ajustarTexto(const char* s, float cy, float anchoMax, int maxLineas, bool redonda, float W, float H, float margen,
                  TextoAjustado* a);
void dibujarAjustado(Lienzo& l, const TextoAjustado& a, float cx, uint32_t color);

// El globo: el texto ajustado sobre una píldora clara, con la colita hacia
// (colaX, colaY) (la boca del bicho). alfa = el fundido.
void dibujarGlobo(Lienzo& l, const char* s, float cx, float cy, float anchoMax, int maxLineas, bool redonda, float W,
                  float H, float colaX, float colaY, float alfa);

}  // namespace sintetica
