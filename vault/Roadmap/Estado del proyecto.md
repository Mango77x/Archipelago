---
tags: [roadmap, estado]
---

# Estado del proyecto

**Fase actual: [[Fase 5 - Primera empresa de IA|Fase 5]] — sin empezar.**

## Fase 0 — cerrada

[[Fase 0 - Prototipo de simulación]] se completó y se verificó con el usuario: simulación de consola en C++ puro (mina → almacén → acería → almacén → barco → puerto), 48 horas simuladas sin violaciones de balance de materiales. Committeado en git (`main`, repo en GitHub).

## Fase 1 — cerrada

Definition of Done confirmada por el usuario ("un jugador nuevo entiende la cadena de producción, la interrumpe, la restaura y observa causa-efecto con claridad, sin leer documentación"). Lo construido:

- Build system: CMake + vcpkg en modo manifiesto (`vcpkg.json`, `CMakeLists.txt`), usando el CMake y vcpkg que vienen empaquetados con las Visual Studio Build Tools (no hizo falta instalar nada aparte).
- Dependencias: SDL3, GLEW (loader de OpenGL real, no SDL_Renderer — decisión explícita del usuario), Dear ImGui (con bindings SDL3 + OpenGL3).
- Mundo renderizado en 2D top-down (placeholder deliberado, ver [[Encarnación y capa de mando]]): mina, almacén, acería y puerto como rectángulos de color con etiqueta.
- Barco de carga jugable con modelo cinemático simple (empuje + arrastre + inercia de giro vía WASD/flechas) — no motor de física, ver [[Vehículos como interfaz]].
- Carga/descarga automática por proximidad a los muelles (mismo comportamiento de balance de materiales que Fase 0, ahora con movimiento manual).
- Panel de estado con Dear ImGui: hora simulada, stock de hierro/acero, estado de la acería (activa/parada), carga del barco, total exportado — resuelve el principio "todo evento importante tiene una causa entendible" (ver [[Principios - Experiencia del jugador]]).
- **Checkpoint de Guardado/Carga**: F5 guarda, F9 carga, formato de texto plano inspeccionable a mano (`archipelago_save.txt`, no versionado en git). Guarda hora simulada, acumulador de hora, totales de mina/acería/puerto, stock del almacén y estado completo del barco (posición, velocidad, rumbo, carga). Probado y confirmado por el usuario: el estado vuelve exactamente a donde se guardó.

Committeado y pusheado en git (`main`, repo en GitHub).

## Fase 2 — cerrada

Definition of Done confirmada por el usuario. Ver detalle completo en [[Fase 2 - Economía]]. Resumen: mercado de oferta/demanda de un solo bien (Acero) con precio que reacciona al ritmo de entrega del jugador, caja del jugador con gastos fijos de mantenimiento que corren siempre, guardado/carga y panel de ImGui extendidos. Committeado y pusheado en git (`main`, repo en GitHub).

## Fase 3 — cerrada

Definition of Done confirmada por el usuario. Ver detalle completo en [[Fase 3 - Automatización]]. Resumen: flota de barcos (el del jugador pilotable a mano, el resto autopilotado con el mismo modelo cinemático), compra de barcos ($500, con la simplificación anotada en [[Vehículos como producto económico]]), mantenimiento por barco, guardado/carga de toda la flota.

## Fase 4 — cerrada

Definition of Done confirmada por el usuario. Ver detalle completo en [[Fase 4 - Logística multi-isla]]. Resumen: 3 islas con la cadena repartida (Mina/Acería/Puerto) a distancia real, cámara siguiendo al jugador, flota con rutas (jugador libre, bots restringidos a su ruta), anillos visuales de muelle, autopiloto con frenado, reequilibrio del reloj económico (varias vueltas de ajuste tras pruebas del usuario: barco atascado, violación de balance, bancarrota por reloj mal calibrado), y el checkpoint de Sistema de Replay (F6/F7, paso de simulación fijo).

## Siguiente paso

[[Fase 5 - Primera empresa de IA]] — una empresa autónoma que usa exactamente las mismas mecánicas que el jugador. El propio roadmap avisa de que esta fase, tal como está descrita, es en realidad un proyecto de IA en sí mismo — cuando se empiece, hay que desglosarla en sub-fases con su propio Definition of Done antes de tocar código, igual que se hizo con este roadmap completo.

## Decisiones de diseño añadidas durante el desarrollo (vigentes para cuando toque implementarlas)

- [[Encarnación y capa de mando]] — el jugador es un personaje físico en primera persona, no un cursor de gestión; coexiste con un mapa de mando estilo X4.
- [[Vehículos como interfaz]] — roster completo (naval/aéreo/terrestre, civil/militar), no solo barcos.
- [[Carga física - contenedores]] — el contenedor como única unidad físicamente interactuable, base de [[Fase 9 - Seguridad]].
- [[Vehículos como producto económico]] — los vehículos son productos con productor propio; comprar barcos a facciones neutrales es una simplificación hasta que existan facciones/astilleros simulados de verdad.

Ninguna de estas afecta el código actual — son visión a largo plazo documentada para no perderla, salvo la última, que ya tiene una simplificación concreta implementada en Fase 3.
