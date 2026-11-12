// Protocolo de SintéticaOS (v1.4), por BLE y por la nube. Documentado en docs/protocolo-ble.md.
// Lo mismo viaja por BLE en la placa y por el panel del simulador.
#pragma once

#include "formato.h"
#include "hal.h"
#include "mate.h"

namespace sintetica {
namespace protocolo {

// UUID base: 5157e71c-a000-4b1c-8a5e-0000000000XX (XX = el id de abajo).
constexpr const char* UUID_SERVICIO = "5157e71c-a000-4b1c-8a5e-000000000001";
constexpr const char* UUID_CONFIG = "5157e71c-a000-4b1c-8a5e-000000000002";
constexpr const char* UUID_COMANDO = "5157e71c-a000-4b1c-8a5e-000000000003";
constexpr const char* UUID_ESTADO = "5157e71c-a000-4b1c-8a5e-000000000004";
constexpr const char* UUID_OTA = "5157e71c-a000-4b1c-8a5e-000000000005";
constexpr const char* UUID_IMAGEN = "5157e71c-a000-4b1c-8a5e-000000000006";

enum Caracteristica : uint8_t { CarConfig = 1, CarComando = 2, CarEstado = 3, CarOta = 4, CarImagen = 5 };

// El peor caso de QOI (3 canales, ruido) es ancho × alto × 4 + 22 bytes: el
// límite de cada bicho depende de su pantalla (en el broche, 230 KB; una foto
// común ocupa entre 80 y 130 KB). Este es el de la pantalla más grande en placa.
constexpr uint32_t MAX_IMAGEN_BYTES = 320 * 240 * 4 + 22;
inline uint32_t limiteImagen(int ancho, int alto) { return static_cast<uint32_t>(ancho) * static_cast<uint32_t>(alto) * 4u + 22u; }

// Operaciones (primer byte de cada escritura).
enum Op : uint8_t {
  // Config
  ConfigInicio = 0x01,   // u32 largo, u32 crc32
  ConfigBloque = 0x02,   // u32 desplazamiento, datos
  ConfigFin = 0x03,      // (nada)
  ConfigPedir = 0x04,    // (nada) → el bicho manda su config en notificaciones
  // Comando
  CmdExpresion = 0x10,   // nombre UTF-8; vacío = volver a automático
  CmdNombre = 0x11,      // nombre UTF-8, 1 a 24 bytes
  CmdEvento = 0x12,      // u8 evento
  CmdVincular = 0x13,    // código del QR (6 bytes) → responde con una clave de 16 bytes
  CmdAutenticar = 0x14,  // clave de 16 bytes (en cada conexión nueva)
  CmdDesvincular = 0x15, // olvida todas las claves y vuelve a mostrar el QR
  CmdRed = 0x16,         // [n][ssid][n][clave]: el WiFi (v1.3)
  CmdNube = 0x17,        // [n][url mqtt][n][usuario][n][clave]: la cuenta en la nube (v1.3)
  CmdOlvidarRed = 0x18,  // olvida el WiFi y la nube (v1.3)
  CmdReproducir = 0x19,  // nombre de una animación; vacío = parar (v1.3)
  CmdHora = 0x1A,        // [u32 epoch UTC][i16 minutos de la zona]: la hora (v1.4)
  CmdVida = 0x1B,        // [u8 superposición 0/1]? → el estado del motor de vida (v1.4)
  CmdMemoria = 0x1C,     // → [0x9C][0][memoria, 112 bytes] (v1.4)
  CmdRestaurar = 0x1D,   // [memoria, 112 bytes]: la mudanza (v1.4)
  CmdRegistro = 0x1E,    // → [0x9E][0][n][n × (u32 segundos, u8 tipo, u8 dato)] (v1.4)
  CmdComportamiento = 0x1F,  // nombre de un comportamiento: lo empieza ya (para probarlo) (v1.4)
  CmdDecir = 0x40,       // [n][expresión][texto 1-60 bytes]: lo muestra en un globo (v1.6)
  CmdClima = 0x41,       // [i16 décimas °C][u8 WMO][i8 máx][i8 mín][u8 flags][n][ciudad] (v1.6)
  CmdAlarma = 0x42,      // [u8: 0 apagar, 1 posponer, 2 sonar ya, 3 despertar] (v1.6)
  CmdJugar = 0x43,       // [u8 juego: 0 ppt, 1 reflejos, 2 mirada, 3 dado, 4 moneda; 255 terminar] (v1.7)
  CmdAviso = 0x44,       // [u8 0 notificación, 1 llamada][n][app][n][contacto] (v1.7)
  CmdAnimo = 0x45,       // [u8 0 bien, 1 más o menos, 2 mal] (v1.7)
  CmdMirar = 0x46,       // [i8 x][i8 y] (-100..100): de dónde viene un sonido (v1.7)
  CmdToque = 0x47,       // [u8 tipo: 0 toque, 1 abrazo, 2 beso, 3 carita][n][de][n][expresión] (v1.9)
  CmdAgenda = 0x48,      // [u16 minutos (0xFFFF = mañana)][u16 duración (o cantidad)][n][título] (v1.9)
  CmdPresencia = 0x49,   // [u8 0 llegaste, 1 te fuiste] (v1.9)
  CmdHabito = 0x4A,      // [u8 índice][u8 0 hecho, 1 deshacer][u8 racha] (v1.9)
  CmdMusica = 0x4B,      // [u8 bpm][u8 flags: bit 0 sonando][n][título][n][artista] (v1.9)
  CmdCharlar = 0x4C,     // [n][texto 1-200 bytes]: lo charla él (charla sin IA) → [0xCC]... (v1.10)
  CmdDiario = 0x4D,      // → n × [0xCD][0][u16 día][u8 i][u8 n][u8 tipo][u8 dato][u8 etapa][texto] (v3; también aviso)
  // Imagen
  ImgInicio = 0x20,      // u8 espacio (0-7 imágenes, 8-19 sprites), u32 largo, u32 crc32
  ImgBloque = 0x21,      // u32 desplazamiento, datos
  ImgFin = 0x22,         // (nada)
  ImgBorrar = 0x23,      // u8 espacio
  ImgListar = 0x24,      // → [0xA4][0][máscara u8] + por espacio ocupado [espacio][u32 largo][u32 crc]
  ImgPedir = 0x25,       // u8 espacio → [0xA5][0], varios 0xA7 y un 0xA8 (v1.3)
  ImgListarSprites = 0x26,  // → [0xA6][0][máscara u16] + por sprite [espacio 8-19][u32 largo][u32 crc] (v1.5)
  // OTA (v1.3)
  OtaInicio = 0x30,      // manifiesto → después, bloques
  OtaBloque = 0x31,      // u32 desplazamiento, datos
  OtaFin = 0x32,         // verifica el SHA-256 e instala; reinicia
  OtaDescargar = 0x33,   // manifiesto + [u16 n][url]: la baja él solo por WiFi
  OtaProgreso = 0x34,    // (solo notificación) [0xB4][código][porcentaje]
  OtaCancelar = 0x35,
  // Respuestas (notificación): op | 0x80, código, [detalle]
  Respuesta = 0x80,
  ConfigTrozo = 0x84,    // u32 desplazamiento, datos (respuesta a ConfigPedir)
  ConfigCierre = 0x85,   // u32 largo, u32 crc32 (fin de ConfigPedir)
  ImgTrozo = 0xA7,       // u32 desplazamiento, datos (respuesta a ImgPedir)
  ImgCierre = 0xA8,      // u8 espacio, u32 largo, u32 crc32
};

enum Codigo : uint8_t {
  Ok = 0,
  MuyGrande = 1,
  FueraDeOrden = 2,
  CrcDistinto = 3,
  JsonInvalido = 4,
  ConfigInvalida = 5,
  SinInicio = 6,
  NoExiste = 7,
  Malformado = 8,
  NoAutorizado = 9,      // falta vincular o autenticar, o el código/clave no coincide
  NoEmparejando = 10,    // para vincular, el bicho tiene que estar mostrando el QR
  ImagenInvalida = 11,   // no es un QOI del tamaño de la pantalla
  FirmaInvalida = 12,    // la firma del firmware no es de Sintética Labs
  HashDistinto = 13,     // lo que llegó no es lo que se firmó
  PerfilDistinto = 14,   // es el firmware de otra placa
  SinRed = 15,           // para descargar hace falta WiFi
  Ocupado = 16,          // ya hay una actualización en curso
  FalloOta = 17,         // no se pudo escribir o bajar (el detalle dice por qué)
  NoImplementado = 0xFF,
};

constexpr size_t MAX_NOTIFICACION = 180;  // entra con un MTU de 185

uint32_t crc32(const uint8_t* datos, size_t n, uint32_t crc = 0);

inline uint32_t leer32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
         static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}
inline void escribir32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v), p[1] = static_cast<uint8_t>(v >> 8);
  p[2] = static_cast<uint8_t>(v >> 16), p[3] = static_cast<uint8_t>(v >> 24);
}

