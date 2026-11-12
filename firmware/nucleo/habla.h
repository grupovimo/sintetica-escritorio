// Que el bicho hable solo: frases cortas según el momento y su carácter, en un
// globo de texto. Las frases son datos (shared/formato/vida/frases.json,
// embebidas en vida_base.h como tabla, en la flash); el usuario suma las suyas
// en la config ("habla"). El motor de vida pide una situación (la acción
// "decir:<situación>" de las reglas, el campo "decir" de los pasos, los hitos)
// y el Hablador decide si de verdad habla: con probabilidad, enfriamiento por
// situación, separación mínima y un tope por día, para no ser pesado.
#pragma once

#include "json.h"
#include "mate.h"
#include "vida_datos.h"

namespace sintetica {

constexpr int LARGO_FRASE = 61;          // 60 bytes UTF-8 + terminador (lo mismo que el comando Decir)
constexpr int MAX_FRASES_PROPIAS = 12;   // en total, entre todas las situaciones

// Los nombres son los de frases.json y los de "habla.propias" (ASCII).
enum class Situacion : uint8_t {
  Despertar, BuenasNoches, Volviste, TeExtrane, Aburrido, Sueno, BateriaBaja, Cargando, Caricia, Saludo,
  Hito, CumpleDueno, CumpleRobot, RecordatorioHecho, Despertador, Posponer,
  ClimaFrio, ClimaCalor, ClimaLluvia, ClimaLindo,
  // v1.7: el día con vos, cuidarte, juegos y fechas.
  BuenDia, Comiste, ADormir, Tardaste, Agua, Pausa, ComoEstas, AnimoBien, AnimoMedio, AnimoMal,
  Jugar, Gane, Perdi, Empate, AnioNuevo, Navidad, DiaDelAmigo, DiaDelNino, Aniversario,
  // v1.9: amigos, agenda, presencia, hábitos, música y avisos de afuera.
  ToqueRecibido, ToqueDevuelto, Agenda, AgendaManana, TeVas, HabitoHecho, HabitoRacha, HabitoCortado,
  Musica, Pago, Mail, Github, Pedido, AvisoOtro,
  // v3: la memoria, las propuestas al volver, crecer, las fechas propias y la empatía.
  Recuerdo, Propuesta, Crecio, MesJuntos, Feriado, FechaTuya, FechaManana, Mimos,
  Cantidad
};
constexpr int N_SITUACIONES = static_cast<int>(Situacion::Cantidad);
const char* nombreSituacion(Situacion s);
bool situacionPorNombre(const char* n, size_t largo, Situacion* s);

struct FrasePropia {
  Situacion situacion = Situacion::Despertar;
  char texto[LARGO_FRASE] = {};
};

// "habla" de la config v2.
struct Habla {
  uint8_t frecuencia = 50;   // centésimos (0 = nunca habla solo; 100 = el doble que lo normal)
  uint8_t nPropias = 0;
  FrasePropia propias[MAX_FRASES_PROPIAS];
};

// "despertador" de la config v2: uno solo por bicho.
struct Despertador {
  bool hay = false;
  bool activo = true;
  bool sonido = true;
  uint16_t minuto = 0;        // del día
  uint8_t dias = 0x7F;        // bit 0 lunes … bit 6 domingo
  uint8_t posponer = 9;       // minutos (1-30)
  char texto[LARGO_FRASE] = {};  // vacío = una frase de "despertador"
};

// "cuidado" (v1.7): recordar el agua, la pausa activa en el horario de
// trabajo, el "¿cómo estás?" del día; lo demás (te extraña, el resumen) lo usa la nube.
struct Cuidado {
  uint8_t agua = 0;           // horas (0 = nunca)
  uint8_t pausa = 0;          // minutos sin tocarlo en horario de trabajo (0 = nunca)
  uint16_t desde = 9 * 60, hasta = 18 * 60;  // el horario de trabajo (minutos del día)
  uint8_t dias = 0x1F;        // L a V
  bool animo = true;          // "¿Cómo estás?" una vez por día
};

// "avisos" (v1.7): cómo reacciona a una notificación o llamada del celu.
constexpr int MAX_AVISOS = 8;
constexpr int LARGO_APP = 25;       // 24 bytes + terminador (app y contacto)
constexpr int LARGO_DECIR_AVISO = 41;
struct Aviso {
  bool llamada = false;
  bool sonido = true;
  int8_t caraVida = -1, caraDibujo = -1;
  int8_t comportamiento = -1;
  char app[LARGO_APP] = {};
  char contacto[LARGO_APP] = {};
  char decir[LARGO_DECIR_AVISO] = {};
};
struct Avisos {
  uint8_t n = 0;
  Aviso lista[MAX_AVISOS];
};

// "habitos" (v1.9): hasta 4; a su hora (si tiene) pregunta "¿{habito}?" y un
// toque lo marca hecho.
constexpr int MAX_HABITOS = 4;
constexpr int LARGO_HABITO = 17;      // 16 bytes + terminador
constexpr int LARGO_PRESENCIA = 41;   // 40 bytes + terminador
enum class IconoHabito : uint8_t { Gota, Libro, Pesa, Zen, Pastilla, Cama, Fruta, Estrella, Cantidad };
struct Habito {
  char nombre[LARGO_HABITO] = {};
  IconoHabito icono = IconoHabito::Estrella;
  bool conHora = false;
  uint16_t minuto = 0;        // del día (si conHora)
  uint8_t dias = 0x7F;        // bit 0 lunes … bit 6 domingo
};
struct Habitos {
  uint8_t n = 0;
  Habito lista[MAX_HABITOS];
};
// "presencia" (v1.9): si dice algo cuando te vas y qué (vacío = las frases de te-vas).
struct Presencia {
  bool preguntar = true;
  char texto[LARGO_PRESENCIA] = {};
};

// "fechas" (v3): las fechas que le importan al dueño ("el cumple de mamá"): el
// día antes avisa y ese día lo recuerda.
constexpr int MAX_FECHAS = 6;
constexpr int LARGO_FECHA = 41;       // 40 bytes + terminador
struct FechaTuya {
  int16_t fecha = 0;                  // mes·100 + día
  char texto[LARGO_FECHA] = {};
};
struct FechasTuyas {
  uint8_t n = 0;
  FechaTuya lista[MAX_FECHAS];
};
const char* leerFechas(const Json& j, int raiz, FechasTuyas* f);

// Lee "habla", "despertador" y "ubicacion" (esta la usa la nube: acá solo se
// valida). nullptr si está bien; si no, el motivo.
const char* leerHabla(const Json& j, int raiz, Habla* h);
const char* leerDespertador(const Json& j, int raiz, Despertador* d);
const char* validarUbicacion(const Json& j, int raiz);
const char* leerCuidado(const Json& j, int raiz, Cuidado* c);
struct Config;
const char* leerAvisos(const Json& j, int raiz, const Config& c, Avisos* a);
// El aviso que corresponde (-1 = ninguno): sin distinguir mayúsculas; app o
// contacto vacíos en el aviso = cualquiera.
int buscarAviso(const Avisos& a, bool llamada, const char* app, const char* contacto);
const char* leerHabitos(const Json& j, int raiz, Habitos* h);
const char* leerPresencia(const Json& j, int raiz, Presencia* p);
const char* nombreIconoHabito(IconoHabito i);

// Más plantillas para una frase (v1.9): {de}, {titulo}, {minutos}, {cantidad},
// {racha}, {habito}, {artista}. `clave` va sin las llaves.
struct Plantilla {
  const char* clave;
  const char* valor;
};
struct Plantillas {
  const Plantilla* lista = nullptr;
  int n = 0;
};

// Reemplaza {dueno}, {robot} y {clima} (y, en los avisos, {app} y {contacto})
// y corta en `cap - 1` bytes sin partir una letra. Lo que está vacío se lleva
// la coma, los dos puntos o el espacio de antes ("¡Hola, {dueno}!" → "¡Hola!").
size_t completarFrase(const char* plantilla, const char* dueno, const char* robot, char* salida, size_t cap,
                      const char* clima = nullptr, const char* app = nullptr, const char* contacto = nullptr,
                      Plantillas extra = {});

// El preset más parecido a esa personalidad (los de personalidades.json),
// para elegir las variantes de las frases.
const char* presetCercano(const Personalidad& p);

// Elige una frase de la situación: las propias del usuario (si tiene) pesan
// más; si no, las de fábrica para todos y las de su preset (doble peso). Sin
// dueño, saltea las que empiezan con {dueno}. false si no hay ninguna.
bool elegirFrase(const Habla& h, Situacion s, const char* preset, const char* dueno, const char* robot, Azar& azar,
                 char* salida, size_t cap, const char* clima = nullptr, Plantillas extra = {});

// Lo que decide si habla o no (en RAM: unos pocos bytes).
class Hablador {
 public:
  // ¿Habla ahora? `segundos` es un reloj que avanza (los del sistema); `dia`,
  // el día del calendario (0 si no se sabe). Si dice que sí, ya lo anotó.
  bool quiere(Situacion s, uint32_t segundos, uint32_t dia, uint8_t frecuencia, Azar& azar);
  // Para los tests.
  int hoy() const { return hoy_; }

 private:
  // En minutos + 1 (0 = nunca), de 16 bits: el broche no tiene RAM de sobra.
  // Da la vuelta a los 45 días; las cuentas son con resta sin signo.
  uint16_t ultima_[N_SITUACIONES] = {};
  uint16_t ultimaCualquiera_ = 0;
  uint32_t dia_ = 0;
  uint32_t diaDesde_ = 0;   // sin calendario: un "día" cada 24 h de reloj
  uint8_t hoy_ = 0;
};

// Si la situación habla siempre (el cumple, el despertador: frases.json "siempre").
bool situacionSiempre(Situacion s);
// Un texto UTF-8 bien formado (sin terminador: n bytes).
bool textoUtf8Valido(const char* s, size_t n);

// La duración del globo según el largo: 2,5 s + 60 ms por byte, entre 3 y 10 s.
uint32_t duracionGlobo(const char* texto);

}  // namespace sintetica
