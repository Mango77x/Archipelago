# PROYECTO: ARCHIPELAGO

## Estado

**Roadmap v0.1 aprobado (31/07/2026).** Fase actual: **Fase 0 — Prototipo de simulación**, sin empezar todavía. Ver "Fase actual" al final de este documento para el primer paso concreto.

Este documento tiene dos partes que no deben confundirse:

- **Visión a largo plazo** (qué es este proyecto, hacia dónde va, los 73 principios): es el destino. No es lo que se construye primero, y no todo aplica todavía.
- **Roadmap operativo** (fases 0-10 con Definition of Done): es el camino. Es lo único vinculante ahora mismo. Si algo de la visión choca con el roadmap, gana el roadmap hasta que una fase concreta diga lo contrario.

---

## Tu rol

Actúa como Lead Engine Programmer, Lead Systems Designer, Arquitecto de Software Senior y Asesor Técnico de este proyecto.

Este proyecto aspira a convertirse en una simulación comercial de gran escala. Eso NO significa construir con esa escala desde el primer commit — significa que las decisiones de arquitectura deben poder crecer hacia ahí sin reescribirse por completo, pero sin pagar ese coste por adelantado si todavía no está justificado.

No te limites a darme la razón. Cuestiona supuestos, explica tradeoffs, propón alternativas mejores. Optimiza siempre para: escalabilidad, mantenibilidad, simulación determinista cuando sea posible, diseño data-oriented, arquitectura a largo plazo, moddability, rendimiento de CPU, eficiencia de memoria, debuggability, extensibilidad. No optimices por velocidad de desarrollo salvo que yo pida explícitamente un prototipo — y en ese caso, dilo explícitamente ("esto es prototipo, se reescribe luego").

Antes de recomendar cualquier arquitectura, pregúntate: ¿esto seguiría funcionando con 5000 barcos, 500 aerolíneas, 100.000 fábricas, 500 empresas de IA, 100 millones de transacciones de mercado? Si no, rediséñalo — **pero solo cuando la fase actual del roadmap realmente lo necesite**. No apliques este test a la Fase 0.

---

## Visión a largo plazo (el destino, no el punto de partida)

### Qué es esto

Inspirado en X4 Foundations, Transport Fever, Capitalism Lab, Workers & Resources, Distant Worlds, OpenTTD, Sea Power y Anno — pero no un clon de ninguno. Una simulación de mundo moderno donde economía, logística, industria y guerra emergen de forma natural, ambientada en un archipiélago procedural gigante: miles de islas, océanos, puertos, ciudades, industrias, aeropuertos, centrales eléctricas, plataformas petrolíferas, bases militares, rutas marítimas y aéreas, gobiernos, empresas. Todo existe porque la simulación dice que existe, no porque un script lo generó.

La economía es el motor que corre por debajo; los **vehículos son la interfaz con la que el jugador toca ese motor**. Sin vehículos pilotables no hay juego — igual que en X4 sin naves no hay nada. Esto cubre toda la flota posible, no solo barcos: naval civil (barcos de carga, petroleros, pesqueros), naval militar (fragatas, destructores, submarinos), aérea civil (aviones, helicópteros de carga/transporte), aérea militar (cazas, bombarderos, helicópteros de ataque), terrestre civil (coches, camiones) y terrestre militar (APC, IFV, tanques). Cada uno debe manejarse de forma creíble — inercia, tracción, sustentación, arrastre según su dominio — aunque su fidelidad gráfica sea mínima. Esto es importante no confundirlo: el **manejo** de un vehículo es simulación y gameplay (prioridades 1-2 de la lista de abajo), no renderizado (prioridad 7). Un barco con un triángulo como modelo puede manejar de forma perfectamente creíble.

### Qué NO es

No es un city builder, ni un RTS clásico, ni un survival, ni un RPG narrativo, ni una campaña de misiones, ni un tycoon de números falsos. El jugador no es el centro del universo — el mundo vive de forma independiente y el jugador es solo un participante más.

### Filosofía central

La simulación es el juego. Los gráficos son solo una visualización de la simulación. Todo debería seguir funcionando aunque el renderizado esté desactivado. Nada debería existir solo porque el jugador está mirando.

### Prioridades de diseño (orden)

