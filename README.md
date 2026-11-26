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

C++20 · CMake · [JUCE 9](https://juce.com) · Catch2 · GitHub Actions

## Compilar

### Requisitos
| Herramienta | Versión | Instalación (macOS) |
|---|---|---|
| CMake | ≥ 3.25 | `brew install cmake` |
| Ninja | cualquiera | `brew install ninja` |
| Compilador C++20 | Apple clang ≥ 15 | `xcode-select --install` (basta con las Command Line Tools) |
| uv *(opcional)* | cualquiera | `brew install uv`, solo para el formato y clang-tidy |

JUCE 9 y Catch2 se descargan solos al configurar (versión fijada y verificada por SHA-256). No hace falta instalarlos.

### Compilar y testear
```bash
cmake --preset debug          # configura en build/debug (la primera vez descarga JUCE)
cmake --build --preset debug  # compila el plugin, la app y los tests
ctest --preset debug          # ejecuta los tests
```
Para una build optimizada, usa `release` en lugar de `debug`.

### Qué se genera
| Formato | Ruta |
|---|---|
| VST3 | `build/debug/plugin/Melisma_artefacts/Debug/VST3/Melisma.vst3` |
| AU | `build/debug/plugin/Melisma_artefacts/Debug/AU/Melisma.component` |
| App standalone | `build/debug/plugin/Melisma_artefacts/Debug/Standalone/Melisma.app` |

### Instalar el plugin para probarlo en el DAW
```bash
cmake --preset debug -DMELISMA_COPY_PLUGIN_AFTER_BUILD=ON && cmake --build --preset debug
```
Esto copia el VST3 a `~/Library/Audio/Plug-Ins/VST3` y el AU a `~/Library/Audio/Plug-Ins/Components`.
En **Ableton Live**: *Settings → Plug-Ins →* activa *Use Audio Units v2* y *Use VST3 Plug-In System Folders* y pulsa *Rescan*.

También puedes bajar los plugins ya compilados desde la pestaña **Actions** de GitHub, en los artefactos de cada build Release.

### Opciones de CMake
| Opción | Por defecto | Qué hace |
|---|---|---|
| `MELISMA_BUILD_PLUGIN` | `ON` | Compila el plugin y la app |
| `MELISMA_BUILD_TESTS` | `ON` | Compila los tests (descarga Catch2) |
| `MELISMA_BUILD_TOOLS` | `ON` | Compila las herramientas de línea de comandos (`melisma-info`…) |
| `MELISMA_COPY_PLUGIN_AFTER_BUILD` | `OFF` | Instala el plugin en `~/Library/Audio/Plug-Ins` |

### Estilo de código
```bash
tools/format.sh           # formatea (clang-format fijado a una versión concreta)
tools/format.sh --check   # solo comprueba; es lo que hace la CI
tools/tidy.sh             # análisis estático con clang-tidy
```

## Licencia

[AGPL-3.0](LICENSE), compatible con la licencia open source de JUCE.

"Melodyne" es una marca de Celemony. Melisma es un proyecto independiente, sin relación con Celemony.
