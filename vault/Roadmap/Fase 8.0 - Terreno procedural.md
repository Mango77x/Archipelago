---
tags: [roadmap, fase]
---

# Fase 8.0 — Terreno procedural (EN CURSO)

Primer paso concreto de [[Fase 8 - Mundo procedural]]. Alcance decidido con el usuario: no solo el fondo marino, también la disposición del archipiélago (número/posición/forma de las islas) — y con soporte para semillas distintas desde el principio, no una única semilla fija. Es el salto de alcance más grande de todo el proyecto hasta ahora; se divide en pasos concretos, cada uno verificado antes de seguir al siguiente (mismo patrón que [[Fase 7.0 - Motor de física real (Jolt)|Fase 7.0]]/[[Fase 7.1 - Oleaje y boyantez|7.1]]/[[Fase 7.2 - Tormentas|7.2]]).

**Por qué ahora**: el usuario decidió aplazar el resto de [[Fase 7 - Entorno]] (pesca, estaciones, demanda energética) — "eso sobra hasta que no haya algo tangible" — y pasar directamente a generar terreno real, en vez de seguir añadiendo sistemas sobre un mundo de 3 islas fijas.

## Pasos

1. **Ruido 2D determinista con semilla** ✅ — `noise.h`: value noise + fBm (varias octavas), validado con visualización ASCII en Python antes de integrarlo.
2. **Fondo marino con profundidad real** ✅ — `terrain.h`/`jolt_world.cpp`: heightmap real vía `JPH::HeightFieldShape`, sustituyendo la caja plana de Fase 7.1. Al principio se mantenía siempre bajo el nivel del mar (clamp deliberado).
3. **Islas como parte del mismo terreno** ✅ — se quitó el clamp: donde el ruido supera `kSeaLevelNoiseThreshold`, hay tierra por encima del nivel del mar. Umbral calibrado empíricamente (0.4) para un archipiélago disperso, no un continente.
4. **Colocar mina/acería/puerto en el terreno generado** ✅ — `Terrain::ComputeWorldLayout`: relleno por inundación (flood fill) sobre una rejilla de muestreo encuentra la isla más grande, calcula un desplazamiento para que esa isla quede siempre centrada en el mundo (petición del usuario — antes salía en cualquier sitio según la semilla), y planta los 3 edificios en su costa bien repartidos (muestreo del punto más lejano), cada uno con su muelle en la celda de mar más cercana. Reemplaza las coordenadas fijas de Fase 4, que ahora chocarían con tierra sólida de verdad.
5. **Render del terreno real** ✅ (parcial, hecho junto al paso 2) — malla con elevación y normales por diferencias finitas (`GenerateTerrainMesh`/`DrawTerrain`), reutilizando el shader iluminado existente.
6. **Guardar la semilla en el save** — pendiente.

**Determinismo**: igual que olas/tormentas, todo depende de la semilla (no de `rand()` en tiempo real) — necesario para que Guardado/Carga y Replay sigan funcionando sobre un mundo generado.

**Añadido durante el desarrollo, no estaba en el plan original**:
- **Vista de mapa** (tecla M): cámara ortográfica cenital reutilizando todo el render 3D existente (nada de textura horneada aparte), con paneo por clic-arrastre y zoom con la rueda (estilo X4). Necesario en cuanto el mundo dejó de caber a simple vista.
- **Mundo 4x más grande** (20000×20000 → 80000×80000): el usuario señaló que el mapa "va a tener que ser mucho más grande" para un juego real; mallas de agua/terreno subidas de 128-200 a 256 segmentos para mantener detalle proporcional.
- **Recalibración del tamaño de isla** (`kBaseCellSize` 3000→15000): con el mundo 4x más grande y el mismo tamaño de célula de ruido, salían "miles de mini islas" en vez de un puñado de islas grandes y jugables. Recalibrado contando componentes conexas (Python) hasta dar ~12-15 islas, varias de 7000-20000 unidades de diámetro.

Feedback recurrente del usuario en esta fase: "necesitamos terreno que se pueda jugar, hacer guerras y producir y conducir" — el criterio no es solo "que se vea bien", es que las islas sean lo bastante grandes para ser territorio de verdad.

Anterior: [[Fase 7.2 - Tormentas]] (resto de [[Fase 7 - Entorno|Fase 7]] aplazado). Siguiente: una vez el terreno esté generado y la economía plantada sobre él, decidir si [[Fase 6 - Gobiernos]] ya tiene suficiente mundo para retomarse (ver [[Facciones establecidas y el hueco del jugador]]).
