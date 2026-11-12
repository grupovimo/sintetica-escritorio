//! El núcleo de SintéticaOS (firmware/api/sintetica_c.h) en su hilo. Como en
//! el bicho-celu (android-bicho/app/src/main/cpp/puente.cpp): el núcleo usa
//! memoria estática y todas sus funciones se llaman desde el mismo hilo, así
//! que lo demás le manda trabajo con `en_nucleo`.
use std::ffi::{c_char, c_int, c_void, CStr, CString};
use std::path::PathBuf;
use std::sync::mpsc::{channel, Receiver, Sender};
use std::sync::{Arc, Mutex, OnceLock};
use std::time::{Duration, Instant};

use chrono::{Local, Offset, Timelike};

use crate::parlante::Parlante;

pub const PERFIL: &str = "escritorio";
pub const ANCHO: c_int = 320;
pub const ALTO: c_int = 172;
pub const CAR_COMANDO: c_int = 2;
pub const CAR_ESTADO: c_int = 3;

#[repr(C)]
struct Hal {
    contexto: *mut c_void,
    ahora_ms: Option<extern "C" fn(*mut c_void) -> u32>,
    semilla: Option<extern "C" fn(*mut c_void) -> u32>,
    minuto_del_dia: Option<extern "C" fn(*mut c_void) -> c_int>,
    bateria: Option<extern "C" fn(*mut c_void) -> c_int>,
    guardar: Option<extern "C" fn(*mut c_void, *const c_char, *const u8, usize) -> c_int>,
    leer: Option<extern "C" fn(*mut c_void, *const c_char, *mut u8, usize) -> c_int>,
    notificar: Option<extern "C" fn(*mut c_void, c_int, *const u8, usize)>,
    registrar: Option<extern "C" fn(*mut c_void, *const c_char)>,
    identificador: *const c_char,
    notificar_nube: Option<extern "C" fn(*mut c_void, c_int, *const u8, usize)>,
    red_cambio: Option<extern "C" fn(*mut c_void)>,
    ota_empezar: *const c_void,
    ota_escribir: *const c_void,
    ota_terminar: *const c_void,
    ota_descargar: *const c_void,
    ota_cancelar: *const c_void,
    perfil: *const c_char,
    version: *const c_char,
    escribir_trozo: Option<extern "C" fn(*mut c_void, *const c_char, u32, *const u8, usize) -> c_int>,
    leer_trozo: Option<extern "C" fn(*mut c_void, *const c_char, u32, *mut u8, usize) -> c_int>,
    borrar: Option<extern "C" fn(*mut c_void, *const c_char) -> c_int>,
    epoch: Option<extern "C" fn(*mut c_void) -> u32>,
    sonido: Option<extern "C" fn(*mut c_void, *const c_char)>,
    cargando: Option<extern "C" fn(*mut c_void) -> c_int>,
    audio: Option<extern "C" fn(*mut c_void, *const i16, usize)>,
}

/// Lo que entendió el motor de charla (`sintetica_charla`, solo lo que se usa acá).
#[repr(C)]
struct Charla {
    intencion: [c_char; 25],
    confianza: c_int,
    no_entendi: c_int,
    falta: c_int,
    accion: c_int,
    accion_nombre: [c_char; 16],
    dato: c_int,
    minuto: c_int,
    en_minutos: c_int,
    dia: c_int,
    semana: c_int,
    dias: c_int,
    texto: [c_char; 41],
    frase: [c_char; 61],
    expresion: [c_char; 25],
    comportamiento: [c_char; 25],
    confirma: c_int,
}

extern "C" {
    fn sintetica_iniciar(hal: *const Hal, ancho: c_int, alto: c_int);
    fn sintetica_tick();
    fn sintetica_ancho() -> c_int;
    fn sintetica_alto() -> c_int;
    fn sintetica_pixeles_rgba(salida: *mut u8);
    fn sintetica_tacto(apretado: c_int);
    fn sintetica_ble_conexion(conectado: c_int);
    fn sintetica_ble_escribir(car: c_int, datos: *const u8, n: usize);
    fn sintetica_ble_estado(salida: *mut u8, cap: usize) -> usize;
    fn sintetica_nube_escribir(car: c_int, datos: *const u8, n: usize);
    fn sintetica_conexiones(wifi: c_int, nube: c_int);
    fn sintetica_nube(url: *mut c_char, cu: usize, usuario: *mut c_char, cus: usize, clave: *mut c_char, cc: usize) -> c_int;
    fn sintetica_qr() -> *const c_char;
    fn sintetica_mostrar_qr();
    fn sintetica_qr_por_mantener(si: c_int);
    fn sintetica_fps_sugerido() -> c_int;
    fn sintetica_charlar(texto: *const c_char, n: usize, salida: *mut Charla) -> c_int;
    fn sintetica_globo() -> *const c_char;
    fn sintetica_decir(texto: *const c_char, expresion: *const c_char) -> c_int;
}