// Arma la configuración que llega por partes.
class Receptor {
 public:
  Codigo inicio(uint32_t largo, uint32_t crc);
  Codigo bloque(uint32_t desplazamiento, const uint8_t* datos, size_t n);
  Codigo fin();  // verifica largo y CRC
  const char* texto() const { return reinterpret_cast<const char*>(buf_); }
  size_t largo() const { return recibido_; }
  void reiniciar() { activo_ = false, recibido_ = 0; }
  bool activo() const { return activo_; }
  // El buffer, prestado mientras no hay una transferencia (para leer la config
  // guardada sin otra copia en RAM). Entra MAX_CONFIG_BYTES + 1.
  char* prestar() { return activo_ ? nullptr : reinterpret_cast<char*>(buf_); }

 private:
  uint8_t buf_[MAX_CONFIG_BYTES + 1];
  uint32_t esperado_ = 0, crc_ = 0, recibido_ = 0;
  bool activo_ = false;
};

// Lee un manifiesto: [u32 tamaño][32 sha256][64 firma][n][versión][n][perfil].
// Devuelve los bytes que ocupó (0 si está mal).
size_t leerManifiesto(const uint8_t* d, size_t n, ManifiestoOta* m);
size_t escribirManifiesto(const ManifiestoOta& m, uint8_t* salida, size_t cap);
// El texto que se firma:
// "SINTETICA-OTA-V1\n<perfil>\n<versión>\n<tamaño>\n<sha256 en hex minúscula>\n".
size_t mensajeOta(const ManifiestoOta& m, char* salida, size_t cap);