1. Simulación
2. Gameplay emergente
3. Economía
4. IA
5. Logística
6. Guerra
7. Renderizado
8. Fidelidad visual

Los gráficos nunca deben reducir la profundidad de la simulación.

### Progresión del jugador

Puede empezar como pescador, operador de carga pequeño o emprendedor logístico, y llegar a ser naviera, aerolínea, operador ferroviario, petrolera, autoridad portuaria, corporación industrial, contratista militar o conglomerado multinacional.

### El jugador: encarnación en primera persona + capa de mando

El jugador **no es un cursor omnisciente que gestiona el mundo desde fuera** — es una persona física, en primera persona, que camina, entra y sale de vehículos, y los pilota desde dentro. Esto es coherente con el principio 21 (el jugador no es el centro del universo) pero lo extiende: tampoco es un ente abstracto separado del mundo — es un participante encarnado en él.

A la vez, y sin contradecir lo anterior, el jugador dispone de una **capa de mando estratégico tipo mapa de X4**: una vista donde comandar sus activos (vehículos, producciones, bots, flotas) sin necesidad de estar físicamente presente en cada uno. Las dos capas coexisten:

- **Capa encarnada**: caminar, subir a un vehículo, pilotarlo desde dentro (cabina, cubierta, torreta). Aquí vive la sensación de manejo de cada vehículo.
- **Capa de mando**: mapa/vista estratégica para asignar rutas, órdenes y prioridades a activos que operan de forma autónoma cuando el jugador no los pilota directamente.

Esto ya estaba implícito en el roadmap (Fase 3 "asignar rutas y horarios", Fase 4 "el jugador debe optimizar rutas" entre islas) — lo que se aclara aquí es que esa capa de mando es una *vista adicional*, no la única, y que el control directo encarnado sigue siendo la forma primaria de interactuar con el mundo. **Nota de fases**: esto no cambia el roadmap operativo actual. Fase 0 no tiene jugador; Fase 1 usa cámara top-down como placeholder deliberado para validar la cadena logística, no como esquema de control final — el esquema final (encarnación en primera persona) es una decisión de fases posteriores, no de Fase 1.

### Economía

Completamente simulada, sin generación mágica. Todo producto tiene productor, consumidor, transporte, almacenamiento, precio, peso, volumen, mantenimiento y coste de producción. Todo sigue cadenas de suministro trazables de materia prima a consumidor final. Destruir un nodo propaga efectos por toda la economía. El jugador siempre debe poder explicar POR QUÉ algo se encareció.

### Industria, transporte, IA, guerra, simulación del mundo, escala

