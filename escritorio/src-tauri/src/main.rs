// Sintética en la compu: el núcleo de SintéticaOS vivo todo el tiempo (aunque
// la ventana esté escondida), una cara flotante que lo muestra y el robot en
// la nube como uno más de la cuenta (recordatorios, Telegram, MCP, charla).
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod cuenta;
mod fondo;
mod nucleo;
mod parlante;
mod red;
mod sentidos;

use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use serde_json::json;
use tauri::menu::{CheckMenuItem, Menu, MenuItem, PredefinedMenuItem, Submenu};
use tauri::tray::TrayIconBuilder;
use tauri::{AppHandle, Emitter, Manager, State, WebviewUrl, WebviewWindowBuilder};
use tauri_plugin_autostart::{MacosLauncher, ManagerExt};
use tauri_plugin_global_shortcut::ShortcutState;
use tauri_plugin_notification::NotificationExt;
use tauri_plugin_opener::OpenerExt;

use nucleo::{Aviso, Nucleo};
use parlante::Parlante;
use red::Red;

const CONSOLA: &str = "https://sintetica.grupovimo.com.ar/robots";
// Para hablarle desde cualquier lado: abre el panel con el renglón para escribir (o dictar).
const ATAJO: &str = "CmdOrCtrl+Shift+Space";

struct App {
    nucleo: Nucleo,
    red: Arc<Red>,
    carpeta: PathBuf,
    entrada: Mutex<Option<Arc<cuenta::Entrada>>>,
}

/// La serie: 12 hex al azar la primera vez (una MAC "administrada localmente"), como el bicho-celu.
fn serie(carpeta: &std::path::Path) -> String {
    let f = carpeta.join("id");
    if let Ok(s) = std::fs::read_to_string(&f) {
        let s = s.trim().to_string();
        if s.len() == 12 && s.chars().all(|c| c.is_ascii_hexdigit()) {
            return s.to_uppercase();
        }
    }
    let mut mac: [u8; 6] = rand::random();
    mac[0] = (mac[0] & 0xFC) | 0x02;
    let s: String = mac.iter().map(|b| format!("{b:02X}")).collect();
    let _ = std::fs::write(&f, &s);
    s
}

/// El último cuadro si es más nuevo que `desde`: [número u32][ancho u16][alto u16][RGBA…].
#[tauri::command]
fn cuadro(app: State<App>, desde: u32) -> tauri::ipc::Response {
    let c = app.nucleo.cuadro.lock().unwrap();
    if c.numero == desde || c.rgba.is_empty() {
        return tauri::ipc::Response::new(Vec::new());
    }
    let mut b = Vec::with_capacity(8 + c.rgba.len());
    b.extend_from_slice(&c.numero.to_le_bytes());
    b.extend_from_slice(&c.ancho.to_le_bytes());
    b.extend_from_slice(&c.alto.to_le_bytes());
    b.extend_from_slice(&c.rgba);
    tauri::ipc::Response::new(b)
}

#[tauri::command]
fn tocar(app: State<App>, apretado: bool) {
    app.nucleo.tocar(apretado);
}

#[tauri::command]
fn como_esta(app: State<App>) -> serde_json::Value {
    json!({ "id": &*app.nucleo.id, "sumado": app.nucleo.nube().is_some(), "enLinea": app.red.en_linea() })
}

/// Entra con Telegram y se suma a la cuenta. Avisa los pasos con el evento "entrada".
#[tauri::command]
fn entrar(handle: AppHandle) {
    std::thread::spawn(move || {
        let avisar = |paso: &str, dato: &str| {
            let _ = handle.emit("entrada", json!({ "paso": paso, "dato": dato }));
        };
        let app = handle.state::<App>();
        let r = (|| -> Result<(), String> {
            let e = Arc::new(cuenta::empezar_entrada()?);
            *app.entrada.lock().unwrap() = Some(e.clone());
            let _ = handle.opener().open_url(&e.enlace, None::<&str>);
            avisar("telegram", &e.enlace);
            let sesion = loop {
                match cuenta::esperar_entrada(&e)? {
                    cuenta::Espera::Lista(t) => break t,
                    cuenta::Espera::PideCodigo => avisar("codigo", ""),
                }
            };
            let _ = std::fs::write(app.carpeta.join("sesion"), &sesion);
            let (url, usuario, clave) = cuenta::reclamar(&sesion, &app.nucleo.id, nucleo::PERFIL)?;
            app.nucleo.sumar_a_la_nube(url, usuario, clave)
        })();
        *app.entrada.lock().unwrap() = None;
        match r {
            Ok(()) => avisar("listo", ""),
            Err(e) => avisar("error", &e),
        }
    });
}

