// La frontera con el hardware. Cada perfil (dev-147b, el simulador, los tests)
// llena esta tabla; el núcleo no sabe nada de pines, pantallas ni radios.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sintetica {

// Una actualización de firmware firmada (docs/protocolo-ble.md, "OTA"). El
// perfil verifica la firma con la clave pública que trae embebida: ECDSA P-256
// sobre SHA-256 de protocolo::mensajeOta(manifiesto).
struct ManifiestoOta {
  uint32_t tamano = 0;
  uint8_t sha256[32] = {};
  uint8_t firma[64] = {};  // r || s, 32 bytes cada uno, big-endian
  char version[24] = {};
  char perfil[24] = {};
};

struct Hal {
  // Milisegundos desde que arrancó (puede dar la vuelta; el núcleo lo tolera).
  uint32_t (*ahoraMs)() = nullptr;
  // Identificador único del bicho, 12 caracteres hexadecimales en mayúscula
  // (en la placa, la MAC). nullptr = "000000000000".
  const char* (*identificador)() = nullptr;
  // Una semilla para el azar (ruido del ADC, RNG de hardware, etc.).
  uint32_t (*semilla)() = nullptr;
  // Muestra el framebuffer RGB565 completo (cuando se dibuja de una vez).
  void (*presentar)(const uint16_t* pixeles, int ancho, int alto) = nullptr;
  // Muestra una franja: `filas` filas a partir de la fila y0 de la pantalla.
  // Si está, se usa en vez de presentar (también con una sola franja).
  void (*presentarFranja)(const uint16_t* pixeles, int ancho, int y0, int filas) = nullptr;
  // Minuto del día (0-1439) o -1 si no se sabe la hora.
  int (*minutoDelDia)() = nullptr;
  // Batería 0-100, o -1 si no hay o no se sabe.
  int (*bateria)() = nullptr;
  // 1 cargando, 0 no, -1 no se sabe.
  int (*cargando)() = nullptr;
  // Segundos desde 1970 en hora local (con la zona ya sumada), o 0 si no se
  // sabe (en ese caso vale la que mande la app con el comando Hora).
  uint32_t (*epoch)() = nullptr;
  // La app mandó la hora (comando Hora): el perfil pone en hora su reloj (en
  // el ESP32 sigue andando durante el sueño profundo). nullptr = la guarda
  // el núcleo (se pierde al reiniciar).
  void (*fijarHora)(uint32_t epochUtc, int zonaMinutos) = nullptr;
  // Almacenamiento persistente por clave (NVS, localStorage, memoria).
  bool (*guardar)(const char* clave, const uint8_t* datos, size_t n) = nullptr;
  int (*leer)(const char* clave, uint8_t* datos, size_t cap) = nullptr;  // -1 si no está
  // Archivos grandes (imágenes), por partes. escribirTrozo con desplazamiento 0
  // empieza el archivo de cero. leerTrozo devuelve los bytes leídos (0 al
  // final) o -1 si no está.
  bool (*escribirTrozo)(const char* clave, uint32_t desplazamiento, const uint8_t* datos, size_t n) = nullptr;
  int (*leerTrozo)(const char* clave, uint32_t desplazamiento, uint8_t* datos, size_t n) = nullptr;
  bool (*borrar)(const char* clave) = nullptr;
  // Los bytes libres para archivos grandes (nullptr = sin límite conocido): una
  // subida que no entra se rechaza de entrada, no a mitad de camino.
  int64_t (*espacioLibre)() = nullptr;
  // Notificación BLE en una característica (protocolo::Caracteristica).
  void (*notificar)(uint8_t caracteristica, const uint8_t* datos, size_t n) = nullptr;
  // Lo mismo por la nube (MQTT, bicho/{id}/resp y bicho/{id}/state): las
  // respuestas a lo que llegó por la nube y el estado. nullptr = sin nube.
  void (*notificarNube)(uint8_t caracteristica, const uint8_t* datos, size_t n) = nullptr;
  // Cambiaron el WiFi o la nube guardados (Sistema::red, Sistema::nube): el
  // perfil se reconecta.
  void (*redCambio)() = nullptr;
  // OTA. Devuelven un protocolo::Codigo. otaEmpezar verifica la firma, el
  // perfil y el tamaño antes de aceptar bloques; otaTerminar verifica el
  // SHA-256 y deja lista la partición nueva (el perfil reinicia solo, después
  // de que se mande la respuesta). otaDescargar baja el binario por HTTP en
  // segundo plano y avisa con Sistema::otaProgreso.
  uint8_t (*otaEmpezar)(const ManifiestoOta& m) = nullptr;
  uint8_t (*otaEscribir)(uint32_t desplazamiento, const uint8_t* datos, size_t n) = nullptr;
  uint8_t (*otaTerminar)() = nullptr;
  uint8_t (*otaDescargar)(const ManifiestoOta& m, const char* url) = nullptr;
  void (*otaCancelar)() = nullptr;
  // Salidas opcionales.
  void (*sonido)(const char* nombre) = nullptr;
  void (*led)(uint32_t rgb) = nullptr;
  // La voz propia (voz_propia.h): PCM de 16 bits, 16 kHz, mono, de a bloques,
  // un poco adelantado (~150 ms). El perfil lo encola en su parlante. nullptr
  // = sin parlante (el robot no habla en voz alta).
  void (*audio)(const int16_t* muestras, size_t n) = nullptr;
  void (*registrar)(const char* mensaje) = nullptr;
};

}  // namespace sintetica