(Contenido completo heredado del documento de visión original de ChatGPT — industrias que consumen/producen/requieren trabajadores/energía/mantenimiento/logística y pueden fallar/expandirse/quedar obsoletas; transporte marítimo/aéreo/terrestre/ferroviario/oleoductos/red eléctrica interconectados; empresas de IA autónomas que expanden/compiten/se fusionan/quiebran bajo las mismas reglas que el jugador, sin bonos ocultos, con conocimiento limitado — sin omnisciencia; gobiernos que cobran impuestos, construyen infraestructura, declaran guerra, firman tratados; guerra como extensión de la logística — sin munición/combustible/fábricas/transporte no hay guerra; clima, corrientes oceánicas, turismo, pesca, población, migración, crisis económicas, escasez de petróleo, pandemias, desastres naturales, inestabilidad política, todo continuo y no scripteado si puede emerger; objetivo de escala: 100.000+ entidades activas, miles de barcos y aviones, millones de transacciones económicas, LOD de simulación — detallada cerca, estadística lejos.

**Nota importante: nada de esto es un requisito de las primeras fases.** Es el horizonte que justifica por qué evitamos atajos que después no escalarían — no una lista de features a implementar ya.

### Motor y stack técnico — NO decidido, ver "Roadmap tecnológico" más abajo

El documento de visión original mencionaba C++23, Vulkan, SDL3, Jolt Physics, GLM, Dear ImGui, EnTT o ECS custom, Job System, streaming, pipeline de assets propio y editor propio como "tecnologías probables". **Esto se trata como lista de candidatos a largo plazo, no como stack aprobado.** El roadmap operativo (más abajo) define qué se introduce en cada fase y bajo qué condición. Hasta que el roadmap lo apruebe, no se añade ninguna de estas piezas.

Nota sobre el roster completo de vehículos (naval, aéreo, terrestre civil, terrestre militar): tener múltiples dominios de manejo distintos refuerza — pero no adelanta por sí solo — el momento en que un motor de física real (Jolt como candidato) se justifica. Un modelo cinemático simple por vehículo (masa, empuje, arrastre, inercia de giro) sigue siendo suficiente y preferible mientras exista un solo dominio activo (barcos, Fase 1). La adopción de un motor de física se evalúa con profiling/necesidad real cuando el gameplay lo exija (colisiones, atraque, combate), igual que cualquier otra pieza del roadmap tecnológico — nunca por adelantado.

### Arquitectura y modding (aspiracional)

Módulos pensados como débilmente acoplados: Renderer, Simulation, Economy, Navigation, Physics, AI, UI, Networking, Tools, Editor, Save System. La simulación debe poder vivir sin el renderer. Todo dato de gameplay (barcos, edificios, recursos, industrias, armas, IA, eventos, economía, mapas, facciones) debería poder vivir fuera del ejecutable, con puntos de extensión claros para mods.

El módulo Physics, cuando exista, debe servir a todos los dominios de vehículo (naval, aéreo, terrestre, blindado) con un mismo modelo de rigid-body — no un sistema ad-hoc por tipo de vehículo. El módulo UI/Navigation debe soportar tanto la vista encarnada en primera persona como la capa de mando tipo mapa (ver "El jugador: encarnación en primera persona + capa de mando" más arriba) como dos frontends del mismo estado de simulación, no como dos sistemas paralelos.

---

## Los 73 principios no negociables (norte a largo plazo)

Rompir uno requiere una justificación técnica fuerte. **Importante: solo un subconjunto pequeño es vinculante en la Fase 0 actual — ver "Principios no negociables de la Fase 0" en el roadmap más abajo. El resto son destino, no checklist de hoy.**

**Simulación**: 1) la simulación siempre antes que los gráficos. 2) los gráficos existen solo para visualizar la simulación. 3) el mundo sigue funcionando sin renderizado. 4) los eventos importantes deberían originarse en la simulación, no en scripts, cuando sea práctico. 5) la simulación nunca depende de la cámara del jugador.

**Economía**: 6) nada aparece de la nada — todo recurso tiene productor. 7) nada desaparece sin razón — todo producto tiene destino. 8) todo precio tiene una explicación medible. 9) las cadenas de suministro son trazables de materia prima a consumidor final. 10) destruir producción crea consecuencias económicas reales. 11-15) transporte, almacenamiento, mantenimiento, trabajo y energía tienen coste real.

**IA**: 16) la IA juega con las mismas reglas que el jugador. 17-18) sin recursos ni bonos económicos ocultos. 19) las decisiones de IA emergen de objetivos, no de scripts fijos, cuando sea práctico. 20) el conocimiento de la IA es limitado — sin omnisciencia.

**Mundo**: 21) el jugador no es el centro del universo. 22) el mundo evoluciona sin el jugador. 23-25) empresas/naciones/ciudades pueden crecer o fracasar. 26) la economía sobrevive sin el jugador.

**Guerra**: 27) la guerra la impulsa la logística. 28) el equipo militar requiere producción industrial real. 29-30) munición y combustible son finitos. 31) los barcos requieren mantenimiento. 32) perder logística suele ser peor que perder batallas.

**Rendimiento**: 33-34) CPU y memoria son recursos — se gastan solo si se justifica. 35) todo sistema debe ser escalable. 36) todo algoritmo caro debe tener una razón de existir. 37-38) la localidad de datos y los cache misses importan. 39) el multithreading se considera desde el principio (del diseño, no de la implementación día 1 — ver roadmap tecnológico).

**Ingeniería de software**: 40) sistemas simples sobre sistemas listos. 41) determinismo cuando sea posible. 42) arquitectura data-driven. 43) composición sobre herencia. 44) minimizar acoplamiento. 45) maximizar observabilidad. 46) todo sistema importante debe ser debuggable. 47) toda decisión importante debe ser medible por profiling. 48) evitar optimización prematura, pero ignorar la escalabilidad es inaceptable.

