//! El parlante: la voz propia del núcleo (PCM de 16 bits, 16 kHz, mono) a la
//! salida de audio de la compu, como el AudioTrack del bicho-celu. La compu
//! suele andar a 44,1 o 48 kHz: se remuestrea lineal (es un balbuceo, alcanza).
use std::collections::VecDeque;
use std::sync::{Arc, Mutex};

use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use cpal::SampleFormat;

const MUESTREO: f32 = 16_000.0;
// Como mucho 10 s encolados: si nadie escucha (sin salida de audio), no crece.
const MAXIMO: usize = 16_000 * 10;

#[derive(Clone, Default)]
pub struct Parlante {
    cola: Arc<Mutex<VecDeque<i16>>>,
}

impl Parlante {
    pub fn encolar(&self, muestras: &[i16]) {
        let mut c = self.cola.lock().unwrap();
        if c.len() + muestras.len() <= MAXIMO {
            c.extend(muestras);
        }
    }

    #[cfg(test)]
    pub fn encoladas(&self) -> usize {
        self.cola.lock().unwrap().len()
    }

    /// Abre la salida de audio en su hilo (el stream de cpal no se puede mover
    /// entre hilos). Sin salida, la voz se descarta y el robot sigue mudo.
    pub fn abrir(&self) {
        let cola = self.cola.clone();
        std::thread::Builder::new()
            .name("parlante".into())
            .spawn(move || match abrir(cola) {
                Ok(_stream) => loop {
                    std::thread::park();
                },
                Err(e) => eprintln!("SinteticaOS: sin parlante ({e})"),
            })
            .unwrap();
    }
}

/// De 16 kHz a lo que pida la compu, interpolando entre dos muestras.
struct Remuestreo {
    paso: f32,
    fase: f32,
    actual: f32,
    siguiente: f32,
}

impl Remuestreo {
    fn muestra(&mut self, cola: &mut VecDeque<i16>) -> f32 {
        self.fase += self.paso;
        while self.fase >= 1.0 {
            self.fase -= 1.0;
            self.actual = self.siguiente;
            self.siguiente = cola.pop_front().map_or(0.0, |v| v as f32 / 32768.0);
        }
        self.actual + (self.siguiente - self.actual) * self.fase
    }
}

fn abrir(cola: Arc<Mutex<VecDeque<i16>>>) -> Result<cpal::Stream, String> {
    let salida = cpal::default_host().default_output_device().ok_or("no hay salida de audio")?;
    let conf = salida.default_output_config().map_err(|e| e.to_string())?;
    let canales = conf.channels() as usize;
    let formato = conf.sample_format();
    let mut r = Remuestreo { paso: MUESTREO / conf.sample_rate().0 as f32, fase: 0.0, actual: 0.0, siguiente: 0.0 };
    let conf: cpal::StreamConfig = conf.into();
    let error = |e| eprintln!("SinteticaOS: parlante: {e}");
    let stream = match formato {
        SampleFormat::F32 => salida.build_output_stream(
            &conf,
            move |d: &mut [f32], _: &_| {
                let mut c = cola.lock().unwrap();
                for marco in d.chunks_mut(canales) {
                    marco.fill(r.muestra(&mut c));
                }
            },
            error,
            None,
        ),
        SampleFormat::I16 => salida.build_output_stream(
            &conf,
            move |d: &mut [i16], _: &_| {
                let mut c = cola.lock().unwrap();
                for marco in d.chunks_mut(canales) {
                    marco.fill((r.muestra(&mut c) * 32767.0) as i16);
                }
            },
            error,
            None,
        ),
        f => return Err(format!("formato de audio {f:?}")),
    }
    .map_err(|e| e.to_string())?;
    stream.play().map_err(|e| e.to_string())?;
    Ok(stream)
}
