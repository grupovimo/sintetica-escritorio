// Lectura de la parte del motor de vida de la config (v2). La usa formato.cpp.
#pragma once

#include "formato.h"

namespace sintetica {

// Nombres de expresiones, animaciones y comportamientos: [a-z][a-z0-9-]{0,23}.
bool nombreValido(const char* s);
// Una expresión sin "formas" es paramétrica.
bool esCaraParametrica(const Json& j, int obj);
ResultadoConfig leerCaraVida(const Json& j, int obj, Config& c, const char* nombre, bool propia);
// semilla, personalidad, horario, dueno, cumple, limites_marca, comportamientos,
// recordatorios, voz, pomodoro y protector.
ResultadoConfig leerVidaConfig(const Json& j, int raiz, Config& c);
// Las expresiones paramétricas y los comportamientos base (shared/formato/vida).
ResultadoConfig cargarVidaBase(Config& c);
// Los pasos de un comportamiento (hasta MAX_PASOS) en `salida`: los de la
// config, de su pozo; los de fábrica, leídos de la flash. Devuelve cuántos.
int pasosDe(const Config& c, const Comportamiento& b, Paso* salida);
// Las reglas base del motor de vida (un arreglo JSON, como "reglas").
const char* reglasVidaBase();

}  // namespace sintetica