/// Si hay que pedir que lo compre: sumado, sin licencia y con la prueba vencida.
#[tauri::command(async)]
fn licencia(app: State<App>) -> serde_json::Value {
    let Ok(sesion) = std::fs::read_to_string(app.carpeta.join("sesion")) else { return json!({}) };
    cuenta::licencia(sesion.trim()).unwrap_or_else(|_| json!({}))
}

#[tauri::command(async)]
fn comprar(handle: AppHandle) -> Result<(), String> {
    let app = handle.state::<App>();
    let sesion = std::fs::read_to_string(app.carpeta.join("sesion")).map_err(|_| "primero sumalo a tu cuenta")?;
    let url = cuenta::comprar(sesion.trim())?;
    handle.opener().open_url(url, None::<&str>).map_err(|e| e.to_string())
}

#[tauri::command(async)]
fn codigo(app: State<App>, codigo: String) -> Result<(), String> {
    let e = app.entrada.lock().unwrap().clone().ok_or("no hay una entrada en curso")?;
    cuenta::mandar_codigo(&e, codigo.trim())
}

/// Lo que le dijiste: primero el motor de charla propio (sin red, al toque);
/// si no entiende, a la nube (bicho/{id}/charla: el motor de la nube y la IA),
/// que contesta con un Decir. Sin nube, el "no entendí" del motor.
#[tauri::command(async)]
fn charlar(app: State<App>, texto: String) -> &'static str {
    let mut texto = texto.trim().to_string();
    while texto.len() > 200 {
        texto.pop();
    }
    if texto.is_empty() {
        return "nada";
    }
    let Some((codigo, frase)) = app.nucleo.charlar(texto.clone()) else { return "error" };
    if codigo == 1 {
        return "entendio";
    }
    if app.red.publicar("charla", json!({ "texto": texto }).to_string()) {
        return "nube";
    }
    if !frase.is_empty() {
        app.nucleo.decir(frase);
    }
    "sin-nube"
}

/// Lo del día para el panel: la config del robot (recordatorios, despertador),
/// la agenda, el clima y los mails sin leer. Sin sumar a la cuenta, nada.
#[tauri::command(async)]
fn hoy(app: State<App>) -> serde_json::Value {
    let Ok(sesion) = std::fs::read_to_string(app.carpeta.join("sesion")) else { return json!({}) };
    let (sesion, id) = (sesion.trim(), &*app.nucleo.id);
    let config = cuenta::pedir(sesion, &format!("/v1/bichos/{id}/config")).map(|c| c["config"].clone());
    json!({
        "recordatorios": config.as_ref().map(|c| c["recordatorios"].clone()).unwrap_or_default(),
        "despertador": config.as_ref().map(|c| c["despertador"].clone()).unwrap_or_default(),
        "agenda": cuenta::pedir(sesion, "/v1/agenda").map(|a| a["proximos"].clone()).unwrap_or_default(),
        "clima": cuenta::pedir(sesion, &format!("/v1/bichos/{id}/clima")).map(|c| c["clima"].clone()).unwrap_or_default(),
        "correo": cuenta::pedir(sesion, "/v1/correo").map(|c| c["noLeidos"].clone()).unwrap_or_default(),
    })
}

fn sesion(app: &App) -> Result<String, String> {
    std::fs::read_to_string(app.carpeta.join("sesion")).map(|s| s.trim().to_string()).map_err(|_| "primero sumalo a tu cuenta".into())
}

/// Los robots de la cuenta (las placas, el celu y esta compu), para el panel.
#[tauri::command(async)]
fn robots(app: State<App>) -> serde_json::Value {
    let Ok(s) = sesion(&app) else { return json!([]) };
    let Ok(r) = cuenta::pedir(&s, "/v1/bichos") else { return json!([]) };
    let lista: Vec<_> = r["bichos"]
        .as_array()
        .map(|l| l.as_slice())
        .unwrap_or_default()
        .iter()
        .map(|b| {
            let id = b["id"].as_str().unwrap_or_default();
            json!({
                "id": id,
                "apodo": b["apodo"],
                "perfil": b["perfil"],
                "conectado": b["conectado"],
                "este": id == &*app.nucleo.id,
            })
        })
        .collect();
    json!(lista)
}

