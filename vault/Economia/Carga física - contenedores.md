---
tags: [vision, economia, gameplay]
---

# Carga física: el contenedor como unidad interactuable

No existe "a granel" como modo de transporte aparte — es un caso particular de lo de abajo, no un sistema distinto. Todo lo que produce una fábrica (mina, acería, refinería, granja) se acumula en un **buffer interno invisible**, sin representación física en el mundo — el mismo contador simple que ya existe en el código de [[Fase 0 - Prototipo de simulación|Fase 0]]/[[Fase 1 - Primer prototipo jugable|Fase 1]] (`Warehouse::Get(Resource::X)`). Mientras ese buffer no llegue a la capacidad de un contenedor (un barril, una caja, un contenedor estándar), no existe ningún objeto físico. En el instante en que el buffer cruza ese umbral, se **materializa un contenedor físico real** en el mundo (en el muelle de salida de la fábrica), y el buffer se queda con el remanente acumulando para el siguiente.

## Consecuencias de este modelo

- **El contenedor es la única unidad físicamente interactuable del mundo.** Tiene posición, peso propio (contenedor + contenido) y puede robarse, cargarse a mano, con carretilla o con grúa, transportarse y perderse como un objeto entero. Lo que hay dentro es solo un dato (tipo de recurso + cantidad) — nunca se simulan litros, kilos o unidades sueltas dentro de un contenedor.
- **El tamaño del contenedor determina quién puede moverlo.** Un barril o una caja pequeña la puede cargar el jugador (limitado por su stat de fuerza) o un estibador contratado a mano. Un contenedor estándar o un palet grande necesita sí o sí una carretilla o una grúa — ninguna cantidad de fuerza del jugador lo compensa.
- **Cargar/descargar consume tiempo y capacidad de trabajo real** (fuerza del jugador, la de un estibador contratado, o el rendimiento de una carretilla/grúa). Esto convierte la logística portuaria en una decisión económica: pagar por escalar el rendimiento del puerto (más estibadores, más máquinas) o hacerlo tú mismo, más lento pero gratis.
- **Esto es la base física de [[Fase 9 - Seguridad|Fase 9 (Piratería)]]**: robar contenedores concretos bajo presión de tiempo (te llevas 3 de los 20 antes de que lleguen refuerzos), en vez de vaciar una barra de progreso abstracta como en X4.
- **No contradice el LOD de simulación** descrito en [[Industria, transporte, IA, guerra, escala]] ("detallada cerca, estadística lejos"). Lejos del jugador, la carga de un barco o fábrica sigue siendo una cantidad agregada — no hace falta materializar ningún contenedor. Los contenedores físicos concretos solo se generan/importan cuando el jugador está lo bastante cerca como para interactuar con ellos (abordar, robar, cargar a mano). Esto evita el problema de escala de simular millones de contenedores individuales a la vez.
- **No cambia nada en Fase 0/1 actuales.** El contador simple de Hierro/Acero que ya existe en `src/main.cpp` es exactamente el buffer interno pre-contenedor de este modelo — es la base correcta, no hay que tocarlo hasta que una fase futura (probablemente [[Fase 2 - Economía|Fase 2]] en adelante) decida empezar a materializar contenedores de verdad.

Ver también: [[Economía]], [[Principios - Economía]], [[Vehículos como interfaz]] (fuerza/estibadores como capacidad de trabajo).
