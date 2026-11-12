//! El robot en la nube: MQTT con las credenciales que guarda el núcleo, como
//! el bicho-celu (android-bicho/.../Red.kt). Escucha bicho/{id}/cmd y publica
//! el estado (retenido), las respuestas y bicho/{id}/conexion.
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use rumqttc::{Client, Event, LastWill, MqttOptions, Packet, QoS, Transport};

use crate::nucleo::{Anfitrion, Nucleo, CAR_ESTADO};

#[derive(Default)]
pub struct Red {
    cliente: Mutex<Option<Client>>,
    conectado: AtomicBool,
    // Cada cambio de credenciales sube la vuelta: la conexión vieja se cierra sola.
    vuelta: AtomicU32,
    id: Mutex<String>,
}

impl Anfitrion for Red {
    fn notificar_nube(&self, car: i32, datos: &[u8]) {
        if !self.conectado.load(Ordering::Relaxed) {
            return;
        }
        let Some(c) = self.cliente.lock().unwrap().clone() else { return };
        let id = self.id.lock().unwrap().clone();
        let _ = if car == CAR_ESTADO {
            c.try_publish(format!("bicho/{id}/state"), QoS::AtLeastOnce, true, datos.to_vec())
        } else {
            c.try_publish(format!("bicho/{id}/resp"), QoS::AtMostOnce, false, [&[car as u8][..], datos].concat())
        };
    }

    fn red_cambio(&self) {
        self.vuelta.fetch_add(1, Ordering::SeqCst);
    }
}

impl Red {
    pub fn en_linea(&self) -> bool {
        self.conectado.load(Ordering::Relaxed)
    }

    /// Un JSON a un tópico propio (bicho/{id}/charla…); false si no hay nube.
    pub fn publicar(&self, topico: &str, json: String) -> bool {
        if !self.en_linea() {
            return false;
        }
        let Some(c) = self.cliente.lock().unwrap().clone() else { return false };
        let id = self.id.lock().unwrap().clone();
        c.try_publish(format!("bicho/{id}/{topico}"), QoS::AtLeastOnce, false, json.into_bytes()).is_ok()
    }

    /// Sostiene la conexión para siempre: si cambian las credenciales, reconecta.
    pub fn mantener(self: Arc<Self>, nucleo: Nucleo) {
        *self.id.lock().unwrap() = nucleo.id.to_string();
        std::thread::Builder::new()
            .name("nube".into())
            .spawn(move || loop {
                let vuelta = self.vuelta.load(Ordering::SeqCst);
                match nucleo.nube() {
                    Some((url, usuario, clave)) => self.conectar(&nucleo, vuelta, &url, &usuario, &clave),
                    None => std::thread::sleep(Duration::from_secs(1)),
                }
            })
            .unwrap();
    }

    fn conectar(&self, nucleo: &Nucleo, vuelta: u32, url: &str, usuario: &str, clave: &str) {
        let id = nucleo.id.to_string();
        let (tls, resto) = match url.split_once("://") {
            Some(("mqtts", r)) => (true, r),
            Some((_, r)) => (false, r),
            None => (false, url),
        };
        let (host, puerto) = match resto.trim_end_matches('/').rsplit_once(':') {
            Some((h, p)) => (h.to_string(), p.parse().unwrap_or(if tls { 8883 } else { 1883 })),
            None => (resto.to_string(), if tls { 8883 } else { 1883 }),
        };
        let conexion = format!("bicho/{id}/conexion");
        let mut o = MqttOptions::new(id.clone(), host, puerto);
        o.set_credentials(usuario, clave);
        o.set_keep_alive(Duration::from_secs(30));
        o.set_clean_session(true);
        o.set_max_packet_size(256 * 1024, 256 * 1024);
        o.set_last_will(LastWill::new(&conexion, "0", QoS::AtLeastOnce, true));
        if tls {
            o.set_transport(Transport::tls_with_default_config());
        }
        let (c, mut eventos) = Client::new(o, 64);
        *self.cliente.lock().unwrap() = Some(c.clone());
        let cmd = format!("bicho/{id}/cmd");
        while self.vuelta.load(Ordering::SeqCst) == vuelta {
            match eventos.recv_timeout(Duration::from_secs(1)) {
                Ok(Ok(Event::Incoming(Packet::ConnAck(_)))) => {
                    let _ = c.try_subscribe(&cmd, QoS::AtLeastOnce);
                    let _ = c.try_publish(&conexion, QoS::AtLeastOnce, true, "1");
                    self.conectado.store(true, Ordering::Relaxed);
                    nucleo.conexiones(true, true);
                    if let Some(e) = nucleo.estado() {
                        let _ = c.try_publish(format!("bicho/{id}/state"), QoS::AtLeastOnce, true, e);
                    }
                    eprintln!("nube: conectado");
                }
                Ok(Ok(Event::Incoming(Packet::Publish(p)))) if p.topic == cmd && p.payload.len() >= 2 => {
                    nucleo.nube_escribir(p.payload[0] as i32, p.payload[1..].to_vec());
                }
                Ok(Ok(_)) | Err(_) => {}
                Ok(Err(e)) => {
                    if self.conectado.swap(false, Ordering::Relaxed) {
                        nucleo.conexiones(true, false);
                    }
                    eprintln!("nube: {e}");
                    std::thread::sleep(Duration::from_secs(5));
                }
            }
        }
        self.conectado.store(false, Ordering::Relaxed);
        nucleo.conexiones(true, false);
        let _ = c.disconnect();
        *self.cliente.lock().unwrap() = None;
    }
}
