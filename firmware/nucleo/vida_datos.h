// Los datos del motor de vida (config v2): personalidad, horario, expresiones
// paramétricas, comportamientos y las condiciones y acciones de las reglas.
// Es la traducción de shared/formato/config.schema.json (v2) con límites fijos.
#pragma once

#include "mate.h"


namespace sintetica {

struct Rostro;

constexpr int MAX_CARAS_VIDA = 48;
constexpr int MAX_AJUSTES = 28;
constexpr int MAX_COMPORTAMIENTOS = 75;  // 66 de base + 9 del usuario
constexpr int MAX_PASOS = 6;
constexpr int MAX_CUANDO = 3;
constexpr int LARGO_VIDA = 25;  // nombres: 24 bytes + terminador
constexpr int8_t DENTRO_SUENO = 100;  // "dentro": "sueno": un corazón, una estrella, una nota o una oveja

// Caras de imagen (docs/motor-de-vida.md, "Caras de imagen"): una expresión
// v2 que arma los ojos y la boca con sprites (QOI con transparencia, hasta
// 128 × 128) en vez de con la SDF. El motor la anima igual que a cualquier cara.
constexpr int MAX_SPRITES = 12;        // espacios de sprites (aparte de las 8 imágenes)
constexpr int LADO_SPRITE = 128;       // lo más grande que puede ser un sprite, por lado
constexpr int MAX_CARAS_IMAGEN = 8;    // expresiones de imagen en una config

// Las variables que leen las condiciones ("sueno>0.5", "rasgo.timidez>0.6").
enum class VarVida : uint8_t {
  // Estado interno (0 a 1).
  Energia, Animo, Afecto, Aburrimiento, Curiosidad, Sueno, Estres,
  // Personalidad (0 a 1).
  RasgoEnergia, RasgoSociabilidad, RasgoPicardia, RasgoTimidez, RasgoCuriosidad, RasgoMalGenio,
  // Contexto.
  Bateria,      // 0 a 100 (-1 si no hay)
  Hora,         // 0 a 24 (-1 si no se sabe)
  Humor,        // el de la v1: ánimo llevado a -1..1
  Ignorado,     // minutos desde la última interacción
  Ausencia,     // horas que estuvo sin verte antes de esta interacción
  Rencor,       // 1 si tiene un rencor vivo (lo sacudieron fuerte hace menos de un día)
  Dormido,      // 1 o 0
  Noche,        // 1 en el horario de dormir
  Cargando,     // 1 o 0
  // v3.
  Etapa,        // 0, 1, 2… (caracter.json "etapas"; nunca baja)
  Dias,         // días juntos
  Solo,         // 1 si no hay nadie (te fuiste, o hace caracter.json "solo_minutos" que nadie lo toca)
  Bajon,        // 0 a 1: cuánto anduvo bajoneado el dueño los últimos días
  Lluvia,       // 1 si llueve (el clima de la nube)
  Esperando,    // 1 si es la hora en que solés llegar y todavía no llegaste
  Cantidad
};
constexpr int N_ESTADO = 7;
constexpr int N_RASGOS = 6;
const char* nombreVarVida(VarVida v);
// Acepta "sueno", "estado.sueno", "rasgo.timidez", "bateria", ...
bool varVidaPorNombre(const char* n, size_t largo, VarVida* v);

struct CondVida {
  VarVida var = VarVida::Energia;
  enum class Op : uint8_t { Mayor, Menor, Verdad } op = Op::Mayor;  // Verdad: "dormido", "noche"
  float valor = 0;
};
// Una o varias condiciones separadas por coma: "sueno>0.5,noche".
// Devuelve cuántas leyó (0 = inválida).
int leerCondiciones(const char* s, CondVida* salida, int max);

struct Personalidad {
  float energia = 0.5f, sociabilidad = 0.5f, picardia = 0.45f, timidez = 0.3f, curiosidad = 0.55f, malGenio = 0.25f;
  float& operator[](int i) { return (&energia)[i]; }
  float operator[](int i) const { return (&energia)[i]; }
};
const char* nombreRasgo(int i);  // "energia", "sociabilidad", "picardia", "timidez", "curiosidad", "malGenio"
// Los presets de la especificación: timido, picaro, dormilon, carinoso, curioso, grunon, electrico.
bool presetPersonalidad(const char* nombre, Personalidad* p);

// Cada capa de movimiento propia de una expresión (la columna "Capa de movimiento").
enum class Movimiento : uint8_t {
  Ninguno, Rebote, Saltitos, Risa, MiradaLado, Salto, Encogerse, Dormir, Temblor, Resoplido, Lagrima,
  Achicarse, Picaro, Latido, Bamboleo, Escuchar, Hablar, Pensar, Duda, Acercarse, Suspiro, Quieto,
  Cabeceo, Carga, Orbita, Buscar, Caer, Glitch, Estirarse, Cantidad
};
const char* nombreMovimiento(Movimiento m);
bool movimientoPorNombre(const char* n, Movimiento* m);

enum class Parpadeo : uint8_t { Normal, Menos, Mas, No, Lento, Rapido, Doble, Cantidad };
const char* nombreParpadeo(Parpadeo p);
bool parpadeoPorNombre(const char* n, Parpadeo* p);

// Una expresión paramétrica: lo que cambia respecto de la neutral (o de la
// que extiende), una capa de movimiento y, opcional, una partícula.
// (En diezmilésimos: todos los rangos entran en ±3,2 y así ocupa 4 bytes.)
struct Ajuste {
  uint8_t param;
  int16_t diezmil;
  float valor() const { return diezmil / 10000.0f; }
  static Ajuste de(int p, float v) {
    return {static_cast<uint8_t>(p), static_cast<int16_t>(v * 10000.0f + (v < 0 ? -0.5f : 0.5f))};
  }
};
// Las partes de una cara de imagen (las claves de "sprites").
enum class ParteSprite : uint8_t { Ojo, OjoCerrado, OjoDer, OjoDerCerrado, Boca, Cantidad };
constexpr int N_PARTES_SPRITE = static_cast<int>(ParteSprite::Cantidad);
const char* nombreParteSprite(int parte);  // "ojo", "ojo_cerrado", "ojo_der", "ojo_der_cerrado", "boca"

// Una cara de imagen: qué sprite va en cada parte (-1 = no tiene) y su
// geometría, en diezmilésimos de unidad del rostro.
struct CaraImagen {
  int8_t sprite[N_PARTES_SPRITE] = {-1, -1, -1, -1, -1};
  bool espejar = true;     // sin ojo_der, el derecho es el izquierdo espejado
  int16_t tamano = 3600;   // el ancho de cada ojo
  int16_t bocaTamano = 0;  // el ancho de la boca (0 = a la escala de los ojos)
  int16_t bocaY = -3600;   // la altura de la boca (como la de la SDF: y hacia arriba)
  bool tiene(ParteSprite p) const { return sprite[static_cast<int>(p)] >= 0; }
};

struct CaraVida {
  char nombre[LARGO_VIDA] = {};
  uint8_t nAjustes = 0;
  Ajuste ajustes[MAX_AJUSTES];
  Movimiento movimiento = Movimiento::Ninguno;
  int8_t particula = -1;  // TipoParticula que va soltando (-1 = ninguna)
  Parpadeo parpadeo = Parpadeo::Normal;
  bool propia = false;    // vino de la config del usuario
  int8_t imagen = -1;     // una cara de imagen: su índice en DatosVida::imagenes
  // Arma el destino completo: la neutral + los ajustes.
  void aplicar(Rostro& r) const;
};

// A dónde mira durante un paso.
enum class Mirada : uint8_t {
  Libre, Frente, Arriba, Abajo, Izquierda, Derecha, Lado, Alrededor, Perdida, Reojo, Estimulo, Rem, Ovejas,
  Particula, Puerta,  // v3: sigue a la última partícula; mira hacia "la puerta" (te espera)
  Cantidad
};
const char* nombreMirada(Mirada m);
bool miradaPorNombre(const char* n, Mirada* m);

struct Paso {
  int8_t cara = -1;               // índice en Config::vida.caras (-1 = la neutral)
  uint16_t duraMin = 1000, duraMax = 1000;  // ms
  uint8_t vecesMin = 1, vecesMax = 1;
  Mirada mirada = Mirada::Libre;
  Parpadeo parpadeo = Parpadeo::Normal;
  int8_t particula = -1;
  uint8_t cuantas = 1;            // cuántas partículas suelta al empezar
  uint8_t pesoQ = 255;            // cuánto de la cara (0 a 255 = 0 a 1)
  char sonido[9] = {};
  int8_t estadoVar = -1;          // empujón al estado al empezar
  int8_t estadoCent = 0;          // en centésimos
  int8_t decir = -1;              // al empezar, pide una frase de esa Situacion (habla.h)
  int8_t dentro = -1;             // con "particula": "burbuja", lo que se sueña adentro (DENTRO_SUENO = al azar)
  float peso() const { return pesoQ / 255.0f; }
  float estadoDelta() const { return estadoCent / 100.0f; }
};

enum class TipoComportamiento : uint8_t { Ocioso, Reaccion, Rutina };

struct Comportamiento {
  char nombre[LARGO_VIDA] = {};
  TipoComportamiento tipo = TipoComportamiento::Ocioso;
  uint8_t nCuando = 0;
  CondVida cuando[MAX_CUANDO];
  int8_t necesidad = -1;          // VarVida que mide la necesidad (-1: la saca de "cuando")
  float base = 0.3f;              // puntaje sin condiciones
  int8_t afinidad[N_RASGOS] = {}; // peso de cada rasgo, -100..100 (centésimos)
  uint16_t enfriamiento = 60;     // s
  float probabilidad = 1;         // < 1: raro (se tira el dado cada vez que se lo considera)
  bool interrumpible = true;
  uint8_t nPasos = 0;
  Paso pasos[MAX_PASOS];
  bool propio = false;
};

// Los ajustes del motor de vida que vienen en la config.
struct DatosVida {
  uint32_t semilla = 0;           // 0 = la del hardware
  Personalidad personalidad;
  int16_t despierta = 8 * 60, duerme = 23 * 60 + 30;  // minutos del día
  char dueno[LARGO_VIDA] = {};
  int16_t cumpleDueno = -1;       // mes·100 + día (-1 = no se sabe)
  int16_t cumpleBicho = -1;
  float brilloMax = 1.0f;         // límites de la marca (brillo_max)
  uint8_t nCaras = 0;
  CaraVida caras[MAX_CARAS_VIDA];
  uint8_t nComportamientos = 0;
  Comportamiento comportamientos[MAX_COMPORTAMIENTOS];
  uint8_t nImagenes = 0;
  CaraImagen imagenes[MAX_CARAS_IMAGEN];
  int8_t reposo = -1;             // "cara": {"expresion": …}: la cara de siempre (-1 = la neutral)

  int indiceCara(const char* n) const;
  int indiceComportamiento(const char* n) const;
};

}  // namespace sintetica
