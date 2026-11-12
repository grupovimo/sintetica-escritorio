// El panel: el día (agenda, recordatorios, despertador), la charla con el
// robot y el renglón para hablarle. Lo que dice llega con el evento "dijo"
// (cada globo nuevo del núcleo), sea una respuesta, un aviso o algo suyo.
const { invoke } = window.__TAURI__.core;
const { listen } = window.__TAURI__.event;
const $ = (id) => document.getElementById(id);

const LETRAS = 'DLMXJVS'; // la letra de cada día en "dias" ("LMXJV"), desde el domingo
const hhmm = (d) => d.toTimeString().slice(0, 5);
const el = (tag, clase, texto) => {
  const e = document.createElement(tag);
  if (clase) e.className = clase;
  if (texto != null) e.append(texto);
  return e;
};
const icono = (id) => {
  const s = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  const u = document.createElementNS('http://www.w3.org/2000/svg', 'use');
  u.setAttribute('href', `#i-${id}`);
  s.append(u);
  return s;
};
// En la Mac los semáforos de la ventana van arriba del panel.
if (navigator.userAgent.includes('Mac')) document.body.classList.add('mac');

// tipo: recordatorio | agenda | alarma | mail (la forma de su marca).
function renglon(tipo, hora, texto) {
  const li = el('li');
  li.append(el('span', `marca ${tipo}`), el('span', 'hora digitos', hora), el('span', 'que', texto));
  if (hora && hora < hhmm(new Date()) && tipo !== 'mail') li.classList.add('pasado');
  return li;
}

let todoElDia = false;
async function cargarDia() {
  const ahora = new Date();
  const semana = ahora.toLocaleDateString('es-AR', { weekday: 'short' }).replace('.', '');
  $('dia-numero').textContent = `${semana} ${ahora.getDate()}`;
  $('dia-semana').textContent = ahora.toLocaleDateString('es-AR', { month: 'long' });
  const d = await invoke('hoy');
  $('sin-sumar').hidden = Object.keys(d).length > 0;
  $('clima').hidden = d.clima?.temp == null;
  if (d.clima?.temp != null) {
    $('temp').textContent = `${Math.round(d.clima.temp)}°`;
    $('cielo').textContent = d.clima.descripcion || '';
  }

  const hoyToca = (x) => x && x.activo !== false && (!x.dias || x.dias.includes(LETRAS[ahora.getDay()]));
  const items = [];
  for (const e of d.agenda || []) {
    const ini = new Date(e.inicio);
    if (ini.toDateString() === ahora.toDateString()) items.push(['agenda', e.todoElDia ? '' : hhmm(ini), e.titulo]);
  }
  for (const r of d.recordatorios || []) if (hoyToca(r)) items.push(['recordatorio', r.hora, r.texto]);
  if (hoyToca(d.despertador)) items.push(['alarma', d.despertador.hora, 'Despertador']);
  items.sort((a, b) => a[1].localeCompare(b[1]));
  // Los mails sin leer (Gmail, Outlook), al final: los tres últimos.
  for (const m of (d.correo || []).slice(0, 3)) items.push(['mail', hhmm(new Date(m.en)), `${m.de}: ${m.asunto}`]);
  // Lo que viene primero (como mucho 3); lo pasado y el resto, plegado.
  const filas = items.map((i) => renglon(...i));
  const proximas = filas.filter((f) => !f.classList.contains('pasado'));
  const resto = filas.filter((f) => !proximas.slice(0, 3).includes(f));
  const ver = [...proximas.slice(0, 3)];
  if (resto.length && !todoElDia) {
    const mas = el('li');
    const b = el('button', 'mas grabado', `+${resto.length}`);
    b.type = 'button';
    b.title = 'Ver todo el día';
    b.addEventListener('click', () => { todoElDia = true; cargarDia(); });
    mas.append(b);
    ver.push(mas);
  }
  $('dia').replaceChildren(...(todoElDia ? filas : ver));
  $('nada-hoy').hidden = !$('sin-sumar').hidden || filas.length > 0;
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
    const b = el('button', 'llave');
    b.type = 'button';
    b.setAttribute('aria-pressed', String(r.este ? elegido === null : elegido === r.id));
    b.append(el('span', `led${r.conectado || r.este ? ' verde' : ''}`), nombreDe(r));
    b.addEventListener('click', () => {
      elegido = r.este ? null : r.id;
      cargarRobots();
      $('frase').placeholder = r.este ? 'Decile algo' : `Decile algo a ${nombreDe(r)}`;
      $('frase').focus();
    });
    return b;
  });
  const otro = robots.find((r) => r.id === elegido);
  if (otro?.conectado) {
    const m = el('button', 'llave mimo');
    m.type = 'button';
    m.title = `Hacerle un mimo a ${nombreDe(otro)}`;
    m.setAttribute('aria-label', m.title);
    m.append(icono('mimo'));
    m.addEventListener('click', () => invoke('mimo', { id: otro.id }));
    chips.push(m);
  }
  $('robots').replaceChildren(...chips);
}

