#include "protocolo.h"

namespace sintetica {
namespace protocolo {

uint32_t crc32(const uint8_t* datos, size_t n, uint32_t crc) {
  crc = ~crc;
  for (size_t i = 0; i < n; i++) {
    crc ^= datos[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

Codigo Receptor::inicio(uint32_t largo, uint32_t crc) {
  activo_ = false;
  recibido_ = 0;
  if (largo == 0 || largo > MAX_CONFIG_BYTES) return MuyGrande;
  esperado_ = largo;
  crc_ = crc;
  activo_ = true;
  return Ok;
}

Codigo Receptor::bloque(uint32_t desplazamiento, const uint8_t* datos, size_t n) {
  if (!activo_) return SinInicio;
  // Un bloque malo aborta la transferencia: la app tiene que empezar de nuevo.
  if (desplazamiento != recibido_) return activo_ = false, FueraDeOrden;
  if (recibido_ + n > esperado_) return activo_ = false, MuyGrande;
  copiarBytes(buf_ + recibido_, datos, n);
  recibido_ += static_cast<uint32_t>(n);
  return Ok;
}

Codigo Receptor::fin() {
  if (!activo_) return SinInicio;
  activo_ = false;
  if (recibido_ != esperado_) return FueraDeOrden;
  if (crc32(buf_, recibido_) != crc_) return CrcDistinto;
  buf_[recibido_] = 0;
  return Ok;
}

size_t codificarEstado(const EstadoPublico& e, uint8_t* s, size_t cap) {
  const size_t ne = largo(e.expresion), nn = largo(e.nombre), ns = largo(e.serie), ni = largo(e.id);
  const size_t nv = largo(e.version), np = largo(e.perfil);
  const size_t total = 5 + 1 + ne + 1 + nn + 1 + ns + 1 + ni + 5 + 1 + nv + 1 + np + 8;
  if (cap < total || ne > 255 || nn > 255 || ns > 255 || ni > 255 || nv > 255 || np > 255) return 0;
  size_t i = 0;
  s[i++] = VERSION_ESTADO;
  s[i++] = e.bateria;
  s[i++] = static_cast<uint8_t>(e.humor);
  s[i++] = e.modo;
  s[i++] = static_cast<uint8_t>((e.dormido ? 1 : 0) | (e.fija ? 2 : 0) | (e.emparejando ? 4 : 0) |
                                (e.autenticado ? 8 : 0) | (e.wifi ? 16 : 0) | (e.nube ? 32 : 0) |
                                (e.redConfigurada ? 64 : 0) | (e.actualizando ? 128 : 0));
  s[i++] = static_cast<uint8_t>(ne);
  copiarBytes(s + i, e.expresion, ne);
  i += ne;
  s[i++] = static_cast<uint8_t>(nn);
  copiarBytes(s + i, e.nombre, nn);
  i += nn;
  s[i++] = static_cast<uint8_t>(ns);
  copiarBytes(s + i, e.serie, ns);
  i += ns;
  s[i++] = static_cast<uint8_t>(ni);
  copiarBytes(s + i, e.id, ni);
  i += ni;
  s[i++] = static_cast<uint8_t>(e.ancho), s[i++] = static_cast<uint8_t>(e.ancho >> 8);
  s[i++] = static_cast<uint8_t>(e.alto), s[i++] = static_cast<uint8_t>(e.alto >> 8);
  s[i++] = e.redonda ? 1 : 0;
  s[i++] = static_cast<uint8_t>(nv);
  copiarBytes(s + i, e.version, nv);
  i += nv;
  s[i++] = static_cast<uint8_t>(np);
  copiarBytes(s + i, e.perfil, np);
  i += np;
  escribir32(s + i, e.crcConfig);
  escribir32(s + i + 4, e.encendido);
  return i + 8;
}

size_t leerTextoCorto(const uint8_t* d, size_t n, char* destino, size_t cap) {
  if (n < 1 || static_cast<size_t>(d[0]) + 1 > n || d[0] >= cap) return 0;
  copiarTexto(destino, cap, reinterpret_cast<const char*>(d + 1), d[0]);
  // Sin terminadores en el medio.
  for (size_t i = 0; i < d[0]; i++)
    if (d[1 + i] == 0) return 0;
  return 1 + d[0];
}

size_t leerManifiesto(const uint8_t* d, size_t n, ManifiestoOta* m) {
  if (n < 4 + 32 + 64 + 2) return 0;
  *m = ManifiestoOta();
  m->tamano = leer32(d);
  copiarBytes(m->sha256, d + 4, 32);
  copiarBytes(m->firma, d + 36, 64);
  size_t i = 100;
  size_t k = leerTextoCorto(d + i, n - i, m->version, sizeof m->version);
  if (!k || !m->version[0]) return 0;
  i += k;
  k = leerTextoCorto(d + i, n - i, m->perfil, sizeof m->perfil);
  if (!k || !m->perfil[0]) return 0;
  return i + k;
}

size_t escribirManifiesto(const ManifiestoOta& m, uint8_t* s, size_t cap) {
  const size_t nv = largo(m.version), np = largo(m.perfil);
  if (cap < 100 + 2 + nv + np) return 0;
  escribir32(s, m.tamano);
  copiarBytes(s + 4, m.sha256, 32);
  copiarBytes(s + 36, m.firma, 64);
  size_t i = 100;
  s[i++] = static_cast<uint8_t>(nv);
  copiarBytes(s + i, m.version, nv);
  i += nv;
  s[i++] = static_cast<uint8_t>(np);
  copiarBytes(s + i, m.perfil, np);
  return i + np;
}

size_t mensajeOta(const ManifiestoOta& m, char* s, size_t cap) {
  char num[12];
  int nd = 0;
  uint32_t v = m.tamano;
  do {
    num[nd++] = static_cast<char>('0' + v % 10);
    v /= 10;
  } while (v);
  const size_t total = 17 + largo(m.perfil) + 1 + largo(m.version) + 1 + static_cast<size_t>(nd) + 1 + 64 + 1;
  if (cap < total + 1) return 0;
  size_t i = copiarTexto(s, cap, "SINTETICA-OTA-V1\n");
  i += copiarTexto(s + i, cap - i, m.perfil);
  s[i++] = '\n';
  i += copiarTexto(s + i, cap - i, m.version);
  s[i++] = '\n';
  while (nd) s[i++] = num[--nd];
  s[i++] = '\n';
  static const char HEX[] = "0123456789abcdef";
  for (int k = 0; k < 32; k++) {
    s[i++] = HEX[m.sha256[k] >> 4];
    s[i++] = HEX[m.sha256[k] & 15];
  }
  s[i++] = '\n';
  s[i] = 0;
  return i;
}

}  // namespace protocolo
}  // namespace sintetica
