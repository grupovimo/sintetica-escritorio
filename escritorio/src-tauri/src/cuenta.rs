//! Sumar el robot a una cuenta, como android-bicho/herramientas/sumar.py:
//! entrar con Telegram, pedir las credenciales (POST /v1/bichos/reclamar) y
//! dárselas al núcleo. La app es a la vez la app y el bicho.
use std::time::{Duration, Instant};

use serde_json::{json, Value};

pub const PRODUCCION: &str = "https://api-sintetica.grupovimo.com.ar";

pub fn api() -> String {
    std::env::var("SINTETICA_API").unwrap_or_else(|_| PRODUCCION.into()).trim_end_matches('/').to_string()
}

fn agente() -> ureq::Agent {
    ureq::AgentBuilder::new().timeout(Duration::from_secs(40)).build()
}

fn cuerpo(r: Result<ureq::Response, ureq::Error>) -> Result<(u16, Value), String> {
    match r {
        Ok(r) => Ok((r.status(), r.into_json().unwrap_or(Value::Null))),
        Err(ureq::Error::Status(c, r)) => Ok((c, r.into_json().unwrap_or(Value::Null))),
        Err(e) => Err(format!("sin conexión con la nube ({e})")),
    }
}

/// Una entrada con Telegram en curso: el link para abrir y con qué esperarla.
pub struct Entrada {
    pub enlace: String,
    token: String,
    secreto: String,
}

pub fn empezar_entrada() -> Result<Entrada, String> {
    let (estado, e) = cuerpo(agente().post(&format!("{}/v1/sesion/telegram", api())).send_json(json!({"cliente": "app"})))?;
    if estado != 201 {
        return Err(format!("no pude pedir la entrada con Telegram ({estado})"));
    }
    let s = |k: &str| e[k].as_str().unwrap_or_default().to_string();
    Ok(Entrada { enlace: s("enlace"), token: s("token"), secreto: s("secreto") })
}

pub enum Espera {
    Lista(String),
    PideCodigo,
}

/// Espera a que la persona toque Start en el bot (hasta 10 minutos).
pub fn esperar_entrada(e: &Entrada) -> Result<Espera, String> {
    let hasta = Instant::now() + Duration::from_secs(600);
    while Instant::now() < hasta {
        let url = format!("{}/v1/sesion/telegram/{}?esperar=25", api(), e.token);
        let (estado, r) = cuerpo(agente().get(&url).set("X-Sintetica-Secreto", &e.secreto).call())?;
        if estado != 200 {
            return Err(format!("la entrada con Telegram falló ({estado})"));
        }
        if let Some(t) = r["token"].as_str() {
            return Ok(Espera::Lista(t.to_string()));
        }
        if r["estado"] == "confirmar" {
            return Ok(Espera::PideCodigo);
        }
    }
    Err("se venció la entrada con Telegram".into())
}

/// El código que el bot le mandó (cuando Telegram pide confirmar).
pub fn mandar_codigo(e: &Entrada, codigo: &str) -> Result<(), String> {
    let url = format!("{}/v1/sesion/telegram/{}/codigo", api(), e.token);
    cuerpo(agente().post(&url).set("X-Sintetica-Secreto", &e.secreto).send_json(json!({"codigo": codigo})))?;
    Ok(())
}

/// Credenciales MQTT para este robot: (url, usuario, clave).
pub fn reclamar(sesion: &str, id: &str, perfil: &str) -> Result<(String, String, String), String> {
    let (estado, r) = cuerpo(
        agente()
            .post(&format!("{}/v1/bichos/reclamar", api()))
            .set("Authorization", &format!("Bearer {sesion}"))
            .send_json(json!({"id": id, "perfil": perfil})),
    )?;
    if estado != 200 {
        return Err(format!("la nube no dio las credenciales ({estado}: {})", r["error"].as_str().unwrap_or("")));
    }
    let m = &r["mqtt"];
    let s = |k: &str| m[k].as_str().unwrap_or_default().to_string();
    Ok((s("url"), s("usuario"), s("clave")))
}

/// La licencia del escritorio (GET /v1/escritorio): {licencia, enPrueba, venta, precio…}.
pub fn licencia(sesion: &str) -> Result<Value, String> {
    let (estado, r) = cuerpo(
        agente().get(&format!("{}/v1/escritorio", api())).set("Authorization", &format!("Bearer {sesion}")).call(),
    )?;
    if estado != 200 {
        return Err(format!("no pude ver la licencia ({estado})"));
    }
    Ok(r)
}

/// El link de pago de la licencia (POST /v1/escritorio/comprar).
pub fn comprar(sesion: &str) -> Result<String, String> {
    let (estado, r) = cuerpo(
        agente().post(&format!("{}/v1/escritorio/comprar", api())).set("Authorization", &format!("Bearer {sesion}")).call(),
    )?;
    match r["url"].as_str() {
        Some(u) if estado == 201 => Ok(u.to_string()),
        _ => Err(r["error"].as_str().unwrap_or("no se pudo comprar").to_string()),
    }
}

/// Un GET de la cuenta (la config del robot, la agenda, el clima).
pub fn pedir(sesion: &str, ruta: &str) -> Result<Value, String> {
    let (estado, r) =
        cuerpo(agente().get(&format!("{}{ruta}", api())).set("Authorization", &format!("Bearer {sesion}")).call())?;
    if estado != 200 {
        return Err(format!("{ruta}: {estado}"));
    }
    Ok(r)
}
