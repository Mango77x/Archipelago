---
tags: [vision, tecnologia]
---

# Motor y stack técnico — NO decidido de antemano

El documento de visión original mencionaba C++23, Vulkan, SDL3, Jolt Physics, GLM, Dear ImGui, EnTT o ECS custom, Job System, streaming, pipeline de assets propio y editor propio como "tecnologías probables". **Esto se trata como lista de candidatos a largo plazo, no como stack aprobado.** El [[Roadmap tecnológico]] define qué se introduce en cada fase y bajo qué condición. Hasta que el roadmap lo apruebe, no se añade ninguna de estas piezas — ver [[Convenciones de trabajo con Claude Code]].

Nota sobre el roster completo de vehículos (ver [[Vehículos como interfaz]]: naval, aéreo, terrestre civil, terrestre militar): tener múltiples dominios de manejo distintos refuerza — pero no adelanta por sí solo — el momento en que un motor de física real (Jolt como candidato) se justifica. Un modelo cinemático simple por vehículo (masa, empuje, arrastre, inercia de giro) sigue siendo suficiente y preferible mientras exista un solo dominio activo (barcos, [[Fase 1 - Primer prototipo jugable|Fase 1]]). La adopción de un motor de física se evalúa con profiling/necesidad real cuando el gameplay lo exija (colisiones, atraque, combate), igual que cualquier otra pieza del roadmap tecnológico — nunca por adelantado.

Ver también: [[Arquitectura y modding]], [[Determinismo vs multithreading]].
