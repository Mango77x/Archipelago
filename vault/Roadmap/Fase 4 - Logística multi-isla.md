---
tags: [roadmap, fase]
---

# Fase 4 — Logística multi-isla (EN CURSO)

Introduce decisiones logísticas reales: múltiples islas, industrias, recursos, cadenas de producción y distancias de transporte distintas.

**Definition of Done**: tres islas intercambian recursos de forma autónoma y el jugador debe optimizar rutas.

## Implementado

- **3 islas con cadena repartida**: Isla 1 (Minera, Mina de Hierro), Isla 2 (Industrial, Acería), Isla 3 (Portuaria, Puerto+Mercado), a distancia real entre sí (miles de unidades, no cientos).
- **Cámara 2D que sigue al barco del jugador** — el mundo ya no cabe en una pantalla.
- **Flota con rutas**: el barco del jugador puede atracar y cargar/descargar en cualquier isla libremente; los barcos comprados (bots) están restringidos a la ruta que se les asignó al comprarlos (Hierro Isla1→2 o Acero Isla2→3) — necesario para que no hagan cosas sin sentido como recoger el recurso equivocado y quedarse parados.
- **Anillo visual en cada muelle** marcando el radio real de carga/descarga.
- **Autopiloto con frenado** ("arrive" steering): reduce velocidad según se acerca al destino en vez de derrapar a máxima velocidad.
- **Reequilibrio económico**: el reloj de horas simuladas (`kSecondsPerSimulatedHour`) se subió de 2.0 a 8.0 — el valor de Fase 1-3 asumía viajes casi instantáneos (todo en una pantalla) y la mina producía ~10x más rápido de lo que cualquier flota realista podía transportar, drenando la caja pasara lo que pasara. Caja inicial subida a $2000, mantenimiento bajado, stock inicial de Hierro en Isla 2 (contabilizado correctamente en `mine.totalProduced()` para no violar el balance de materiales).
- **Guardado/carga** reescrito para dos almacenes + rutas por barco.
- **Checkpoint de Sistema de Replay** (ver más abajo): implementado.

Varios bugs encontrados y corregidos jugando en esta fase — ver el detalle en el historial de commits: barco atascado en su ruta asignada, violación de balance por un stock inicial mal contabilizado, y el desequilibrio del reloj económico.

**Decisiones de diseño concretas para esta fase** (ver [[Industria, transporte, IA, guerra, escala]], "la distancia es parte del diseño"):

- **Layout fijo de 3 islas hecho a mano**, no generación procedural — eso es [[Fase 8 - Mundo procedural|Fase 8]], su propia fase con su propio DoD. Adelantarlo aquí sería resolver dos problemas de golpe (multi-isla + generación) en vez de uno.
- **Las islas deben espaciarse con distancia real**, no estar todas cerca — si el viaje entre islas es trivial, la fase no valida nada distinto de Fase 1-3. La distancia es la que convierte "optimizar rutas" en una decisión de verdad, no un adorno.
- **Desacoplar espacio del mundo de espacio de pantalla**: hasta Fase 3 la posición del mundo y la posición en píxeles son la misma cosa (un solo almacén, un solo puerto, todo cabe en una ventana). Con varias islas separadas de verdad, hace falta una cámara con posición/zoom propios — el mundo ya no cabe entero en pantalla.

**Checkpoint — Sistema de Replay**: antes de la [[Fase 5 - Primera empresa de IA|Fase 5]], añadir la capacidad de grabar y reproducir una partida (secuencia de inputs deterministas). Es la herramienta que hace depurable la Fase 5 — sin poder reproducir una partida, entender por qué una IA autónoma tomó una decisión rara es casi imposible. Requiere que la simulación sea determinista de un solo hilo antes de este punto (ver [[Determinismo vs multithreading]]) — todavía no se ha introducido paralelismo, así que esto se cumple por construcción.

**Implementado**: el bucle principal pasó a paso de simulación fijo (`kFixedDt`, desacoplado del framerate real) — condición necesaria para que un mismo input grabado reproduzca siempre el mismo resultado. F6 graba/detiene una grabación de los inputs resueltos del jugador (empuje/giro + acciones de compra) en `archipelago_replay.txt`; F7 la reproduce. Es una herramienta de depuración (para cuando aparezca una IA autónoma en Fase 5 y un bug no sea trivial de reproducir jugando), no un sistema de guardado — no snapshotea el mundo, solo reproduce los mismos inputs desde un estado inicial equivalente.

Anterior: [[Fase 3 - Automatización]]. Siguiente: [[Fase 5 - Primera empresa de IA]].