/// Lo que el núcleo le avisa al resto de la app.
pub trait Anfitrion: Send + Sync {
    fn notificar_nube(&self, car: i32, datos: &[u8]);
    fn red_cambio(&self);
}

struct Global {
    carpeta: PathBuf,
    inicio: Instant,
    anfitrion: Arc<dyn Anfitrion>,
    parlante: Parlante,
    // Lo que avisó solo (recordatorio, despertador) y todavía no se mostró: su globo es el aviso.
    aviso: Mutex<Option<Aviso>>,
    // Lo que notifica mientras la app se escribe a sí misma "por BLE".
    capturadas: Mutex<Option<Vec<(i32, Vec<u8>)>>>,
}

static GLOBAL: OnceLock<Global> = OnceLock::new();
fn g() -> &'static Global {
    GLOBAL.get().expect("el núcleo no arrancó")
}

fn ruta(clave: *const c_char) -> PathBuf {
    // Las claves del núcleo son cortas ("config", "img/3"…): se guardan en hexadecimal.
    let c = unsafe { CStr::from_ptr(clave) }.to_bytes();
    let nombre: String = c.iter().map(|b| format!("{b:02x}")).collect();
    g().carpeta.join(nombre)
}

extern "C" fn ahora_ms(_: *mut c_void) -> u32 {
    g().inicio.elapsed().as_millis() as u32
}
extern "C" fn semilla(_: *mut c_void) -> u32 {
    rand::random()
}
extern "C" fn minuto(_: *mut c_void) -> c_int {
    let a = Local::now();
    (a.hour() * 60 + a.minute()) as c_int
}
extern "C" fn epoch(_: *mut c_void) -> u32 {
    let a = Local::now();
    (a.timestamp() + a.offset().fix().local_minus_utc() as i64) as u32
}
extern "C" fn sin_bateria(_: *mut c_void) -> c_int {
    -1
}
extern "C" fn guardar(_: *mut c_void, clave: *const c_char, d: *const u8, n: usize) -> c_int {
    let p = ruta(clave);
    let tmp = p.with_extension("nuevo");
    let datos = unsafe { std::slice::from_raw_parts(d, n) };
    // Escritura atómica: un corte de luz no deja una config a medias.
    let ok = std::fs::write(&tmp, datos).and_then(|_| std::fs::rename(&tmp, &p)).is_ok();
    ok as c_int
}
extern "C" fn leer(_: *mut c_void, clave: *const c_char, d: *mut u8, cap: usize) -> c_int {
    match std::fs::read(ruta(clave)) {
        Ok(v) if v.len() <= cap => {
            unsafe { std::ptr::copy_nonoverlapping(v.as_ptr(), d, v.len()) };
            v.len() as c_int
        }
        _ => -1,
    }
}
extern "C" fn escribir_trozo(_: *mut c_void, clave: *const c_char, off: u32, d: *const u8, n: usize) -> c_int {
    use std::io::{Seek, SeekFrom, Write};
    let datos = unsafe { std::slice::from_raw_parts(d, n) };
    let r = std::fs::OpenOptions::new().create(true).write(true).truncate(off == 0).open(ruta(clave)).and_then(|mut f| {
        f.seek(SeekFrom::Start(off as u64))?;
        f.write_all(datos)
    });
    r.is_ok() as c_int
}
extern "C" fn leer_trozo(_: *mut c_void, clave: *const c_char, off: u32, d: *mut u8, n: usize) -> c_int {
    use std::io::{Read, Seek, SeekFrom};
    let Ok(mut f) = std::fs::File::open(ruta(clave)) else { return -1 };
    if f.seek(SeekFrom::Start(off as u64)).is_err() {
        return 0;
    }
    let buf = unsafe { std::slice::from_raw_parts_mut(d, n) };
    let mut leidos = 0;
    while leidos < n {
        match f.read(&mut buf[leidos..]) {
            Ok(0) | Err(_) => break,
            Ok(k) => leidos += k,
        }
    }
    leidos as c_int
}
extern "C" fn borrar(_: *mut c_void, clave: *const c_char) -> c_int {
    let _ = std::fs::remove_file(ruta(clave));
    1
}
extern "C" fn notificar(_: *mut c_void, car: c_int, d: *const u8, n: usize) {
    if let Some(l) = g().capturadas.lock().unwrap().as_mut() {
        l.push((car, unsafe { std::slice::from_raw_parts(d, n) }.to_vec()));
    }
}
extern "C" fn notificar_nube(_: *mut c_void, car: c_int, d: *const u8, n: usize) {
    let d = unsafe { std::slice::from_raw_parts(d, n) };
    // Los avisos solos (protocolo-ble.md, "Eventos que avisa el bicho solo"): [0x9E][0x00][n][u32][0x00][evento]…
    if d.len() >= 9 && d[0] == 0x9E && d[7] == 0x00 {
        let aviso = match d[8] {
            0x1F => Some(Aviso::Recordatorio),
            0x21 => Some(Aviso::Despertador),
            _ => None,
        };
        if aviso.is_some() {
            *g().aviso.lock().unwrap() = aviso;
        }
    }
    g().anfitrion.notificar_nube(car, d);
}
extern "C" fn red_cambio(_: *mut c_void) {
    g().anfitrion.red_cambio();
}
extern "C" fn audio(_: *mut c_void, m: *const i16, n: usize) {
    g().parlante.encolar(unsafe { std::slice::from_raw_parts(m, n) });
}
extern "C" fn registrar(_: *mut c_void, m: *const c_char) {
    eprintln!("SinteticaOS: {}", unsafe { CStr::from_ptr(m) }.to_string_lossy());
}

