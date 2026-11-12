// El panel: el día (agenda, recordatorios, despertador), la charla con el
// robot y el renglón para hablarle. Lo que dice llega con el evento "dijo"
// (cada globo nuevo del núcleo), sea una respuesta, un aviso o algo suyo.
const { invoke } = window.__TAURI__.core;
const { listen } = window.__TAURI__.event;
const $ = (id) => document.getElementById(id);

const LETRAS = 'DLMXJVS'; // la letra de cada día en "dias" ("LMXJV"), desde el domingo
const hhmm = (d) => d.toTimeString().slice(0, 5);

function renglon(icono, hora, texto) {
  const li = document.createElement('li');
  for (const [clase, t] of [['', icono], ['hora', hora], ['', texto]]) {
    const s = document.createElement('span');
    s.className = clase;
    s.textContent = t;
    li.append(s);
  }
  if (hora && hora < hhmm(new Date())) li.classList.add('pasado');
  return li;
}

async function cargarDia() {
  const ahora = new Date();
  $('fecha').textContent = ahora.toLocaleDateString('es-AR', { weekday: 'long', day: 'numeric' });
  const d = await invoke('hoy');
  $('sin-sumar').hidden = Object.keys(d).length > 0;
  if (d.clima?.temp != null) $('clima').textContent = `${Math.round(d.clima.temp)}° · ${d.clima.descripcion}`;

  const hoyToca = (x) => x && x.activo !== false && (!x.dias || x.dias.includes(LETRAS[ahora.getDay()]));
  const items = [];
  for (const e of d.agenda || []) {
    const ini = new Date(e.inicio);
    if (ini.toDateString() === ahora.toDateString()) items.push(['📅', e.todoElDia ? '' : hhmm(ini), e.titulo]);
  }
  for (const r of d.recordatorios || []) if (hoyToca(r)) items.push(['⏰', r.hora, r.texto]);
  if (hoyToca(d.despertador)) items.push(['☀️', d.despertador.hora, 'Despertador']);
  items.sort((a, b) => a[1].localeCompare(b[1]));
  $('dia').replaceChildren(...items.map((i) => renglon(...i)));
}

// La charla: se guarda en esta compu (las últimas 60).
let charla = [];
try { charla = JSON.parse(localStorage.getItem('charla') || '[]'); } catch { /* vacía */ }
function mostrar() {
  $('charla').replaceChildren(...charla.map((m) => {
    const p = document.createElement('p');
    p.className = m.quien + (m.aviso ? ' aviso' : '');
    p.textContent = m.texto;
    return p;
  }));
  $('charla').scrollTop = $('charla').scrollHeight;
}
function sumar(m) {
  charla = [...charla, m].slice(-60);
  try { localStorage.setItem('charla', JSON.stringify(charla)); } catch { /* sin guardar */ }
  mostrar();
}
mostrar();

listen('dijo', ({ payload: p }) => {
  if (p.texto) sumar({ quien: 'el', texto: p.texto, aviso: p.aviso });
  if (p.aviso) cargarDia();
});
listen('hablar', () => $('frase').focus());

$('form').addEventListener('submit', async (e) => {
  e.preventDefault();
  const texto = $('frase').value.trim();
  if (!texto) return;
  $('frase').value = '';
  sumar({ quien: 'vos', texto });
  const r = await invoke('charlar', { texto });
  // Lo que cambia algo (una alarma, un recordatorio) se ve en el día.
  if (r === 'entendio' || r === 'nube') setTimeout(cargarDia, 3000);
});

cargarDia();
setInterval(cargarDia, 5 * 60 * 1000);
$('frase').focus();
