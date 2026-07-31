---
tags: [roadmap, fase]
---

# Fase 4.5 — Migración a 3D (CERRADA)

Insertada entre [[Fase 4 - Logística multi-isla|Fase 4]] y [[Fase 5 - Primera empresa de IA|Fase 5]] a petición explícita del usuario: necesitaba ver y sentir el proyecto en el entorno 3D real para seguir motivado — cuatro fases construyendo economía sobre cajas de colores en 2D, y el gancho central del juego (pilotar vehículos) todavía no se sentía real. Ver la nota de riesgo ya registrada en [[Roadmap tecnológico]] ("3D / renderizado real") — esto es esa mitigación, convertida en fase formal en vez de desvío no numerado.

**Objetivo**: renderizar el mundo que ya existe (3 islas, barcos, muelles) en 3D real — perspectiva, profundidad, cámara conmutable — sin tocar la simulación de por debajo. Es un cambio de capa de renderizado, no un rediseño de gameplay (principio 2: los gráficos visualizan la simulación, no la sustituyen).

**Mundo**: el mismo de Fase 4 — Isla 1 (Minera), Isla 2 (Industrial), Isla 3 (Portuaria), misma economía, mismas rutas, mismo mercado. Nada de eso cambia.

**Implementado**:
- Renderizado 3D real: proyección en perspectiva, profundidad (depth buffer), formas simples en 3D (cajas para edificios, un casco alargado para el barco, un plano de agua) — sigue siendo placeholder, pero ya es un espacio 3D de verdad, no un truco 2D.
- Cámara conmutable con la tecla **C**: tercera persona (persigue al barco) y primera persona (a bordo, mirando hacia donde apunta, sin dibujar el propio casco) — ver [[Vehículos como interfaz]] y [[Encarnación y capa de mando]].
- Iluminación direccional simple (un "sol", sombreado Lambertiano por normal de cara, sin sombras) — **ajuste de alcance**: originalmente se dejó fuera ("fidelidad visual, iluminación..."), pero se añadió dentro de esta misma fase a petición del usuario porque era barato (normales por vértice + un producto escalar, sin assets nuevos) y cambiaba mucho la sensación de "cajas planas" a "objetos con volumen". Sigue sin ser el techo de fidelidad visual — texturas, modelos importados, sombras y demás siguen para cuando se justifiquen (ver [[Roadmap tecnológico]]).
- Posiciones del mundo migradas de `Vec2` a `glm::vec3` (X/Z = plano horizontal, Y = altura, siempre 0 por ahora) en toda la simulación, no solo el renderizado.

**Explícitamente fuera de alcance** (para no disparar el trabajo sin querer):
- Personaje caminando, subir/bajar del barco — eso es la pieza de "encarnación en primera persona" completa, un paso posterior, no parte de esto.
- Otros tipos de vehículo (aviones, coches, tanques) — sigue siendo solo el barco de carga.
- Motor de física real (Jolt) — el modelo cinemático existente (empuje/arrastre/inercia de giro) se mantiene tal cual, solo se renderiza en 3D.
- Texturas, modelos importados, sombras, post-procesado.

**Tecnología nueva**: GLM (matemáticas de vectores/matrices 3D) — justificado porque hacía falta una cámara de verdad (proyección en perspectiva, view matrix, matriz de normales) y reinventar esto a mano sería peor que adoptar la librería estándar de facto para esto en C++. Ver [[Roadmap tecnológico]].

**Definition of Done**: el jugador pilota su barco en un entorno 3D real (con profundidad, perspectiva y cámara conmutable primera/tercera persona) sobre el mismo mundo y mecánicas de Fase 4, sin regresión de gameplay (economía, rutas, guardado/carga y replay siguen funcionando igual). **Confirmada por el usuario.**

Anterior: [[Fase 4 - Logística multi-isla]]. Siguiente: [[Fase 5 - Primera empresa de IA]].