type Trabajo = Box<dyn FnOnce() + Send>;

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Aviso {
    Recordatorio,
    Despertador,
}

/// Lo último que dijo en su globo (cada globo nuevo sube el número) y si era un aviso.
#[derive(Clone, Default)]
pub struct Globo {
    pub numero: u32,
    pub texto: String,
    pub aviso: Option<Aviso>,
}

/// El último cuadro (RGBA) y cuál es.
#[derive(Default)]
pub struct Cuadro {
    pub numero: u32,
    pub ancho: u16,
    pub alto: u16,
    pub rgba: Vec<u8>,
}

#[derive(Clone)]
pub struct Nucleo {
    trabajos: Sender<Trabajo>,
    pub cuadro: Arc<Mutex<Cuadro>>,
    pub globo: Arc<Mutex<Globo>>,
    pub id: Arc<str>,
}

impl Nucleo {
    /// Arranca el núcleo en su hilo, con la serie `id` y lo guardado en `carpeta`.
    pub fn arrancar(carpeta: PathBuf, id: String, anfitrion: Arc<dyn Anfitrion>, parlante: Parlante) -> Nucleo {
        std::fs::create_dir_all(&carpeta).ok();
        let _ = GLOBAL.set(Global { carpeta, inicio: Instant::now(), anfitrion, parlante, aviso: Mutex::new(None), capturadas: Mutex::new(None) });
        let (tx, rx) = channel::<Trabajo>();
        let cuadro = Arc::new(Mutex::new(Cuadro::default()));
        let c = cuadro.clone();
        let globo = Arc::new(Mutex::new(Globo::default()));
        let gl = globo.clone();
        let ident = CString::new(id.clone()).unwrap();
        std::thread::Builder::new().name("nucleo".into()).spawn(move || vivir(ident, rx, c, gl)).unwrap();
        Nucleo { trabajos: tx, cuadro, globo, id: id.into() }
    }

