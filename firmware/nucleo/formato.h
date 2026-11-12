// El formato de configuración en memoria: caras, reglas y personalidad.
// Es la traducción directa de shared/formato/config.schema.json, con límites
// fijos para no usar memoria dinámica.
#pragma once

#include "habla.h"
#include "json.h"
#include "mate.h"
#include "vida_datos.h"

// Rearmar un objeto grande en su lugar, sin <new> (el núcleo no tiene libc):
// `x = T()` arma la temporal en la pila, y en la placa la pila de la tarea
// principal es de 8 KB (Config pesa ~110 KB). La etiqueta evita chocar con el
// placement new de la biblioteca estándar.
struct EnSuLugar {};
inline void* operator new(decltype(sizeof 0), void* lugar, EnSuLugar) noexcept { return lugar; }

namespace sintetica {

constexpr int MAX_PUNTOS = 12;
constexpr int MAX_BANDAS = 6;
constexpr int MAX_FORMAS = 12;
constexpr int MAX_EXPRESIONES = 24;
constexpr int MAX_REGLAS = 68;         // las del usuario (hasta 24) más las base (44)
constexpr int MAX_REGLAS_USUARIO = 24;
constexpr int MAX_ACCIONES = 6;
constexpr int MAX_Y = 2;               // condiciones extra de una regla ("y")
constexpr int LARGO_NOMBRE = 25;  // 24 bytes + terminador
constexpr int MAX_TOKENS_JSON = 2400;
constexpr size_t MAX_CONFIG_BYTES = 16384;
constexpr int MAX_LISTA = 16;
constexpr int MAX_IMAGENES = 8;
constexpr int MAX_ANIMACIONES = 6;
constexpr int MAX_PISTAS = 6;
constexpr int MAX_CLAVES = 8;
constexpr uint32_t MAX_DURACION_MS = 60000;
constexpr int MAX_RECORDATORIOS = 12;
constexpr int LARGO_RECORDATORIO = 41;  // 40 bytes UTF-8 + terminador
constexpr int LARGO_PALABRA = 17;       // 16 bytes UTF-8 + terminador

// Paleta de la marca, tema Pantalla (manual de marca, tokens.json).
namespace paleta {
constexpr uint32_t NARANJA = 0xFF6A33;
constexpr uint32_t NARANJA_PROFUNDO = 0xFF8A5C;
constexpr uint32_t PANTALLA = 0x0B0A09;
constexpr uint32_t TINTA = 0xF5F2EC;
constexpr uint32_t TINTA_SUAVE = 0xA39E96;
constexpr uint32_t LINEA = 0x2C2925;
}  // namespace paleta

enum class TipoForma : uint8_t { Pildora, Corte, Circulo, Arco, Corazon, Espiral, Poligono, Franjas };

struct Punto {
  float x, y;
};

struct Banda {
  float y, h, dx;
};

struct Forma {
  TipoForma tipo = TipoForma::Pildora;
  uint32_t color = paleta::NARANJA;
  bool acento = true;     // usa el color de ojos que elija el dueño
  bool fijo = false;      // no sigue mirada, respiración ni parpadeo
  bool relleno = true;
  bool progreso = false;
  int8_t audio = -1;
  float x = 0, y = 0, w = 0, h = 0, r = -1;  // r < 0: la mitad del lado corto
  float izq = 0, der = 0;
  float trazo = 0;
  float tamano = 1;
  float r0 = 0, paso = 0, giro = 0;
  uint8_t nPuntos = 0;
  union {
    Punto puntos[MAX_PUNTOS];
    Banda bandas[MAX_BANDAS];
  };
  Forma() : puntos{} {}
};

struct Expresion {
  char nombre[LARGO_NOMBRE] = {};
  bool parpadea = true;
  bool propia = false;  // vino de la config del usuario (no es una de las 16 base)
  uint8_t nFormas = 0;
  Forma formas[MAX_FORMAS];
};

enum class Evento : uint8_t {
  Tacto,
  DobleToque,
  Levantado,
  Sacudida,
  BocaAbajo,
  Vinculado,
  AppAbierta,
  Despertado,
  // v2 (motor de vida). Los números de arriba no cambian: viajan por BLE.
  Caricia,
  MuchosToques,
  SacudidaSuave,
  SacudidaFuerte,
  Inclinado,
  Caida,
  Voz,
  Nombre,
  Ruido,
  Musica,
  Silencio,
  Notificacion,
  Llamada,
  BateriaBaja,
  BateriaCritica,
  Enchufado,
  Desenchufado,
  Cargado,
  SinWifi,
  CambioDeCara,
  BichoCerca,
  Volvio,      // la primera interacción después de una ausencia (lo genera el motor)
  Hito,        // un hito de la relación (lo genera el motor)
  Recordatorio,  // se disparó un recordatorio (lo genera el sistema)
  Pomodoro,      // terminó un tramo del pomodoro (lo genera el sistema)
  Despertador,   // empezó a sonar el despertador (lo genera el sistema; v1.6)
  Juego,         // terminó una partida de un juego (v1.7)
  Animo,         // respondió el "¿cómo estás?" (v1.7)
  CaraDetectada,  // la cámara ve una cara después de un rato sin ver ninguna (v1.8)
  Oscuridad,      // se apagó la luz del cuarto (v1.8)
  ManoCerca,      // una mano cerca de la pantalla, sin tocarla (v1.8)
  ToqueRecibido,  // llegó un toque de un amigo (v1.9)
  DevolverToque,  // lo devolviste con un toque o una caricia (v1.9; lo avisa solo)
  Agenda,         // se viene un evento del calendario, en 0 a 15 min (v1.9)
  Llegaste,       // presencia: llegaste (v1.9)
  TeFuiste,       // presencia: te fuiste (v1.9)
  Habito,         // se marcó un hábito (v1.9; si fue en el bicho, lo avisa solo)
  Crecio,         // pasó a otra etapa (v3; lo genera el motor; si no hay regla, la de hito)
  Cantidad
};
// El evento "padre" (sacudida-fuerte → sacudida, caricia → tacto): si no hay
// una regla para el evento, se prueba con el padre. Cantidad = no tiene.
Evento eventoPadre(Evento e);
const char* nombreEvento(Evento e);
bool eventoPorNombre(const char* nombre, Evento* salida);

enum class Variable : uint8_t { Bateria, Humor };

struct Condicion {
  enum class Tipo : uint8_t { Evento, Menor, Mayor } tipo = Tipo::Evento;
  Evento evento = Evento::Tacto;
  Variable variable = Variable::Bateria;
  float valor = 0;
};

// Animaciones: cada pista es una forma que se mueve, escala, gira y cambia de
// color y opacidad con claves en el tiempo (keyframes) y una curva por tramo.
enum class Curva : uint8_t { Lineal, Suave, Entrada, Salida, Rebote, Paso };
float aplicarCurva(Curva c, float t);
bool curvaPorNombre(const char* nombre, Curva* salida);

struct Clave {
  uint16_t t = 0;                 // ms desde el comienzo
  Curva curva = Curva::Suave;     // cómo se va de esta clave a la siguiente
  bool hayColor = false;
  float x = 0, y = 0;             // corrimiento, en unidades normalizadas
  float escala = 1;
  float rotacion = 0;             // grados, en sentido horario
  float opacidad = 1;
  uint32_t color = 0;
};

struct Pista {
  Forma forma;
  bool hayCentro = false;  // centro de la escala y el giro; si no, el de la forma
  float cx = 0, cy = 0;
  uint8_t nClaves = 0;
  Clave claves[MAX_CLAVES];
};

struct Animacion {
  char nombre[LARGO_NOMBRE] = {};
  uint16_t duracion = 1000;
  bool repetir = false;
  int8_t sobre = -1;  // expresión que se dibuja abajo, viva (-1 = ninguna)
  uint8_t nPistas = 0;
  Pista pistas[MAX_PISTAS];
};

// Cómo queda una pista en el instante t.
struct Cuadro {
  float x = 0, y = 0, escala = 1, rotacion = 0, opacidad = 1;
  bool hayColor = false;
  uint32_t color = 0;
};
Cuadro cuadroDePista(const Pista& p, uint32_t t);

struct Accion {
  // Expresion: una reacción de unos segundos. Reflejo: inmediata (< 50 ms),
  // gana sobre todo. Comportamiento: lo empieza ya. Estado: empuja una
  // variable del estado interno. Particula: suelta una.
  // Decir: pide una frase de esa situación (habla.h; el sistema decide si habla).
  enum class Tipo : uint8_t { Expresion, Sonido, Led, Animacion, Reflejo, Comportamiento, Estado, Particula, Decir } tipo = Tipo::Expresion;
  char argumento[LARGO_NOMBRE] = {};
  int8_t indice = -1;     // la cara, el comportamiento, la partícula o la situación ya resueltos
  int8_t variable = -1;   // Estado: la VarVida
  int8_t cent = 0;        // Estado: cuánto, en centésimos (como los pasos; sin float, entra en el broche)
  float delta() const { return cent / 100.0f; }
};

struct Regla {
  Condicion si;
  CondVida y[MAX_Y];      // condiciones extra ("y": "afecto>0.6,noche")
  uint8_t nY = 0;
  uint8_t nAcciones = 0;
  Accion entonces[MAX_ACCIONES];
};

enum class Modo : uint8_t { Mascota, Reloj, Pomodoro, Clima, Notificaciones, Galeria };

// Lo que se muestra, en orden: un toque pasa al siguiente (en el broche) y la
// lista puede rotar sola.
struct ElementoLista {
  enum class Tipo : uint8_t { Cara, Imagen, Animacion, CaraVida } tipo = Tipo::Cara;
  uint8_t indice = 0;  // expresión (dibujo), espacio de imagen (0-7), animación o cara paramétrica
};

// Un aviso a una hora ("recordatorios"), compacto: el broche no tiene PSRAM.
struct Recordatorio {
  uint16_t minuto = 0;     // minuto del día (0-1439)
  uint8_t dias = 0x7F;     // bit 0 lunes … bit 6 domingo
  bool sonido = true;
  bool activo = true;
  int8_t caraVida = -1;    // la expresión (Config::Cara): índice en vida.caras
  int8_t caraDibujo = -1;  // o en expresiones
  char texto[LARGO_RECORDATORIO] = {};
};
// "dias": "LMXJV" → máscara (bit 0 lunes … bit 6 domingo); vacío = todos.
// false si tiene letras que no son de L M X J V S D o alguna repetida.
bool leerDias(const char* s, uint8_t* mascara);

// "voz": la guarda y la valida (los modelos con parlante llegan después).
struct Voz {
  enum class Tipo : uint8_t { Grave, Media, Aguda, Robot };
  uint8_t volumen = 70;    // centésimos (0 = silencio)
  uint8_t microfono = 60;  // centésimos
  Tipo tipo = Tipo::Media;
  bool conversar = false;
  char palabra[LARGO_PALABRA] = {};  // vacío = su nombre
};

// "pomodoro": los tramos, en minutos.
struct TramosPomodoro {
  uint8_t trabajo = 25;   // 1-120
  uint8_t descanso = 5;   // 1-60
};

// "protector": después de `espera` segundos sin que pase nada, muestra la
// imagen de ese espacio (pantalla completa, en loop).
struct Protector {
  int8_t imagen = -1;     // espacio 0-7; -1 = sin protector
  uint16_t espera = 10;   // segundos, 3-3600
};

struct Config {
  int version = 1;
  char nombre[LARGO_NOMBRE] = {};
  bool hayColorOjos = false;
  uint32_t colorOjos = paleta::NARANJA;
  float tamano = 1.0f;
  float energia = 0.5f;
  float timidez = 0.3f;
  int16_t suenoDesde = -1;  // minutos desde la medianoche; -1 = sin horario
  int16_t suenoHasta = -1;
  Modo modo = Modo::Mascota;
  uint8_t nExpresiones = 0;
  Expresion expresiones[MAX_EXPRESIONES];
  uint8_t nReglas = 0;
  Regla reglas[MAX_REGLAS];
  uint8_t nLista = 0;
  ElementoLista lista[MAX_LISTA];
  uint16_t rotarMinutos = 0;  // 0 = no rota sola
  uint16_t galeriaSegundos = 30;  // modo galería: cada cuánto pasa a la siguiente imagen
  uint8_t nAnimaciones = 0;
  Animacion animaciones[MAX_ANIMACIONES];
  DatosVida vida;
  uint8_t nRecordatorios = 0;
  Recordatorio recordatorios[MAX_RECORDATORIOS];
  Voz voz;
  TramosPomodoro pomodoro;
  Protector protector;
  Habla habla;
  Despertador despertador;
  Cuidado cuidado;
  Avisos avisos;
  Habitos habitos;     // v1.9
  Presencia presencia;  // v1.9
  FechasTuyas fechas;   // v3

