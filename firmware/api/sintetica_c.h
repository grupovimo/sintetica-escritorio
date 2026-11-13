/* API en C del núcleo de SintéticaOS, para las apps (Swift la importa con un
 * module map; Kotlin, por JNI) y cualquier otro anfitrión.
 *
 * Hay UNA instancia por proceso: el núcleo usa memoria estática. Todas las
 * funciones se llaman desde el mismo hilo.
 *
 * Dos usos:
 *  - Vista previa idéntica: sintetica_iniciar + sintetica_vista_previa, se le
 *    manda la config del bicho (protocolo Config) y la expresión, y se dibuja
 *    con sintetica_tick. La geometría es la misma que en la placa; el ancho
 *    puede ser el doble (640 × 344) para que se vea nítido en el celu.
 *  - Bicho virtual ("probar sin bicho"): lo mismo, pero la app le habla con los
 *    bytes del protocolo (sintetica_ble_escribir) y recibe las notificaciones
 *    por hal.notificar, como si fuera BLE.
 */
#ifndef SINTETICA_C_H
#define SINTETICA_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Una actualización firmada (ver docs/protocolo-ble.md, "OTA"). */
typedef struct sintetica_manifiesto {
  uint32_t tamano;
  uint8_t sha256[32];
  uint8_t firma[64]; /* ECDSA P-256, r || s */
  char version[24];
  char perfil[24];
} sintetica_manifiesto;

typedef struct sintetica_hal {
  void* contexto; /* se pasa tal cual a cada función */
  uint32_t (*ahora_ms)(void* contexto);
  uint32_t (*semilla)(void* contexto);
  int (*minuto_del_dia)(void* contexto); /* -1 si no se sabe */
  int (*bateria)(void* contexto);        /* 0-100, -1 si no hay */
  int (*guardar)(void* contexto, const char* clave, const uint8_t* datos, size_t n); /* 1 = ok */
  int (*leer)(void* contexto, const char* clave, uint8_t* datos, size_t cap);        /* -1 si no está */
  void (*notificar)(void* contexto, int caracteristica, const uint8_t* datos, size_t n);
  void (*registrar)(void* contexto, const char* mensaje);
  const char* identificador; /* 12 caracteres hexadecimales; NULL = "000000000000" */
  /* v1.3: nube y OTA (todo opcional). */
  void (*notificar_nube)(void* contexto, int caracteristica, const uint8_t* datos, size_t n);
  void (*red_cambio)(void* contexto);
  int (*ota_empezar)(void* contexto, const sintetica_manifiesto* m);  /* devuelven un código del protocolo */
  int (*ota_escribir)(void* contexto, uint32_t desplazamiento, const uint8_t* datos, size_t n);
  int (*ota_terminar)(void* contexto);
  int (*ota_descargar)(void* contexto, const sintetica_manifiesto* m, const char* url);
  void (*ota_cancelar)(void* contexto);
  const char* perfil;  /* "dev-147b", "broche-c3", "virtual"…; NULL = "sim" */
  const char* version; /* del firmware; NULL = la del núcleo */
  /* Archivos grandes (imágenes) por partes; NULL = en memoria, dentro de la API. */
  int (*escribir_trozo)(void* contexto, const char* clave, uint32_t desplazamiento, const uint8_t* datos, size_t n);
  int (*leer_trozo)(void* contexto, const char* clave, uint32_t desplazamiento, uint8_t* datos, size_t n);
  int (*borrar)(void* contexto, const char* clave);
  /* Segundos desde 1970 en hora local (0 = no se sabe): la memoria del motor de vida
   * (ausencias, días juntos, cumpleaños). NULL = solo el minuto del día. */
  uint32_t (*epoch)(void* contexto);
  /* v1.8 (el bicho-celu): los sonidos del núcleo por nombre ("despertador:3",
   * "ronquido", "dado"… ver docs/protocolo-ble.md, "Voz") y si está cargando
   * (1 sí, 0 no, -1 no se sabe; NULL = no se sabe). */
  void (*sonido)(void* contexto, const char* nombre);
  int (*cargando)(void* contexto);
  /* v1.10: la voz propia (el balbuceo que lee el globo). PCM de 16 bits,
   * 16 kHz, mono, de a bloques y un poco adelantado (~150 ms): encolarlo en
   * el parlante (AudioTrack en modo stream). NULL = no habla en voz alta. */
  void (*audio)(void* contexto, const int16_t* muestras, size_t n);
} sintetica_hal;