// Las listas (la del súper, pendientes…): las mismas que en el celu y Telegram.
let listas = [];
let abierta = null; // null = el canal de hoy; si no, el id de la lista que se ve
async function cargarListas() {
  const r = await invoke('listas');
  listas = r.listas || [];
  // Sin ninguna (y con cuenta), la del súper vacía para empezar a anotar.
  const ver = listas.length || !r.listas ? listas : [{ id: '', nombre: r.porDefecto || 'súper', items: [] }];
  const tecla = (nombre, id, cuenta) => {
    const b = el('button', 'tecla negra', nombre);
    b.type = 'button';
    b.setAttribute('role', 'tab');
    b.setAttribute('aria-pressed', String(abierta === id));
    b.setAttribute('aria-selected', String(abierta === id));
    if (cuenta) b.append(el('span', 'cuenta', cuenta));
    b.addEventListener('click', () => { abierta = id; cargarListas(); });
    return b;
  };
  $('pestanas').replaceChildren(tecla('Hoy', null), ...ver.map((l) => tecla(l.nombre, l.id, l.items.filter((i) => !i.hecho).length)));
  // Sin cuenta todavía no hay listas: el selector con "Hoy" solo no hace falta.
  $('pestanas').hidden = ver.length === 0;
  const l = ver.find((x) => x.id === abierta);
  if (!l) abierta = null;
  $('canal-hoy').hidden = !!l;
  $('bandeja').hidden = !l;
  if (!l) return;
  const orden = [...l.items.filter((i) => !i.hecho), ...l.items.filter((i) => i.hecho)];
  $('items').replaceChildren(...orden.map((i) => {
    const li = document.createElement('li');
    const lab = document.createElement('label');
    if (i.hecho) lab.className = 'hecho';
    const c = document.createElement('input');
    c.type = 'checkbox';
    c.checked = i.hecho;
    c.addEventListener('change', async () => {
      await invoke('lista', { accion: 'marcar', lista: l.id, item: i.id, hecho: c.checked });
      cargarListas();
    });
    const t = document.createElement('span');
    t.textContent = i.texto;
    lab.append(c, t);
    li.append(lab);
    return li;
  }));
}
$('form-item').addEventListener('submit', async (e) => {
  e.preventDefault();
  const texto = $('nuevo-item').value.trim();
  if (!texto) return;
  $('nuevo-item').value = '';
  let id = abierta;
  if (!id) {
    // La primera: se crea la del súper.
    await invoke('lista', { accion: 'crear', lista: '', texto: 'súper' });
    id = (await invoke('listas')).listas?.[0]?.id;
    abierta = id;
  }
  if (id) await invoke('lista', { accion: 'agregar', lista: id, texto });
  cargarListas();
});

// Sumarlo a la cuenta desde el panel (con Telegram), como el botón de la cara.
$('p-entrar').addEventListener('click', () => {
  $('p-entrar').hidden = true;
  $('p-paso').textContent = 'Abriendo Telegram';
  invoke('entrar');
});
$('p-codigo').addEventListener('submit', (e) => {
  e.preventDefault();
  invoke('codigo', { codigo: $('p-codigo-texto').value });
  $('p-codigo').hidden = true;
  $('p-paso').textContent = 'Revisando';
});
listen('entrada', ({ payload: p }) => {
  $('p-codigo').hidden = p.paso !== 'codigo';
  $('p-entrar').hidden = p.paso === 'telegram' || p.paso === 'codigo';
  $('p-paso').textContent = { telegram: 'Seguí en Telegram', codigo: 'El código del bot', listo: 'Listo', error: 'No salió, probá de nuevo' }[p.paso] ?? '';
  if (p.paso === 'codigo') $('p-codigo-texto').focus();
  if (p.paso === 'error') $('p-entrar').hidden = false;
  if (p.paso === 'listo') setTimeout(() => { cargarDia(); cargarListas(); cargarRobots(); }, 800);
});

// La charla: se guarda en esta compu (las últimas 60).
let charla = [];
try { charla = JSON.parse(localStorage.getItem('charla') || '[]'); } catch { /* vacía */ }
function mostrar() {
  $('charla').replaceChildren(...charla.map((m) => {
    const p = el('p', m.quien + (m.aviso ? ' aviso' : ''));
    if (m.robot) p.append(el('span', 'quien grabado', m.robot));
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
  if (r === 'entendio' || r === 'nube') setTimeout(() => { cargarDia(); cargarListas(); }, 3000);
});

cargarDia();
cargarRobots();
cargarListas();
setInterval(cargarListas, 60 * 1000);
setInterval(cargarDia, 5 * 60 * 1000);
setInterval(cargarRobots, 30 * 1000);
$('frase').focus();