    /// Corre `f` en el hilo del núcleo y espera lo que devuelve.
    pub fn en_nucleo<T: Send + 'static>(&self, f: impl FnOnce() -> T + Send + 'static) -> Option<T> {
        let (tx, rx) = channel();
        self.trabajos.send(Box::new(move || { let _ = tx.send(f()); })).ok()?;
        rx.recv_timeout(Duration::from_secs(5)).ok()
    }

    pub fn tocar(&self, apretado: bool) {
        let _ = self.trabajos.send(Box::new(move || unsafe { sintetica_tacto(apretado as c_int) }));
    }

    pub fn nube_escribir(&self, car: i32, datos: Vec<u8>) {
        let _ = self.trabajos.send(Box::new(move || unsafe { sintetica_nube_escribir(car, datos.as_ptr(), datos.len()) }));
    }

    pub fn conexiones(&self, wifi: bool, nube: bool) {
        let _ = self.trabajos.send(Box::new(move || unsafe { sintetica_conexiones(wifi as c_int, nube as c_int) }));
    }

    /// La charla sin IA del núcleo, primero siempre: (1 entendió y ya
    /// contestó con su voz; 0 no entendió, con la frase de "no entendí").
    pub fn charlar(&self, texto: String) -> Option<(i32, String)> {
        self.en_nucleo(move || {
            let mut c: Charla = unsafe { std::mem::zeroed() };
            let codigo = unsafe { sintetica_charlar(texto.as_ptr() as *const c_char, texto.len(), &mut c) };
            (codigo, unsafe { CStr::from_ptr(c.frase.as_ptr()) }.to_string_lossy().into_owned())
        })
    }

    /// Un globo (y lo dice en voz alta), como el comando 0x40 Decir.
    pub fn decir(&self, frase: String) {
        let Ok(f) = CString::new(frase) else { return };
        let _ = self.trabajos.send(Box::new(move || unsafe {
            sintetica_decir(f.as_ptr(), std::ptr::null());
        }));
    }

    /// La nube guardada en el núcleo: (url, usuario, clave), si hay.
    pub fn nube(&self) -> Option<(String, String, String)> {
        self.en_nucleo(|| {
            let (mut u, mut us, mut c) = ([0 as c_char; 256], [0 as c_char; 64], [0 as c_char; 128]);
            let hay = unsafe { sintetica_nube(u.as_mut_ptr(), u.len(), us.as_mut_ptr(), us.len(), c.as_mut_ptr(), c.len()) };
            let s = |b: &[c_char]| unsafe { CStr::from_ptr(b.as_ptr()) }.to_string_lossy().into_owned();
            (hay == 1).then(|| (s(&u), s(&us), s(&c)))
        })
        .flatten()
        .filter(|n| !n.1.is_empty())
    }

    /// El estado (lo que se publica retenido en bicho/{id}/state).
    pub fn estado(&self) -> Option<Vec<u8>> {
        self.en_nucleo(|| {
            let mut b = [0u8; 512];
            let n = unsafe { sintetica_ble_estado(b.as_mut_ptr(), b.len()) };
            b[..n].to_vec()
        })
    }

    /// Lo que hace la app al vincular un bicho por BLE, pero a sí misma: pide
    /// el código del QR, se vincula con él y se guarda las credenciales MQTT
    /// (comandos 0x13 y 0x17). Devuelve un error si el núcleo dijo que no.
    pub fn sumar_a_la_nube(&self, url: String, usuario: String, clave: String) -> Result<(), String> {
        self.en_nucleo(move || {
            unsafe { sintetica_mostrar_qr() };
            let qr = unsafe { CStr::from_ptr(sintetica_qr()) }.to_string_lossy().into_owned();
            let codigo = qr.rsplit('/').next().unwrap_or("").to_string();
            if !qr.starts_with("SINTETICA:V1/") || codigo.is_empty() {
                return Err(format!("no salió el QR ({qr})"));
            }
            let corto = |s: &str| [&[s.len() as u8][..], s.as_bytes()].concat();
            let vincular = [&[0x13u8][..], codigo.as_bytes()].concat();
            let nube = [vec![0x17u8], corto(&url), corto(&usuario), corto(&clave)].concat();
            *g().capturadas.lock().unwrap() = Some(Vec::new());
            unsafe {
                sintetica_ble_conexion(1);
                sintetica_ble_escribir(CAR_COMANDO, vincular.as_ptr(), vincular.len());
                sintetica_ble_escribir(CAR_COMANDO, nube.as_ptr(), nube.len());
                sintetica_ble_conexion(0);
            }
            let notis = g().capturadas.lock().unwrap().take().unwrap_or_default();
            // Los resultados de Vincular (0x93) y Nube (0x97): [op][código]…, 0 = bien.
            for (_, d) in notis {
                if d.len() >= 2 && (d[0] == 0x93 || d[0] == 0x97) && d[1] != 0 {
                    return Err(format!("el núcleo no aceptó ({:#x}, error {:#x})", d[0], d[1]));
                }
            }
            Ok(())
        })
        .unwrap_or_else(|| Err("el núcleo no respondió".into()))
    }
}

