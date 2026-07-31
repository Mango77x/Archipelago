---
tags: [roadmap, tecnologia]
---

# Determinismo vs multithreading

Este proyecto valora simulación determinista, multithreading y arquitectura data-oriented — pero intentar resolver los tres a la vez desde el día uno es complejidad innecesaria. Orden recomendado:

1. Simulación correcta
2. Simulación determinista de un solo hilo
3. Guardar/Cargar
4. Sistema de replay
5. Profiling
6. Paralelización
7. Scheduling avanzado

Nunca paralelizar código que todavía no ha sido validado.

Este orden es la razón por la que el checkpoint de Guardado/Carga cae al final de [[Fase 1 - Primer prototipo jugable|Fase 1]] y el de Replay antes de [[Fase 5 - Primera empresa de IA|Fase 5]] — ambos dependen de que la simulación siga siendo determinista de un solo hilo hasta ese punto.

Ver también: [[Roadmap tecnológico]], [[Principios - Rendimiento]].
