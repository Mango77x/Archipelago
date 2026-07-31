---
tags: [vision, tecnologia]
---

# Arquitectura y modding (aspiracional)

Módulos pensados como débilmente acoplados: Renderer, Simulation, Economy, Navigation, Physics, AI, UI, Networking, Tools, Editor, Save System. La simulación debe poder vivir sin el renderer. Todo dato de gameplay (barcos, edificios, recursos, industrias, armas, IA, eventos, economía, mapas, facciones) debería poder vivir fuera del ejecutable, con puntos de extensión claros para mods.

El módulo **Physics**, cuando exista, debe servir a todos los dominios de vehículo (naval, aéreo, terrestre, blindado) con un mismo modelo de rigid-body — no un sistema ad-hoc por tipo de vehículo. Ver [[Vehículos como interfaz]].

El módulo **UI/Navigation** debe soportar tanto la vista encarnada en primera persona como la capa de mando tipo mapa (ver [[Encarnación y capa de mando]]) como dos frontends del mismo estado de simulación, no como dos sistemas paralelos.

Ver también: [[Motor y stack técnico]], [[Principios - Ingeniería de software]], [[Principios - Modding]].
