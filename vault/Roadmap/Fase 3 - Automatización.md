---
tags: [roadmap, fase]
---

# Fase 3 — Automatización (CERRADA)

De controlar barcos a gestionarlos: comprar barcos adicionales, asignar rutas y horarios, los barcos trabajan de forma autónoma.

**Definition of Done**: el jugador puede dejar la simulación corriendo 30 minutos y la economía sigue funcionando correctamente. **Confirmada por el usuario.**

## Implementado

- **Flota** (`std::vector<CargoShip>`): el barco #0 es siempre el del jugador (pilotable a mano); cualquier barco adicional comprado es siempre autopilotado — consecuencia directa de [[Encarnación y capa de mando]] (el jugador es una persona física, solo puede pilotar un casco a la vez).
- **Autopiloto**: reutiliza el mismo modelo cinemático (empuje/arrastre/inercia de giro) que el barco del jugador — gira hacia el objetivo y acelera cuando está encarado, no hay teletransporte ni atajos de movimiento para la IA. El destino se decide por el estado de la carga: vacío → rumbo al almacén de la acería; cargado → rumbo al puerto. La lógica de carga/descarga por proximidad ya existente no cambió nada.
- **Comprar barco**: botón en el panel de ImGui ($500, deshabilitado si no hay caja suficiente). Ver [[Vehículos como producto económico]] — es una simplificación deliberada (precio fijo a un vendedor abstracto no simulado, deuda técnica anotada para cuando existan facciones/astilleros reales en [[Fase 5 - Primera empresa de IA|Fase 5]]/[[Fase 6 - Gobiernos|Fase 6]]).
- **Mantenimiento por barco**: cada barco de la flota (incluido el del jugador) añade su propio coste fijo por hora, además del coste base de mina+acería — evita que comprar barcos sin límite sea beneficio gratis; con la demanda del mercado limitada, más barcos de los que el mercado absorbe hunde el precio.
- **Guardado/carga**: reescrito para serializar toda la flota (N barcos), no un barco fijo.

Esta es la primera fase donde aparece de forma explícita la [[Encarnación y capa de mando|capa de mando]] (asignar rutas y horarios sin pilotar cada barco) — aunque con una sola ruta posible (acería↔puerto) todavía no hay nada real que "asignar"; eso llega con [[Fase 4 - Logística multi-isla|Fase 4]] (múltiples islas, múltiples rutas).

Probado por el usuario: compra de barco, autopiloto físico, mantenimiento por flota, y guardado/carga con varios barcos, todo funcionando.

Anterior: [[Fase 2 - Economía]]. Siguiente: [[Fase 4 - Logística multi-isla]].
