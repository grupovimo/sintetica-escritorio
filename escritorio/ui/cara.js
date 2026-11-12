// La cara: pide al núcleo (que vive en Rust) el último cuadro y lo dibuja.
// Un toque es tocarlo, dejar apretado es acariciarlo, arrastrar la mueve.
const { invoke } = window.__TAURI__.core;
const { listen } = window.__TAURI__.event;
const ventana = window.__TAURI__.window.getCurrentWindow();

const $ = (id) => document.getElementById(id);
const lienzo = $('cara');
const g = lienzo.getContext('2d');
let ultimo = 0;

async function dibujar() {
  try {
    const b = await invoke('cuadro', { desde: ultimo });
    if (b.byteLength > 8) {
      const v = new DataView(b);
      ultimo = v.getUint32(0, true);
      const ancho = v.getUint16(4, true), alto = v.getUint16(6, true);
      if (lienzo.width !== ancho || lienzo.height !== alto) {
        lienzo.width = ancho;
        lienzo.height = alto;
      }
      g.putImageData(new ImageData(new Uint8ClampedArray(b, 8, ancho * alto * 4), ancho, alto), 0, 0);
    }
  } catch { /* la app se está cerrando */ }
  requestAnimationFrame(dibujar);
}
requestAnimationFrame(dibujar);

// Tocar o mover: si el puntero se corre más de 4 px antes de soltar, es arrastrar.
let apretado = null;
$('pantalla').addEventListener('pointerdown', (e) => {
  if (e.button !== 0 || e.target.closest('.sumar, .botones')) return;
  apretado = { x: e.screenX, y: e.screenY, tocando: true };
  invoke('tocar', { apretado: true });
});
window.addEventListener('pointermove', (e) => {
  if (!apretado?.tocando) return;
  if (Math.hypot(e.screenX - apretado.x, e.screenY - apretado.y) > 4) {
    apretado.tocando = false;
    invoke('tocar', { apretado: false });
    ventana.startDragging();
  }
});
window.addEventListener('pointerup', () => {
  if (apretado?.tocando) invoke('tocar', { apretado: false });
  apretado = null;
});

// Sumarlo a la cuenta (con Telegram) y el puntito de la nube.
async function revisar() {
  const e = await invoke('como_esta');
  $('sumar').hidden = e.sumado;
  $('punto').classList.toggle('en-linea', e.enLinea);
  $('punto').title = e.enLinea ? 'En la nube' : 'Sin nube';
}
setInterval(revisar, 3000);
revisar();

// La licencia (pago único): después de la prueba, el botón para comprarlo.
async function revisarLicencia() {
  const l = await invoke('licencia');
  $('compra').hidden = !(l.venta && l.licencia === false && l.enPrueba === false);
  if (l.precio) $('comprar').title = `Comprarlo · $ ${l.precio.toLocaleString('es-AR')} (pago único)`;
}
setInterval(revisarLicencia, 30 * 60 * 1000);
revisarLicencia();
$('comprar').addEventListener('click', () => invoke('comprar').then(() => setTimeout(revisarLicencia, 60000)));

$('entrar').addEventListener('click', () => {
  $('entrar').hidden = true;
  $('paso').textContent = '…';
  invoke('entrar');
});

$('form-codigo').addEventListener('submit', (e) => {
  e.preventDefault();
  invoke('codigo', { codigo: $('codigo').value });
  $('form-codigo').hidden = true;
  $('paso').textContent = '…';
});

listen('entrada', ({ payload: p }) => {
  $('form-codigo').hidden = p.paso !== 'codigo';
  $('entrar').hidden = p.paso === 'telegram' || p.paso === 'codigo';
  $('paso').textContent = { telegram: 'Seguí en Telegram ↗', codigo: '', listo: '✓', error: '✕' }[p.paso] ?? '';
  if (p.paso === 'codigo') $('codigo').focus();
  if (p.paso === 'error') {
    $('paso').title = p.dato;
    $('entrar').hidden = false;
  }
  if (p.paso === 'listo') setTimeout(() => { revisar(); revisarLicencia(); }, 500);
});

// Doble clic: el panel (el día y la charla).
$('pantalla').addEventListener('dblclick', (e) => {
  if (!e.target.closest('.sumar, .botones')) invoke('panel');
});
$('b-hablar').addEventListener('click', () => invoke('panel'));
$('b-esconder').addEventListener('click', () => invoke('esconder'));
