---
tags: [vision, jugador]
---

# El jugador: encarnación en primera persona + capa de mando

El jugador **no es un cursor omnisciente que gestiona el mundo desde fuera** — es una persona física, en primera persona, que camina, entra y sale de vehículos, y los pilota desde dentro. Esto es coherente con [[Principios - Mundo|el principio 21]] (el jugador no es el centro del universo) pero lo extiende: tampoco es un ente abstracto separado del mundo — es un participante encarnado en él.

A la vez, y sin contradecir lo anterior, el jugador dispone de una **capa de mando estratégico tipo mapa de X4**: una vista donde comandar sus activos (vehículos, producciones, bots, flotas) sin necesidad de estar físicamente presente en cada uno. Las dos capas coexisten:

- **Capa encarnada**: caminar, subir a un vehículo, pilotarlo desde dentro (cabina, cubierta, torreta). Aquí vive la sensación de manejo de cada [[Vehículos como interfaz|vehículo]].
- **Capa de mando**: mapa/vista estratégica para asignar rutas, órdenes y prioridades a activos que operan de forma autónoma cuando el jugador no los pilota directamente.

Esto ya estaba implícito en el roadmap ([[Fase 3 - Automatización|Fase 3]] "asignar rutas y horarios", [[Fase 4 - Logística multi-isla|Fase 4]] "el jugador debe optimizar rutas" entre islas) — lo que se aclara aquí es que esa capa de mando es una *vista adicional*, no la única, y que el control directo encarnado sigue siendo la forma primaria de interactuar con el mundo.

**Nota de fases**: esto no cambia el roadmap operativo actual. [[Fase 0 - Prototipo de simulación|Fase 0]] no tiene jugador; [[Fase 1 - Primer prototipo jugable|Fase 1]] usa cámara top-down como placeholder deliberado para validar la cadena logística, no como esquema de control final — el esquema final (encarnación en primera persona) es una decisión de fases posteriores.

Implicación de arquitectura (ver [[Arquitectura y modding]]): el módulo UI/Navigation debe soportar ambas capas como dos frontends del mismo estado de simulación, no como dos sistemas paralelos.
