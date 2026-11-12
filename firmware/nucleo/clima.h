// El clima que manda la nube (comando Clima, 0x41) y cómo se muestra: un ícono
// simple dibujado (sol, luna, nubes, lluvia, tormenta, nieve, niebla), la
// temperatura grande y la cara reaccionando (frío: tiembla; lluvia: triste;
// sol: contento).
#pragma once

#include "lienzo.h"
#include "habla.h"

namespace sintetica {

constexpr uint32_t CLIMA_VALE_MS = 3u * 3600u * 1000u;  // después de 3 h sin datos nuevos, "--°"
constexpr int LARGO_CIUDAD = 25;                       // 24 bytes + terminador

struct DatosClima {
  bool hay = false;
  int16_t decimas = 0;   // temperatura actual en décimas de °C
  uint8_t codigo = 0;    // código WMO (el de Open-Meteo)
  int8_t maxima = 0, minima = 0;
  bool dia = true;
  char ciudad[LARGO_CIUDAD] = {};
  uint32_t recibido = 0;  // ms (Hal::ahoraMs)
  int temperatura() const { return decimas >= 0 ? (decimas + 5) / 10 : -((-decimas + 5) / 10); }
};

// [i16 décimas][u8 código][i8 máx][i8 mín][u8 flags: bit 0 de día][n][ciudad]. false si está mal.
bool leerClima(const uint8_t* d, size_t n, DatosClima* c);

enum class Cielo : uint8_t { Despejado, Parcial, Nublado, Niebla, Llovizna, Lluvia, Nieve, Tormenta };
Cielo cieloDe(uint8_t codigoWmo);
const char* descripcionCielo(Cielo c);  // "despejado", "algo nublado", "nublado", "niebla", …

// "18° y nublado" (o "" sin datos).
size_t textoClima(const DatosClima& c, char* salida, size_t cap);
// "18°"
size_t textoTemperatura(int grados, char* salida, size_t cap);

// Cómo reacciona la cara (nombre de una expresión, o nullptr) y qué dice
// (Situacion::Cantidad = nada).
const char* caraClima(const DatosClima& c);
Situacion situacionClima(const DatosClima& c);

// Dibuja el ícono centrado en (cx, cy), de `tam` píxeles de lado; t en segundos
// (las gotas caen, el sol gira despacio, la niebla se corre) y alfa, la
// opacidad de todo (el fundido del panel; la deja así al terminar).
void dibujarIconoClima(Lienzo& l, Cielo cielo, bool dia, float cx, float cy, float tam, float t, float alfa = 1);

}  // namespace sintetica
