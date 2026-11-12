// La cara paramétrica del motor de vida (docs/motor-de-vida.md): dos ojos, una
// boca opcional, cachetes y partículas, todo como funciones de distancia (SDF).
// Es el port del "Motor de caras v0" (docs/referencia/motor-de-caras-v0) a la
// CPU: misma matemática, mismos parámetros, dibujado por bloques para que en el
// ESP32 solo se calcule la SDF fina en los bordes.
//
// Coordenadas del rostro: el centro de la pantalla es (0, 0), y hacia arriba es
// positivo (como en el v0). La unidad es Vista::unidad píxeles.
#pragma once

#include "lienzo.h"
#include "mate.h"

namespace sintetica {

// Los parámetros, en un arreglo plano para que los resortes, el mezclador y el
// JSON los traten igual. Los del ojo van dos veces: izquierdo y derecho.
namespace param {
enum Ojo : uint8_t { W, H, R, TILT, LT, LTA, LB, LBA, SMILE, HEART, STAR, CROSS, RING, PUPIL, SQUASH, POR_OJO };
constexpr int IZQ = 0;
constexpr int DER = POR_OJO;
enum Global : uint8_t {
  GX = 2 * POR_OJO, GY, EY, SEP, FTILT, FSCALE, SPIN, ORBIT, PULSE, BEAT, GLITCH, DIM, HUE, MO, MW, MCURVE, BLUSH, SHAKE,
  CANTIDAD
};
}  // namespace param
constexpr int N_PARAMETROS = param::CANTIDAD;

// Cómo se mueve cada parámetro hacia su objetivo (la tabla de "Transiciones").
enum class GrupoResorte : uint8_t { Forma, Parpados, Mirada, Cara, Color, Cantidad };

struct InfoParametro {
  const char* nombre;  // la clave en el JSON (sin el ojo)
  float minimo, maximo, defecto;
  GrupoResorte grupo;
};
// Información del parámetro i (0 a N_PARAMETROS - 1).
const InfoParametro& infoParametro(int i);
// Índice por nombre: "w" da el del ojo (izquierdo; el derecho es + param::DER),
// "gx" el global. -1 si no existe. *esOjo dice cuál de las dos cosas es.
int parametroPorNombre(const char* nombre, bool* esOjo);

struct Rostro {
  float v[N_PARAMETROS];
  Rostro();  // la neutral del v0
  float& operator[](int i) { return v[i]; }
  float operator[](int i) const { return v[i]; }
  float ojo(int lado, int p) const { return v[lado * param::POR_OJO + p]; }
  // Lleva cada parámetro a su rango (los topes de la marca).
  void limitar();
};

// Partículas: formas chiquitas que nacen, flotan y se apagan.
enum class TipoParticula : uint8_t { Zzz, Corazon, Lagrima, Gota, Chispa, Pregunta, Exclamacion, Nota, Estrella, Burbuja, Oveja, Reloj, Garabato, Cantidad };
const char* nombreParticula(TipoParticula t);
bool particulaPorNombre(const char* n, TipoParticula* t);

constexpr int MAX_PARTICULAS = 6;
struct Particula {
  bool viva = false;
  TipoParticula tipo = TipoParticula::Zzz;
  float x = 0, y = 0, vx = 0, vy = 0;  // unidades y unidades por segundo
  float edad = 0, vida = 1;            // segundos
  float tamano = 0.05f;
  float fase = 0;
  bool enBurbuja = false;  // lo que se sueña: va adentro de la burbuja, con ella
};

// Todo lo que hace falta para dibujar un cuadro: los parámetros ya mezclados y
// lo que la micro-vida suma por fuera (respiración, aplastamiento, fases).
struct CuadroRostro {
  Rostro r;
  float escalaX = 1, escalaY = 1;  // respiración y anticipación
  float derivaX = 0, derivaY = 0;  // flotación
  float faseSpin = 0, faseOrbita = 0;
  float latido = 0;                // 0 a 1.65: el doble golpe del corazón
  float pulso = 1;                 // escala de alto por el sonido
  float glitch = 0;                // intensidad de este cuadro
  float semillaGlitch = 0;
  float progreso = 0.6f;           // el anillo de carga (0 a 1)
  float brillo = 1;                // el de la noche (lo baja el ritmo diario)
  uint32_t colorOjos = 0xFF6A33;
  uint32_t colorRojo = 0xFF3820;   // a donde va con hue = 1
  float tiempo = 0;                // segundos (temblor)
  Particula particulas[MAX_PARTICULAS];
};

// Dónde y de qué tamaño va el rostro en una pantalla.
struct VistaRostro {
  float unidad = 156;              // píxeles por unidad del rostro
  float origenX = 160, origenY = 86;
  bool halo = true;                // el resplandor alrededor de los ojos (el v0 lo tiene)
  // Rectangular: los ojos de la neutral (h = 0,23) miden 72 px de alto en la
  // 1.47B, como en el manual. Redonda (el broche): la unidad es el 60 % del
  // ancho, para que la cara llene el círculo sin tocar el borde.
  static VistaRostro para(int ancho, int alto, bool redonda, bool halo = true) {
    VistaRostro v;
    v.unidad = redonda ? ancho * 0.6f : ancho * 0.49f;
    v.origenX = ancho / 2.0f;
    v.origenY = alto / 2.0f;
    v.halo = halo;
    return v;
  }
};

// Cómo va la cara entera en un cuadro (inclinación, escala con la
// respiración, deriva y temblor): de un píxel a las coordenadas del rostro es
// p = rot(-ftilt) · ((px - origen) / unidad - (dx, dy)) / (ex, ey), con y hacia arriba.
struct MarcoRostro {
  float cRot = 1, sRot = 0;  // rot(-ftilt)
  float ex = 1, ey = 1;      // escalas de x e y (con fscale)
  float dx = 0, dy = 0;      // corrimiento (deriva y temblor)
};
MarcoRostro marcoRostro(const CuadroRostro& c);

class CacheSprites;

// Dibuja el cuadro en la franja actual del lienzo (limpia la franja antes).
// Con una cara de imagen (sprites cargados): los ojos y la boca de la SDF se
// apagan en `ocultarOjos` (0 a 1) y los sprites se dibujan encima con
// `opSprites` (0 a 1), antes de las partículas. Así se pasa de una a otra con
// un fundido, nunca con un corte.
void dibujarRostro(Lienzo& lienzo, const VistaRostro& vista, const CuadroRostro& c, const CacheSprites* sprites = nullptr,
                   float opSprites = 0, float ocultarOjos = 0);

// La distancia de la cara (los dos ojos y la boca) a un punto, en unidades.
// Negativa adentro. Para los tests y las herramientas.
float distanciaRostro(const CuadroRostro& c, float x, float y);

// Cuántos píxeles se evaluaron con la SDF fina en el último dibujo (para medir
// que el dibujo por bloques ahorra).
uint32_t pixelesFinosUltimoDibujo();

}  // namespace sintetica
