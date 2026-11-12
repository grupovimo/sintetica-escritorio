//! Lo que la cara nota de la compu y cómo se mueve en ella:
//! - te mira: los ojos siguen al mouse por toda la pantalla;
//! - nota si estás: sin teclado ni mouse un rato, "te fuiste" (0x49, como la
//!   presencia del celu: se aburre y le da sueño); al volver, "llegaste";
//! - camina sola por el piso (el borde de abajo, arriba del dock o la barra),
//!   de vez en cuando y sin molestar: no si la estás usando, si el panel está
//!   abierto o si la dejaste en otro lado;
//! - los clics pasan de largo por lo transparente: solo la cara (y sus botones,
//!   con el mouse encima) atrapa el mouse.
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::time::{Duration, Instant};

use tauri::{AppHandle, Manager, PhysicalPosition, WebviewWindow};

use crate::nucleo::Nucleo;

// Sin tocar nada este rato, te fuiste.
const TE_FUISTE: Duration = Duration::from_secs(5 * 60);
// Cada cuánto, como mucho, sale a caminar (al azar entre los dos).
const PASEO_MIN: u64 = 90;
const PASEO_MAX: u64 = 240;
// Píxeles lógicos por segundo al caminar.
const VELOCIDAD: f64 = 70.0;
// Si la soltás a menos de esto del piso, se acomoda en el piso.
const IMAN: f64 = 80.0;
// Alrededor de la cara, lo que también cuenta como "encima".
const MARGEN: f64 = 10.0;

/// Lo que comparte con el evento de ventana movida (main.rs).
pub struct Movida {
    // Mientras la mueve la cara misma, los Moved no son un arrastre.
    pub yo: AtomicBool,
    // Cuándo la arrastraste por última vez (ms desde que arrancó; 0 = no).
    pub arrastre: AtomicU64,
    pub inicio: Instant,
}

impl Default for Movida {
    fn default() -> Self {
        Movida { yo: AtomicBool::new(false), arrastre: AtomicU64::new(0), inicio: Instant::now() }
    }
}

impl Movida {
    pub fn arrastrada(&self) {
        if !self.yo.load(Ordering::Relaxed) {
            self.arrastre.store(self.inicio.elapsed().as_millis() as u64 + 1, Ordering::Relaxed);
        }
    }
}

struct Piso {
    x0: f64,
    x1: f64,
    y: f64,
}

/// El piso de la pantalla de la cara, en píxeles físicos (x de la ventana: de x0 a x1).
fn piso(v: &WebviewWindow) -> Option<Piso> {
    let m = v.current_monitor().ok()??;
    let t = v.outer_size().ok()?;
    let a = m.work_area();
    Some(Piso {
        x0: a.position.x as f64,
        x1: (a.position.x + a.size.width as i32) as f64 - t.width as f64,
        y: (a.position.y + a.size.height as i32) as f64 - t.height as f64,
    })
}

fn mover(v: &WebviewWindow, m: &Movida, x: f64, y: f64) {
    m.yo.store(true, Ordering::Relaxed);
    let _ = v.set_position(PhysicalPosition::new(x.round() as i32, y.round() as i32));
    m.yo.store(false, Ordering::Relaxed);
}

/// Camina de donde está hasta x (en el piso), mirando para donde va.
fn caminar(v: &WebviewWindow, m: &Movida, nucleo: &Nucleo, hasta: f64) {
    let (Ok(p), Ok(f)) = (v.outer_position(), v.scale_factor()) else { return };
    let (desde, y) = (p.x as f64, p.y as f64);
    let dist = (hasta - desde).abs();
    let dura = dist / (VELOCIDAD * f);
    let dir = if hasta > desde { 0.9 } else { -0.9 };
    let t0 = Instant::now();
    let mut mirada = Instant::now() - Duration::from_secs(2);
    loop {
        let t = (t0.elapsed().as_secs_f64() / dura).min(1.0);
        // Arranca y frena suave.
        let s = t * t * (3.0 - 2.0 * t);
        mover(v, m, desde + (hasta - desde) * s, y);
        if mirada.elapsed() >= Duration::from_secs(1) {
            nucleo.seguir(dir, -0.15);
            mirada = Instant::now();
        }
        if t >= 1.0 {
            break;
        }
        std::thread::sleep(Duration::from_millis(16));
    }
}

fn guardar_lugar(archivo: &PathBuf, v: &WebviewWindow) {
    if let Ok(p) = v.outer_position() {
        let _ = std::fs::write(archivo, format!("{},{}", p.x, p.y));
    }
}

/// Donde la dejaste la última vez (si sigue adentro de alguna pantalla); si no, el piso a la derecha.
fn acomodar(v: &WebviewWindow, m: &Movida, archivo: &PathBuf) {
    let guardado = std::fs::read_to_string(archivo).ok().and_then(|s| {
        let (x, y) = s.trim().split_once(',')?;
        Some((x.parse::<i32>().ok()?, y.parse::<i32>().ok()?))
    });
    if let Some((x, y)) = guardado {
        let adentro = v.available_monitors().unwrap_or_default().iter().any(|mo| {
            let (p, t) = (mo.position(), mo.size());
            x >= p.x && y >= p.y && x < p.x + t.width as i32 - 40 && y < p.y + t.height as i32 - 40
        });
        if adentro {
            return mover(v, m, x as f64, y as f64);
        }
    }
    if let Some(p) = piso(v) {
        let f = v.scale_factor().unwrap_or(1.0);
        mover(v, m, p.x1 - 40.0 * f, p.y);
    }
}

