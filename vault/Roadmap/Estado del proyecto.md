---
tags: [roadmap, estado]
---

# Estado del proyecto

**Fase actual: [[Fase 1 - Primer prototipo jugable|Fase 1]] — en curso.**

## Fase 0 — cerrada

[[Fase 0 - Prototipo de simulación]] se completó y se verificó con el usuario: simulación de consola en C++ puro (mina → almacén → acería → almacén → barco → puerto), 48 horas simuladas sin violaciones de balance de materiales. Committeado en git (`main`, repo en GitHub).

## Fase 1 — progreso hasta ahora

Ya funciona y está verificado visualmente por el usuario:

- Build system: CMake + vcpkg en modo manifiesto (`vcpkg.json`, `CMakeLists.txt`), usando el CMake y vcpkg que vienen empaquetados con las Visual Studio Build Tools (no hizo falta instalar nada aparte).
- Dependencias: SDL3, GLEW (loader de OpenGL real, no SDL_Renderer — decisión explícita del usuario), Dear ImGui (con bindings SDL3 + OpenGL3).
- Mundo renderizado en 2D top-down (placeholder deliberado, ver [[Encarnación y capa de mando]]): mina, almacén, acería y puerto como rectángulos de color con etiqueta.
- Barco de carga jugable con modelo cinemático simple (empuje + arrastre + inercia de giro vía WASD/flechas) — no motor de física, ver [[Vehículos como interfaz]].
- Carga/descarga automática por proximidad a los muelles (mismo comportamiento de balance de materiales que Fase 0, ahora con movimiento manual).
- Panel de estado con Dear ImGui: hora simulada, stock de hierro/acero, estado de la acería (activa/parada), carga del barco, total exportado — resuelve el principio "todo evento importante tiene una causa entendible" (ver [[Principios - Experiencia del jugador]]).

## Pendiente para cerrar Fase 1

- **Checkpoint de Guardado/Carga** (última tarea de Fase 1, ver la propia nota de [[Fase 1 - Primer prototipo jugable]]) — todavía no implementado.
- Confirmar con el usuario que el Definition of Done completo se cumple antes de dar la fase por cerrada (ver [[Convenciones de trabajo con Claude Code]]).
- Commit de todo el trabajo de Fase 1 (CMake, vcpkg, main.cpp reescrito, este vault) — todavía no committeado a git en el momento de escribir esta nota.

## Decisiones de diseño añadidas durante Fase 1 (vigentes para cuando toque implementarlas)

- [[Encarnación y capa de mando]] — el jugador es un personaje físico en primera persona, no un cursor de gestión; coexiste con un mapa de mando estilo X4.
- [[Vehículos como interfaz]] — roster completo (naval/aéreo/terrestre, civil/militar), no solo barcos.
- [[Carga física - contenedores]] — el contenedor como única unidad físicamente interactuable, base de [[Fase 9 - Seguridad]].

Ninguna de estas tres afecta el código actual de Fase 0/1 — son visión a largo plazo documentada para no perderla.
