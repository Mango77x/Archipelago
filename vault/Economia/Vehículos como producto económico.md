---
tags: [vision, economia, vehiculos]
---

# Vehículos como producto económico

Los vehículos no son un caso especial exento de la economía — son un producto más, sujeto a las mismas reglas que el Acero o cualquier otro bien (ver [[Principios - Economía]], principio 6: "nada aparece de la nada — todo recurso tiene productor", aplicado literalmente). Un barco, un avión, un tanque tiene un productor (un astillero/fábrica de vehículos), una cadena de producción con sus propias piezas, un coste real y un precio de mercado — exactamente igual que el Acero de [[Economía]].

## Progresión de capacidad industrial (igual que X4)

- **Al principio**, el jugador no tiene capacidad industrial propia para fabricar vehículos. Los compra a **facciones neutrales o aliadas** que sí tienen esa cadena de producción montada.
- **Más adelante**, cuando el jugador construye su propia producción de piezas/vehículos (un astillero propio), puede fabricar sus propios vehículos — y automatizar esa producción, igual que ya automatiza la producción de Acero en [[Fase 2 - Economía|Fase 2]].

Esto encaja directamente con [[Progresión del jugador]]: el salto de "operador logístico" a "corporación industrial" incluye, en algún punto, pasar de comprador de vehículos a fabricante de vehículos.

## Nota de fase (por qué esto no bloquea Fase 3)

En [[Fase 3 - Automatización|Fase 3]] (estado actual del desarrollo) todavía no existe ninguna otra facción simulada — eso llega con las empresas de IA en [[Fase 5 - Primera empresa de IA|Fase 5]] y los gobiernos en [[Fase 6 - Gobiernos|Fase 6]]. Por tanto, "comprar un barco" en Fase 3 se implementa como una **simplificación explícita y documentada**, no como el sistema final: el jugador paga un precio de marcador de posición a un vendedor abstracto no simulado ("una facción neutral fuera de pantalla"), sin cadena de producción real detrás todavía.

**Deuda técnica registrada a propósito** (principio 72: "la deuda técnica se rastrea, no se ignora"): cuando existan facciones/astilleros de verdad simulados, comprar un barco debe conectarse a esa economía real — stock disponible, tiempo de construcción, cadena de piezas — en vez de seguir siendo un botón con precio fijo e infinito. No se resuelve ahora porque no hay todavía ninguna facción que lo justifique (principio 68: "toda dependencia debe justificar su existencia").

Ver también: [[Vehículos como interfaz]], [[Carga física - contenedores]] (mismo patrón: simplificación honesta ahora, con la nota de qué cambia cuando el sistema real llegue).