/// Hablarle a otro robot de la cuenta (la placa del escritorio, el celu):
/// contesta en su pantalla y la respuesta vuelve para el panel.
#[tauri::command(async)]
fn charlar_con(app: State<App>, id: String, texto: String) -> Result<String, String> {
    let s = sesion(&app)?;
    let r = cuenta::enviar(&s, "POST", &format!("/v1/bichos/{id}/charla"), json!({ "texto": texto }))?;
    Ok(r["texto"].as_str().unwrap_or_default().to_string())
}

/// Un mimo a otro robot (como acariciarlo).
#[tauri::command(async)]
fn mimo(app: State<App>, id: String) -> Result<(), String> {
    let s = sesion(&app)?;
    cuenta::enviar(&s, "POST", &format!("/v1/bichos/{id}/comando"), json!({ "evento": "caricia" })).map(|_| ())
}

/// El panel al lado de la cara: el día, la charla y el renglón para hablarle.
fn abrir_panel(app: &AppHandle) {
    let v = match app.get_webview_window("panel") {
        Some(v) => v,
        None => {
            let mut b = WebviewWindowBuilder::new(app, "panel", WebviewUrl::App("panel.html".into()))
                .title("Sintética")
                .inner_size(360.0, 560.0)
                .min_inner_size(300.0, 380.0);
            // Pegado a la derecha de la cara.
            if let Some(c) = app.get_webview_window("cara") {
                if let (Ok(p), Ok(t), Ok(f)) = (c.outer_position(), c.outer_size(), c.scale_factor()) {
                    b = b.position((p.x as f64 + t.width as f64) / f + 8.0, p.y as f64 / f);
                }
            }
            match b.build() {
                Ok(v) => v,
                Err(_) => return,
            }
        }
    };
    let _ = v.show();
    let _ = v.set_focus();
    let _ = v.emit("hablar", ());
}

#[tauri::command]
fn panel(handle: AppHandle) {
    abrir_panel(&handle);
}

#[tauri::command]
fn esconder(handle: AppHandle) {
    if let Some(v) = handle.get_webview_window("cara") {
        let _ = v.hide();
    }
}

/// Cada globo nuevo va al panel (la charla); si era un recordatorio o el
/// despertador, también como notificación del sistema.
fn mirar_globos(app: AppHandle) {
    std::thread::Builder::new()
        .name("globos".into())
        .spawn(move || {
            let globo = app.state::<App>().nucleo.globo.clone();
            let mut visto = 0;
            loop {
                std::thread::sleep(std::time::Duration::from_millis(250));
                let g = globo.lock().unwrap().clone();
                if g.numero == visto {
                    continue;
                }
                visto = g.numero;
                let _ = app.emit("dijo", json!({ "texto": g.texto, "aviso": g.aviso.is_some() }));
                if let Some(a) = g.aviso {
                    let titulo = if a == Aviso::Despertador { "Despertador" } else { "Recordatorio" };
                    let _ = app.notification().builder().title(titulo).body(&g.texto).show();
                }
            }
        })
        .unwrap();
}

fn carpeta_app<R: tauri::Runtime>(app: &impl Manager<R>) -> PathBuf {
    app.path().app_data_dir().unwrap_or_default()
}

fn abrir_consola(app: &AppHandle) {
    if let Some(v) = app.get_webview_window("consola") {
        let _ = v.show();
        let _ = v.set_focus();
        return;
    }
    let _ = WebviewWindowBuilder::new(app, "consola", WebviewUrl::External(CONSOLA.parse().unwrap()))
        .title("Sintética")
        .inner_size(1120.0, 780.0)
        .build();
}

