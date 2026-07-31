---
tags: [roadmap, fase]
---

# Fase 2 — Economía (CERRADA)

Introduce precios, compra, venta, beneficio, gastos. El jugador se convierte en emprendedor logístico.

**Definition of Done**: el jugador solo puede aumentar beneficios mejorando la logística — nada de recompensas scripteadas.

## Implementado

- **`Market`**: mercado de oferta/demanda de un único bien (Acero). Precio = `basePrice / (1 + stock * sensitivity)`, con suelo mínimo. Vender Acero suma al stock (glut de oferta); cada hora simulada el mercado absorbe una cantidad fija de demanda, y el precio se recupera a medida que el stock baja. Esto es el pilar de precio dinámico que pidió el usuario explícitamente — decisión de diseño: no es un mercado multi-isla/multi-actor (eso es [[Fase 4 - Logística multi-isla|Fase 4]]/[[Fase 5 - Primera empresa de IA|Fase 5]]), es la versión mínima de un solo mercado reaccionando al propio ritmo de entrega del jugador.
- **`Economy`**: caja del jugador (`cash`, empieza en $1000). Ingresos solo vienen de vender en el `Market` al descargar en el puerto. Gastos: mantenimiento fijo por hora ($15, mina+acería+barco combinados) que corre **siempre**, mueva o no el barco el jugador — esto es lo que fuerza que la única forma de mejorar beneficio sea la logística real (entregar más, más rápido, sin hundir el precio), no un truco.
- Guardado/carga (F5/F9) extendido para incluir el estado de `Market` y `Economy`.
- Panel de ImGui extendido: caja, precio actual, stock de mercado, ingresos y gastos totales.

Probado por el usuario: el precio reacciona de forma perceptible al ritmo de entrega ("pinta bien"). Definition of Done confirmada por el usuario — fase cerrada.

## Deliberadamente fuera de este primer corte

- Comprar como transacción interactiva explícita (sigue sin hacer falta; los gastos fijos ya cubren "compra/gastos" del roadmap).
- Consecuencias de caja negativa/quiebra (el número puede ir negativo, sin efecto mecánico todavía).
- Combustible ligado al empuje del barco (refinamiento natural futuro, no necesario para el DoD actual).

Podría ser el punto natural para empezar a decidir qué productos son [[Carga física - contenedores|carga unitizada]] desde el principio (no implementación todavía).

Anterior: [[Fase 1 - Primer prototipo jugable]]. Siguiente: [[Fase 3 - Automatización]].
