# escritorio

Sintética en la compu (Mac y Windows): el robot vive en la bandeja y su cara
flota arriba de todo. Es el bicho-celu (`android-bicho/`) para la compu, con
Tauri 2.

La cara y el motor de vida **no** están acá: son el núcleo C++ de
`firmware/nucleo/` (el mismo de la placa), que `build.rs` compila adentro de
la app. Esta app solo pone el anfitrión:

| Archivo | Qué hace |
|---|---|
| `src-tauri/src/nucleo.rs` | La API C (`firmware/api/sintetica_c.h`) en su hilo: el hal (hora, guardado en archivos), el tick y el último cuadro |
| `src-tauri/src/red.rs` | MQTT a la nube con las credenciales que guarda el núcleo, como `Red.kt` |
| `src-tauri/src/cuenta.rs` | Entrar con Telegram y `POST /v1/bichos/reclamar` (perfil `escritorio`), como `sumar.py` |
| `src-tauri/src/parlante.rs` | La voz propia del núcleo (16 kHz) a la salida de audio de la compu (cpal) |
| `src-tauri/src/main.rs` | La ventana de la cara, la bandeja, la consola (la web), la charla y el atajo para hablarle |
| `ui/` | La cara: dibuja los cuadros; toque, caricia y arrastre; el renglón para hablarle |

El panel (`ui/panel.*`, al lado de la cara): el día (agenda, recordatorios,
despertador y clima, de la cuenta), la charla y el renglón para hablarle. Cada
globo del núcleo llega a la charla; si es un recordatorio o el despertador,
también como notificación del sistema.

Hablarle: ⌘/Ctrl+Shift+Espacio (o doble clic en la cara) abre el panel.
Primero entiende el motor de charla del núcleo, sin red; si no, va a la nube
(`bicho/{id}/charla`), que contesta con un Decir. `cargo test` prueba la charla
y la voz contra el núcleo de verdad.

La nube por defecto es producción; `SINTETICA_API=http://localhost:8080` usa la local.

## Compilar

No se compila en la Mac: `sh herramientas/publicar-escritorio.sh` publica HEAD
en `grupovimo/sintetica-escritorio` y Actions arma el `.dmg` (universal) y el
instalador de Windows (sin firma por ahora). Un tag `escritorio-v*` allá los
publica como release.
