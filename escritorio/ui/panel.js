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
  if (hora && hora < hhmm(new Date()) && icono !== '✉️') li.classList.add('pasado');
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
  // Los mails sin leer (Gmail, Outlook), al final: los tres últimos.
  for (const m of (d.correo || []).slice(0, 3)) items.push(['✉️', hhmm(new Date(m.en)), `${m.de}: ${m.asunto}`]);
  $('dia').replaceChildren(...items.map((i) => renglon(...i)));
}

// A quién le hablás: esta compu o cualquier robot de la cuenta (la placa del
// escritorio, el celu); la respuesta sale en su pantalla y vuelve acá.
const NOMBRES = { escritorio: 'Compu', 'dev-147b': 'Unidad', 'broche-c3': 'Broche', celu: 'Celu' };
let robots = [];
let elegido = null; // null = esta compu
const nombreDe = (r) => (r.este ? 'Esta compu' : r.apodo || NOMBRES[r.perfil] || r.perfil);

async function cargarRobots() {
  robots = (await invoke('robots')) || [];
  robots.sort((a, b) => (b.este ? 1 : 0) - (a.este ? 1 : 0));
  // Con un solo robot (esta compu) no hace falta elegir.
  if (robots.filter((r) => !r.este).length === 0) return $('robots').replaceChildren();
  const chips = robots.map((r) => {
    const b = document.createElement('button');
    b.type = 'button';
    b.setAttribute('aria-pressed', String(r.este ? elegido === null : elegido === r.id));
    const punto = document.createElement('i');
    if (r.conectado || r.este) punto.className = 'en-linea';
    b.append(punto, nombreDe(r));
    b.addEventListener('click', () => {
      elegido = r.este ? null : r.id;
      cargarRobots();
      $('frase').placeholder = r.este ? 'Decile algo…' : `Decile algo a ${nombreDe(r)}…`;
      $('frase').focus();
    });
    return b;
  });
  const otro = robots.find((r) => r.id === elegido);
  if (otro?.conectado) {
    const m = document.createElement('button');
    m.type = 'button';
    m.className = 'mimo';
    m.title = `Hacerle un mimo a ${nombreDe(otro)}`;
    m.textContent = '♥';
    m.addEventListener('click', () => invoke('mimo', { id: otro.id }));
    chips.push(m);
  }
  $('robots').replaceChildren(...chips);
}

// La charla: se guarda en esta compu (las últimas 60).
let charla = [];
try { charla = JSON.parse(localStorage.getItem('charla') || '[]'); } catch { /* vacía */ }
function mostrar() {
  $('charla').replaceChildren(...charla.map((m) => {
    const p = document.createElement('p');
    p.className = m.quien + (m.aviso ? ' aviso' : '');
    if (m.robot) {
      const q = document.createElement('span');
      q.className = 'quien';
      q.textContent = m.robot;
      p.append(q);
    }
    p.append(m.texto);
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
  const otro = robots.find((r) => r.id === elegido);
  sumar({ quien: 'vos', texto, robot: otro ? `a ${nombreDe(otro)}` : undefined });
  if (otro) {
    try {
      const dijo = await invoke('charlar_con', { id: otro.id, texto });
      if (dijo) sumar({ quien: 'el', texto: dijo, robot: nombreDe(otro) });
    } catch (err) {
      sumar({ quien: 'el', texto: String(err), robot: nombreDe(otro) });
    }
    return;
  }
  const r = await invoke('charlar', { texto });
  // Lo que cambia algo (una alarma, un recordatorio) se ve en el día.
  if (r === 'entendio' || r === 'nube') setTimeout(cargarDia, 3000);
});

cargarDia();
cargarRobots();
setInterval(cargarDia, 5 * 60 * 1000);
setInterval(cargarRobots, 30 * 1000);
$('frase').focus();
