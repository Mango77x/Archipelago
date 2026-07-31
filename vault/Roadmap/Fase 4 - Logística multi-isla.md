---
tags: [roadmap, fase]
---

# Fase 4 — Logística multi-isla

Introduce decisiones logísticas reales: múltiples islas, industrias, recursos, cadenas de producción y distancias de transporte distintas.

**Definition of Done**: tres islas intercambian recursos de forma autónoma y el jugador debe optimizar rutas.

**Decisiones de diseño concretas para esta fase** (ver [[Industria, transporte, IA, guerra, escala]], "la distancia es parte del diseño"):

- **Layout fijo de 3 islas hecho a mano**, no generación procedural — eso es [[Fase 8 - Mundo procedural|Fase 8]], su propia fase con su propio DoD. Adelantarlo aquí sería resolver dos problemas de golpe (multi-isla + generación) en vez de uno.
- **Las islas deben espaciarse con distancia real**, no estar todas cerca — si el viaje entre islas es trivial, la fase no valida nada distinto de Fase 1-3. La distancia es la que convierte "optimizar rutas" en una decisión de verdad, no un adorno.
- **Desacoplar espacio del mundo de espacio de pantalla**: hasta Fase 3 la posición del mundo y la posición en píxeles son la misma cosa (un solo almacén, un solo puerto, todo cabe en una ventana). Con varias islas separadas de verdad, hace falta una cámara con posición/zoom propios — el mundo ya no cabe entero en pantalla.

**Checkpoint añadido — Sistema de Replay**: antes de la [[Fase 5 - Primera empresa de IA|Fase 5]], añadir la capacidad de grabar y reproducir una partida (secuencia de eventos/inputs deterministas). Es la herramienta que hace depurable la Fase 5 — sin poder reproducir una partida, entender por qué una IA autónoma tomó una decisión rara es casi imposible. Requiere que la simulación sea determinista de un solo hilo antes de este punto (ver [[Determinismo vs multithreading]]) — todavía no se ha introducido paralelismo, así que esto debería cumplirse por construcción.

Anterior: [[Fase 3 - Automatización]]. Siguiente: [[Fase 5 - Primera empresa de IA]].