  int indiceExpresion(const char* nombre) const;
  int indiceAnimacion(const char* nombre) const;
  // Una cara por nombre: un dibujo propio (expresión de formas que vino en la
  // config del usuario) gana; si no, la paramétrica; si no, el dibujo base.
  struct Cara {
    int8_t vida = -1;    // índice en vida.caras
    int8_t dibujo = -1;  // índice en expresiones
    bool existe() const { return vida >= 0 || dibujo >= 0; }
  };
  Cara resolverCara(const char* nombre) const;
};

enum class ErrorConfig : uint8_t {
  Ninguno = 0,
  JsonInvalido,
  Version,
  Campo,          // un campo con tipo o valor fuera de rango
  Forma,          // una forma desconocida o incompleta
  Demasiadas,     // superó un límite (formas, expresiones, reglas)
  Regla,
};

struct ResultadoConfig {
  ErrorConfig error = ErrorConfig::Ninguno;
  const char* detalle = "";
  bool ok() const { return error == ErrorConfig::Ninguno; }
};

// Carga las 16 caras base embebidas y las reglas del manual.
ResultadoConfig cargarBase(Config& c);

// Aplica una expresión suelta (un archivo de shared/formato/caras).
ResultadoConfig aplicarExpresion(Config& c, const char* json, size_t largo);

// Aplica una configuración de usuario encima de la que hay. Si falla, `c` queda
// como estaba (trabaja sobre una copia: es para las herramientas y los tests;
// el firmware no la usa, así esa copia no ocupa RAM en la placa).
#ifdef SINTETICA_HERRAMIENTAS
ResultadoConfig aplicarConfig(Config& c, const char* json, size_t largo);
#endif

// La base más una configuración de usuario, de cero, sin copias. Si falla,
// `destino` queda con la base (quien llama vuelve a cargar la anterior si la
// quiere: el sistema la lee de la flash).
ResultadoConfig cargarConfig(Config& destino, const char* json, size_t largo);

// Interpreta "#RRGGBB" o un token de la paleta. Devuelve false si no lo conoce.
bool interpretarColor(const char* s, uint32_t* rgb, bool* acento);

}  // namespace sintetica