pub fn arrancar(app: AppHandle, nucleo: Nucleo, carpeta: PathBuf) {
    std::thread::Builder::new()
        .name("sentidos".into())
        .spawn(move || {
            let Some(v) = app.get_webview_window("cara") else { return };
            let m = app.state::<Movida>();
            let lugar = carpeta.join("lugar");
            acomodar(&v, &m, &lugar);
            let mut cursor = PhysicalPosition::new(f64::NAN, f64::NAN);
            let mut presente = true;
            let mut traspasa: Option<bool> = None;
            let mut encima = false;
            let mut proximo_paseo = Instant::now() + Duration::from_secs(rand::random::<u64>() % 60 + 60);
            loop {
                std::thread::sleep(Duration::from_millis(50));
                if !v.is_visible().unwrap_or(false) {
                    continue;
                }
                let inactivo = user_idle::UserIdle::get_time().map(|t| t.duration()).unwrap_or_default();

                // Presencia: el "te fuiste" después de un rato; el "llegaste" con el primer movimiento.
                if presente && inactivo >= TE_FUISTE {
                    presente = false;
                    nucleo.presencia(false);
                } else if !presente && inactivo < Duration::from_secs(2) {
                    presente = true;
                    nucleo.presencia(true);
                }

                let (Ok(p), Ok(t), Ok(f)) = (v.outer_position(), v.outer_size(), v.scale_factor()) else { continue };
                let Ok(c) = app.cursor_position() else { continue };
                let (x, y, w, h) = (p.x as f64, p.y as f64, t.width as f64, t.height as f64);

                // Te mira: hacia el mouse, relativo al centro de la cara y al tamaño de la pantalla.
                if (c.x - cursor.x).abs() + (c.y - cursor.y).abs() > 2.0 {
                    cursor = c;
                    if let Ok(Some(mo)) = v.current_monitor() {
                        let (mw, mh) = (mo.size().width as f64, mo.size().height as f64);
                        let dx = ((c.x - (x + w / 2.0)) / (mw * 0.4)).clamp(-1.0, 1.0);
                        let dy = (((y + h / 2.0) - c.y) / (mh * 0.4)).clamp(-1.0, 1.0);
                        nucleo.seguir(dx as f32, dy as f32);
                    }
                }

                // Los clics: solo sobre la cara (o, con el mouse ya encima, toda la ventana para los botones).
                let adentro = c.x >= x && c.y >= y && c.x < x + w && c.y < y + h;
                let caja = {
                    let q = nucleo.cuadro.lock().unwrap();
                    q.caja.map(|(x0, y0, x1, y1)| {
                        let (ex, ey) = (w / q.ancho.max(1) as f64, h / q.alto.max(1) as f64);
                        let mg = MARGEN * f;
                        (x + x0 as f64 * ex - mg, y + y0 as f64 * ey - mg, x + (x1 + 1) as f64 * ex + mg, y + (y1 + 1) as f64 * ey + mg)
                    })
                };
                let sobre_cara = caja.is_some_and(|(a, b, cc, d)| c.x >= a && c.y >= b && c.x < cc && c.y < d);
                encima = adentro && (encima || sobre_cara);
                if traspasa != Some(!encima) {
                    traspasa = Some(!encima);
                    let _ = v.set_ignore_cursor_events(!encima);
                }

                // La soltaste: si quedó cerca del piso, se acomoda en el piso; y se acuerda dónde.
                let arrastre = m.arrastre.load(Ordering::Relaxed);
                if arrastre > 0 && m.inicio.elapsed().as_millis() as u64 > arrastre + 600 {
                    m.arrastre.store(0, Ordering::Relaxed);
                    if let Some(pi) = piso(&v) {
                        if (y - pi.y).abs() < IMAN * f && y != pi.y {
                            mover(&v, &m, x.clamp(pi.x0, pi.x1), pi.y);
                        }
                    }
                    guardar_lugar(&lugar, &v);
                    proximo_paseo = Instant::now() + Duration::from_secs(PASEO_MAX);
                }

                // Sale a caminar: si está en el piso, estás, no la estás tocando y el panel está cerrado.
                if Instant::now() >= proximo_paseo {
                    proximo_paseo =
                        Instant::now() + Duration::from_secs(PASEO_MIN + rand::random::<u64>() % (PASEO_MAX - PASEO_MIN));
                    let panel = app.get_webview_window("panel").is_some_and(|pa| pa.is_visible().unwrap_or(false));
                    let Some(pi) = piso(&v) else { continue };
                    if presente && !encima && !panel && (y - pi.y).abs() <= 2.0 && arrastre == 0 {
                        let ancho = pi.x1 - pi.x0;
                        let hasta = pi.x0 + rand::random::<f64>() * ancho;
                        if (hasta - x).abs() > 150.0 * f {
                            caminar(&v, &m, &nucleo, hasta);
                            guardar_lugar(&lugar, &v);
                        }
                    }
                }
            }
        })
        .unwrap();
}