fn alternar_cara(app: &AppHandle) {
    if let Some(v) = app.get_webview_window("cara") {
        if v.is_visible().unwrap_or(false) {
            let _ = v.hide();
        } else {
            let _ = v.show();
            let _ = v.set_focus();
        }
    }
}

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_notification::init())
        .plugin(tauri_plugin_autostart::init(MacosLauncher::LaunchAgent, None))
        .plugin(
            tauri_plugin_global_shortcut::Builder::new()
                .with_shortcuts([ATAJO])
                .expect("el atajo para hablarle")
                .with_handler(|app, _, e| {
                    if e.state == ShortcutState::Pressed {
                        abrir_panel(app);
                    }
                })
                .build(),
        )
        .setup(|app| {
            let carpeta = app.path().app_data_dir()?;
            std::fs::create_dir_all(&carpeta)?;
            let red = Arc::new(Red::default());
            let parlante = Parlante::default();
            parlante.abrir();
            let nucleo = Nucleo::arrancar(carpeta.join("nucleo"), serie(&carpeta), red.clone(), parlante);
            red.clone().mantener(nucleo.clone());
            app.manage(App { nucleo, red, carpeta, entrada: Mutex::new(None) });
            mirar_globos(app.handle().clone());
            app.manage(sentidos::Movida::default());
            let a = app.state::<App>();
            sentidos::arrancar(app.handle().clone(), a.nucleo.clone(), a.carpeta.clone());

            let mostrar = MenuItem::with_id(app, "cara", "Mostrar / esconder", true, None::<&str>)?;
            let hablar = MenuItem::with_id(app, "hablar", "Hablarle", true, Some(ATAJO))?;
            let consola = MenuItem::with_id(app, "consola", "Abrir la consola", true, None::<&str>)?;
            let sumar = MenuItem::with_id(app, "entrar", "Sumarlo a mi cuenta", true, None::<&str>)?;
            let tamano = Submenu::with_items(
                app,
                "Tamaño",
                true,
                &[
                    &MenuItem::with_id(app, "tam-200", "Chica", true, None::<&str>)?,
                    &MenuItem::with_id(app, "tam-280", "Mediana", true, None::<&str>)?,
                    &MenuItem::with_id(app, "tam-380", "Grande", true, None::<&str>)?,
                ],
            )?;
            // Abrir al prender la compu: prendido la primera vez; después, lo que elijas.
            let marca = carpeta_app(app).join("autoarranque");
            if !marca.exists() {
                let _ = app.autolaunch().enable();
                let _ = std::fs::write(&marca, "1");
            }
            let al_prender = app.autolaunch().is_enabled().unwrap_or(false);
            let arranque = CheckMenuItem::with_id(app, "arranque", "Abrir al prender la compu", true, al_prender, None::<&str>)?;
            let salir = MenuItem::with_id(app, "salir", "Salir", true, None::<&str>)?;
            let sep = PredefinedMenuItem::separator(app)?;
            let menu = Menu::with_items(app, &[&mostrar, &hablar, &consola, &sumar, &sep, &tamano, &arranque, &PredefinedMenuItem::separator(app)?, &salir])?;
            let mut bandeja = TrayIconBuilder::with_id("sintetica").menu(&menu).tooltip("Sintética");
            if let Some(i) = app.default_window_icon() {
                bandeja = bandeja.icon(i.clone());
            }
            bandeja
                .on_menu_event(|app, e| match e.id.as_ref() {
                    "cara" => alternar_cara(app),
                    "hablar" => abrir_panel(app),
                    "arranque" => {
                        let a = app.autolaunch();
                        let _ = if a.is_enabled().unwrap_or(false) { a.disable() } else { a.enable() };
                    }
                    t if t.starts_with("tam-") => {
                        if let (Some(v), Ok(ancho)) = (app.get_webview_window("cara"), t[4..].parse::<f64>()) {
                            let (m, archivo) = (app.state::<sentidos::Movida>(), carpeta_app(app).join("tamano"));
                            sentidos::tamano(&v, &m, ancho, &archivo);
                        }
                    }
                    "consola" => abrir_consola(app),
                    "entrar" => entrar(app.clone()),
                    "salir" => app.exit(0),
                    _ => {}
                })
                .build(app)?;
            Ok(())
        })
        // Cerrar la cara la esconde: el robot sigue vivo en la bandeja.
        .on_window_event(|v, e| match e {
            tauri::WindowEvent::CloseRequested { api, .. } if v.label() == "cara" => {
                api.prevent_close();
                let _ = v.hide();
            }
            tauri::WindowEvent::Moved(_) if v.label() == "cara" => {
                if let Some(m) = v.try_state::<sentidos::Movida>() {
                    m.arrastrada();
                }
            }
            _ => {}
        })
        .invoke_handler(tauri::generate_handler![cuadro, tocar, como_esta, entrar, codigo, licencia, comprar, charlar, hoy, panel, esconder, robots, charlar_con, mimo])
        .run(tauri::generate_context!())
        .expect("no arrancó Sintética");
}
