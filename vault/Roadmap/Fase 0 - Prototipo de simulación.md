---
tags: [roadmap, fase]
---

# Fase 0 — Prototipo de simulación (CERRADA)

**Objetivo**: validar la simulación de producción sin gráficos, sin jugador, sin input. Solo un bucle de simulación.

**Mundo**: 1 isla, 1 mina de hierro, 1 almacén, 1 acería, 1 puerto, 1 barco de carga.

**Comportamiento**: Mina de hierro → Almacén → Acería → Almacén → Barco de carga → Puerto. El barco transporta automáticamente. La simulación solo escribe logs (ej: "Iron Mine produced 10 Iron", "Steel Mill consumed 10 Iron").

**Tecnología**: C++, solo librería estándar. Sin rendering, sin física, sin motor.

**Definition of Done**: la simulación corre una hora simulada sin que aparezcan ni desaparezcan recursos de la nada; el balance de materiales es correcto en todo momento; cada cadena de producción se comporta correctamente.

**Resultado**: cerrada y verificada — 48 horas simuladas sin violaciones de balance, committeada en git. Ver [[Estado del proyecto]].

Principios vinculantes en esta fase: [[Principios no negociables de la Fase 0]].

Siguiente: [[Fase 1 - Primer prototipo jugable]].