fn vivir(id: CString, trabajos: Receiver<Trabajo>, cuadro: Arc<Mutex<Cuadro>>, globo: Arc<Mutex<Globo>>) {
    let perfil = CString::new(PERFIL).unwrap();
    let version = CString::new(concat!("escritorio-", env!("CARGO_PKG_VERSION"))).unwrap();
    let hal = Box::leak(Box::new(Hal {
        contexto: std::ptr::null_mut(),
        ahora_ms: Some(ahora_ms),
        semilla: Some(semilla),
        minuto_del_dia: Some(minuto),
        bateria: Some(sin_bateria),
        guardar: Some(guardar),
        leer: Some(leer),
        notificar: Some(notificar),
        registrar: Some(registrar),
        identificador: id.into_raw(),
        notificar_nube: Some(notificar_nube),
        red_cambio: Some(red_cambio),
        ota_empezar: std::ptr::null(),
        ota_escribir: std::ptr::null(),
        ota_terminar: std::ptr::null(),
        ota_descargar: std::ptr::null(),
        ota_cancelar: std::ptr::null(),
        perfil: perfil.into_raw(),
        version: version.into_raw(),
        escribir_trozo: Some(escribir_trozo),
        leer_trozo: Some(leer_trozo),
        borrar: Some(borrar),
        epoch: Some(epoch),
        sonido: None,
        cargando: None,
        audio: Some(audio),
    }));
    unsafe {
        sintetica_iniciar(hal, ANCHO, ALTO);
        // En la compu, dejar apretado es acariciarlo: se suma desde el menú.
        sintetica_qr_por_mantener(0);
        sintetica_conexiones(1, 0);
    }
    let mut numero = 0u32;
    loop {
        let inicio = Instant::now();
        while let Ok(t) = trabajos.try_recv() {
            t();
        }
        unsafe { sintetica_tick() };
        let dice = unsafe { CStr::from_ptr(sintetica_globo()) }.to_bytes();
        {
            let mut gl = globo.lock().unwrap();
            if gl.texto.as_bytes() != dice {
                gl.texto = String::from_utf8_lossy(dice).into_owned();
                if !dice.is_empty() {
                    gl.numero = gl.numero.wrapping_add(1);
                    gl.aviso = g().aviso.lock().unwrap().take();
                }
            }
        }
        let (w, h) = unsafe { (sintetica_ancho(), sintetica_alto()) };
        let mut rgba = vec![0u8; (w * h * 4) as usize];
        unsafe { sintetica_pixeles_rgba(rgba.as_mut_ptr()) };
        numero = numero.wrapping_add(1);
        *cuadro.lock().unwrap() = Cuadro { numero, ancho: w as u16, alto: h as u16, rgba };
        let fps = unsafe { sintetica_fps_sugerido() }.clamp(5, 60) as u64;
        let paso = Duration::from_millis(1000 / fps);
        if let Some(resto) = paso.checked_sub(inicio.elapsed()) {
            // Despierta antes si llega trabajo (un toque, un comando de la nube).
            if let Ok(t) = trabajos.recv_timeout(resto) {
                t();
            }
        }
    }
}

#[cfg(test)]
mod pruebas {
    use super::*;

    struct Nadie;
    impl Anfitrion for Nadie {
        fn notificar_nube(&self, _: i32, _: &[u8]) {}
        fn red_cambio(&self) {}
    }

    // Un solo test: el núcleo es uno por proceso.
    #[test]
    fn charla_sin_red_y_voz() {
        let carpeta = std::env::temp_dir().join(format!("sintetica-prueba-{}", std::process::id()));
        let parlante = Parlante::default();
        let n = Nucleo::arrancar(carpeta.clone(), "02AABBCCDDEE".into(), Arc::new(Nadie), parlante.clone());
        let (codigo, frase) = n.charlar("despertame mañana a las 7".into()).expect("el núcleo contesta");
        assert_eq!(codigo, 1, "entiende la alarma sin red ({frase})");
        let (codigo, frase) = n.charlar("explicame la teoría de cuerdas".into()).unwrap();
        assert_eq!(codigo, 0, "lo que no sabe va a la nube");
        assert!(!frase.is_empty(), "con la frase de no entendí");
        // Lo dice en voz alta: llegan muestras al parlante.
        n.decir("hola, soy yo".into());
        std::thread::sleep(Duration::from_millis(1500));
        assert!(parlante.encoladas() > 0, "la voz propia llega al parlante");
        let _ = std::fs::remove_dir_all(carpeta);
    }
}