// Texto con largo adelante (u8), como en los comandos de red y nube.
// Devuelve los bytes que ocupó o 0 si no entra.
size_t leerTextoCorto(const uint8_t* d, size_t n, char* destino, size_t cap);

// El estado público que se lee o se notifica.
struct EstadoPublico {
  uint8_t bateria = 255;  // 0-100; 255 = no se sabe
  int8_t humor = 0;       // -100..100
  uint8_t modo = 0;
  bool dormido = false;
  bool fija = false;
  bool emparejando = false;
  bool autenticado = false;
  const char* expresion = "";
  const char* nombre = "";
  const char* serie = "";
  const char* id = "";
  uint16_t ancho = 0, alto = 0;
  bool redonda = false;
  bool wifi = false;          // conectado al WiFi
  bool nube = false;          // conectado a la nube
  bool redConfigurada = false;
  bool actualizando = false;  // instalando una OTA
  const char* version = "";   // del firmware
  const char* perfil = "";
  uint32_t crcConfig = 0;     // CRC-32 de la config guardada (0 = la de fábrica)
  uint32_t encendido = 0;     // segundos desde que arrancó (no dispara notificaciones)
};
constexpr uint8_t VERSION_ESTADO = 4;
// [versión=4][batería][humor][modo][flags][n][expresión][n][nombre][n][serie][n][id]
// [u16 ancho][u16 alto][forma: 0 rectangular, 1 redonda][n][versión][n][perfil][u32 crc config]
// [u32 segundos encendido]
// flags: bit 0 dormido, 1 expresión fija, 2 mostrando el QR, 3 esta conexión autenticada,
// 4 WiFi conectado, 5 nube conectada, 6 WiFi configurado, 7 actualizando
constexpr size_t MAX_ESTADO = 160;
size_t codificarEstado(const EstadoPublico& e, uint8_t* salida, size_t cap);

}  // namespace protocolo
}  // namespace sintetica
