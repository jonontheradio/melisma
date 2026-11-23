# Melisma

Editor de tono y tiempo por notas, de código abierto, hecho desde cero en C++. Inspirado en los editores de afinación "por blobs": detecta cada nota de una voz y te deja moverla, estirarla y afinarla sin que suene robótico.

> 🚧 **En desarrollo temprano.** Todavía no hay nada usable; mira el [plan de acción](docs/PLAN.md) y los [milestones](https://github.com/jonontheradio/melisma/milestones).

## Objetivos

- **Standalone** para abrir un WAV, editar y exportar.
- **Plugin VST3/AU** que funcione en **Ableton Live** (modo captura) y en cualquier DAW.
- **ARA2** más adelante, para los DAWs que lo soportan.
- Primero voz monofónica; polifonía como línea de investigación.

## Cómo funciona (resumen)

1. **Análisis**: detecta la curva de tono (YIN/pYIN), separa las partes con y sin voz, y divide el audio en notas.
2. **Edición**: cada nota es un "blob" en un piano roll; cambias tono, tiempo, deriva, vibrato, formantes y volumen.
3. **Resíntesis**: reconstruye el audio con PSOLA (y más adelante con un vocoder), sin tocar nunca el original.

Los detalles, la arquitectura y todas las opciones configurables están en [docs/PLAN.md](docs/PLAN.md).

## Stack

C++20 · CMake · [JUCE 8](https://juce.com) · Catch2 · GitHub Actions

## Compilar

*Llegará con el milestone M0.*

## Licencia

[AGPL-3.0](LICENSE), compatible con la licencia open source de JUCE.

"Melodyne" es una marca de Celemony. Melisma es un proyecto independiente, sin relación con Celemony.
