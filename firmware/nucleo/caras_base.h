// GENERADO por firmware/herramientas/embeber_caras.py desde shared/formato/caras.
// No editar a mano.
#pragma once

namespace sintetica {

constexpr int CANTIDAD_CARAS_BASE = 16;

inline const char* const CARAS_BASE[CANTIDAD_CARAS_BASE] = {
    // 01-neutral.json
    R"json({"nombre":"neutral","numero":1,"disparador":"En reposo","formas":[{"forma":"pildora","x":-0.3125,"y":0.0,"w":0.225,"h":0.45},{"forma":"pildora","x":0.3125,"y":0.0,"w":0.225,"h":0.45}]})json",
    // 02-contento.json
    R"json({"nombre":"contento","numero":2,"disparador":"Caricia en el sensor táctil","formas":[{"forma":"arco","puntos":[[-0.4125,0.1125],[-0.3125,-0.175],[-0.2125,0.1125]],"trazo":0.1},{"forma":"arco","puntos":[[0.2125,0.1125],[0.3125,-0.175],[0.4125,0.1125]],"trazo":0.1}]})json",
    // 03-sospecha.json
    R"json({"nombre":"sospecha","numero":3,"disparador":"Doble toque","formas":[{"forma":"pildora","x":-0.3125,"y":0.0625,"w":0.225,"h":0.325},{"forma":"corte","x":0.3125,"y":0.00625,"w":0.225,"h":0.4375,"izq":0.0,"der":0.1125}]})json",
    // 04-sorpresa.json
    R"json({"nombre":"sorpresa","numero":4,"disparador":"Lo levantan (IMU)","formas":[{"forma":"circulo","x":-0.3125,"y":0.0,"r":0.1875},{"forma":"circulo","x":0.3125,"y":0.0,"r":0.1875},{"forma":"circulo","x":-0.25,"y":-0.075,"r":0.05,"color":"pantalla"},{"forma":"circulo","x":0.375,"y":-0.075,"r":0.05,"color":"pantalla"}]})json",
    // 05-dormido.json
    R"json({"nombre":"dormido","numero":5,"disparador":"10 min sin interacción","parpadea":false,"formas":[{"forma":"pildora","x":-0.3125,"y":0.06875,"w":0.225,"h":0.0625},{"forma":"pildora","x":0.3125,"y":0.06875,"w":0.225,"h":0.0625},{"forma":"poligono","puntos":[[0.54627,-0.1375],[0.54627,-0.15021],[0.57856,-0.187],[0.54729,-0.187],[0.54729,-0.19937],[0.59499,-0.19937],[0.59499,-0.18666],[0.56236,-0.14987],[0.59622,-0.14987],[0.59622,-0.1375]],"color":"tinta-suave","fijo":true},{"forma":"poligono","puntos":[[0.64384,-0.2625],[0.64384,-0.27168],[0.66716,-0.29825],[0.64457,-0.29825],[0.64457,-0.30719],[0.67902,-0.30719],[0.67902,-0.29801],[0.65546,-0.27144],[0.67991,-0.27144],[0.67991,-0.2625]],"color":"tinta-suave","fijo":true}]})json",
    // 06-enojado.json
    R"json({"nombre":"enojado","numero":6,"disparador":"Lo despiertan de la siesta","formas":[{"forma":"corte","x":-0.3125,"y":0.0,"w":0.225,"h":0.45,"izq":0.0,"der":0.1375},{"forma":"corte","x":0.3125,"y":0.0,"w":0.225,"h":0.45,"izq":0.1375,"der":0.0}]})json",
    // 07-triste.json
    R"json({"nombre":"triste","numero":7,"disparador":"Boca abajo o 24 h sin uso","formas":[{"forma":"corte","x":-0.3125,"y":0.0125,"w":0.225,"h":0.425,"izq":0.1125,"der":0.0},{"forma":"corte","x":0.3125,"y":0.0125,"w":0.225,"h":0.425,"izq":0.0,"der":0.1125}]})json",
    // 08-guino.json
    R"json({"nombre":"guino","numero":8,"disparador":"Vínculo exitoso con la app","formas":[{"forma":"pildora","x":-0.3125,"y":0.0,"w":0.225,"h":0.45},{"forma":"arco","puntos":[[0.2125,0.0625],[0.3125,-0.125],[0.4125,0.0625]],"trazo":0.0875}]})json",
    // 09-enamorado.json
    R"json({"nombre":"enamorado","numero":9,"disparador":"Su dueño abre la app","formas":[{"forma":"corazon","x":-0.3125,"y":0.0,"tamano":1.0},{"forma":"corazon","x":0.3125,"y":0.0,"tamano":1.0}]})json",
    // 10-mareado.json
    R"json({"nombre":"mareado","numero":10,"disparador":"Sacudida fuerte (IMU)","parpadea":false,"formas":[{"forma":"espiral","x":-0.3125,"y":0.0,"r0":0.01875,"paso":0.00881,"giro":16.34,"trazo":0.04375},{"forma":"espiral","x":0.3125,"y":0.0,"r0":0.01875,"paso":0.00881,"giro":16.34,"trazo":0.04375}]})json",
    // 11-escuchando.json
    R"json({"nombre":"escuchando","numero":11,"disparador":"Palabra de activación","formas":[{"forma":"pildora","x":-0.3125,"y":-0.1375,"w":0.225,"h":0.35},{"forma":"pildora","x":0.3125,"y":-0.1375,"w":0.225,"h":0.35},{"forma":"pildora","x":-0.10938,"y":0.2875,"w":0.03125,"h":0.05,"fijo":true,"audio":0},{"forma":"pildora","x":-0.05312,"y":0.2875,"w":0.03125,"h":0.1125,"fijo":true,"audio":1},{"forma":"pildora","x":0.00313,"y":0.2875,"w":0.03125,"h":0.175,"fijo":true,"audio":2},{"forma":"pildora","x":0.05937,"y":0.2875,"w":0.03125,"h":0.1,"fijo":true,"audio":3},{"forma":"pildora","x":0.11563,"y":0.2875,"w":0.03125,"h":0.0625,"fijo":true,"audio":4}]})json",
    // 12-pensando.json
    R"json({"nombre":"pensando","numero":12,"disparador":"Esperando la respuesta de la IA","formas":[{"forma":"pildora","x":-0.2375,"y":-0.15625,"w":0.2,"h":0.3375},{"forma":"pildora","x":0.3875,"y":-0.15625,"w":0.2,"h":0.3375},{"forma":"circulo","x":0.5,"y":0.3375,"r":0.025,"color":"tinta-suave","fijo":true},{"forma":"circulo","x":0.5875,"y":0.3375,"r":0.025,"color":"tinta-suave","fijo":true},{"forma":"circulo","x":0.675,"y":0.3375,"r":0.025,"color":"tinta-suave","fijo":true}]})json",
    // 13-curioso.json
    R"json({"nombre":"curioso","numero":13,"disparador":"Gira la cabeza (servo)","formas":[{"forma":"pildora","x":-0.3125,"y":0.0,"w":0.225,"h":0.5},{"forma":"pildora","x":0.3125,"y":0.0,"w":0.175,"h":0.275}]})json",
    // 14-bateria-baja.json
    R"json({"nombre":"bateria-baja","numero":14,"disparador":"Menos de 15% de batería","formas":[{"forma":"corte","x":-0.3125,"y":0.1,"w":0.225,"h":0.25,"izq":0.0,"der":0.0},{"forma":"corte","x":0.3125,"y":0.1,"w":0.225,"h":0.25,"izq":0.0,"der":0.0},{"forma":"pildora","x":0.75,"y":-0.36875,"w":0.175,"h":0.0875,"r":0.01875,"color":"tinta-suave","fijo":true,"relleno":false,"trazo":0.0125},{"forma":"pildora","x":0.85313,"y":-0.36875,"w":0.01875,"h":0.0375,"r":0.00625,"color":"tinta-suave","fijo":true},{"forma":"pildora","x":0.69688,"y":-0.36875,"w":0.03125,"h":0.05,"r":0.00625,"fijo":true}]})json",
    // 15-actualizando.json
    R"json({"nombre":"actualizando","numero":15,"disparador":"Firmware OTA en curso","formas":[{"forma":"pildora","x":-0.3125,"y":-0.06875,"w":0.225,"h":0.0625},{"forma":"pildora","x":0.3125,"y":-0.06875,"w":0.225,"h":0.0625},{"forma":"pildora","x":0.0,"y":0.1875,"w":0.85,"h":0.05,"color":"linea","fijo":true},{"forma":"pildora","x":0.0,"y":0.1875,"w":0.85,"h":0.05,"fijo":true,"progreso":true}]})json",
    // 16-glitch.json
    R"json({"nombre":"glitch","numero":16,"disparador":"Error de sistema","parpadea":false,"formas":[{"forma":"pildora","x":-0.3125,"y":0.0,"w":0.225,"h":0.45},{"forma":"pildora","x":0.3125,"y":0.0,"w":0.225,"h":0.45},{"forma":"franjas","bandas":[{"y":-0.2875,"h":0.175,"dx":-0.03125},{"y":-0.1125,"h":0.1125,"dx":0.04375},{"y":0.0,"h":0.1125,"dx":-0.01875},{"y":0.1125,"h":0.175,"dx":0.025}]},{"forma":"pildora","x":-0.425,"y":0.0625,"w":0.35,"h":0.0125,"fijo":true},{"forma":"pildora","x":0.4625,"y":0.0625,"w":0.25,"h":0.0125,"fijo":true}]})json",
};

}  // namespace sintetica
