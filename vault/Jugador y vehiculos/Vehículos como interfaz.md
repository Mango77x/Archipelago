---
tags: [vision, jugador, vehiculos]
---

# Vehículos como interfaz del jugador con la economía

La [[Economía]] es el motor que corre por debajo; los vehículos son la interfaz con la que el jugador toca ese motor. Sin vehículos pilotables no hay juego — igual que en X4 sin naves no hay nada.

Esto cubre toda la flota posible, no solo barcos:

- **Naval civil**: barcos de carga, petroleros, pesqueros.
- **Naval militar**: fragatas, destructores, submarinos.
- **Aérea civil**: aviones, helicópteros de carga/transporte.
- **Aérea militar**: cazas, bombarderos, helicópteros de ataque.
- **Terrestre civil**: coches, camiones.
- **Terrestre militar**: APC, IFV, tanques.

Cada uno debe manejarse de forma creíble — inercia, tracción, sustentación, arrastre según su dominio — aunque su fidelidad gráfica sea mínima. El **manejo** de un vehículo es simulación y gameplay (ver [[Prioridades de diseño]], prioridades 1-2), no renderizado (prioridad 7). Un barco con un triángulo como modelo puede manejar de forma perfectamente creíble — así es como está construido el barco jugable de [[Fase 1 - Primer prototipo jugable|Fase 1]] ahora mismo (modelo cinemático simple: empuje, arrastre, inercia de giro, sin motor de física).

## Implicaciones técnicas

- **Physics**: cuando exista un motor de física real, debe servir a todos los dominios (naval, aéreo, terrestre, blindado) con un mismo modelo de rigid-body — no un sistema ad-hoc por tipo de vehículo. Ver [[Motor y stack técnico]] y [[Roadmap tecnológico]] (Jolt).
- **SDL3**: elegido en parte porque tiene soporte HID amplio (joysticks, volantes, haptics), relevante porque cada dominio de vehículo probablemente quiera un esquema de control distinto (palanca, volante, throttle).
- **Arquitectura**: ver [[Arquitectura y modding]] — el módulo Physics debe ser común a todos los dominios, no fragmentado.

Ver también: [[Encarnación y capa de mando]], [[Progresión del jugador]].