/* Arranca el núcleo. ancho ≤ 640, alto ≤ 640. Cualquier función de la tabla puede ser NULL.
 * Las imágenes (protocolo Imagen) se guardan en memoria, dentro de la API. */
void sintetica_iniciar(const sintetica_hal* hal, int ancho, int alto);
/* Igual, para una pantalla redonda (el broche) o con su pulsador (boton_broche = 1). */
void sintetica_iniciar_perfil(const sintetica_hal* hal, int ancho, int alto, int redonda, int boton_broche);
void sintetica_vista_previa(void);

void sintetica_tick(void);
const uint16_t* sintetica_pixeles(void); /* RGB565, ancho × alto, fila por fila */
int sintetica_ancho(void);
int sintetica_alto(void);
/* Convierte el último cuadro a RGBA de 8 bits (ancho × alto × 4 bytes). */
void sintetica_pixeles_rgba(uint8_t* salida);

void sintetica_tacto(int apretado);
void sintetica_imu(float ax, float ay, float az);
void sintetica_evento(int evento);

void sintetica_ble_conexion(int conectado);
void sintetica_ble_escribir(int caracteristica, const uint8_t* datos, size_t n);
size_t sintetica_ble_estado(uint8_t* salida, size_t cap);

/* Lo que llega por la nube (MQTT): [característica][datos] ya separados. */
void sintetica_nube_escribir(int caracteristica, const uint8_t* datos, size_t n);
void sintetica_conexiones(int wifi, int nube);
/* Las credenciales guardadas; 1 si hay. */
int sintetica_red(char* ssid, size_t cap_ssid, char* clave, size_t cap_clave);
int sintetica_nube(char* url, size_t cap_url, char* usuario, size_t cap_usuario, char* clave, size_t cap_clave);
void sintetica_ota_progreso(int codigo, int porcentaje, const char* detalle);
uint32_t sintetica_crc_config(void);
/* El texto que se firma para una OTA. Devuelve el largo (0 si no entra). */
size_t sintetica_mensaje_ota(const sintetica_manifiesto* m, char* salida, size_t cap);

/* Lo que se ve en el broche dormido: 1 = la pantalla apagada (quiere dormir). */
int sintetica_quiere_dormir(void);

/* Valida una config de usuario (sin tocar la instancia). 0 = válida; si no,
 * el código de error y el detalle en `detalle`. No hace falta haber llamado a
 * sintetica_iniciar. */
int sintetica_validar_config(const char* json, size_t n, char* detalle, size_t cap);

/* Dibuja una expresión paramétrica (del motor de vida) como la ve el editor,
 * sin tocar la instancia: la de la config `json` (n bytes; NULL o n = 0 = solo
 * las de fábrica) que se llama `nombre`, en el instante `t` (segundos: fases
 * del giro, el latido, el brillo), con una partícula de muestra si la cara
 * suelta alguna. `rgba` recibe ancho × alto × 4 bytes (ancho y alto de 16 a 640).
 * Devuelve 0 si la dibujó; -1 si no hay una expresión paramétrica con ese
 * nombre; > 0 si la config no es válida (el código, con el detalle en `detalle`).
 * No hace falta haber llamado a sintetica_iniciar. */
int sintetica_dibujar_expresion(const char* json, size_t n, const char* nombre, int ancho, int alto, int redonda,
                                float t, uint8_t* rgba, char* detalle, size_t cap);

/* Como sintetica_dibujar_expresion, con un parpadeo: `cierre` de 0 (abierto) a
 * 1 (cerrado). Una cara de imagen ("sprites") se dibuja con los sprites
 * sueltos (sintetica_sprite_suelto); si no está el de su ojo, con la SDF. */
int sintetica_dibujar_expresion_cierre(const char* json, size_t n, const char* nombre, int ancho, int alto, int redonda,
                                       float t, float cierre, uint8_t* rgba, char* detalle, size_t cap);

/* Los sprites que usa sintetica_dibujar_expresion (aparte de los del bicho de
 * la instancia): `sprite` de 0 a 11, un QOI de 3 o 4 canales de hasta
 * 128 × 128. n = 0 lo borra. Devuelve 0 si lo guardó, -1 si no es válido. */
int sintetica_sprite_suelto(int sprite, const uint8_t* qoi, size_t n);

