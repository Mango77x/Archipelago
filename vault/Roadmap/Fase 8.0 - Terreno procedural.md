---
tags: [roadmap, fase]
---

# Fase 8.0 — Terreno procedural (EN CURSO)

Primer paso concreto de [[Fase 8 - Mundo procedural]]. Alcance decidido con el usuario: no solo el fondo marino, también la disposición del archipiélago (número/posición/forma de las islas) — y con soporte para semillas distintas desde el principio, no una única semilla fija. Es el salto de alcance más grande de todo el proyecto hasta ahora; se divide en pasos concretos, cada uno verificado antes de seguir al siguiente (mismo patrón que [[Fase 7.0 - Motor de física real (Jolt)|Fase 7.0]]/[[Fase 7.1 - Oleaje y boyantez|7.1]]/[[Fase 7.2 - Tormentas|7.2]]).

**Por qué ahora**: el usuario decidió aplazar el resto de [[Fase 7 - Entorno]] (pesca, estaciones, demanda energética) — "eso sobra hasta que no haya algo tangible" — y pasar directamente a generar terreno real, en vez de seguir añadiendo sistemas sobre un mundo de 3 islas fijas.

## Pasos

1. **Ruido 2D determinista con semilla** — función de ruido (tipo Perlin/value noise) que da una altura en cualquier (x,z) para una semilla dada. Base de todo lo demás. Mismo patrón que el campo de olas/tormentas: una implementación en C++ (física/lógica) y su espejo a mano en GLSL (render).
2. **Fondo marino con profundidad real** — el fondo plano de Fase 7.1 (`CreateSeaFloorBody`, una caja) pasa a ser un heightmap generado por esa función de ruido, usando `JPH::HeightFieldShape` de Jolt en vez de una caja.
3. **Islas como parte del mismo terreno** — no una lista fija de 3 cajas: donde el ruido supera el nivel del mar, hay tierra. La semilla decide cuántas islas salen, dónde, y su forma de costa.
4. **Colocar mina/acería/puerto en el terreno generado** — algoritmo para encontrar sitios válidos en la tierra generada y plantar ahí los edificios y sus muelles. Aquí es donde la semilla empieza a cambiar la economía de verdad, que es el Definition of Done de [[Fase 8 - Mundo procedural]] ("mundos generados distintos producen economías notablemente distintas").
5. **Render del terreno real** — malla con elevación real y normales correctas, sustituyendo el fondo plano y las cajas de isla actuales.
6. **Guardar la semilla en el save** — para que cargar una partida reproduzca exactamente el mismo mundo generado.

**Determinismo**: igual que olas/tormentas, todo depende de la semilla (no de `rand()` en tiempo real) — necesario para que Guardado/Carga y Replay sigan funcionando sobre un mundo generado.

Anterior: [[Fase 7.2 - Tormentas]] (resto de [[Fase 7 - Entorno|Fase 7]] aplazado). Siguiente: una vez el terreno esté generado y la economía plantada sobre él, decidir si [[Fase 6 - Gobiernos]] ya tiene suficiente mundo para retomarse (ver [[Facciones establecidas y el hueco del jugador]]).
