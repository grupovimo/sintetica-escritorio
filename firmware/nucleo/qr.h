// Codificador de códigos QR (ISO/IEC 18004), sin dependencias ni memoria
// dinámica. Alcanza para lo que muestra el bicho: textos cortos, nivel de
// corrección M, versiones 1 a 6 (de 21 × 21 a 41 × 41 módulos).
#pragma once

#include "mate.h"

namespace sintetica {

class Qr {
 public:
  static constexpr int MAX_LADO = 41;

  // Codifica el texto (modo alfanumérico si se puede, si no, byte). Devuelve
  // false si no entra en una versión 6-M (106 bytes / 154 alfanuméricos).
  bool codificar(const char* texto);

  int lado() const { return lado_; }
  int version() const { return version_; }
  int mascara() const { return mascara_; }
  bool oscuro(int x, int y) const { return modulos_[y][x] & 1; }

 private:
  void ponerFuncion(int x, int y, bool oscuro);
  void patronesDeFuncion();
  void formato(int mascara);
  void ubicarDatos(const uint8_t* datos, int n);
  void aplicarMascara(int mascara);
  long penalidad() const;

  int lado_ = 0, version_ = 0, mascara_ = 0;
  // bit 0: oscuro; bit 1: módulo de función (no se enmascara)
  uint8_t modulos_[MAX_LADO][MAX_LADO] = {};
};

}  // namespace sintetica
