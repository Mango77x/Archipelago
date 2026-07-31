---
tags: [roadmap, fase]
---

# Fase 5.1 — Competidor con capital propio (CERRADA)

Primer sub-paso de [[Fase 5 - Primera empresa de IA]] — el propio roadmap advertía que esa fase, tal cual estaba escrita, era en realidad un proyecto de IA en sí mismo, y pedía desglosarla al llegar aquí.

**Objetivo**: una segunda empresa (IA) que compite de verdad con el jugador, usando exactamente las mismas mecánicas — sin cadena de producción propia ni mercado aparte.

**Implementado**:
- **`Economy` propia** para la IA, misma caja inicial que el jugador ($2000) — sin bonos ocultos (principios 16-18).
- **Compite por el mismo Hierro/Acero y el mismo Mercado** que el jugador — no tiene su propia mina/acería. Decisión de diseño explícita: más simple de construir que una cadena paralela, y más interesante (competencia real por recursos escasos y por el precio del mercado compartido, no dos economías aisladas que solo comparten el nombre).
- **Flota separada** (`aiShips`), siempre autopilotada, empieza con 1 barco en la Ruta Hierro.
- **Decisión de expansión simple** (`RunAiDecisionLogic`, evaluada una vez por hora simulada): si su caja supera el doble del coste de un barco, compra uno nuevo en la ruta con menos barcos propios — regla fija, no aprendizaje ni IA sofisticada. El principio 19 ("decisiones emergen de objetivos, no de scripts fijos") queda como aspiración para un sub-paso posterior, no exigible aquí.
- Tope de flota (`kAiMaxShips = 5`) para no crecer sin límite en sesiones largas.
- Barcos rivales en color distinto (rojizo) para distinguirlos a simple vista.
- Panel "Empresa rival (IA)": caja, ingresos, gastos, composición de flota por ruta.
- Guardado/carga extendido para incluir la economía y flota de la IA.
- El Sistema de Replay no necesitó cambios: las decisiones de la IA son función pura del estado simulado (no un input externo), así que reproducir las mismas acciones del jugador desde el mismo punto de partida reproduce el mismo comportamiento de la IA.

**Definition of Done** (heredado de la Fase 5 original): la empresa rival gana dinero, se expande (compra barcos) y sobrevive una hora de simulación — compitiendo con el jugador por el mismo suministro, sin ayudas. **Confirmado por el usuario**: "va mejorando su flota y ganando dinero".

Anterior: [[Fase 4.5 - Migración a 3D]]. Ver [[Fase 5 - Primera empresa de IA]] para el resto del desglose (decisiones más inteligentes, transparencia vía Replay, etc. — a definir cuando se retome).
