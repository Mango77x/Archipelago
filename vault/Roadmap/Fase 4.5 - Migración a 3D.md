---
tags: [roadmap, fase]
---

# Fase 4.5 — Migración a 3D (EN CURSO)

Insertada entre [[Fase 4 - Logística multi-isla|Fase 4]] y [[Fase 5 - Primera empresa de IA|Fase 5]] a petición explícita del usuario: necesitaba ver y sentir el proyecto en el entorno 3D real para seguir motivado — cuatro fases construyendo economía sobre cajas de colores en 2D, y el gancho central del juego (pilotar vehículos) todavía no se sentía real. Ver la nota de riesgo ya registrada en [[Roadmap tecnológico]] ("3D / renderizado real") — esto es esa mitigación, convertida en fase formal en vez de desvío no numerado.

**Objetivo**: renderizar el mundo que ya existe (3 islas, barcos, muelles) en 3D real — perspectiva, profundidad, cámara conmutable — sin tocar la simulación de por debajo. Es un cambio de capa de renderizado, no un rediseño de gameplay (principio 2: los gráficos visualizan la simulación, no la sustituyen).

**Mundo**: el mismo de Fase 4 — Isla 1 (Minera), Isla 2 (Industrial), Isla 3 (Portuaria), misma economía, mismas rutas, mismo mercado. Nada de eso cambia.

**Alcance explícito**:
- Renderizado 3D real: proyección en perspectiva, profundidad (depth buffer), formas simples en 3D (cajas para edificios, un casco simple para el barco) — sigue sin ser fidelidad visual, sigue siendo placeholder, pero ya es un espacio 3D de verdad.
- Cámara conmutable con una tecla: tercera persona (persigue al barco) y primera persona (a bordo, mirando hacia donde apunta) — ver [[Vehículos como interfaz]] y [[Encarnación y capa de mando]].
- Plano de agua y horizonte simple, para que se sienta como un entorno, no como el vacío.

**Explícitamente fuera de alcance** (para no disparar el trabajo sin querer):
- Personaje caminando, subir/bajar del barco — eso es la pieza de "encarnación en primera persona" completa, un paso posterior, no parte de esto.
- Otros tipos de vehículo (aviones, coches, tanques) — sigue siendo solo el barco de carga.
- Motor de física real (Jolt) — el modelo cinemático existente (empuje/arrastre/inercia de giro) se mantiene tal cual, solo se renderiza en 3D.
- Fidelidad visual, iluminación, texturas, modelos importados.

**Tecnología nueva**: GLM (matemáticas de vectores/matrices 3D) — justificado ahora porque hace falta una cámara de verdad (proyección en perspectiva, view matrix) y reinventar esto a mano sería peor que adoptar la librería estándar de facto para esto en C++. Ver [[Roadmap tecnológico]].

**Definition of Done**: el jugador pilota su barco en un entorno 3D real (con profundidad, perspectiva y cámara conmutable primera/tercera persona) sobre el mismo mundo y mecánicas de Fase 4, sin regresión de gameplay (economía, rutas, guardado/carga y replay siguen funcionando igual).

Anterior: [[Fase 4 - Logística multi-isla]]. Siguiente: [[Fase 5 - Primera empresa de IA]].
