// La voz propia del robot: un balbuceo de robot (como el "animalese") que
// sigue el texto del globo letra por letra. Cada vocal es una sílaba con su
// tono (que sube y baja según la letra, el ánimo y la personalidad) y sus
// formantes (dos resonadores enteros por vocal); las consonantes son ruido,
// chasquidos o un zumbido nasal. Todo con enteros (Q14) y tablas: sin libc ni
// float por muestra, igual en la placa, el simulador, las apps y la nube.
//
// Sale PCM de 16 bits, 16 kHz, mono, de a bloques (generar). Con el mismo
// texto y los mismos parámetros, las mismas muestras.
#pragma once

#include "vida_datos.h"

namespace sintetica {

constexpr int MUESTRAS_POR_SEGUNDO_VOZ = 16000;

struct ParamsVoz {
  uint16_t tono = 185;       // Hz de la voz (el de cada sílaba varía alrededor)
  uint8_t velocidad = 100;   // centésimos (100 = normal; de 50 a 200)
  uint8_t volumen = 70;      // 0-100
  uint8_t variacion = 50;    // cuánto sube y baja el tono entre sílabas (0-100)
  bool robot = false;        // onda cuadrada y casi sin melodía
};

// Los parámetros según la voz de la config (0 grave, 1 media, 2 aguda,
// 3 robot), el volumen (0-100), la personalidad y el estado (energía y
// ánimo, de 0 a 1).
ParamsVoz paramsVoz(int tipo, int volumen, const Personalidad& p, float energia, float animo);

class VozPropia {
 public:
  // Empieza a decir `texto` (UTF-8, hasta el terminador o 255 bytes). El
  // texto tiene que seguir vivo mientras habla (el del globo lo está).
  void empezar(const char* texto, const ParamsVoz& p);
  void parar() { texto_ = nullptr; }
  bool hablando() const { return texto_ != nullptr; }
  // Llena hasta n muestras; devuelve cuántas (menos que n: terminó).
  size_t generar(int16_t* muestras, size_t n);
  // Si ahora suena una vocal (para mover la boca).
  bool vocal() const { return texto_ && tipo_ == Vocal; }
  // Cuántas muestras va a durar todo (para los tests y para quien quiera saberlo).
  static uint32_t duracion(const char* texto, const ParamsVoz& p);

 private:
  enum Tipo : uint8_t { Silencio, Vocal, Sonora, Ruido, Explosiva };
  bool siguiente();
  void segmento(Tipo t, uint32_t ms, int formante, int amplitud);

  const char* texto_ = nullptr;
  uint8_t pos_ = 0, largo_ = 0;
  ParamsVoz p_;
  Tipo tipo_ = Silencio;
  uint32_t resta_ = 0, total_ = 0;
  uint32_t fase_ = 0, paso_ = 0;
  int16_t c_[6] = {};        // b1, b2 y ganancia de los dos formantes
  int32_t ya1_ = 0, ya2_ = 0, yb1_ = 0, yb2_ = 0;
  uint32_t ruido_ = 0x5157E71Cu;
  int32_t anteriorRuido_ = 0;
  int16_t amplitud_ = 0;     // 0-256
  uint16_t silaba_ = 0, silabas_ = 0;
  bool pregunta_ = false, exclama_ = false;
};

}  // namespace sintetica
