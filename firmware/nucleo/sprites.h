// Las caras de imagen (docs/motor-de-vida.md, "Caras de imagen"): los ojos y
// la boca son sprites (QOI con transparencia, hasta 128 × 128) que el motor de
// vida anima como a cualquier cara: parpadean, miran, respiran, tiemblan, se
// inclinan, se apagan de noche y entran y salen con un fundido.
//
// En la placa no se decodifica cada cuadro: al empezar a mostrar una cara de
// imagen, sus partes se decodifican una vez, achicadas a lo que se ve en esa
// pantalla, en un buffer fijo (CacheSprites). Se guardan en RGB565
// premultiplicado por la transparencia y se dibujan "aclarando" (el máximo por
// canal): sobre la pantalla casi negra es lo mismo que mezclar con la
// transparencia y cuesta la mitad de memoria que guardarla (ver la medición
// en docs/motor-de-vida.md).
#pragma once

#include "lienzo.h"
#include "rostro.h"
#include "vida_datos.h"

namespace sintetica {

// Cuántos píxeles entran en el buffer de los sprites de la cara que se ve (2
// bytes cada uno). Es un buffer interno: cada perfil lo puede achicar (el
// broche, sin PSRAM) y las partes se achican para entrar; no cambia qué
// config se acepta.
#ifndef SINTETICA_CACHE_SPRITES_PX
#define SINTETICA_CACHE_SPRITES_PX 16384
#endif
constexpr int CACHE_SPRITES_PX = SINTETICA_CACHE_SPRITES_PX;

// Lo más grande que puede pesar un sprite: el peor caso de un QOI de 4 canales.
constexpr uint32_t LIMITE_SPRITE = static_cast<uint32_t>(LADO_SPRITE) * LADO_SPRITE * 5 + 22;
// El primer espacio de la característica Imagen que es un sprite (8 a 19).
constexpr int PRIMER_SPRITE = 8;

// Un QOI de 3 o 4 canales de 1 a 128 por lado (la cabecera, 14 bytes).
bool cabeceraSprite(const uint8_t* b, size_t n, uint32_t* ancho, uint32_t* alto);

// Lee n bytes del sprite `sprite` (0 a 11) desde `desplazamiento`; devuelve
// cuántos leyó (0 al final, -1 si no está).
using LeerSprite = int (*)(void* contexto, int sprite, uint32_t desplazamiento, uint8_t* destino, size_t n);

class CacheSprites {
 public:
  struct Parte {
    uint16_t ancho = 0, alto = 0;              // en el buffer
    uint16_t anchoOriginal = 0, altoOriginal = 0;  // el del archivo
    uint32_t inicio = 0;                       // dónde empieza en el buffer
    bool hay() const { return ancho > 0; }
  };

  // Decodifica las partes de `cara`, achicadas a lo que se ve a `unidad`
  // píxeles por unidad del rostro (y a lo que entra en el buffer). false si no
  // se pudo leer el ojo (las otras partes que no estén se ignoran).
  bool cargar(const CaraImagen& cara, float unidad, LeerSprite leer, void* contexto);
  void vaciar() { listo_ = false; }
  bool listo() const { return listo_; }
  const CaraImagen& cara() const { return cara_; }
  const Parte& parte(ParteSprite p) const { return partes_[static_cast<int>(p)]; }
  const uint16_t* pixeles() const { return px_; }
  uint32_t usados() const { return usados_; }
  // Unidades del rostro por píxel original del ojo (la escala de la cara).
  float escala() const { return escala_; }

 private:
  uint16_t px_[CACHE_SPRITES_PX];
  Parte partes_[N_PARTES_SPRITE];
  CaraImagen cara_;
  float escala_ = 0;
  uint32_t usados_ = 0;
  bool listo_ = false;
};

// Dibuja la cara de imagen en la franja actual, encima de lo que hay, con la
// geometría del cuadro: la posición y el tamaño de los ojos (sep, gx, gy, ey,
// w, h, squash, órbita, latido), los párpados (lt, lb: el parpadeo usa
// ojo_cerrado si está; si no, aplasta el ojo), la inclinación, la respiración,
// el temblor, el glitch, el brillo (dim, hue, la noche) y la boca (mo, mw).
void dibujarSprites(Lienzo& lienzo, const VistaRostro& vista, const CuadroRostro& c, const CacheSprites& s, float opacidad);

}  // namespace sintetica
