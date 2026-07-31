---
tags: [roadmap, fase]
---

# Fase 7.2 — Tormentas (CERRADA)

Primer sub-tema de [[Fase 7 - Entorno]] (clima, pesca, estaciones, demanda energética). Construye directamente sobre [[Fase 7.1 - Oleaje y boyantez]]: el campo de olas ya existía, esto le añade una intensidad de viento que varía en el tiempo y el espacio en vez de ser constante.

**Investigación previa**: se miró cómo lo hace Stormworks (Build and Rescue) antes de diseñar esto — ahí el viento es un valor continuo 0-100% (no un interruptor calma/tormenta) que varía con el tiempo, y la altura de ola escala con ese porcentaje. Dato curioso descartado por ahora: el peor estado de mar en Stormworks no es al 100% de viento sino en un rango medio (68-75%), porque ahí las olas ya son grandes pero su frecuencia sigue siendo alta (el barco no termina de subir una ola antes de que le golpee la siguiente) — no se implementó esa curva no lineal en esta pasada, queda anotado como posible refinamiento futuro.

**Modelo implementado**: `Weather::WindIntensity(x, z, t)` en [`weather.h`](../../src/weather.h) — no es un interruptor global, es un campo continuo:

1. **Viento ambiente permanente** (`AmbientWind(t)`): siempre entre ~10% y ~35%, varía lentamente con el tiempo (dos senos con periodos distintos, deterministas). Nunca es exactamente 0% — a petición del usuario, un mar sin ninguna ola en absoluto "se siente raro"; el dead calm real (las calmas ecuatoriales) existe pero es la excepción, no el estado por defecto.
2. **Células de tormenta móviles** (2 "generadores" en paralelo, cada uno con su propio ciclo de ~30 min): cada célula nace en un punto determinista (derivado por hash del índice de ciclo, no RNG en tiempo real — necesario para que el Replay no se rompa), viaja en línea recta desde su origen, sube y baja de intensidad con una envolvente suave durante su vida (~7-10 min), y desaparece del todo — no vuelve cíclicamente al mismo sitio. Cada ciclo tiene un 50% de probabilidad de no generar ninguna célula, para que a veces no pase nada en todo el mapa.
3. La intensidad de viento escala la amplitud del campo de olas existente (`Waves::Height`) — hasta 2x más alta en tormenta plena que en un día normal. Se probó primero con 3.5x: las olas superaban el calado del casco y el modelo de boyantez (un muelle "de un lado", sin succión hacia abajo cuando una esquina sale del agua) perdía fuerza media conforme las olas crecían, hundiendo los barcos poco a poco. 2x se queda claramente dramático sin desbordar lo que ese modelo aguanta bien.

**Determinismo**: todo depende únicamente de `waveTime` (el mismo reloj de simulación que ya usan las olas y el Replay) — nada de `rand()` en tiempo real. El mismo cálculo está duplicado a mano en C++ (para la boyantez, una muestra por barco en su centro) y en GLSL (para la malla de agua, una muestra por vértice, para que se vea agitado justo donde hay tormenta y tranquilo donde no).

**Bug encontrado durante el ajuste**: el casco se dibujaba con un `+8` extra en altura (`s.position() + Vec3{0,8,0}`), heredado de antes de tener boyantez real (cuando la posición era solo una referencia plana, no el centro de masa físico). Con boyantez real esto hacía que el casco se viera 8 unidades más alto de donde la física lo tenía — el vaivén visual no coincidía con la ola. Corregido: el render ya usa `s.position()` directamente.

**Feedback al jugador**: HUD con "Viento (aqui): X%", muestreado en la posición del jugador.

**Explícitamente fuera de esta pasada** (se puede añadir después):
- La curva no lineal "peor a medio viento, no al máximo" de Stormworks.
- Efectos visuales de cielo/lluvia/niebla.
- Consecuencias de gameplay más allá del manejo (daño, riesgo de perder carga).
- Los barcos de la IA se comportan de forma un poco errática/inestable en tormenta plena — aceptado como "decente por ahora" por el usuario, no bloqueante, pendiente de revisar si vuelve a ser un problema.

**Definition of Done**: confirmado por el usuario jugando en directo — viento nunca a 0% real, tormentas que se notan al pasar cerca, sin romper atraque/economía/replay.

Anterior: [[Fase 7.1 - Oleaje y boyantez]]. Siguiente: resto de [[Fase 7 - Entorno]] (pesca, estaciones, demanda energética) o [[Fase 8 - Mundo procedural]].
