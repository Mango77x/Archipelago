---
tags: [roadmap, fase]
---

# Fase 7.1 — Oleaje y boyantez (CERRADA)

Construye directamente sobre [[Fase 7.0 - Motor de física real (Jolt)]]: ahí se migró `CargoShip` a un rigid body real pero restringido a X/Z + guiñada, sobre agua todavía plana, precisamente para aislar "¿el manejo por física se siente bien?" de "¿el oleaje funciona?". Esta fase resuelve la segunda mitad — es el motivo original por el que el usuario pidió física real ("mis juegos dependen siempre de física buena y real... como Sea of Thieves o Stormworks... el juego para el usuario es usar vehículos y los vehículos tienen que convivir con el entorno").

**Objetivo**: el barco boya y cabecea/balancea de verdad sobre un campo de olas real, y el agua se ve ondulando igual que se siente — no una malla plana con el barco meciéndose sobre "cristal".

**Alcance**:
1. **Función de olas Gerstner compartida**, 2-3 olas superpuestas (direcciones, longitudes de onda, amplitudes y velocidades distintas) para un mar con aspecto irregular sin coste de cómputo relevante. Implementada una vez en C++ (para el muestreo físico) y replicada en GLSL con los mismos parámetros (para el vertex shader del agua) — muestreo y render tienen que coincidir, o el barco "flotará mal" a ojo aunque la física esté bien.
2. **Malla de agua real**: sustituir el plano único actual (`DrawBox` escalado, ver `main.cpp` ~línea 1381) por una malla subdividida (grid), desplazada por vértice en el vertex shader usando la función de olas + tiempo transcurrido.
3. **Boyantez**: muestrear la altura de ola en varios puntos bajo el casco (mínimo 4, una por esquina) y aplicar fuerza de flotación en cada punto según su profundidad sumergida, vía `AddForce` con punto de aplicación — el par de cabeceo/balanceo tiene que salir de la física real (fuerzas asimétricas), no simularse a mano.
4. **Relajar los grados de libertad** del cuerpo del barco: de `TranslationX | TranslationZ | RotationY` a los 6 completos (añadir traslación Y, cabeceo, balanceo). Reactivar la gravedad (ahora mismo `mGravityFactor = 0.0f`, ver `CreateShipBody`) para que la boyantez tenga algo que contrarrestar. Es probable que la amortiguación angular necesite retunearse por sensación otra vez (mismo patrón que en 7.0: los valores de fuerza/par están marcados como "en ajuste", no derivados de primeros principios).
5. **Revisar la cámara**: con cabeceo/balanceo real, la cámara en tercera/primera persona puede necesitar desacoplarse parcialmente del roll del barco para no resultar mareante. A probar y decidir sobre la marcha, no bloqueante para cerrar la fase.

**Explícitamente fuera de este paso** (sigue en el resto de [[Fase 7 - Entorno]], después):
- Tormentas como oleaje extremo (amplitud/frecuencia aumentadas, no un sistema nuevo).
- Pesca, estaciones, demanda energética.

**Definition of Done**: el barco boya y cabecea/balancea de forma creíble sobre un mar que visualmente ondula igual que la física que se siente (mismo campo de olas en muestreo y render), sin romper atraque, carga/descarga, guardado/carga, replay ni la IA — confirmado por el usuario jugando en directo, igual que en 7.0.

**Confirmado por el usuario jugando en directo.** Ajustes hechos durante la validación (los mismos "en ajuste por sensación" que en Fase 7.0):
- Boyantez inicial (rigidez 3000, sin amortiguación) hacía que el barco saliera disparado fuera del agua y rebotara sin parar — un muelle sin amortiguador es un oscilador no amortiguado. Se añadió un término de amortiguación (`kBuoyancyDamping`, cerca del crítico) que se opone a la velocidad vertical real de cada esquina (via `GetPointVelocity`, para que cabeceo/balanceo también contribuyan), y se bajó la rigidez (2000) para que el barco cale hasta una línea de flotación creíble en vez de ir "planeando" por encima del agua.
- Los barcos se posicionan ligeramente por encima del nivel nominal del agua al crearse (`kInitialDraftOffset`), para no arrancar con un "chapoteo" inicial mientras la boyantez aún no tiene nada que empujar.
- Se añadió un fondo marino plano (cuerpo estático de colisión + visual, sin deformación) a petición del usuario — "para ciertas embarcaciones o futuros submarinos". Sin forma de terreno todavía; eso espera a mundo procedural (Fase 8).
- La cámara ya solo usaba el rumbo (yaw) del barco, no la rotación completa, así que quedó desacoplada del balanceo/cabeceo sin tocar código — confirmado que no marea.

Anterior: [[Fase 7.0 - Motor de física real (Jolt)]]. Siguiente: resto de [[Fase 7 - Entorno]] (tormentas, pesca, estaciones, demanda energética), luego [[Fase 8 - Mundo procedural]].
