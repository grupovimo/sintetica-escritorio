#include "qr.h"

namespace sintetica {

namespace {

// Nivel M, versiones 1 a 6: palabras totales, de corrección por bloque, bloques.
struct Capacidad {
  int total, correccion, bloques;
};
const Capacidad CAP_M[7] = {{0, 0, 0}, {26, 10, 1}, {44, 16, 1}, {70, 26, 1}, {100, 18, 2}, {134, 24, 2}, {172, 16, 4}};
const int ALINEACION[7] = {0, 0, 18, 22, 26, 30, 34};  // el segundo centro (el primero es 6)

const char ALFANUMERICO[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

int valorAlfanumerico(char c) {
  for (int i = 0; ALFANUMERICO[i]; i++)
    if (ALFANUMERICO[i] == c) return i;
  return -1;
}

// Aritmética en GF(256) con el polinomio 0x11D.
uint8_t multiplicar(uint8_t a, uint8_t b) {
  uint8_t r = 0;
  for (int i = 7; i >= 0; i--) {
    r = static_cast<uint8_t>((r << 1) ^ ((r >> 7) * 0x1D));
    if ((b >> i) & 1) r ^= a;
  }
  return r;
}

void reedSolomon(const uint8_t* datos, int n, uint8_t* correccion, int grado) {
  uint8_t gen[32] = {};
  gen[grado - 1] = 1;
  uint8_t raiz = 1;
  for (int i = 0; i < grado; i++) {
    for (int j = 0; j < grado; j++) {
      gen[j] = multiplicar(gen[j], raiz);
      if (j + 1 < grado) gen[j] ^= gen[j + 1];
    }
    raiz = multiplicar(raiz, 0x02);
  }
  for (int i = 0; i < grado; i++) correccion[i] = 0;
  for (int k = 0; k < n; k++) {
    uint8_t factor = datos[k] ^ correccion[0];
    for (int i = 0; i + 1 < grado; i++) correccion[i] = correccion[i + 1];
    correccion[grado - 1] = 0;
    for (int i = 0; i < grado; i++) correccion[i] ^= multiplicar(gen[i], factor);
  }
}

struct Bits {
  uint8_t buf[200] = {};
  int n = 0;
  void poner(uint32_t v, int cant) {
    for (int i = cant - 1; i >= 0; i--, n++)
      if ((v >> i) & 1) buf[n >> 3] |= static_cast<uint8_t>(0x80 >> (n & 7));
  }
};

}  // namespace

void Qr::ponerFuncion(int x, int y, bool oscuro) {
  modulos_[y][x] = static_cast<uint8_t>(2 | (oscuro ? 1 : 0));
}

void Qr::patronesDeFuncion() {
  // Patrones de sincronismo.
  for (int i = 0; i < lado_; i++) {
    ponerFuncion(6, i, i % 2 == 0);
    ponerFuncion(i, 6, i % 2 == 0);
  }
  // Buscadores con su separador.
  const int esquinas[3][2] = {{3, 3}, {lado_ - 4, 3}, {3, lado_ - 4}};
  for (const auto& e : esquinas) {
    for (int dy = -4; dy <= 4; dy++) {
      for (int dx = -4; dx <= 4; dx++) {
        int x = e[0] + dx, y = e[1] + dy;
        if (x < 0 || y < 0 || x >= lado_ || y >= lado_) continue;
        int d = maxi(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
        ponerFuncion(x, y, d != 2 && d != 4);
      }
    }
  }
  // Alineación (una sola desde la versión 2 hasta la 6).
  if (version_ >= 2) {
    const int c = ALINEACION[version_];
    for (int dy = -2; dy <= 2; dy++)
      for (int dx = -2; dx <= 2; dx++) ponerFuncion(c + dx, c + dy, maxi(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy) != 1);
  }
  formato(0);  // reserva las zonas de formato
}

void Qr::formato(int mascara) {
  // Nivel M = 00. BCH(15,5) y la máscara fija del estándar.
  const int datos = (0 << 3) | mascara;
  int rem = datos;
  for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
  const int bits = ((datos << 10) | rem) ^ 0x5412;
  auto bit = [&](int i) { return ((bits >> i) & 1) != 0; };
  for (int i = 0; i <= 5; i++) ponerFuncion(8, i, bit(i));
  ponerFuncion(8, 7, bit(6));
  ponerFuncion(8, 8, bit(7));
  ponerFuncion(7, 8, bit(8));
  for (int i = 9; i < 15; i++) ponerFuncion(14 - i, 8, bit(i));
  for (int i = 0; i < 8; i++) ponerFuncion(lado_ - 1 - i, 8, bit(i));
  for (int i = 8; i < 15; i++) ponerFuncion(8, lado_ - 15 + i, bit(i));
  ponerFuncion(8, lado_ - 8, true);  // el módulo oscuro fijo
}

void Qr::ubicarDatos(const uint8_t* datos, int n) {
  int i = 0;
  for (int derecha = lado_ - 1; derecha >= 1; derecha -= 2) {
    if (derecha == 6) derecha = 5;
    for (int v = 0; v < lado_; v++) {
      for (int j = 0; j < 2; j++) {
        const int x = derecha - j;
        const bool subiendo = ((derecha + 1) & 2) == 0;
        const int y = subiendo ? lado_ - 1 - v : v;
        if (modulos_[y][x] & 2) continue;
        bool b = i < n * 8 && ((datos[i >> 3] >> (7 - (i & 7))) & 1);
        modulos_[y][x] = b ? 1 : 0;
        i++;
      }
    }
  }
}

void Qr::aplicarMascara(int m) {
  for (int y = 0; y < lado_; y++) {
    for (int x = 0; x < lado_; x++) {
      if (modulos_[y][x] & 2) continue;
      bool invertir;
      switch (m) {
        case 0: invertir = (x + y) % 2 == 0; break;
        case 1: invertir = y % 2 == 0; break;
        case 2: invertir = x % 3 == 0; break;
        case 3: invertir = (x + y) % 3 == 0; break;
        case 4: invertir = (x / 3 + y / 2) % 2 == 0; break;
        case 5: invertir = x * y % 2 + x * y % 3 == 0; break;
        case 6: invertir = (x * y % 2 + x * y % 3) % 2 == 0; break;
        default: invertir = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
      }
      if (invertir) modulos_[y][x] ^= 1;
    }
  }
}

long Qr::penalidad() const {
  long p = 0;
  auto en = [&](int x, int y, bool fila) { return fila ? oscuro(x, y) : oscuro(y, x); };
  // Reglas 1 y 3, por filas y por columnas.
  for (int pasada = 0; pasada < 2; pasada++) {
    const bool fila = pasada == 0;
    for (int a = 0; a < lado_; a++) {
      int racha = 1;
      for (int b = 1; b <= lado_; b++) {
        if (b < lado_ && en(b, a, fila) == en(b - 1, a, fila)) {
          racha++;
        } else {
          if (racha >= 5) p += 3 + (racha - 5);
          racha = 1;
        }
      }
      // 1011101 con cuatro claros a un lado.
      static const bool patron[7] = {1, 0, 1, 1, 1, 0, 1};
      for (int b = 0; b + 7 <= lado_; b++) {
        bool ok = true;
        for (int k = 0; k < 7 && ok; k++) ok = en(b + k, a, fila) == patron[k];
        if (!ok) continue;
        bool antes = true, despues = true;
        for (int k = 1; k <= 4; k++) {
          if (b - k >= 0 && en(b - k, a, fila)) antes = false;
          if (b + 6 + k < lado_ && en(b + 6 + k, a, fila)) despues = false;
        }
        if (antes || despues) p += 40;
      }
    }
  }
  // Regla 2: bloques de 2 × 2 del mismo color.
  for (int y = 0; y + 1 < lado_; y++)
    for (int x = 0; x + 1 < lado_; x++) {
      bool c = oscuro(x, y);
      if (c == oscuro(x + 1, y) && c == oscuro(x, y + 1) && c == oscuro(x + 1, y + 1)) p += 3;
    }
  // Regla 4: proporción de oscuros.
  int oscuros = 0;
  for (int y = 0; y < lado_; y++)
    for (int x = 0; x < lado_; x++) oscuros += oscuro(x, y);
  const int total = lado_ * lado_;
  int desvio = oscuros * 20 - total * 10;
  if (desvio < 0) desvio = -desvio;
  p += static_cast<long>((desvio + total - 1) / total - 1) * 10;
  return p;
}

bool Qr::codificar(const char* texto) {
  const int n = static_cast<int>(largo(texto));
  bool alfa = true;
  for (int i = 0; i < n; i++) alfa = alfa && valorAlfanumerico(texto[i]) >= 0;
  const int bitsDatos = alfa ? 4 + 9 + (n / 2) * 11 + (n % 2) * 6 : 4 + 8 + n * 8;

  version_ = 0;
  for (int v = 1; v <= 6; v++) {
    if (bitsDatos <= (CAP_M[v].total - CAP_M[v].correccion * CAP_M[v].bloques) * 8) {
      version_ = v;
      break;
    }
  }
  if (!version_) return false;
  lado_ = version_ * 4 + 17;
  const Capacidad cap = CAP_M[version_];
  const int capDatos = cap.total - cap.correccion * cap.bloques;

  Bits b;
  if (alfa) {
    b.poner(0x2, 4);
    b.poner(static_cast<uint32_t>(n), 9);
    for (int i = 0; i + 1 < n; i += 2)
      b.poner(static_cast<uint32_t>(valorAlfanumerico(texto[i]) * 45 + valorAlfanumerico(texto[i + 1])), 11);
    if (n % 2) b.poner(static_cast<uint32_t>(valorAlfanumerico(texto[n - 1])), 6);
  } else {
    b.poner(0x4, 4);
    b.poner(static_cast<uint32_t>(n), 8);
    for (int i = 0; i < n; i++) b.poner(static_cast<uint8_t>(texto[i]), 8);
  }
  b.poner(0, mini(4, capDatos * 8 - b.n));
  b.poner(0, (8 - b.n % 8) % 8);
  for (uint8_t relleno = 0xEC; b.n < capDatos * 8; relleno ^= 0xEC ^ 0x11) b.poner(relleno, 8);

  // Bloques de corrección y entrelazado (en M v1–6 todos los bloques miden lo mismo).
  const int porBloque = capDatos / cap.bloques;
  uint8_t correccion[4][32];
  for (int k = 0; k < cap.bloques; k++) reedSolomon(b.buf + k * porBloque, porBloque, correccion[k], cap.correccion);
  uint8_t final[200];
  int f = 0;
  for (int i = 0; i < porBloque; i++)
    for (int k = 0; k < cap.bloques; k++) final[f++] = b.buf[k * porBloque + i];
  for (int i = 0; i < cap.correccion; i++)
    for (int k = 0; k < cap.bloques; k++) final[f++] = correccion[k][i];

  // Probar las 8 máscaras y quedarse con la de menor penalidad.
  long mejor = -1;
  int mejorMascara = 0;
  for (int m = 0; m < 8; m++) {
    llenarBytes(modulos_, 0, sizeof modulos_);
    patronesDeFuncion();
    ubicarDatos(final, f);
    aplicarMascara(m);
    formato(m);
    long p = penalidad();
    if (mejor < 0 || p < mejor) mejor = p, mejorMascara = m;
  }
  llenarBytes(modulos_, 0, sizeof modulos_);
  patronesDeFuncion();
  ubicarDatos(final, f);
  aplicarMascara(mejorMascara);
  formato(mejorMascara);
  mascara_ = mejorMascara;
  return true;
}

}  // namespace sintetica
