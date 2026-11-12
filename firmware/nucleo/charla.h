// La charla sin IA: entra lo que dijiste (ya en texto) y sale qué entendió
// (una intención), qué contesta (una frase de 60 bytes como mucho, con su
// expresión) y, si corresponde, una acción (poner una alarma, recordarte algo,
// marcar un hábito, jugar, cambiar de modo, guardar un gusto...).
//
// Las intenciones, sus patrones y sus respuestas son datos
// (shared/formato/charla/intenciones.json, embebidas en charla_base.h como
// tablas en la flash). El motor es una función pura: no usa memoria dinámica
// ni estado global; lo que recuerda entre frases (el contexto: "¿y mañana?"
// después del clima) va en un EstadoCharla chiquito que guarda quien llama.
// Lo usan el robot (comando 0x4C Charlar), la nube (cgo) y las apps (API C).
#pragma once

#include "formato.h"

namespace sintetica {

// El orden es el de ACCIONES_CHARLA (charla_base.h): un test lo verifica.
enum class AccionCharla : uint8_t {
  Ninguna, Alarma, CancelarAlarma, Posponer, ApagarAlarma, Recordatorio, Habito, Jugar, Modo, Gusto,
  OlvidarGusto, Toque, Musica, Dormir, Despertar, Dueno, Volumen, Callar, Animo, Cantidad
};
const char* nombreAccionCharla(AccionCharla a);

constexpr int LARGO_TEXTO_CHARLA = 41;   // 40 bytes + terminador (lo que entra en un recordatorio)
constexpr int MAX_ENTRADA_CHARLA = 200;  // lo que dijiste: lo que pase de acá se ignora
constexpr int MAX_GUSTOS = 4;
constexpr int LARGO_GUSTO = 25;          // 24 bytes + terminador
constexpr int LARGO_LISTA_GUSTOS = MAX_GUSTOS * LARGO_GUSTO;  // separados por '\n'

// Lo que sacó de la frase.
struct EntidadesCharla {
  int16_t minuto = -1;      // del día, ya resuelto (-1 = no dijo una hora)
  int16_t enMinutos = -1;   // "en 10 minutos" (-1 = no)
  int8_t dia = -1;          // días desde hoy: 0 hoy, 1 mañana... (-1 = no dijo)
  int8_t semana = -1;       // si dijo un día de la semana: 0 lunes ... 6 domingo
  uint8_t dias = 0;         // repetir: bit 0 lunes ... bit 6 domingo (0 = una vez)
  int8_t juego = -1, modo = -1, toque = -1, habito = -1;
  bool hayNumero = false;
  int32_t numero = 0;
  char texto[LARGO_TEXTO_CHARLA] = {};  // el recordatorio, el gusto, el nombre, a quién
};

// Lo que sabe quien llama (todo opcional).
struct ContextoCharla {
  const char* dueno = nullptr;
  const char* robot = nullptr;
  const char* preset = nullptr;     // la personalidad (presetCercano): elige las variantes
  int minuto = -1;                  // del día (-1 = no se sabe la hora)
  uint32_t epoch = 0;               // hora local, segundos desde 1970 (0 = no se sabe el día)
  const char* clima = nullptr;      // "11° y despejado" (textoClima)
  const char* climaManana = nullptr;
  const char* agenda = nullptr;     // "Dentista a las 15"
  const char* agendaManana = nullptr;
  const char* musica = nullptr;     // "Muchachos, de La Mosca"
  const char* habitos[MAX_HABITOS] = {};
  int nHabitos = 0;
  const char* gustos = nullptr;     // separados por '\n'
  int diasJuntos = -1;
  bool alarmaSonando = false;
  uint32_t segundos = 0;            // un reloj que avanza (el contexto dura CHARLA_CONTEXTO_S)
  uint32_t semilla = 0;             // para elegir la respuesta (misma semilla, misma respuesta)
};

// Lo que recuerda entre frases (unos 70 bytes): quien llama lo guarda y lo
// vuelve a pasar. Arranca vacío.
struct EstadoCharla {
  int16_t ultima = -1;      // la última intención (-1 = ninguna)
  uint8_t espera = 0;       // lo que preguntó (bits de "necesita": 1 hora, 2 texto, 4 juego, 8 hábito, 16 quién)
  uint16_t respuesta = 0xFFFF;  // la última respuesta (para no repetirla)
  uint32_t cuando = 0;      // ContextoCharla::segundos de la última
  EntidadesCharla e;        // lo que ya se sabía
};

struct RespuestaCharla {
  int16_t intencion = -1;   // en INTENCIONES_CHARLA (-1 = no entendió)
  AccionCharla accion = AccionCharla::Ninguna;
  // Jugar: el juego (0 ppt, 1 reflejos, 2 mirada, 3 dado, 4 moneda); Modo: 0
  // mascota, 1 reloj, 2 pomodoro, 3 clima; Toque: 0 toque, 1 abrazo, 2 beso,
  // 3 carita; Volumen: +1 o -1; Ánimo: 0 bien, 1 más o menos, 2 mal; Hábito:
  // su índice.
  int8_t dato = -1;
  EntidadesCharla e;
  const char* expresion = "";       // "" = la de hablar
  const char* comportamiento = "";  // "" = ninguno
  char frase[LARGO_FRASE] = {};
  uint8_t confianza = 0;            // 0-100
  bool noEntendi = false;  // no entendió (o es de las que contesta mejor la IA): si hay IA, que pruebe ella
  bool falta = false;      // preguntó lo que faltaba ("¿a qué hora?"): la acción queda para la próxima
  bool confirma = false;   // no estaba seguro y preguntó antes de hacerla ("¿Te llamás Zoe?"): con "sí", la hace
  const char* nombre() const;       // el de la intención ("" = no entendió)
};

// Charla: entiende `texto` (UTF-8, n bytes) y arma la respuesta. Actualiza
// `estado`. Siempre deja una frase (si no entendió, una con gracia).
void charlar(const char* texto, size_t n, const ContextoCharla& ctx, EstadoCharla& estado, RespuestaCharla* r);

// Lo que usa para comparar: minúsculas, sin tildes ni signos, las palabras
// separadas por un espacio y las abreviaturas ("q", "xq") ya expandidas.
size_t normalizarCharla(const char* texto, size_t n, char* salida, size_t cap);

// Cuánto se parecen dos palabras ya normalizadas (100 iguales, 0 nada): tolera
// un error de tipeo (dos en las largas) y raíces ("recorda*").
int parecidoCharla(const char* patron, size_t lp, bool raiz, const char* palabra, size_t lw);

int buscarIntencionCharla(const char* nombre);
const char* nombreIntencionCharla(int i);

// La lista de gustos ("el mate\nla pizza"): sumar uno (sin repetir; con más
// de MAX_GUSTOS se va el más viejo) o sacarlo. Devuelven el largo nuevo.
size_t sumarGusto(char* lista, size_t cap, const char* gusto);
size_t sacarGusto(char* lista, size_t cap, const char* gusto);

}  // namespace sintetica
