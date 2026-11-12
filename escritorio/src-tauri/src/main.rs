// Sintética en la compu: el núcleo de SintéticaOS vivo todo el tiempo (aunque
// la ventana esté escondida), una cara flotante que lo muestra y el robot en
// la nube como uno más de la cuenta (recordatorios, Telegram, MCP, charla).
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod cuenta;
mod nucleo;
mod red;

use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use serde_json::json;
use tauri::menu::{Menu, MenuItem, PredefinedMenuItem};
use tauri::tray::TrayIconBuilder;
use tauri::{AppHandle, Emitter, Manager, State, WebviewUrl, WebviewWindowBuilder};
use tauri_plugin_opener::OpenerExt;

use nucleo::Nucleo;
use red::Red;

const CONSOLA: &str = "https://sintetica.grupovimo.com.ar/robots";

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

#[tauri::command]
fn codigo(app: State<App>, codigo: String) -> Result<(), String> {
    let e = app.entrada.lock().unwrap().clone().ok_or("no hay una entrada en curso")?;
    cuenta::mandar_codigo(&e, codigo.trim())
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
        .setup(|app| {
            let carpeta = app.path().app_data_dir()?;
            std::fs::create_dir_all(&carpeta)?;
            let red = Arc::new(Red::default());
            let nucleo = Nucleo::arrancar(carpeta.join("nucleo"), serie(&carpeta), red.clone());
            red.clone().mantener(nucleo.clone());
            app.manage(App { nucleo, red, carpeta, entrada: Mutex::new(None) });

            let mostrar = MenuItem::with_id(app, "cara", "Mostrar / esconder", true, None::<&str>)?;
            let consola = MenuItem::with_id(app, "consola", "Abrir la consola", true, None::<&str>)?;
            let sumar = MenuItem::with_id(app, "entrar", "Sumarlo a mi cuenta", true, None::<&str>)?;
            let salir = MenuItem::with_id(app, "salir", "Salir", true, None::<&str>)?;
            let sep = PredefinedMenuItem::separator(app)?;
            let menu = Menu::with_items(app, &[&mostrar, &consola, &sumar, &sep, &salir])?;
            let mut bandeja = TrayIconBuilder::with_id("sintetica").menu(&menu).tooltip("Sintética");
            if let Some(i) = app.default_window_icon() {
                bandeja = bandeja.icon(i.clone());
            }
            bandeja
                .on_menu_event(|app, e| match e.id.as_ref() {
                    "cara" => alternar_cara(app),
                    "consola" => abrir_consola(app),
                    "entrar" => entrar(app.clone()),
                    "salir" => app.exit(0),
                    _ => {}
                })
                .build(app)?;
            Ok(())
        })
        // Cerrar la cara la esconde: el robot sigue vivo en la bandeja.
        .on_window_event(|v, e| {
            if let tauri::WindowEvent::CloseRequested { api, .. } = e {
                if v.label() == "cara" {
                    api.prevent_close();
                    let _ = v.hide();
                }
            }
        })
        .invoke_handler(tauri::generate_handler![cuadro, tocar, como_esta, entrar, codigo])
        .run(tauri::generate_context!())
        .expect("no arrancó Sintética");
}
