//! La cara sin caja: el fondo del cuadro del núcleo pasa a transparente para
//! que floten solo los ojos y la boca (con su brillo) sobre el escritorio.
//!
//! Se rellena desde los bordes por lo que se parece al fondo (el color de la
//! esquina): así el texto oscuro del globo, que está encerrado por el globo
//! claro, no se borra. En lo rellenado, la opacidad es cuánto se aleja del
//! fondo, y el color se "despremultiplica": el brillo naranja queda naranja y
//! semitransparente, sin un halo negro.

// Desde esta distancia al fondo (0-255, el canal que más se aleja) ya es opaco.
const UMBRAL: i32 = 160;

/// Lo vuelve transparente en el lugar y devuelve la caja de lo que se ve
/// (x0, y0, x1, y1 en píxeles del cuadro), o None si no queda nada.
pub fn sacar(rgba: &mut [u8], ancho: usize, alto: usize) -> Option<(u16, u16, u16, u16)> {
    if ancho == 0 || alto == 0 || rgba.len() < ancho * alto * 4 {
        return None;
    }
    let fondo = [rgba[0] as i32, rgba[1] as i32, rgba[2] as i32];
    let distancia = |i: usize| -> i32 {
        (0..3).map(|c| (rgba[i * 4 + c] as i32 - fondo[c]).abs()).max().unwrap_or(0)
    };
    let mut visto = vec![false; ancho * alto];
    let mut pila: Vec<usize> = Vec::with_capacity(ancho * 2 + alto * 2);
    for x in 0..ancho {
        pila.push(x);
        pila.push((alto - 1) * ancho + x);
    }
    for y in 0..alto {
        pila.push(y * ancho);
        pila.push(y * ancho + ancho - 1);
    }
    while let Some(i) = pila.pop() {
        if visto[i] || distancia(i) >= UMBRAL {
            continue;
        }
        visto[i] = true;
        let (x, y) = (i % ancho, i / ancho);
        if x > 0 {
            pila.push(i - 1);
        }
        if x + 1 < ancho {
            pila.push(i + 1);
        }
        if y > 0 {
            pila.push(i - ancho);
        }
        if y + 1 < alto {
            pila.push(i + ancho);
        }
    }
    let mut caja: Option<(u16, u16, u16, u16)> = None;
    for i in 0..ancho * alto {
        let p = &mut rgba[i * 4..i * 4 + 4];
        if visto[i] {
            let d = (0..3).map(|c| (p[c] as i32 - fondo[c]).abs()).max().unwrap_or(0);
            let a = d as f32 / UMBRAL as f32;
            if a < 0.02 {
                p.copy_from_slice(&[0, 0, 0, 0]);
                continue;
            }
            for c in 0..3 {
                let v = fondo[c] as f32 + (p[c] as f32 - fondo[c] as f32) / a;
                p[c] = v.clamp(0.0, 255.0) as u8;
            }
            p[3] = (a * 255.0) as u8;
        }
        // Lo que se nota (no el brillo más tenue) arma la caja para los clics.
        if p[3] > 60 {
            let (x, y) = ((i % ancho) as u16, (i / ancho) as u16);
            caja = Some(match caja {
                None => (x, y, x, y),
                Some((x0, y0, x1, y1)) => (x0.min(x), y0.min(y), x1.max(x), y1.max(y)),
            });
        }
    }
    caja
}

#[cfg(test)]
mod pruebas {
    use super::*;

    #[test]
    fn el_fondo_se_va_y_el_globo_queda() {
        // 7×7 negro con un "globo" blanco de 5×5 que encierra un punto negro (la letra).
        let (an, al) = (7, 7);
        let mut b = vec![0u8; an * al * 4];
        for i in 0..an * al {
            b[i * 4 + 3] = 255;
        }
        for y in 1..6 {
            for x in 1..6 {
                let i = (y * an + x) * 4;
                b[i..i + 3].copy_from_slice(&[240, 235, 225]);
            }
        }
        let letra = (3 * an + 3) * 4;
        b[letra..letra + 3].copy_from_slice(&[10, 10, 10]);
        let caja = sacar(&mut b, an, al);
        assert_eq!(b[3], 0, "la esquina es transparente");
        assert_eq!(b[letra + 3], 255, "la letra adentro del globo sigue opaca");
        assert_eq!(caja, Some((1, 1, 5, 5)));
    }

    #[test]
    fn el_brillo_queda_naranja_y_a_medias() {
        let mut b = vec![0, 0, 0, 255, 80, 32, 10, 255, 0, 0, 0, 255];
        sacar(&mut b, 3, 1);
        assert!(b[7] > 100 && b[7] < 160, "semitransparente ({})", b[7]);
        assert!(b[4] > 150, "el color, despremultiplicado ({})", b[4]);
    }
}
