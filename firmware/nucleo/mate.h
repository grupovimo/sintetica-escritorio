// Matemática mínima y autocontenida: el núcleo no depende de libc para poder
// compilarse igual en el ESP32, en la PC y en WebAssembly sin biblioteca estándar.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sintetica {

constexpr float MEDIA_VUELTA = 3.14159265358979f;
constexpr float VUELTA = 6.28318530717959f;

inline float absf(float x) { return x < 0 ? -x : x; }
inline float minf(float a, float b) { return a < b ? a : b; }
inline float maxf(float a, float b) { return a > b ? a : b; }
inline int mini(int a, int b) { return a < b ? a : b; }
inline int maxi(int a, int b) { return a > b ? a : b; }
inline float limitar(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float mezclar(float a, float b, float t) { return a + (b - a) * t; }
inline float raiz(float x) { return x <= 0 ? 0 : __builtin_sqrtf(x); }
inline float piso(float x) { return __builtin_floorf(x); }
inline float techo(float x) { return __builtin_ceilf(x); }

// Seno con reducción de rango y polinomio de Taylor hasta x^9 (error < 1e-5).
inline float seno(float x) {
  x = x - VUELTA * piso((x + MEDIA_VUELTA) / VUELTA);  // a [-π, π)
  if (x > MEDIA_VUELTA / 2) x = MEDIA_VUELTA - x;
  if (x < -MEDIA_VUELTA / 2) x = -MEDIA_VUELTA - x;
  const float x2 = x * x;
  return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f * (1.0f - x2 / 72.0f))));
}
inline float coseno(float x) { return seno(x + MEDIA_VUELTA / 2); }

inline float fraccion(float x) { return x - piso(x); }
inline float signo(float x) { return x < 0 ? -1.0f : (x > 0 ? 1.0f : 0.0f); }
// smoothstep de GLSL.
inline float suaveEntre(float a, float b, float x) {
  const float t = limitar((x - a) / (b - a), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

// e^x: 2^(x·log2 e), con la parte entera en el exponente del float y la
// fraccionaria con un polinomio (error relativo < 2e-4, alcanza para colores y
// decaimientos). Para x < -80 devuelve 0.
inline float exponencial(float x) {
  if (x < -80.0f) return 0.0f;
  if (x > 80.0f) x = 80.0f;
  const float t = x * 1.44269504f;
  const float e = piso(t);
  const float f = t - e;  // [0, 1)
  const float p = 1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f + f * 0.00961813f)));
  union {
    float f;
    uint32_t u;
  } b;
  b.u = static_cast<uint32_t>(static_cast<int32_t>(e) + 127) << 23;
  return p * b.f;
}

// atan2(y, x) en radianes (error < 0,002 rad).
inline float arcotangente2(float y, float x) {
  const float ax = absf(x), ay = absf(y);
  if (ax < 1e-12f && ay < 1e-12f) return 0.0f;
  const float a = minf(ax, ay) / maxf(ax, ay);
  const float s = a * a;
  float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
  if (ay > ax) r = 1.57079637f - r;
  if (x < 0) r = 3.14159274f - r;
  if (y < 0) r = -r;
  return r;
}

// Hash de un real a [0, 1) (el del v0: fract(sin(n)·43758,5453), pero con
// enteros para que dé igual en todos lados).
inline float hashReal(float n) {
  uint32_t x = static_cast<uint32_t>(static_cast<int32_t>(n * 1000.0f));
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return (x >> 8) * (1.0f / 16777216.0f);
}

// Curva ease-in-out cúbica, la que pide el manual para el parpadeo.
inline float suave(float t) {
  t = limitar(t, 0.0f, 1.0f);
  if (t < 0.5f) return 4.0f * t * t * t;
  const float u = -2.0f * t + 2.0f;
  return 1.0f - u * u * u / 2.0f;
}

// Generador xorshift32: determinista con la misma semilla (los tests lo usan).
struct Azar {
  uint32_t estado = 0x5157E71Cu;
  void sembrar(uint32_t s) { estado = s ? s : 0x5157E71Cu; }
  uint32_t siguiente() {
    uint32_t x = estado;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    estado = x;
    return x;
  }
  // Entero en [lo, hi] incluido.
  uint32_t entre(uint32_t lo, uint32_t hi) { return lo + siguiente() % (hi - lo + 1); }
  // Real en [0, 1).
  float real() { return (siguiente() >> 8) * (1.0f / 16777216.0f); }
};

// Utilidades de bytes y texto sin libc.
inline void copiarBytes(void* dst, const void* src, size_t n) {
  auto* d = static_cast<uint8_t*>(dst);
  auto* s = static_cast<const uint8_t*>(src);
  for (size_t i = 0; i < n; i++) d[i] = s[i];
}
inline void llenarBytes(void* dst, uint8_t v, size_t n) {
  auto* d = static_cast<uint8_t*>(dst);
  for (size_t i = 0; i < n; i++) d[i] = v;
}
inline size_t largo(const char* s) {
  size_t n = 0;
  while (s && s[n]) n++;
  return n;
}
inline bool iguales(const char* a, const char* b) {
  if (!a || !b) return a == b;
  while (*a && *a == *b) a++, b++;
  return *a == *b;
}
// Compara los primeros n bytes (sin cortar en un terminador).
inline bool iguales(const char* a, const char* b, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (a[i] != b[i]) return false;
  return true;
}
// Copia con terminador; devuelve cuántos bytes copió (sin el terminador).
inline size_t copiarTexto(char* dst, size_t cap, const char* src, size_t n) {
  if (cap == 0) return 0;
  if (n > cap - 1) n = cap - 1;
  for (size_t i = 0; i < n; i++) dst[i] = src[i];
  dst[n] = 0;
  return n;
}
inline size_t copiarTexto(char* dst, size_t cap, const char* src) {
  return copiarTexto(dst, cap, src, largo(src));
}

}  // namespace sintetica