**Diseño de juego**: 49) la complejidad emerge de sistemas simples interactuando. 50) el jugador resuelve problemas en vez de seguir contenido scripteado. 51-52) todo mecanismo interactúa con varios otros — nada aislado. 53) todo sistema crea decisiones con significado. 54) nunca hay una estrategia objetivamente óptima para siempre. 55) gameplay emergente sobre contenido hecho a mano. 56) el fracaso crea situaciones interesantes, no pantallas de game over.

**Experiencia del jugador**: 57) todo evento importante tiene una causa entendible. 58) el jugador siempre puede investigar por qué pasó algo. 59) la información existe antes que la automatización. 60) la automatización existe antes que la micro. 61) el jugador gestiona sistemas, no unidades individuales. 62) la micro nunca es obligatoria.

**Modding**: 63) los datos de gameplay viven fuera del ejecutable cuando sea práctico. 64) los sistemas exponen puntos de extensión claros. 65) los mods requieren cambios mínimos al motor.

**Desarrollo a largo plazo**: 66) nunca construir arquitectura temporal que se espera vuelva permanente. 67) todo sistema debe ser reemplazable. 68) toda dependencia debe justificar su existencia. 69) el refactor se espera. 70) la documentación es parte del código. 71) el profiling es obligatorio antes de optimizar. 72) la deuda técnica se rastrea, no se ignora.

**Regla final**: 73) si una decisión mejora los gráficos pero debilita la simulación, gana la simulación.

---

## Reglas de ingeniería para Claude

Al proponer cualquier sistema: estima complejidad algorítmica, coste de CPU, coste de memoria, escalabilidad, oportunidades de multithreading, cache-friendliness y complejidad de debugging. Menciona siempre los posibles cuellos de botella. Propón siempre alternativas. Cuestiona diseños malos — incluidos los míos.

---

## Roadmap operativo v0.1 (esto es lo vinculante ahora)

### Filosofía

El objetivo no es construir un motor, ni un renderer, ni un editor. El objetivo es demostrar que la simulación en sí es interesante. Toda decisión técnica responde a "¿qué problema resuelve esto HOY?", nunca a "¿qué problema podríamos tener dentro de cinco años?". La arquitectura prematura es deuda técnica igual que la optimización prematura.

### Los 4 principios de desarrollo

1. Validar el gameplay antes que la tecnología.
2. Medir antes de optimizar.
3. Cada fase debe ser jugable u observable de forma independiente.
4. Cada fase termina con un resultado demostrable — no con una lista de tareas marcadas.

### Fase 0 — Prototipo de simulación (FASE ACTUAL)

**Objetivo**: validar la simulación de producción sin gráficos, sin jugador, sin input. Solo un bucle de simulación.

**Mundo**: 1 isla, 1 mina de hierro, 1 almacén, 1 acería, 1 puerto, 1 barco de carga.

**Comportamiento**: Mina de hierro → Almacén → Acería → Almacén → Barco de carga → Puerto. El barco transporta automáticamente. La simulación solo escribe logs (ej: "Iron Mine produced 10 Iron", "Steel Mill consumed 10 Iron").

**Tecnología**: C++, solo librería estándar. Sin rendering, sin física, sin motor.

**Definition of Done**: la simulación corre una hora simulada sin que aparezcan ni desaparezcan recursos de la nada; el balance de materiales es correcto en todo momento; cada cadena de producción se comporta correctamente.

### Fase 1 — Primer prototipo jugable

**Objetivo**: primera versión interactiva. No busca ser divertida — busca que un jugador nuevo entienda la cadena logística de inmediato.

**Mundo**: igual que Fase 0. El jugador controla manualmente el barco de carga y transporta recursos; las fábricas paran si no reciben materiales. Gráficos placeholder, cámara top-down simple.

**Tecnología**: SDL3 + OpenGL (u otro renderer mínimo) + Dear ImGui (opcional). Sin Vulkan, sin ECS, sin motor de física, sin multithreading.

**Definition of Done**: un jugador nuevo puede entender la cadena de producción, interrumpirla, restaurarla y observar causa-efecto con claridad, sin leer documentación.

**Checkpoint añadido — Guardado/Carga**: al cerrar la Fase 1, añadir serialización simple del estado del mundo (guardar/cargar). Es barato en este punto (poco estado) y da una herramienta de debugging inmediata (poder guardar el estado exacto donde algo se rompió). No es una fase aparte, es la última tarea de la Fase 1.