/* Lo de fábrica del motor de vida (shared/formato/vida): tipo 0 = las
 * expresiones paramétricas, 1 = los comportamientos. Devuelve cuántos hay; si
 * `indice` está en rango, deja en *nombre y *json los de ese (el JSON, como en
 * la config). */
int sintetica_vida_base(int tipo, int indice, const char** nombre, const char** json);

int sintetica_cantidad_expresiones(void);
const char* sintetica_nombre_expresion(int indice);
/* La expresión visible, o "animacion:<nombre>" mientras se ve una animación. */
const char* sintetica_expresion_visible(void);
const char* sintetica_qr(void);
/* La imagen que se ve (-1 = una cara). */
int sintetica_imagen_visible(void);
const char* sintetica_version(void);

/* v1.8, para un anfitrión que es el bicho (el bicho-celu). */
/* El comportamiento que corre ("" = ninguno). Vale hasta el próximo tick. */
const char* sintetica_comportamiento(void);
/* El estado interno del motor de vida (energía, ánimo, afecto, aburrimiento,
 * curiosidad, sueño, estrés; de 0 a 1). Devuelve cuántos hay y copia hasta cap. */
int sintetica_estado_interno(float* salida, int cap);
/* La voz de la config: volumen y micrófono en centésimos, tipo (0 grave,
 * 1 media, 2 aguda, 3 robot) y la palabra que lo llama (su nombre si no hay).
 * Devuelve 1 si conversa (voz.conversar). Cualquier puntero puede ser NULL. */
int sintetica_voz(int* volumen, int* microfono, int* tipo, const char** palabra);
/* Cuántos cuadros por segundo hacen falta ahora (dormido, menos). */
int sintetica_fps_sugerido(void);
/* 0 = mantener apretado no muestra el QR (por defecto sí). */
void sintetica_qr_por_mantener(int si);
/* 1 = nunca dibuja la pantalla del QR (el escritorio se vincula solo, desde
 * su menú): sin vínculo se ve la cara; el vínculo anda igual. */
void sintetica_sin_pantalla_qr(int si);
/* 1 = no dibuja el globo: lo dibuja el anfitrión con sintetica_globo(). */
void sintetica_globo_afuera(int si);
/* Muestra el QR para vincular otro celu un rato (como mantener el botón). */
void sintetica_mostrar_qr(void);
/* El texto del globo que se ve (lo que dice), "" si no hay. */
const char* sintetica_globo(void);
/* Algo que mirar (una cara que ve la cámara), de -1 a 1; y hacia arriba es +. */
void sintetica_seguir(float x, float y);
/* La superposición de depuración del núcleo (comportamiento, estado, fps). */
void sintetica_depurar(int si);

/* ---- v1.10: la charla sin IA y la voz propia ---------------------------- */

/* Lo que entendió (el motor de charla, firmware/nucleo/charla.h). */
typedef struct sintetica_charla {
  char intencion[25];      /* "alarma", "saludo"... ("" = no entendió) */
  int confianza;           /* 0-100 */
  int no_entendi;          /* 1 = no entendió (o es de las que contesta mejor la IA) */
  int falta;               /* 1 = preguntó lo que faltaba ("¿a qué hora?") */
  int accion;              /* 0 ninguna, 1 alarma, 2 cancelar-alarma, 3 posponer, 4 apagar-alarma,
                              5 recordatorio, 6 habito, 7 jugar, 8 modo, 9 gusto, 10 olvidar-gusto,
                              11 toque, 12 musica, 13 dormir, 14 despertar, 15 dueno, 16 volumen,
                              17 callar, 18 animo */
  char accion_nombre[16];  /* el de arriba, en texto */
  int dato;                /* el juego, el modo, el toque, el hábito, ±1 de volumen o el ánimo (-1 = nada) */
  int minuto;              /* del día (-1 = no dijo hora) */
  int en_minutos;          /* "en 10 minutos" (-1 = no) */
  int dia;                 /* días desde hoy (-1 = no dijo) */
  int semana;              /* 0 lunes ... 6 domingo (-1) */
  int dias;                /* repetir: bit 0 lunes ... bit 6 domingo (0 = una vez) */
  char texto[41];          /* el recordatorio, el gusto, el nombre, a quién */
  char frase[61];          /* lo que contesta */
  char expresion[25];      /* "" = la de hablar */
  char comportamiento[25]; /* "" = ninguno */
  int confirma;            /* 1 = no estaba seguro y preguntó antes de hacerla ("¿Te llamás Zoe?", con falta = 1):
                              con "sí", la hace; con "no", nada (v1.11) */
} sintetica_charla;

