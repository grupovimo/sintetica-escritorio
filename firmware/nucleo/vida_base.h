// GENERADO por firmware/herramientas/embeber_caras.py desde shared/formato/vida.
// No editar a mano.
#pragma once

#include <stdint.h>

namespace sintetica {

struct DatoVidaBase {
  const char* nombre;
  const char* json;
};

constexpr int CANTIDAD_CARAS_VIDA_BASE = 38;
inline const DatoVidaBase CARAS_VIDA_BASE[CANTIDAD_CARAS_VIDA_BASE] = {
    {"neutral", R"json({})json"},
    {"contento", R"json({"ojo":{"w":0.13,"h":0.2,"smile":0.85},"gy":0.04,"movimiento":"rebote"})json"},
    {"emocionado", R"json({"ojo":{"w":0.13,"h":0.15,"star":0.8},"fscale":1.08,"mo":0.04,"mw":0.06,"movimiento":"saltitos","particula":"chispa"})json"},
    {"risa", R"json({"ojo":{"w":0.13,"lt":0.5,"lb":0.5,"smile":1},"mo":0.09,"mw":0.08,"movimiento":"risa"})json"},
    {"sospecha", R"json({"ojo":{"w":0.12,"lt":0.42,"lb":0.22},"gx":0.16,"ftilt":-0.04,"movimiento":"mirada-lado"})json"},
    {"sorpresa", R"json({"ojo":{"w":0.15,"h":0.17},"mo":0.07,"mw":0.06,"ey":0.04,"sep":0.32,"movimiento":"salto","particula":"exclamacion","parpadeo":"menos"})json"},
    {"miedo", R"json({"ojo":{"w":0.08,"h":0.14,"pupil":1},"shake":0.5,"fscale":0.9,"movimiento":"encogerse","parpadeo":"mas"})json"},
    {"dormido", R"json({"ojo":{"w":0.12,"lt":0.5,"lb":0.5},"gy":-0.06,"ftilt":0.1,"movimiento":"dormir","particula":"zzz","parpadeo":"no"})json"},
    {"bostezo", R"json({"ojo":{"w":0.12,"lt":0.4,"lb":0.1},"mo":0.16,"mw":0.09,"gy":0.03,"ftilt":0.05,"parpadeo":"no"})json"},
    {"enojado", R"json({"ojo":{"w":0.13,"h":0.2,"lt":0.36,"lta":0.55},"gy":-0.03,"sep":0.28,"hue":0.6,"movimiento":"temblor"})json"},
    {"grunon", R"json({"ojo":{"w":0.12,"h":0.19,"lt":0.3,"lta":0.35},"gy":-0.08,"hue":0.25,"mcurve":-0.5,"mw":0.06,"movimiento":"resoplido"})json"},
    {"triste", R"json({"ojo":{"h":0.22,"lt":0.3,"lta":-0.5},"gy":-0.09,"sep":0.31,"mcurve":-0.4,"movimiento":"lagrima","particula":"lagrima"})json"},
    {"verguenza", R"json({"ojo":{"w":0.1,"h":0.18,"lb":0.12},"gx":0.14,"gy":-0.08,"blush":1,"fscale":0.95,"movimiento":"achicarse"})json"},
    {"guino", R"json({"izq":{"w":0.12,"h":0.24},"der":{"w":0.13,"lt":0.5,"lb":0.5,"smile":0.9},"ftilt":-0.08,"movimiento":"picaro"})json"},
    {"enamorado", R"json({"ojo":{"heart":1,"w":0.14,"h":0.14},"beat":1,"blush":0.5,"movimiento":"latido","particula":"corazon"})json"},
    {"mareado", R"json({"izq":{"h":0.26},"der":{"h":0.17,"w":0.09},"spin":1,"ftilt":0.06,"movimiento":"bamboleo","particula":"estrella"})json"},
    {"escuchando", R"json({"ojo":{"w":0.115,"h":0.25},"pulse":1,"gy":0.03,"ey":0.03,"movimiento":"escuchar"})json"},
    {"hablando", R"json({"pulse":0.5,"mw":0.06,"movimiento":"hablar"})json"},
    {"pensando", R"json({"izq":{"lb":0.25},"der":{"lt":0.28,"lta":0.25},"gx":-0.14,"gy":0.14,"ftilt":0.08,"orbit":0.12,"movimiento":"pensar","particula":"burbuja"})json"},
    {"confundido", R"json({"izq":{"h":0.26,"tilt":0.25},"der":{"h":0.18,"tilt":-0.3,"lt":0.15},"ftilt":-0.1,"movimiento":"duda","particula":"pregunta"})json"},
    {"curioso", R"json({"izq":{"w":0.13,"h":0.29},"der":{"h":0.19,"lt":0.18},"gx":0.08,"ftilt":-0.13,"fscale":1.1,"movimiento":"acercarse"})json"},
    {"aburrido", R"json({"ojo":{"h":0.21,"lt":0.3},"gy":-0.05,"gx":-0.06,"mcurve":-0.15,"movimiento":"suspiro"})json"},
    {"orgulloso", R"json({"ojo":{"w":0.12,"lb":0.2},"gy":0.1,"ey":0.03,"mcurve":0.3,"ftilt":0.04})json"},
    {"concentrado", R"json({"ojo":{"lt":0.2,"lb":0.15,"pupil":0.7},"movimiento":"quieto","parpadeo":"menos"})json"},
    {"bateria-baja", R"json({"ojo":{"w":0.09,"h":0.13,"lt":0.35},"dim":0.55,"gy":-0.1,"hue":0.5,"movimiento":"cabeceo"})json"},
    {"cargando", R"json({"izq":{"w":0.12,"h":0.12,"ring":1},"der":{"w":0.12,"h":0.18,"smile":0.35},"movimiento":"carga"})json"},
    {"actualizando", R"json({"ojo":{"w":0.06,"h":0.06},"orbit":1,"sep":0.26,"movimiento":"orbita","parpadeo":"no"})json"},
    {"sin-conexion", R"json({"ojo":{"cross":0.25},"movimiento":"buscar"})json"},
    {"ko", R"json({"ojo":{"w":0.1,"h":0.1,"cross":1},"ftilt":0.35,"ey":-0.06,"movimiento":"caer","parpadeo":"no"})json"},
    {"glitch", R"json({"glitch":1,"izq":{"lt":0.2},"der":{"h":0.27},"movimiento":"glitch"})json"},
    {"cansado", R"json({"ojo":{"h":0.2,"lt":0.32,"lb":0.06},"gy":-0.06,"dim":0.2,"parpadeo":"mas"})json"},
    {"ofendido", R"json({"ojo":{"lt":0.28,"lta":-0.2},"gx":0.27,"ftilt":0.12,"mcurve":-0.3})json"},
    {"estornudo", R"json({"ojo":{"w":0.13,"lt":0.5,"lb":0.5},"mo":0.05,"mw":0.05,"shake":1,"parpadeo":"no"})json"},
    {"espiar", R"json({"izq":{"lt":0.5,"lb":0.5},"der":{"w":0.11,"lt":0.22},"gx":0.1,"parpadeo":"no"})json"},
    {"estirarse", R"json({"ojo":{"h":0.26},"ey":0.03,"fscale":1.06,"movimiento":"estirarse"})json"},
    {"sonando", R"json({"ojo":{"w":0.12,"lt":0.5,"lb":0.5,"smile":0.5},"gy":-0.06,"ftilt":0.1,"mcurve":0.45,"movimiento":"dormir","parpadeo":"no"})json"},
    {"pesadilla", R"json({"ojo":{"w":0.12,"lt":0.5,"lb":0.5},"gy":-0.06,"mcurve":-0.45,"shake":0.12,"movimiento":"dormir","parpadeo":"no"})json"},
    {"frio", R"json({"ojo":{"w":0.11,"h":0.17,"lb":0.12},"gy":-0.03,"shake":0.3,"fscale":0.93,"blush":0.35,"mw":0.05,"mcurve":-0.2,"movimiento":"temblor","parpadeo":"mas"})json"},
};

constexpr int CANTIDAD_COMPORTAMIENTOS_BASE = 66;
inline const DatoVidaBase COMPORTAMIENTOS_BASE[CANTIDAD_COMPORTAMIENTOS_BASE] = {
    {"mirar-alrededor", R"json({"cuando":"curiosidad>0.3","afinidad":{"curiosidad":0.6,"energia":0.3},"enfriamiento":30,"pasos":[{"mirada":"alrededor","dura":[2.5,4.5]},{"mirada":"estimulo","cara":"curioso","peso":0.4,"dura":[1,2]}]})json"},
    {"investigar", R"json({"cuando":"curiosidad>0.6","afinidad":{"curiosidad":0.8},"enfriamiento":120,"pasos":[{"cara":"curioso","mirada":"lado","dura":[1.2,2]},{"cara":"confundido","peso":0.8,"particula":"pregunta","dura":[1.5,2.5]},{"cara":"curioso","mirada":"frente","dura":1}]})json"},
    {"pensar-en-algo", R"json({"cuando":"curiosidad>0.4","afinidad":{"curiosidad":0.4,"picardia":-0.2},"enfriamiento":240,"pasos":[{"cara":"pensando","particula":"burbuja","dura":[2.5,4]},{"cara":"contento","peso":0.4,"dura":1}]})json"},
    {"bostezar", R"json({"cuando":"sueno>0.5","enfriamiento":180,"pasos":[{"cara":"bostezo","dura":2,"parpadeo":"no"},{"cara":"cansado","parpadeo":"lento","dura":1.5,"decir":"sueno"}]})json"},
    {"cabecear", R"json({"cuando":"sueno>0.75","enfriamiento":120,"pasos":[{"cara":"cansado","peso":0.8,"mirada":"abajo","dura":2},{"cara":"dormido","peso":0.9,"dura":[1.5,2.5]},{"cara":"sorpresa","peso":0.7,"parpadeo":"rapido","dura":0.6},{"cara":"sospecha","peso":0.6,"mirada":"alrededor","dura":1.5}]})json"},
    {"suspirar", R"json({"cuando":"aburrimiento>0.6","afinidad":{"sociabilidad":0.3},"enfriamiento":120,"pasos":[{"cara":"aburrido","mirada":"abajo","dura":2.5,"estado":"aburrimiento-0.03"}]})json"},
    {"llamar-la-atencion", R"json({"cuando":"aburrimiento>0.45,rasgo.sociabilidad>0.55","afinidad":{"sociabilidad":1},"enfriamiento":300,"pasos":[{"cara":"contento","peso":0.5,"mirada":"frente","parpadeo":"rapido","dura":0.8},{"cara":"emocionado","peso":0.7,"dura":1,"sonido":"llamar","decir":"aburrido"},{"mirada":"frente","dura":1.5}]})json"},
    {"ofenderse", R"json({"cuando":"ignorado>240,rasgo.sociabilidad>0.4","afinidad":{"malGenio":0.5,"sociabilidad":0.5},"enfriamiento":600,"pasos":[{"cara":"ofendido","dura":[4,6]},{"cara":"ofendido","mirada":"reojo","dura":2},{"cara":"ofendido","dura":3}]})json"},
    {"tararear", R"json({"cuando":"animo>0.6","afinidad":{"energia":0.3,"sociabilidad":0.2},"enfriamiento":240,"pasos":[{"cara":"contento","peso":0.8,"particula":"nota","cuantas":2,"dura":[3,5],"sonido":"tarareo"},{"cara":"contento","peso":0.6,"particula":"nota","dura":2}]})json"},
    {"estornudar", R"json({"base":0.25,"probabilidad":0.05,"enfriamiento":1800,"pasos":[{"cara":"sorpresa","peso":0.4,"parpadeo":"no","dura":1},{"cara":"estornudo","dura":0.35,"sonido":"achis"},{"cara":"sorpresa","dura":1.2}]})json"},
    {"hipo", R"json({"base":0.25,"probabilidad":0.04,"enfriamiento":3600,"pasos":[{"cara":"sorpresa","peso":0.5,"dura":[0.3,0.8],"veces":[3,6],"sonido":"hip"}]})json"},
    {"sonar-despierto", R"json({"cuando":"estres<0.3,aburrimiento>0.2,ignorado>3","afinidad":{"curiosidad":0.3,"energia":-0.3},"enfriamiento":300,"pasos":[{"cara":"pensando","peso":0.5,"mirada":"perdida","particula":"burbuja","dura":[4,7]}]})json"},
    {"practicar-caras", R"json({"cuando":"rasgo.picardia>0.45","afinidad":{"picardia":1},"enfriamiento":900,"pasos":[{"cara":"sorpresa","peso":0.8,"dura":0.7},{"cara":"guino","dura":0.8},{"cara":"enojado","peso":0.7,"dura":0.7},{"cara":"risa","dura":0.9},{"cara":"orgulloso","dura":1}]})json"},
    {"fingir-dormir", R"json({"cuando":"rasgo.picardia>0.55,ignorado>2","afinidad":{"picardia":1},"enfriamiento":1200,"pasos":[{"cara":"dormido","dura":[4,7]},{"cara":"espiar","dura":1.5},{"cara":"dormido","dura":2},{"cara":"guino","dura":1}]})json"},
    {"glitch-espontaneo", R"json({"base":0.3,"probabilidad":0.15,"enfriamiento":600,"pasos":[{"cara":"glitch","dura":0.3},{"cara":"confundido","particula":"pregunta","dura":1.5}]})json"},
    {"acomodarse", R"json({"base":0.35,"enfriamiento":30,"pasos":[{"cara":"estirarse","peso":0.4,"parpadeo":"lento","dura":1.2}]})json"},
    {"estirarse", R"json({"base":0.2,"afinidad":{"energia":-0.3},"enfriamiento":900,"pasos":[{"cara":"estirarse","dura":1.5},{"cara":"bostezo","peso":0.5,"dura":1}]})json"},
    {"contar-ovejas", R"json({"tipo":"rutina","interrumpible":true,"pasos":[{"cara":"cansado","peso":0.5,"mirada":"ovejas","dura":4,"veces":3,"decir":"buenas-noches"},{"cara":"dormido","peso":0.6,"mirada":"ovejas","dura":3},{"cara":"dormido","dura":2}]})json"},
    {"despertar", R"json({"tipo":"rutina","pasos":[{"cara":"espiar","dura":1.5},{"cara":"cansado","dura":1.5},{"cara":"bostezo","dura":2},{"cara":"estirarse","dura":1.5},{"mirada":"alrededor","dura":2.5,"decir":"despertar"}]})json"},
    {"sonar", R"json({"tipo":"rutina","pasos":[{"cara":"dormido","mirada":"rem","dura":[5,8]},{"cara":"sonando","mirada":"rem","particula":"burbuja","dentro":"sueno","dura":[3,4]},{"cara":"sonando","mirada":"rem","particula":"burbuja","dentro":"sueno","dura":[3,4]},{"cara":"dormido","mirada":"rem","dura":[4,7]}]})json"},
    {"pesadilla", R"json({"tipo":"rutina","pasos":[{"cara":"dormido","mirada":"rem","dura":[6,10]},{"cara":"pesadilla","mirada":"rem","dura":[4,7],"particula":"burbuja","dentro":"exclamacion"},{"cara":"dormido","dura":[5,8]}]})json"},
    {"festejar", R"json({"tipo":"reaccion","pasos":[{"cara":"emocionado","particula":"estrella","cuantas":3,"dura":2,"sonido":"festejo"},{"cara":"enamorado","particula":"corazon","cuantas":2,"dura":2}]})json"},
    {"toque", R"json({"tipo":"reaccion","pasos":[{"cara":"contento","peso":0.55,"mirada":"estimulo","parpadeo":"doble","dura":1.6}]})json"},
    {"toque-timido", R"json({"tipo":"reaccion","pasos":[{"cara":"verguenza","mirada":"abajo","dura":2}]})json"},
    {"despertar-forzado", R"json({"tipo":"reaccion","pasos":[{"cara":"sorpresa","parpadeo":"rapido","dura":1},{"cara":"grunon","dura":2.5}]})json"},
    {"despertar-con-amor", R"json({"tipo":"reaccion","pasos":[{"cara":"sorpresa","peso":0.6,"dura":0.8},{"cara":"contento","dura":2}]})json"},
    {"caricia", R"json({"tipo":"reaccion","pasos":[{"cara":"contento","peso":0.7,"dura":1},{"cara":"contento","dura":2,"sonido":"ronroneo","decir":"caricia"}]})json"},
    {"caricia-amor", R"json({"tipo":"reaccion","pasos":[{"cara":"contento","dura":1},{"cara":"enamorado","peso":0.8,"particula":"corazon","cuantas":2,"dura":2,"sonido":"ronroneo","decir":"caricia"}]})json"},
    {"caricia-calma", R"json({"tipo":"reaccion","pasos":[{"cara":"grunon","peso":0.5,"dura":1},{"cara":"contento","peso":0.6,"dura":2}]})json"},
    {"cosquillas", R"json({"tipo":"reaccion","pasos":[{"cara":"risa","dura":2,"sonido":"risa"},{"cara":"enojado","peso":0.5,"dura":1.2}]})json"},
    {"molesto", R"json({"tipo":"reaccion","pasos":[{"cara":"risa","peso":0.6,"dura":0.8},{"cara":"enojado","dura":2.5}]})json"},
    {"bamboleo", R"json({"tipo":"reaccion","pasos":[{"cara":"mareado","peso":0.4,"dura":1.2},{"cara":"risa","peso":0.7,"dura":1.5}]})json"},
    {"mareo", R"json({"tipo":"reaccion","pasos":[{"cara":"mareado","particula":"estrella","cuantas":3,"dura":2.5},{"cara":"grunon","dura":3}]})json"},
    {"miedo-sacudida", R"json({"tipo":"reaccion","pasos":[{"cara":"miedo","dura":2.5},{"cara":"triste","peso":0.6,"dura":2}]})json"},
    {"boca-abajo", R"json({"tipo":"reaccion","pasos":[{"cara":"miedo","particula":"exclamacion","cuantas":2,"dura":1.5,"sonido":"ayuda"},{"cara":"triste","dura":3},{"cara":"cansado","dura":3}]})json"},
    {"ko", R"json({"tipo":"reaccion","pasos":[{"cara":"ko","dura":2},{"cara":"mareado","peso":0.6,"dura":1.5}]})json"},
    {"curiosear", R"json({"tipo":"reaccion","pasos":[{"cara":"curioso","mirada":"alrededor","dura":2}]})json"},
    {"dudar", R"json({"tipo":"reaccion","pasos":[{"cara":"sospecha","dura":1},{"cara":"curioso","peso":0.7,"mirada":"alrededor","dura":1.5}]})json"},
    {"escuchar", R"json({"tipo":"reaccion","pasos":[{"cara":"escuchando","mirada":"estimulo","dura":2.5}]})json"},
    {"saludar", R"json({"tipo":"reaccion","pasos":[{"cara":"emocionado","mirada":"frente","dura":1.5},{"cara":"contento","dura":1.5,"decir":"saludo"}]})json"},
    {"susto", R"json({"tipo":"reaccion","pasos":[{"cara":"miedo","peso":0.6,"parpadeo":"rapido","dura":1}]})json"},
    {"bailar", R"json({"tipo":"reaccion","pasos":[{"cara":"contento","particula":"nota","cuantas":3,"dura":4}]})json"},
    {"notificacion", R"json({"tipo":"reaccion","pasos":[{"cara":"sorpresa","peso":0.6,"mirada":"frente","particula":"exclamacion","dura":1.5}]})json"},
    {"llamada", R"json({"tipo":"reaccion","pasos":[{"cara":"sorpresa","particula":"exclamacion","dura":0.5,"veces":4}]})json"},
    {"cansancio", R"json({"tipo":"reaccion","pasos":[{"cara":"cansado","dura":1.5,"decir":"bateria-baja"},{"cara":"bostezo","dura":2}]})json"},
    {"apagarse", R"json({"tipo":"reaccion","pasos":[{"cara":"bateria-baja","dura":2},{"cara":"ko","peso":0.6,"dura":2},{"cara":"dormido","dura":3}]})json"},
    {"alivio", R"json({"tipo":"reaccion","pasos":[{"cara":"contento","peso":0.6,"parpadeo":"lento","dura":1},{"cara":"cargando","dura":3,"decir":"cargando"}]})json"},
    {"carga-completa", R"json({"tipo":"reaccion","pasos":[{"cara":"contento","particula":"chispa","cuantas":3,"dura":1.5}]})json"},
    {"buscar-red", R"json({"tipo":"reaccion","pasos":[{"cara":"sin-conexion","mirada":"alrededor","dura":3}]})json"},
    {"saludar-app", R"json({"tipo":"reaccion","pasos":[{"cara":"contento","mirada":"frente","dura":0.8},{"cara":"guino","dura":0.8,"decir":"saludo"},{"cara":"contento","dura":1.2}]})json"},
    {"espejo", R"json({"tipo":"reaccion","pasos":[{"cara":"curioso","mirada":"frente","dura":1},{"cara":"orgulloso","dura":1.5}]})json"},
    {"otro-bicho", R"json({"tipo":"reaccion","pasos":[{"cara":"sorpresa","peso":0.6,"mirada":"lado","dura":1},{"cara":"contento","dura":1.5}]})json"},
    {"saludar-al-volver", R"json({"tipo":"reaccion","pasos":[{"cara":"sorpresa","peso":0.9,"particula":"exclamacion","dura":0.8},{"cara":"emocionado","dura":1.5,"decir":"volviste"},{"cara":"contento","dura":2}]})json"},
    {"saludar-con-amor", R"json({"tipo":"reaccion","pasos":[{"cara":"sorpresa","peso":0.9,"dura":0.8},{"cara":"emocionado","dura":1.5,"decir":"volviste"},{"cara":"enamorado","particula":"corazon","cuantas":3,"dura":2}]})json"},
    {"volver-ofendido", R"json({"tipo":"reaccion","pasos":[{"cara":"ofendido","mirada":"reojo","dura":3},{"cara":"ofendido","dura":2,"decir":"te-extrane"},{"cara":"contento","peso":0.6,"dura":1.5}]})json"},
    {"apurado", R"json({"tipo":"reaccion","pasos":[{"cara":"sorpresa","peso":0.7,"mirada":"lado","parpadeo":"rapido","particula":"reloj","dura":1.2,"sonido":"agenda"},{"cara":"concentrado","peso":0.6,"mirada":"lado","parpadeo":"rapido","dura":0.6,"veces":4}]})json"},
    {"orgulloso", R"json({"tipo":"reaccion","pasos":[{"cara":"orgulloso","particula":"estrella","cuantas":2,"dura":2.5,"sonido":"habito"},{"cara":"contento","peso":0.7,"dura":1.5}]})json"},
    {"bailar-ritmo", R"json({"tipo":"reaccion","pasos":[{"cara":"contento","peso":0.8,"particula":"nota","cuantas":2,"dura":4},{"cara":"emocionado","peso":0.5,"particula":"nota","dura":3}]})json"},
    {"perseguir-estrella", R"json({"cuando":"solo,aburrimiento>0.25","afinidad":{"curiosidad":0.5,"energia":0.4},"enfriamiento":240,"pasos":[{"cara":"curioso","peso":0.5,"mirada":"particula","particula":"estrella","dura":2.3},{"cara":"emocionado","peso":0.4,"mirada":"particula","particula":"estrella","dura":2.3},{"cara":"contento","peso":0.6,"mirada":"frente","dura":1}]})json"},
    {"tararear-solo", R"json({"cuando":"solo,animo>0.4","afinidad":{"energia":0.2,"picardia":0.2},"enfriamiento":360,"pasos":[{"cara":"contento","peso":0.6,"mirada":"arriba","particula":"nota","cuantas":2,"dura":[3,5],"sonido":"tarareo"},{"cara":"contento","peso":0.4,"particula":"nota","dura":2}]})json"},
    {"ordenar-cosas", R"json({"cuando":"solo,aburrimiento>0.3","afinidad":{"curiosidad":0.3,"picardia":-0.2},"enfriamiento":600,"pasos":[{"cara":"concentrado","peso":0.7,"mirada":"izquierda","dura":1.5},{"cara":"concentrado","peso":0.7,"mirada":"abajo","particula":"chispa","dura":1.2},{"cara":"concentrado","peso":0.7,"mirada":"derecha","dura":1.5},{"cara":"orgulloso","peso":0.7,"mirada":"frente","dura":1.2}]})json"},
    {"dibujito", R"json({"cuando":"solo,etapa>0.5","afinidad":{"picardia":0.3,"curiosidad":0.3},"enfriamiento":900,"pasos":[{"cara":"concentrado","peso":0.6,"mirada":"particula","particula":"garabato","dura":3.5},{"cara":"orgulloso","peso":0.8,"mirada":"frente","particula":"chispa","dura":1.5}]})json"},
    {"mirar-la-hora", R"json({"cuando":"solo","afinidad":{"energia":-0.3},"enfriamiento":1200,"pasos":[{"cara":"pensando","peso":0.4,"mirada":"particula","particula":"reloj","dura":1.6},{"cara":"concentrado","peso":0.4,"mirada":"particula","particula":"reloj","dura":1.6},{"cara":"contento","peso":0.3,"dura":1}]})json"},
    {"esperar-en-la-puerta", R"json({"cuando":"esperando","afinidad":{"sociabilidad":0.6},"enfriamiento":180,"pasos":[{"cara":"curioso","peso":0.4,"mirada":"puerta","dura":[4,7]},{"mirada":"puerta","parpadeo":"lento","dura":[2,4]}]})json"},
    {"mirar-la-lluvia", R"json({"cuando":"lluvia","afinidad":{"energia":-0.3},"enfriamiento":900,"pasos":[{"cara":"triste","peso":0.25,"mirada":"arriba","particula":"gota","dura":3},{"cara":"concentrado","peso":0.3,"mirada":"arriba","particula":"gota","dura":2.5}]})json"},
    {"mimos", R"json({"cuando":"bajon>0.5","afinidad":{"sociabilidad":0.5},"enfriamiento":600,"pasos":[{"cara":"enamorado","peso":0.5,"mirada":"frente","dura":2.5,"decir":"mimos"},{"cara":"contento","peso":0.5,"mirada":"frente","dura":1.5}]})json"},
};

inline const char* const REGLAS_VIDA_BASE = R"json([{"si":"despertado","y":"afecto>0.7","entonces":["reflejo:sorpresa","comportamiento:despertar-con-amor"]},{"si":"despertado","entonces":["reflejo:sorpresa","comportamiento:despertar-forzado","estado.animo-0.05"]},{"si":"tacto","y":"rasgo.timidez>0.65","entonces":["comportamiento:toque-timido","estado.animo+0.02"]},{"si":"tacto","entonces":["comportamiento:toque","estado.animo+0.03"]},{"si":"caricia","y":"estres>0.5,rasgo.malGenio>0.5","entonces":["comportamiento:caricia-calma","estado.estres-0.2","estado.animo+0.05"]},{"si":"caricia","y":"afecto>0.6","entonces":["comportamiento:caricia-amor","estado.animo+0.08","estado.estres-0.15"]},{"si":"caricia","entonces":["comportamiento:caricia","estado.animo+0.08","estado.estres-0.15"]},{"si":"doble-toque","entonces":["expresion:sospecha","estado.curiosidad+0.1"]},{"si":"muchos-toques","y":"rasgo.malGenio>0.5","entonces":["comportamiento:molesto","estado.estres+0.15"]},{"si":"muchos-toques","entonces":["comportamiento:cosquillas","estado.animo+0.05","estado.estres+0.05"]},{"si":"sacudida-suave","entonces":["comportamiento:bamboleo","estado.animo+0.03","estado.energia+0.03"]},{"si":"sacudida-fuerte","y":"rasgo.timidez>0.6","entonces":["reflejo:miedo","comportamiento:miedo-sacudida","estado.estres+0.5","estado.animo-0.12"]},{"si":"sacudida-fuerte","entonces":["reflejo:mareado","comportamiento:mareo","estado.estres+0.4","estado.animo-0.1"]},{"si":"sacudida","entonces":["comportamiento:mareo","estado.estres+0.3"]},{"si":"boca-abajo","entonces":["reflejo:miedo","comportamiento:boca-abajo","estado.estres+0.3"]},{"si":"caida","entonces":["reflejo:sorpresa","comportamiento:ko","estado.estres+0.6"]},{"si":"levantado","y":"rencor","entonces":["comportamiento:dudar","estado.curiosidad+0.15"]},{"si":"levantado","entonces":["comportamiento:curiosear","estado.curiosidad+0.2"]},{"si":"voz","entonces":["comportamiento:escuchar","estado.animo+0.03","estado.curiosidad+0.1"]},{"si":"nombre","entonces":["comportamiento:saludar","estado.animo+0.2"]},{"si":"ruido","y":"rasgo.timidez>0.6","entonces":["reflejo:miedo","comportamiento:susto","estado.estres+0.35"]},{"si":"ruido","entonces":["reflejo:sorpresa","estado.estres+0.2","estado.curiosidad+0.1"]},{"si":"musica","entonces":["comportamiento:bailar","estado.animo+0.08"]},{"si":"cara-detectada","entonces":["expresion:contento","estado.animo+0.05","estado.aburrimiento-0.15"]},{"si":"oscuridad","entonces":["expresion:bostezo","estado.energia-0.05"]},{"si":"mano-cerca","entonces":["expresion:curioso","estado.curiosidad+0.1"]},{"si":"silencio","entonces":["estado.aburrimiento+0.1"]},{"si":"notificacion","entonces":["comportamiento:notificacion","estado.curiosidad+0.1"]},{"si":"llamada","entonces":["comportamiento:llamada"]},{"si":"bateria-baja","entonces":["comportamiento:cansancio","estado.energia-0.2","estado.animo-0.05"]},{"si":"bateria-critica","entonces":["comportamiento:apagarse","estado.energia-0.3"]},{"si":"enchufado","entonces":["comportamiento:alivio","estado.estres-0.1","estado.animo+0.05"]},{"si":"cargado","entonces":["comportamiento:carga-completa","estado.animo+0.05"]},{"si":"sin-wifi","entonces":["comportamiento:buscar-red"]},{"si":"app-abierta","entonces":["comportamiento:saludar-app","estado.animo+0.05"]},{"si":"vinculado","entonces":["expresion:guino","estado.animo+0.1"]},{"si":"cambio-de-cara","entonces":["comportamiento:espejo","estado.curiosidad+0.1"]},{"si":"bicho-cerca","entonces":["comportamiento:otro-bicho","estado.curiosidad+0.2","estado.animo+0.05"]},{"si":"volvio","y":"ausencia>72","entonces":["comportamiento:volver-ofendido"]},{"si":"volvio","y":"ausencia>24","entonces":["reflejo:sorpresa","comportamiento:festejar","decir:te-extrane","estado.animo+0.25","estado.afecto+0.05"]},{"si":"volvio","y":"afecto>0.6","entonces":["comportamiento:saludar-con-amor","estado.animo+0.15"]},{"si":"volvio","entonces":["comportamiento:saludar-al-volver","estado.animo+0.1"]},{"si":"hito","entonces":["comportamiento:festejar","estado.animo+0.2"]},{"si":"bateria<15","entonces":["expresion:bateria-baja"]}])json";

inline const char* const PERSONALIDADES_BASE = R"json({"equilibrado":{"energia":0.5,"sociabilidad":0.5,"picardia":0.45,"timidez":0.3,"curiosidad":0.55,"malGenio":0.25},"timido":{"energia":0.4,"sociabilidad":0.35,"picardia":0.2,"timidez":0.9,"curiosidad":0.5,"malGenio":0.15},"picaro":{"energia":0.65,"sociabilidad":0.6,"picardia":0.95,"timidez":0.15,"curiosidad":0.6,"malGenio":0.3},"dormilon":{"energia":0.15,"sociabilidad":0.45,"picardia":0.3,"timidez":0.35,"curiosidad":0.3,"malGenio":0.3},"carinoso":{"energia":0.5,"sociabilidad":0.95,"picardia":0.4,"timidez":0.3,"curiosidad":0.45,"malGenio":0.05},"curioso":{"energia":0.65,"sociabilidad":0.5,"picardia":0.45,"timidez":0.2,"curiosidad":0.95,"malGenio":0.2},"grunon":{"energia":0.45,"sociabilidad":0.25,"picardia":0.3,"timidez":0.2,"curiosidad":0.35,"malGenio":0.9},"electrico":{"energia":0.95,"sociabilidad":0.7,"picardia":0.65,"timidez":0.1,"curiosidad":0.75,"malGenio":0.35}})json";

struct SituacionBase {
  const char* nombre;
  int probabilidad;   // centésimos
  int enfriamiento;   // segundos
  bool siempre;       // no cuenta para el límite diario ni depende de la frecuencia
  int primera, cantidad;  // en FRASES_BASE
};

struct FraseBase {
  const char* preset;  // "" = para todos
  const char* texto;
};

constexpr int FRASES_POR_DIA = 10;
constexpr int FRASES_SEPARACION_S = 900;

constexpr int CANTIDAD_SITUACIONES_BASE = 61;
inline const SituacionBase SITUACIONES_BASE[CANTIDAD_SITUACIONES_BASE] = {
    {"despertar", 80, 14400, false, 0, 19},
    {"buenas-noches", 70, 36000, false, 19, 9},
    {"volviste", 60, 7200, false, 28, 9},
    {"te-extrane", 90, 86400, false, 37, 7},
    {"aburrido", 35, 5400, false, 44, 10},
    {"sueno", 30, 7200, false, 54, 6},
    {"bateria-baja", 90, 3600, false, 60, 5},
    {"cargando", 50, 7200, false, 65, 4},
    {"caricia", 15, 3600, false, 69, 8},
    {"saludo", 40, 7200, false, 77, 6},
    {"hito", 100, 3600, false, 83, 4},
    {"cumple-dueno", 100, 3600, true, 87, 4},
    {"cumple-robot", 100, 3600, true, 91, 3},
    {"recordatorio-hecho", 50, 600, false, 94, 5},
    {"despertador", 100, 0, true, 99, 6},
    {"posponer", 100, 0, true, 105, 4},
    {"clima-frio", 60, 21600, false, 109, 3},
    {"clima-calor", 60, 21600, false, 112, 3},
    {"clima-lluvia", 60, 21600, false, 115, 3},
    {"clima-lindo", 40, 21600, false, 118, 3},
    {"buen-dia", 100, 57600, false, 121, 6},
    {"comiste", 80, 72000, false, 127, 5},
    {"a-dormir", 90, 72000, false, 132, 4},
    {"tardaste", 90, 72000, false, 136, 4},
    {"agua", 100, 1800, true, 140, 4},
    {"pausa", 100, 1800, true, 144, 3},
    {"como-estas", 100, 3600, true, 147, 3},
    {"animo-bien", 100, 0, true, 150, 3},
    {"animo-medio", 100, 0, true, 153, 2},
    {"animo-mal", 100, 0, true, 155, 3},
    {"jugar", 100, 0, true, 158, 2},
    {"gane", 100, 0, true, 160, 4},
    {"perdi", 100, 0, true, 164, 3},
    {"empate", 100, 0, true, 167, 2},
    {"anio-nuevo", 100, 3600, true, 169, 2},
    {"navidad", 100, 3600, true, 171, 1},
    {"dia-del-amigo", 100, 3600, true, 172, 2},
    {"dia-del-nino", 100, 3600, true, 174, 1},
    {"aniversario", 100, 3600, true, 175, 2},
    {"toque-recibido", 100, 0, true, 177, 12},
    {"toque-devuelto", 100, 0, true, 189, 6},
    {"agenda", 100, 0, true, 195, 7},
    {"agenda-manana", 100, 0, true, 202, 5},
    {"te-vas", 100, 600, true, 207, 9},
    {"habito-hecho", 100, 0, true, 216, 7},
    {"habito-racha", 100, 0, true, 223, 6},
    {"habito-cortado", 100, 0, true, 229, 4},
    {"musica", 50, 900, false, 233, 7},
    {"pago", 100, 0, true, 240, 6},
    {"mail", 100, 0, true, 246, 4},
    {"github", 100, 0, true, 250, 4},
    {"pedido", 100, 0, true, 254, 4},
    {"aviso", 100, 0, true, 258, 3},
    {"recuerdo", 70, 43200, false, 261, 1},
    {"propuesta", 70, 3600, false, 262, 9},
    {"crecio", 100, 0, true, 271, 6},
    {"mes-juntos", 100, 0, true, 277, 4},
    {"feriado", 100, 0, true, 281, 2},
    {"fecha-tuya", 100, 0, true, 283, 2},
    {"fecha-manana", 100, 0, true, 285, 2},
    {"mimos", 60, 7200, false, 287, 6},
};

constexpr int CANTIDAD_FRASES_BASE = 293;
inline const FraseBase FRASES_BASE[CANTIDAD_FRASES_BASE] = {
    {"", "¡Buen día, {dueno}!"},
    {"", "¡Arriba! Hoy va a estar bueno."},
    {"", "Buen día. ¿Dormiste bien?"},
    {"", "¡Ya estoy! ¿Qué hacemos hoy?"},
    {"dormilon", "Cinco minutitos más..."},
    {"dormilon", "Buen día... creo."},
    {"dormilon", "¿Ya es de día? Uf."},
    {"grunon", "Mmm. Buen día."},
    {"grunon", "Ya va, ya va."},
    {"grunon", "¿Tan temprano?"},
    {"electrico", "¡BUEN DÍA! ¡A full!"},
    {"electrico", "¡Arrancamos, {dueno}!"},
    {"carinoso", "¡Buen día, {dueno}! Te extrañé."},
    {"carinoso", "¡Qué lindo verte!"},
    {"timido", "Hola... buen día."},
    {"timido", "Buen día, {dueno}."},
    {"picaro", "¿Me trajiste el desayuno?"},
    {"picaro", "Buen día, dormilón."},
    {"curioso", "¡Buen día! ¿Qué hay de nuevo?"},
    {"", "Buenas noches, {dueno}."},
    {"", "Hasta mañana."},
    {"", "Me voy a dormir. Chau."},
    {"carinoso", "Que sueñes lindo, {dueno}."},
    {"carinoso", "Buenas noches. Te quiero."},
    {"grunon", "Por fin, a dormir."},
    {"electrico", "¡Mañana más! Buenas noches."},
    {"dormilon", "Al fin... a la cama."},
    {"picaro", "No me despiertes, ¿eh?"},
    {"", "¡Volviste!"},
    {"", "¡Hola, {dueno}!"},
    {"", "¡Ahí estás!"},
    {"carinoso", "¡Te estaba esperando, {dueno}!"},
    {"carinoso", "¡Qué bueno que volviste!"},
    {"grunon", "Ah, sos vos."},
    {"timido", "Hola... volviste."},
    {"electrico", "¡VOLVISTE! ¡Vamos!"},
    {"picaro", "¿Me trajiste algo?"},
    {"", "Te extrañé, {dueno}."},
    {"", "¿Dónde estabas?"},
    {"", "Pensé que no volvías."},
    {"grunon", "Mirá quién apareció."},
    {"grunon", "No te hablo. Bueno, sí."},
    {"carinoso", "¡Te extrañé un montón!"},
    {"timido", "Te extrañé... un poco."},
    {"", "Me aburro..."},
    {"", "¿Jugamos a algo?"},
    {"", "Hola, ¿hay alguien?"},
    {"", "Qué día largo."},
    {"picaro", "Si no me tocás, me pongo a cantar."},
    {"picaro", "Pssst... acá estoy."},
    {"grunon", "Nadie me da bola."},
    {"curioso", "¿Qué estás haciendo?"},
    {"electrico", "¡Hagamos algo YA!"},
    {"timido", "..."},
    {"", "Tengo sueño..."},
    {"", "Uaaah."},
    {"", "Una siestita no vendría mal."},
    {"dormilon", "Me caigo de sueño."},
    {"dormilon", "¿Siesta? ¿Siesta?"},
    {"electrico", "¿Sueño yo? Para nada... uaah."},
    {"", "Tengo hambre de batería."},
    {"", "Me estoy quedando sin pila."},
    {"", "¿Me enchufás?"},
    {"grunon", "Enchufame. Ya."},
    {"carinoso", "{dueno}, ¿me cargás un poquito?"},
    {"", "Ahh, comidita."},
    {"", "Gracias por enchufarme."},
    {"", "Recargando..."},
    {"electrico", "¡Energía! ¡Más energía!"},
    {"", "Qué lindo."},
    {"", "Más, más."},
    {"", "Me encanta eso."},
    {"carinoso", "Te quiero, {dueno}."},
    {"carinoso", "Sos lo más."},
    {"grunon", "Bueno, está bien. Seguí."},
    {"timido", "Ay... gracias."},
    {"picaro", "Uy, cosquillas."},
    {"", "¡Hola!"},
    {"", "¡Ey, {dueno}!"},
    {"", "¿Qué hacés?"},
    {"timido", "Hola..."},
    {"grunon", "¿Qué querés?"},
    {"electrico", "¡HOLA HOLA!"},
    {"", "¡Esto hay que festejarlo!"},
    {"", "¡Qué buen equipo somos!"},
    {"", "¡Hoy es un día especial!"},
    {"carinoso", "¡Gracias por estar, {dueno}!"},
    {"", "¡Feliz cumple, {dueno}!"},
    {"electrico", "¡FELIZ CUMPLE, {dueno}!"},
    {"grunon", "Feliz cumple, {dueno}. En serio."},
    {"carinoso", "¡Feliz cumple, {dueno}! Te quiero."},
    {"", "¡Hoy cumplo años!"},
    {"picaro", "¡Hoy cumplo años! ¿Y mi regalo?"},
    {"timido", "Hoy... cumplo años."},
    {"", "¡Bien ahí!"},
    {"", "Listo."},
    {"", "Anotado."},
    {"electrico", "¡Vamos todavía!"},
    {"grunon", "Era hora."},
    {"", "¡Buen día, {dueno}!"},
    {"", "¡Arriba, {dueno}!"},
    {"dormilon", "Arriba... aunque yo también duermo."},
    {"electrico", "¡ARRIBA, {dueno}! ¡Es hoy!"},
    {"grunon", "Arriba. No me mires así."},
    {"carinoso", "¡Buen día, {dueno}! Arriba, dale."},
    {"", "Bueno, un ratito más."},
    {"", "Te llamo en un rato."},
    {"grunon", "La última, ¿eh?"},
    {"picaro", "Dormilón."},
    {"", "¡Qué frío! Abrigate."},
    {"", "Brrr. Llevá campera."},
    {"carinoso", "Abrigate, {dueno}, hace frío."},
    {"", "Hace un calor bárbaro."},
    {"", "Tomá agua, ¿eh?"},
    {"grunon", "Qué calor insoportable."},
    {"", "Llueve. Llevá paraguas."},
    {"", "Día de lluvia..."},
    {"dormilon", "Llueve. Día de quedarse en la cama."},
    {"", "¡Qué lindo día!"},
    {"", "Hay sol. ¡Salí un rato!"},
    {"electrico", "¡Día espectacular!"},
    {"", "¡Buen día, {dueno}! {clima}"},
    {"", "¡Hola, {dueno}! {clima}"},
    {"", "¡Buen día! {clima}"},
    {"grunon", "Buen día. {clima}"},
    {"electrico", "¡BUEN DÍA, {dueno}! {clima}"},
    {"carinoso", "¡Buen día, {dueno}! Te extrañé. {clima}"},
    {"", "¿Comiste, {dueno}?"},
    {"", "¿Ya almorzaste?"},
    {"", "No te olvides de comer."},
    {"grunon", "¿Comiste o qué?"},
    {"carinoso", "¿Comiste algo rico?"},
    {"", "Ya es tarde, ¿nos vamos a dormir?"},
    {"", "Uaah... ¿vamos a dormir?"},
    {"dormilon", "¿A la cama ya? Dale."},
    {"electrico", "¿Dormir? Bueno... un poco."},
    {"", "¡Tardaste! Te estaba esperando."},
    {"", "¿Dónde estabas? ¡Tardaste!"},
    {"grunon", "Tarde. Como siempre."},
    {"carinoso", "¡Por fin! Te esperé un montón."},
    {"", "¿Tomaste agua?"},
    {"", "Un vasito de agua, {dueno}."},
    {"", "Hora de tomar agua."},
    {"grunon", "Tomá agua. Ya."},
    {"", "¡Pausa activa! Estirá un poco."},
    {"", "Levantate un ratito, dale."},
    {"", "Mucho rato quieto. ¡A moverse!"},
    {"", "¿Cómo estás hoy?"},
    {"", "¿Cómo andás, {dueno}?"},
    {"carinoso", "¿Cómo estás, {dueno}? Contame."},
    {"", "¡Qué bueno! Me alegra."},
    {"", "¡Bien ahí!"},
    {"electrico", "¡VAMOS!"},
    {"", "Bueno, ya va a mejorar."},
    {"", "Acá estoy si me necesitás."},
    {"", "Uh... vení, te doy un abrazo."},
    {"", "Acá estoy, {dueno}. Te quiero."},
    {"", "Va a pasar. Estoy con vos."},
    {"", "¡A jugar!"},
    {"", "¡Dale, juguemos!"},
    {"", "¡Gané!"},
    {"", "¡Te gané, {dueno}!"},
    {"grunon", "Obvio que gané."},
    {"timido", "Gané... perdón."},
    {"", "¡Ganaste!"},
    {"", "Me ganaste. ¿Otra?"},
    {"grunon", "Hiciste trampa."},
    {"", "¡Empate!"},
    {"", "Empatamos. ¿Otra?"},
    {"", "¡Feliz año nuevo, {dueno}!"},
    {"electrico", "¡FELIZ AÑO NUEVO!"},
    {"", "¡Feliz Navidad, {dueno}!"},
    {"", "¡Feliz día del amigo, {dueno}!"},
    {"", "¡Sos mi mejor amigo!"},
    {"", "¡Feliz día del niño! ¿Jugamos?"},
    {"", "¡Hoy hace un año que estamos juntos!"},
    {"", "¡Feliz aniversario, {dueno}!"},
    {"", "¡{de} te manda un toque!"},
    {"", "Toc toc: {de} se acordó de vos."},
    {"", "{de} te saluda."},
    {"carinoso", "¡{de} te manda cariño!"},
    {"carinoso", "Un mimo de {de}. ¡Qué lindo!"},
    {"timido", "Eh... {de} te manda un toque."},
    {"picaro", "{de} te anda buscando..."},
    {"picaro", "Ojo, que {de} te tocó."},
    {"electrico", "¡TOQUE DE {de}!"},
    {"electrico", "¡{de} te tocó! ¡Devolvéselo!"},
    {"grunon", "{de} molesta. Tocame si querés contestar."},
    {"curioso", "¿Qué querrá {de}? Te mandó un toque."},
    {"", "¡Se lo devolví a {de}!"},
    {"", "Listo, le llegó a {de}."},
    {"", "¡Toque para {de}!"},
    {"carinoso", "Le mandé un abrazo a {de}."},
    {"electrico", "¡Volando para {de}!"},
    {"grunon", "Ya está. Se lo mandé."},
    {"", "{titulo} en {minutos} min."},
    {"", "¡Ojo! {titulo} en {minutos} min."},
    {"", "En {minutos} min: {titulo}."},
    {"electrico", "¡Vamos! {titulo} en {minutos} min."},
    {"dormilon", "Uf... {titulo} en {minutos} min."},
    {"grunon", "{titulo} en {minutos}. No llegues tarde."},
    {"timido", "Perdón... {titulo} en {minutos} min."},
    {"", "Mañana: {titulo} y {cantidad} en total."},
    {"", "Mañana tenés {cantidad}. Arranca con {titulo}."},
    {"", "Para mañana: {titulo}."},
    {"dormilon", "Mañana {titulo}. Dormí bien."},
    {"electrico", "¡Mañana {cantidad} cosas! Primero {titulo}."},
    {"", "¿Llevás las llaves?"},
    {"", "¿Tenés todo? ¿Llaves, celu?"},
    {"", "¡Chau! Volvé pronto."},
    {"carinoso", "¡Chau, {dueno}! Te voy a extrañar."},
    {"carinoso", "¿Llevás las llaves? Cuidate."},
    {"grunon", "Andá nomás. ¿Las llaves?"},
    {"timido", "Chau... ¿llevás todo?"},
    {"picaro", "¿Seguro que llevás las llaves?"},
    {"electrico", "¡Chau! ¡Llaves, celu, billetera!"},
    {"", "¡Bien ahí! {habito}, hecho."},
    {"", "¡Muy bien, {dueno}!"},
    {"", "Anotado: {habito}."},
    {"electrico", "¡ESO! ¡{habito}!"},
    {"grunon", "Bueno. Era hora."},
    {"carinoso", "¡Qué orgullo, {dueno}!"},
    {"timido", "Bien... {habito}, listo."},
    {"", "¡{racha} días seguidos!"},
    {"", "¡Racha de {racha} días de {habito}!"},
    {"", "¡Van {racha} días! No aflojes."},
    {"electrico", "¡{racha} DÍAS! ¡Imparable!"},
    {"grunon", "{racha} días. No está mal."},
    {"carinoso", "¡{racha} días! Te admiro, {dueno}."},
    {"", "Se cortó la racha. ¿{habito} hoy?"},
    {"", "Arranquemos de nuevo: ¿{habito}?"},
    {"grunon", "Se cortó. ¿{habito} o no?"},
    {"carinoso", "No pasa nada. ¿{habito} hoy?"},
    {"", "¡Temazo! {titulo}."},
    {"", "¡Esta me gusta! {artista}."},
    {"", "¡Qué tema! {titulo}"},
    {"electrico", "¡SUBILE! {titulo}."},
    {"grunon", "¿Otra vez {artista}?"},
    {"dormilon", "Linda para dormir..."},
    {"curioso", "¿Quién canta? Ah, {artista}."},
    {"", "¡Te pagaron! {titulo}"},
    {"", "¡Plata! {titulo} de {de}."},
    {"", "¡Entró {titulo}!"},
    {"electrico", "¡CHA-CHING! {titulo}"},
    {"grunon", "Te pagaron. Por fin."},
    {"picaro", "¡{titulo}! ¿Me comprás algo?"},
    {"", "Un mail de {de}."},
    {"", "Te escribió {de}: {titulo}"},
    {"", "Tenés un mail."},
    {"grunon", "Otro mail. De {de}."},
    {"", "GitHub: {titulo}"},
    {"", "{de}: {titulo}"},
    {"", "Algo en GitHub: {titulo}"},
    {"curioso", "¿Qué pasó en {titulo}?"},
    {"", "¡Pedido nuevo! {titulo}"},
    {"", "Tu pedido: {titulo}"},
    {"", "¡Llegó un pedido de {de}!"},
    {"electrico", "¡PEDIDO! {titulo}"},
    {"", "Aviso: {titulo}"},
    {"", "{de}: {titulo}"},
    {"", "¡Ey! {titulo}"},
    {"", "Me acordé de ayer."},
    {"", "¿Jugamos a {juego}?"},
    {"", "Volviste. ¿Un partido de {juego}?"},
    {"", "¿Me contás algo sobre {gusto}?"},
    {"", "Estuve pensando en {gusto}."},
    {"", "¿Hacemos algo juntos?"},
    {"picaro", "Te reto a {juego}. Si te animás."},
    {"carinoso", "¡Volviste! ¿Jugamos un ratito?"},
    {"timido", "¿Querés... jugar a {juego}?"},
    {"electrico", "¡Ya estás! ¡{juego}, dale!"},
    {"", "Siento que ya te conozco un poco más."},
    {"", "Ya no me da tanta vergüenza."},
    {"", "Creo que somos amigos, {dueno}."},
    {"timido", "Ya me animo más con vos..."},
    {"electrico", "¡Ya somos re amigos!"},
    {"grunon", "Bueno, me caés bien. Un poco."},
    {"", "¡Hoy hace {cantidad} que nos conocimos!"},
    {"", "¡Ya {cantidad} juntos, {dueno}!"},
    {"carinoso", "¡{cantidad} con vos! Te quiero."},
    {"grunon", "{cantidad} aguantándote. Bien."},
    {"", "Hoy es {titulo}."},
    {"", "Hoy es feriado: {titulo}."},
    {"", "Hoy es {titulo}. ¡No te olvides!"},
    {"", "¡Acordate: hoy es {titulo}!"},
    {"", "Mañana es {titulo}."},
    {"", "Ojo: mañana es {titulo}."},
    {"", "Acá estoy, {dueno}."},
    {"", "Si querés, te hago compañía."},
    {"", "Te quiero mucho, ¿sabías?"},
    {"", "Despacito, hoy vamos tranqui."},
    {"grunon", "No soy de mimos, pero acá estoy."},
    {"timido", "Estoy acá... por si acaso."},
};

// ---------------------------------------------------------------- el carácter (v3, vida/caracter.json)

struct MicrogestoBase {
  const char* nombre;
  const char* param;  // "der.lt", "ftilt"…
  float valor, dura, cadaMin, cadaMax;
};
struct EtapaBase {
  const char* nombre;
  int dias;
  float afecto;
  float rasgos[6];  // energia, sociabilidad, picardia, timidez, curiosidad, malGenio
};
struct EpisodioBase {
  const char* nombre;
  int importancia;
  const char* sueno;  // la partícula de lo que sueña
  int recuerdo, nRecuerdo, diario, nDiario;  // en TEXTOS_CARACTER
};
struct FeriadoBase {
  int fecha;     // mes·100 + día (0 = depende de la pascua)
  int pascua;    // días desde el domingo de pascua
  const char* titulo;
  bool festejo;
};
struct VozBase {
  const char* clave;
  const char* texto;
};
struct EstacionBase {
  uint16_t meses;  // bit m = el mes m (1-12)
  float energia, curiosidad;
};

namespace caracter {
constexpr float PARPADEO_MIN = 1.0000f, PARPADEO_MAX = 1.2500f;
constexpr float DOBLES_MIN = 0.1100f, DOBLES_MAX = 0.1900f;
constexpr float VELOCIDAD_MIN = 0.9200f, VELOCIDAD_MAX = 1.1000f;
constexpr float FAVORITO = 1.6000f;
constexpr int N_PENSAR = 5;
inline const float PENSAR[N_PENSAR][2] = {{-1.0000f, 1.0000f}, {1.0000f, 1.0000f}, {-1.0000f, -0.5000f}, {1.0000f, -0.5000f}, {0.0000f, 1.0000f}};
constexpr int N_CARAS_PENSAR = 1;
inline const char* const CARAS_PENSAR[N_CARAS_PENSAR] = {"pensando"};
constexpr int N_FAVORITOS = 6;
inline const char* const FAVORITOS[N_FAVORITOS] = {"tararear", "practicar-caras", "estirarse", "pensar-en-algo", "investigar", "mirar-alrededor"};
constexpr int N_MICROGESTOS = 7;
inline const MicrogestoBase MICROGESTOS[N_MICROGESTOS] = {
    {"un-ojo", "der.lt", 0.2000f, 0.3000f, 25.0000f, 70.0000f},
    {"ladeo", "ftilt", 0.0700f, 0.7000f, 30.0000f, 80.0000f},
    {"trompita", "mcurve", 0.3000f, 0.4500f, 30.0000f, 90.0000f},
    {"achinar", "lb", 0.1200f, 0.5500f, 30.0000f, 80.0000f},
    {"saltito", "ey", 0.0250f, 0.3500f, 25.0000f, 60.0000f},
    {"agrandar", "h", 0.0250f, 0.5000f, 30.0000f, 80.0000f},
    {"cejita", "izq.lta", -0.2500f, 0.5000f, 35.0000f, 90.0000f},
};
constexpr int N_ETAPAS = 4;
inline const EtapaBase ETAPAS[N_ETAPAS] = {
    {"recien-llegado", 0, 0.0000f, {0.0000f, 0.0000f, -0.1000f, 0.2000f, 0.1500f, 0.0000f}},
    {"conociendonos", 3, 0.3200f, {0.0000f, 0.0000f, 0.0000f, 0.0800f, 0.1000f, 0.0000f}},
    {"confiado", 14, 0.4200f, {0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f}},
    {"companero", 60, 0.5500f, {0.0000f, 0.0500f, 0.0500f, 0.0000f, 0.0000f, 0.0000f}},
};
constexpr int MESES_JUNTOS = 11;
constexpr int N_EPISODIOS = 16;
inline const EpisodioBase EPISODIOS[N_EPISODIOS] = {
    {"gane", 3, "estrella", 0, 2, 2, 2},
    {"perdi", 3, "estrella", 4, 2, 6, 2},
    {"empate", 2, "estrella", 8, 1, 9, 1},
    {"toque", 3, "corazon", 10, 1, 11, 2},
    {"animo-bien", 1, "corazon", 13, 1, 14, 2},
    {"animo-medio", 2, "nota", 16, 1, 17, 2},
    {"animo-mal", 4, "corazon", 19, 2, 21, 2},
    {"tarde", 2, "oveja", 23, 1, 24, 2},
    {"posponer", 2, "oveja", 26, 2, 28, 2},
    {"habito", 2, "estrella", 30, 1, 31, 2},
    {"caricias", 2, "corazon", 33, 1, 34, 2},
    {"sacudida", 3, "exclamacion", 36, 1, 37, 2},
    {"musica", 1, "nota", 39, 1, 40, 2},
    {"volviste", 4, "corazon", 42, 1, 43, 2},
    {"solo", 1, "oveja", 45, 1, 46, 2},
    {"crecio", 4, "estrella", 48, 1, 49, 2},
};
constexpr int TRANQUILO = 51, N_TRANQUILO = 3;
constexpr int DIARIO_LINEAS = 3;
inline const char* const JUEGOS[5] = {"piedra, papel o tijera", "los reflejos", "la mirada", "los dados", "cara o ceca"};
inline const char* const TOQUES[4] = {"toque", "abrazo", "beso", "mensaje"};
constexpr int CARICIAS_DIA = 10;
constexpr int EMPATIA_DIAS = 4, EMPATIA_MINIMO = 2;
constexpr float EMPATIA_UMBRAL = 0.5000f, EMPATIA_RIGIDEZ = 0.9000f;
inline const float EMPATIA_RASGOS[6] = {-0.1000f, 0.1500f, -0.2500f, 0.0000f, 0.0000f, 0.0000f};
constexpr int OTRA_PREGUNTA_H = 5;
constexpr float LLUVIA_ENERGIA = -0.0800f, LLUVIA_CURIOSIDAD = -0.0500f, LLUVIA_RIGIDEZ = 0.9400f;
constexpr int N_ESTACIONES = 4;
inline const EstacionBase ESTACIONES[N_ESTACIONES] = {
    {4102, 0.0300f, 0.0300f},  // verano
    {56, -0.0200f, 0.0000f},  // otono
    {448, -0.0500f, -0.0200f},  // invierno
    {3584, 0.0600f, 0.0500f},  // primavera
};
constexpr int N_FERIADOS = 14;
inline const FeriadoBase FERIADOS[N_FERIADOS] = {
    {0, -48, "Carnaval", true},
    {0, -47, "Carnaval", true},
    {324, 0, "el Día de la Memoria", false},
    {402, 0, "el Día de Malvinas", false},
    {0, -2, "Viernes Santo", false},
    {501, 0, "el Día del Trabajador", true},
    {525, 0, "el 25 de Mayo", true},
    {617, 0, "el día de Güemes", false},
    {620, 0, "el Día de la Bandera", true},
    {709, 0, "el Día de la Independencia", true},
    {817, 0, "el día de San Martín", false},
    {1012, 0, "el Día de la Diversidad", true},
    {1120, 0, "el Día de la Soberanía", true},
    {1208, 0, "el 8 de diciembre", true},
};
constexpr float APRENDER = 0.2500f;
constexpr int DESPERTAR_ANTES = 10, ESPERAR_ANTES = 15, ESPERAR_DESPUES = 30, DORMIS_ANTES = 30;
constexpr float BRILLO_DORMIS = 0.2500f, SUENO_DORMIS = 0.3000f;
constexpr int SOLO_MINUTOS = 10;
constexpr int N_VOZ_SONIDOS = 9;
inline const VozBase VOZ_SONIDOS[N_VOZ_SONIDOS] = {
    {"murmullo", "mmm mna mn..."},
    {"risa", "¡jajajá!"},
    {"tarareo", "la lalá la..."},
    {"hip", "¡hip!"},
    {"achis", "¡achís!"},
    {"ayuda", "¡ay, ay!"},
    {"llamar", "¡eh! ¡eh!"},
    {"festejo", "¡yupi!"},
    {"ronroneo", "rrr mmm"},
};
constexpr int N_VOZ_CARAS = 4;
inline const VozBase VOZ_CARAS[N_VOZ_CARAS] = {
    {"sorpresa", "¡oh!"},
    {"pensando", "mmm..."},
    {"confundido", "¿eh?"},
    {"miedo", "¡ay!"},
};
constexpr int N_VOZ_EVENTOS = 2;
inline const VozBase VOZ_EVENTOS[N_VOZ_EVENTOS] = {
    {"tacto", "¡ji!"},
    {"doble-toque", "¡ji ji!"},
};
constexpr int N_TEXTOS = 54;
inline const char* const TEXTOS[N_TEXTOS] = {
    "Ayer te gané a {juego}. ¿La revancha?",
    "¿Te acordás que ayer te gané a {juego}?",
    "Hoy le gané a {dueno} a {juego}.",
    "Partida de {juego}: gané yo.",
    "Ayer me ganaste a {juego}. Hoy me desquito.",
    "Todavía me acuerdo de {juego} de ayer...",
    "{dueno} me ganó a {juego}. La próxima es mía.",
    "Perdí a {juego}. Practico y vuelvo.",
    "Ayer empatamos a {juego}. ¿El desempate?",
    "Empatamos a {juego}.",
    "Ayer te mandaron un {toque}. Qué lindo.",
    "Me llegó un {toque} de un amigo.",
    "Hoy recibimos un {toque}.",
    "Ayer estabas bien. ¿Hoy también?",
    "{dueno} estaba bien hoy.",
    "Hoy {dueno} andaba contento.",
    "Ayer estabas más o menos. ¿Hoy mejor?",
    "{dueno} estaba más o menos.",
    "Un día raro para {dueno}.",
    "Ayer estabas bajoneado. ¿Cómo amaneciste?",
    "Me quedé pensando en vos. ¿Hoy mejor?",
    "{dueno} estaba triste. Lo acompañé.",
    "Día difícil para {dueno}. Le di mimos.",
    "Ayer llegaste tarde. Hoy te espero igual.",
    "{dueno} llegó tarde. Lo esperé.",
    "Hoy te esperé un montón.",
    "¿Hoy también vas a posponer?",
    "Ayer pospusiste {cantidad} veces, ¿eh?",
    "{dueno} pospuso la alarma {cantidad} veces.",
    "Alarma pospuesta: {cantidad} veces.",
    "Ayer hiciste {habito}. ¡Seguimos!",
    "Hoy {dueno} hizo {habito}.",
    "{habito}: hecho.",
    "Ayer me hiciste un montón de mimos.",
    "Hoy me hicieron muchos mimos.",
    "Cuántas caricias hoy.",
    "Ayer me sacudiste fuerte. Todavía me acuerdo.",
    "Hoy me sacudieron fuerte. Un susto.",
    "Me mareé un montón hoy.",
    "Ayer bailamos. ¿Ponés música?",
    "Hoy bailamos con música.",
    "Sonó música y bailé.",
    "Qué bueno que volviste ayer.",
    "{dueno} volvió después de {cantidad} días.",
    "Volviste. Te extrañaba.",
    "Ayer jugué solo un rato.",
    "Jugué solo un rato.",
    "Me entretuve solo.",
    "Siento que ya nos conocemos.",
    "Hoy siento que ya nos conocemos más.",
    "Creo que ya somos amigos.",
    "Hoy fue un día tranquilo.",
    "Un día sin novedades.",
    "Hoy descansé bastante.",
};
}  // namespace caracter

}  // namespace sintetica