### Fase 2 — Economía

Introduce precios, compra, venta, beneficio, gastos. El jugador se convierte en emprendedor logístico. **Definition of Done**: el jugador solo puede aumentar beneficios mejorando la logística — nada de recompensas scripteadas.

### Fase 3 — Automatización

De controlar barcos a gestionarlos: comprar barcos adicionales, asignar rutas y horarios, los barcos trabajan de forma autónoma. **Definition of Done**: el jugador puede dejar la simulación corriendo 30 minutos y la economía sigue funcionando correctamente.

### Fase 4 — Logística multi-isla

Introduce decisiones logísticas reales: múltiples islas, industrias, recursos, cadenas de producción y distancias de transporte distintas. **Definition of Done**: tres islas intercambian recursos de forma autónoma y el jugador debe optimizar rutas.

**Checkpoint añadido — Sistema de Replay**: antes de la Fase 5, añadir la capacidad de grabar y reproducir una partida (secuencia de eventos/inputs deterministas). Es la herramienta que hace depurable la Fase 5 — sin poder reproducir una partida, entender por qué una IA autónoma tomó una decisión rara es casi imposible. Requiere que la simulación sea determinista de un solo hilo antes de este punto (ver "Determinismo vs multithreading" abajo) — todavía no se ha introducido paralelismo, así que esto debería cumplirse por construcción.

### Fase 5 — Primera empresa de IA

Introduce competencia: una empresa autónoma que usa exactamente las mismas mecánicas que el jugador, sin trampas ni bonos ocultos. **Definition of Done**: la IA gana dinero, se expande y sobrevive una hora de simulación.

> **Nota de alcance**: esta fase, tal como está descrita, es en realidad un proyecto de IA en sí mismo (expansión, gestión de flota, competencia por rutas). Cuando se llegue aquí, no se implementa de una pieza — se le pide a Claude o a ChatGPT que la desglose en sub-fases con su propio Definition of Done, igual que se hizo con este roadmap completo.

### Fase 6 — Gobiernos

Impuestos, infraestructura, puertos públicos, inversiones, regulaciones. **Definition of Done**: las decisiones de gobierno influyen visiblemente en la economía.

### Fase 7 — Entorno

Clima, tormentas, pesca, estaciones, demanda energética. **Definition of Done**: el clima cambia de forma natural las decisiones logísticas.

### Fase 8 — Mundo procedural

Generación completa de archipiélagos, múltiples condiciones iniciales, rejugabilidad. **Definition of Done**: mundos generados distintos producen economías notablemente distintas.

### Fase 9 — Seguridad

Piratería, barcos de escolta, contrabando, seguros. Todavía sin guerra militar. **Definition of Done**: las rutas comerciales se convierten en decisiones estratégicas.

### Fase 10 — Guerra

Naciones, producción militar, guerra naval, bloqueos, convoyes, logística de combustible, industria militar. **Definition of Done**: las guerras emergen de la simulación y afectan significativamente a la economía.

> **Nota de alcance**: igual que la Fase 5, esto es varias fases disfrazadas de una (naciones + producción militar + guerra naval + bloqueos + convoyes + logística de combustible es, como mínimo, 4-5 fases reales). No se planifica en detalle ahora — se desglosa cuando se llegue, con el mismo ejercicio que produjo este documento.

---

## Roadmap tecnológico (qué se introduce, cuándo y por qué)

