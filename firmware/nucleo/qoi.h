// Decodificador QOI ("Quite OK Image", https://qoiformat.org) que lee por
// partes y entrega de a una fila: la placa no necesita la imagen entera en RAM.
#pragma once

#include "mate.h"

namespace sintetica {

class DecodificadorQoi {
 public:
  // Lee `n` bytes desde `desplazamiento`; devuelve cuántos leyó (0 al final, -1 si falla).
  using Leer = int (*)(void* contexto, uint32_t desplazamiento, uint8_t* destino, size_t n);

  // Lee la cabecera y deja todo listo para la primera fila. false si no es QOI.
  bool empezar(Leer leer, void* contexto);
  uint32_t ancho() const { return ancho_; }
  uint32_t alto() const { return alto_; }
  uint32_t fila() const { return fila_; }  // la próxima fila a decodificar
  // Decodifica la próxima fila en `rgb565` (ancho elementos). false si los datos se cortan.
  bool siguienteFila(uint16_t* rgb565);
  // Lo mismo con la transparencia: ancho × 4 bytes (R, G, B, A sin premultiplicar).
  bool siguienteFilaRgba(uint8_t* rgba);
  // Los canales de la cabecera (3 o 4).
  uint8_t canales() const { return canales_; }
  // Salta filas (para empezar a mitad de pantalla).
  bool saltarFilas(uint32_t n);

  // Solo la cabecera (14 bytes): el tamaño de la imagen.
  static bool leerCabecera(const uint8_t* b, size_t n, uint32_t* ancho, uint32_t* alto);

 private:
  bool byte(uint8_t* b);
  bool pixel(uint8_t* r, uint8_t* g, uint8_t* bl);

  Leer leer_ = nullptr;
  void* contexto_ = nullptr;
  uint32_t ancho_ = 0, alto_ = 0, fila_ = 0;
  uint32_t posicion_ = 0;  // del próximo byte del archivo que hay que pedir
  uint8_t buf_[256];
  uint16_t enBuf_ = 0, leidoBuf_ = 0;
  uint8_t indice_[64][4] = {};
  uint8_t r_ = 0, g_ = 0, b_ = 0, a_ = 255;
  uint8_t racha_ = 0;
  uint8_t canales_ = 3;
};

// Una imagen animada ("sani", de un GIF): la cabecera, la tabla de cuadros y
// los cuadros en QOI uno detrás del otro, en el mismo archivo. Little endian:
//   0 "sani" · 4 versión (1) · 5 cuadros (1 a 64) · 6 ancho (u16) · 8 alto (u16)
//   10 + 10·i: el cuadro i: duración en ms (u16), desplazamiento (u32), largo (u32)
// Cada cuadro se decodifica igual que una imagen suelta, de a filas.
namespace animacion {
constexpr size_t CABECERA = 10;
constexpr size_t POR_CUADRO = 10;
constexpr int MAX_CUADROS = 64;
constexpr uint16_t MIN_MS = 20;

struct Cuadro {
  uint16_t ms = 0;
  uint32_t desplazamiento = 0, largo = 0;
};

inline bool es(const uint8_t* b, size_t n) { return n >= 4 && b[0] == 's' && b[1] == 'a' && b[2] == 'n' && b[3] == 'i'; }
// La cabecera (10 bytes). false si no es una animación que se entienda.
bool leerCabecera(const uint8_t* b, size_t n, int* cuadros, uint32_t* ancho, uint32_t* alto);
// Una entrada de la tabla (10 bytes).
Cuadro leerCuadro(const uint8_t* b);
}  // namespace animacion

}  // namespace sintetica
