// Texto con fuentes de bitmap (4 bits de antialias), para la pantalla de
// emparejamiento y los indicadores. Los atlas se generan en fuentes.h.
#pragma once

#include "lienzo.h"
#include "mate.h"

namespace sintetica {

struct Glifo {
  uint16_t codigo;   // punto de código Unicode
  int8_t x, y;       // esquina del bitmap respecto del origen en la línea de base
  uint8_t w, h;
  uint16_t avance;   // en 1/16 de píxel
  uint32_t datos;    // desplazamiento en el arreglo de datos (2 píxeles por byte)
};

struct Fuente {
  const Glifo* glifos;
  uint16_t cantidad;
  const uint8_t* datos;
  float tracking;    // píxeles extra entre letras
  uint8_t ascenso, descenso;
};

// Ancho en píxeles de un texto UTF-8 (sin el tracking después de la última letra).
float anchoTexto(const Fuente& f, const char* utf8);

// Dibuja desde (x, base) y devuelve dónde termina. Los caracteres que la fuente
// no tiene se saltean con el ancho de un espacio.
float dibujarTexto(Lienzo& l, const Fuente& f, const char* utf8, float x, float base, uint32_t rgb);

// Lo mismo, agrandado `escala` veces (con interpolación bilineal): la hora del
// reloj y la cuenta del pomodoro. Solo recorre las filas de la franja actual.
float dibujarTextoEscalado(Lienzo& l, const Fuente& f, const char* utf8, float x, float base, float escala, uint32_t rgb);

// Lee el próximo punto de código de un texto UTF-8 y avanza el puntero.
uint32_t siguienteCodigo(const char** p);

}  // namespace sintetica
