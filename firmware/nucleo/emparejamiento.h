// La pantalla de emparejamiento del manual (pantalla-emparejamiento.svg):
// el QR sobre papel claro a la izquierda; a la derecha la unidad en mono
// naranja, "Escaneame / con la app." en Fraunces y el estado en mono gris.
#pragma once

#include "lienzo.h"

namespace sintetica {

// Contenido del QR: "SINTETICA:V1/<id>/<código>", todo alfanumérico para que
// entre en un QR versión 2 (25 × 25 módulos).
constexpr const char* PREFIJO_QR = "SINTETICA:V1/";

// Rectangular: la del manual (320 × 172). Redonda (broche, 240 × 240): el QR
// centrado, la unidad arriba y "Escaneame" abajo, todo dentro del círculo.
void dibujarEmparejamiento(Lienzo& l, const char* serie, const char* contenidoQr, const char* estado,
                           bool redonda = false);

}  // namespace sintetica
