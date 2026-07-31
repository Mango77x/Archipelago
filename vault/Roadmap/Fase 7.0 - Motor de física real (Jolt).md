---
tags: [roadmap, fase]
---

# Fase 7.0 — Motor de física real (Jolt Physics) (EN CURSO)

Insertada a petición explícita del usuario: quiere oleaje y boyantez de verdad (referencias: Sea of Thieves, Stormworks), no una modulación falsa de constantes. Eso es exactamente el disparador que [[Roadmap tecnológico]] ya tenía anotado para Jolt Physics ("se introduce física real solo cuando el gameplay requiera... dinámica realista... boyantez modelada como fuerzas sobre el rigid body que da el motor"). El usuario decidió pausar el desarrollo económico (Fase 7 Entorno / Fase 8 Mundo procedural) hasta tener esta base bien construida.

**Objetivo**: sustituir el modelo cinemático a mano de `CargoShip` (posición X/Z + rumbo, sin motor de física) por un cuerpo rígido real de Jolt — sentando la base sobre la que [[Fase 7 - Entorno|Fase 7.1 (Oleaje y boyantez)]] construirá después.

**Por qué Jolt y no física a mano**: se valoraron las dos opciones con el usuario. Física a mano (integración 6 grados de libertad + muestreo de olas escrito desde cero) es más rápida de tener funcionando pero se tira a la basura en cuanto lleguen más vehículos, colisiones o física de atraque. Jolt es más trabajo de arranque pero es la respuesta definitiva: determinismo multiplataforma de fábrica, soporte de rigid bodies a gran escala, y sirve para todo el roster de vehículos futuro (no solo barcos) sin reescribirse. Se decidió no repetir el patrón de "hueco pendiente que se acaba pagando dos veces".

**Alcance de este primer paso concreto**:
1. Añadir Jolt Physics como dependencia (vcpkg).
2. Montar un mundo Jolt mínimo (gravedad, un plano/suelo) con un cuerpo rígido de prueba — validar que el motor compila, enlaza y simula correctamente antes de tocar el barco.
3. Migrar `CargoShip` a un rigid body de Jolt: el empuje/giro que antes modificaba velocidad directamente pasa a aplicarse como fuerzas y pares de fuerza reales sobre el cuerpo. Todavía sin oleaje — agua plana, para aislar "¿el manejo por física se siente bien?" de "¿el oleaje funciona?".

**Explícitamente fuera de este primer paso** (viene después, en 7.1):
- Campo de oleaje real (olas tipo Gerstner).
- Boyantez (muestreo de altura de ola bajo el casco, fuerza de flotación, cabeceo/balanceo).
- Tormentas como oleaje extremo.

**Definition of Done de este paso**: el barco del jugador y los de la IA se mueven mediante fuerzas físicas reales de Jolt (no velocidad modificada a mano), sin regresión de manejo perceptible respecto al modelo cinemático anterior, sobre agua todavía plana.

Anterior: [[Fase 5 - Primera empresa de IA]] (con [[Fase 6 - Gobiernos|Fase 6]] aplazada). Siguiente: Fase 7.1 — Oleaje y boyantez (a definir tras cerrar este paso).
