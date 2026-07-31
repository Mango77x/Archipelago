---
tags: [roadmap, fase]
---

# Fase 1 — Primer prototipo jugable (EN CURSO)

**Objetivo**: primera versión interactiva. No busca ser divertida — busca que un jugador nuevo entienda la cadena logística de inmediato.

**Mundo**: igual que [[Fase 0 - Prototipo de simulación|Fase 0]]. El jugador controla manualmente el barco de carga y transporta recursos; las fábricas paran si no reciben materiales. Gráficos placeholder, cámara top-down simple.

**Tecnología**: SDL3 + OpenGL (real, no SDL_Renderer — decisión explícita) + Dear ImGui (usado, no opcional al final: resuelve la falta de feedback visual de las cajas de color). Sin Vulkan, sin ECS, sin motor de física, sin multithreading. Ver [[Roadmap tecnológico]].

**Definition of Done**: un jugador nuevo puede entender la cadena de producción, interrumpirla, restaurarla y observar causa-efecto con claridad, sin leer documentación.

**Checkpoint añadido — Guardado/Carga**: al cerrar la Fase 1, añadir serialización simple del estado del mundo (guardar/cargar). Es barato en este punto (poco estado) y da una herramienta de debugging inmediata (poder guardar el estado exacto donde algo se rompió). No es una fase aparte, es la última tarea de la Fase 1. **Todavía no implementado** — ver [[Estado del proyecto]].

Progreso detallado y pendientes: [[Estado del proyecto]].

Anterior: [[Fase 0 - Prototipo de simulación]]. Siguiente: [[Fase 2 - Economía]].