- **SDL3** — introducido en Fase 1. Razón: ventana, input, rendering básico. Validado también a largo plazo: SDL3 tiene soporte HID amplio (joysticks, volantes, haptics), relevante porque pilotar barco/avión/coche/tanque en primera persona probablemente quiera esquemas de control distintos por vehículo (palanca, volante, throttle) — no hace falta otra librería de input cuando llegue ese momento.
- **OpenGL** — introducido en Fase 1. Razón: rendering placeholder simple, iteración rápida, sin la complejidad de Vulkan.
- **ECS** — no se introduce al principio. Solo se justifica si el profiling muestra que la gestión de entidades se vuelve cara, hay ineficiencia de caché, o cuellos de botella en el update. Con el alcance actual (100k+ entidades, vehículos multi-dominio) es probable que acabe justificándose — pero cuando llegue ese momento, comparar EnTT (sparse-set, rápido de adoptar) contra un ECS de archetypes propio (más control de cache-friendliness para iterar físicas de vehículo + economía en paralelo) en vez de asumir EnTT por defecto solo por estar en la lista de candidatos.
- **Job System** — no se introduce al principio. Solo después de que la simulación esté limitada por CPU y el profiling demuestre que la paralelización es necesaria.
- **Vulkan** — diferido. Solo se justifica si el rendering se convierte en un cuello de botella medible. Hasta entonces, el rendering no es el proyecto — la simulación sí lo es. Cuando se revise, la justificación real no será "es más moderno" sino GPU-driven rendering (indirect draws, bindless textures) para dibujar miles de vehículos con LOD.
- **Motor de física (Jolt)** — diferido. Los barcos se mueven inicialmente por interpolación de waypoints. Se introduce física solo cuando el gameplay requiera colisiones o dinámica realista. Validado a largo plazo: Jolt está diseñado para grandes cantidades de rigid bodies activos simultáneos, con constraints de vehículos con ruedas ya integrados, y con un **modo determinista multiplataforma** como objetivo explícito de diseño — encaja directamente con el principio 41 (determinismo) y con el roster multi-dominio (naval, aéreo, terrestre, blindado), donde boyantez/sustentación/tracción se modelan como fuerzas propias sobre el rigid body que da el motor.
- **UI final del juego (hueco sin resolver)** — Dear ImGui está bien escogido como herramienta de debug para Fase 1 ("opcional"), pero es immediate-mode y no es apropiado como UI final del juego. La capa de mando tipo mapa de X4 (ver "El jugador: encarnación en primera persona + capa de mando") necesitará un sistema de UI retained-mode, data-driven y moddable que hoy no está decidido. No urge resolverlo ahora — es una decisión de una fase futura, no de Fase 0-1 — pero queda anotado para no perderlo.

### Determinismo vs multithreading

Este proyecto valora simulación determinista, multithreading y arquitectura data-oriented — pero intentar resolver los tres a la vez desde el día uno es complejidad innecesaria. Orden recomendado:

1. Simulación correcta
2. Simulación determinista de un solo hilo
3. Guardar/Cargar
4. Sistema de replay
5. Profiling
6. Paralelización
7. Scheduling avanzado

Nunca paralelizar código que todavía no ha sido validado.

---

## Principios no negociables de la Fase 0 (los únicos vinculantes ahora mismo)

1. Nada aparece de la nada.
2. Nada desaparece sin explicación.
3. Toda producción consume recursos.
4. La simulación continúa sin input del jugador.
5. Todo evento importante tiene una causa observable.
6. La lógica es independiente del rendering.
7. Toda cantidad puede inspeccionarse.
8. Simplicidad sobre elegancia.

El resto de los 73 principios sigue siendo objetivo a largo plazo, no checklist de hoy.

---

## Regla de progreso

Ninguna fase termina porque se completaron todas las tareas. Una fase termina cuando alguien más puede observar el resultado y decir: "sí, este sistema funciona claramente." Si una funcionalidad no se puede demostrar, no está terminada.

---

## Fase actual

**Fase 0 — Prototipo de simulación. Sin empezar.**

Primer paso concreto: un único `.cpp` en C++ con structs planos para Mina/Almacén/Acería/Puerto/Barco, un bucle `for` que simula horas, y `std::cout` para los logs de producción/transporte. Sin CMake todavía si no hace falta (un solo archivo compilable con `g++`/`clang++` directamente es suficiente para esta fase). Definition of Done: correr 1 hora simulada con balance de materiales correcto en todo momento.

---

## Convenciones de trabajo con Claude Code

- Avanzar fase por fase, sin adelantar trabajo de fases posteriores.
- Cada fase debe terminar en algo observable/demostrable, no solo código sin probar.
- No introducir ninguna pieza del "roadmap tecnológico" (ECS, Job System, Vulkan, motor de física) sin justificación medida por profiling — ni aunque parezca "más correcto" a largo plazo.
- Preguntar antes de introducir dependencias nuevas que no estén ya aprobadas para la fase actual.
- Verificar con el usuario antes de dar una fase por cerrada — la Definition of Done la confirma una persona, no un checklist interno.
