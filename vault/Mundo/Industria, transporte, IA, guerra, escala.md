---
tags: [vision, mundo]
---

# Industria, transporte, IA, guerra, simulación del mundo, escala

Contenido heredado del documento de visión original: industrias que consumen/producen/requieren trabajadores/energía/mantenimiento/logística y pueden fallar/expandirse/quedar obsoletas; transporte marítimo/aéreo/terrestre/ferroviario/oleoductos/red eléctrica interconectados; empresas de IA autónomas que expanden/compiten/se fusionan/quiebran bajo las mismas reglas que el jugador, sin bonos ocultos, con conocimiento limitado — sin omnisciencia; gobiernos que cobran impuestos, construyen infraestructura, declaran guerra, firman tratados; guerra como extensión de la logística — sin munición/combustible/fábricas/transporte no hay guerra; clima, corrientes oceánicas, turismo, pesca, población, migración, crisis económicas, escasez de petróleo, pandemias, desastres naturales, inestabilidad política, todo continuo y no scripteado si puede emerger.

**Objetivo de escala**: 100.000+ entidades activas, miles de barcos y aviones, millones de transacciones económicas. **LOD de simulación**: detallada cerca del jugador, estadística lejos. (Este LOD es la pieza que hace viable [[Carga física - contenedores]] a escala.)

## La distancia es parte del diseño, no un número para presumir

X4 Foundations funciona como funciona porque es un sistema entero con distancias reales — la vastedad no es decoración, es lo que hace que la logística importe: un viaje largo cuesta tiempo de verdad, y esa tensión es la que crea decisiones con significado (ver [[Principios - Diseño de juego]], principios 53-54). Un archipiélago grande pero con todo pegado no genera ese gameplay — el tamaño tiene que traducirse en distancia real que el jugador sienta al planificar rutas.

Esto tiene una implicación técnica concreta que no puede esperar a [[Fase 8 - Mundo procedural|Fase 8]]: en cuanto [[Fase 4 - Logística multi-isla|Fase 4]] introduce más de una isla, el mundo deja de caber en una sola pantalla como en Fase 0-3 (donde posición del mundo y posición en píxeles son literalmente lo mismo). Fase 4 necesita desacoplar espacio del mundo de espacio de pantalla — una cámara con posición/zoom propios — y las islas deben espaciarse con distancias que hagan la decisión de rutas real, no trivial. [[Fase 8 - Mundo procedural|Fase 8]] no inventa esto de cero: sustituye el layout fijo de Fase 4 por generación real, sobre la misma base de cámara/mundo desacoplado.

**Nota importante**: nada de esto es un requisito de las primeras fases. Es el horizonte que justifica por qué evitamos atajos que después no escalarían — no una lista de features a implementar ya.

Ver también: [[Principios - IA]], [[Principios - Guerra]], [[Principios - Mundo]], [[Fase 5 - Primera empresa de IA]], [[Fase 6 - Gobiernos]], [[Fase 7 - Entorno]], [[Fase 10 - Guerra]].