/* El robot (la instancia) charla: entiende `texto` (lo que dijiste, ya en
 * texto: n bytes UTF-8, hasta 200), hace la acción (alarma, recordatorio,
 * hábito, juego, modo...), pone la expresión y el globo y lo dice con su voz
 * (hal.audio). Síncrona, sin red: unos pocos ms. Devuelve 1 si entendió y ya
 * hizo todo; 0 si no entendió (no dice nada, pone cara de pensar: que conteste
 * la nube o la IA; la frase de "no entendí" queda en salida->frase por si no
 * hay nube: sintetica_decir); < 0 si los parámetros están mal. Si hizo una
 * acción, la avisa (0xCC) por hal.notificar y hal.notificar_nube. `salida`
 * puede ser NULL. */
int sintetica_charlar(const char* texto, size_t n, sintetica_charla* salida);

/* Muestra `texto` (1 a 60 bytes UTF-8) en el globo con esa expresión (NULL o
 * "" = la de hablar), y lo dice con su voz: lo mismo que el comando 0x40 Decir.
 * 0 si lo dijo; -1 si el texto está mal; -2 si la expresión no existe. */
int sintetica_decir(const char* texto, const char* expresion);

/* La voz propia (con el callback de audio). 0 = apagada (por ejemplo, si
 * llega audio de la nube); 1 = todo (por defecto: lee los globos y balbucea);
 * 2 = solo sin texto: balbucea murmullos, hablar dormido, sorpresas cortas, el
 * "mm" al pensar y la reacción a un toque, pero nunca lee un globo. Con 1 o 2,
 * los sonidos de shared/formato/vida/caracter.json "voz.sonidos" (murmullo,
 * risa, tarareo, hip, achis, ayuda, llamar, festejo, ronroneo) salen por la voz
 * y NO por el callback de sonido; los demás (ronquido, despertador:N,
 * recordatorio, toque, ...) siguen por sonido. Ver docs/motor-de-vida.md. */
void sintetica_voz_propia(int modo);

/* El motor de charla suelto (sin la instancia, para la nube por cgo): lo que
 * sabe quien llama, y el contexto entre frases en `estado` (un blob de
 * SINTETICA_LARGO_ESTADO_CHARLA bytes que guarda quien llama, en ceros la
 * primera vez). Mismo valor de retorno que sintetica_charlar; no hace nada. */
#define SINTETICA_LARGO_ESTADO_CHARLA 96
typedef struct sintetica_charla_ctx {
  const char* dueno;        /* todo puede ser NULL */
  const char* robot;
  const char* preset;       /* "grunon", "carinoso"...: las variantes de las respuestas */
  int minuto_del_dia;       /* -1 = no se sabe */
  uint32_t epoch;           /* hora local (0 = no se sabe el día) */
  const char* clima;        /* "11° y despejado" */
  const char* clima_manana;
  const char* agenda;       /* "Dentista a las 15" */
  const char* agenda_manana;
  const char* musica;       /* "Muchachos, de La Mosca" */
  const char* habitos[4];
  int n_habitos;
  const char* gustos;       /* separados por '
' */
  int dias_juntos;          /* -1 = no se sabe */
  int alarma_sonando;
  uint32_t segundos;        /* un reloj que avanza (el contexto dura 90 s) */
  uint32_t semilla;
} sintetica_charla_ctx;
int sintetica_charlar_con(const sintetica_charla_ctx* ctx, const char* texto, size_t n, uint8_t* estado,
                          sintetica_charla* salida);

/* La voz propia suelta (sin la instancia): empieza a decir `texto` con la voz
 * `tipo` (0 grave, 1 media, 2 aguda, 3 robot), el volumen (0-100) y el estado
 * (energía y ánimo, 0 a 1); devuelve cuántas muestras va a durar. Después,
 * sintetica_voz_generar de a bloques (devuelve menos que n al terminar). */
uint32_t sintetica_voz_empezar(const char* texto, int tipo, int volumen, float energia, float animo);
size_t sintetica_voz_generar(int16_t* muestras, size_t n);

#ifdef __cplusplus
}
#endif

#endif
