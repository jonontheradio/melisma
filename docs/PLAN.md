# Plan de acción de Melisma

Este documento explica **cómo funciona** Melisma por dentro, **en qué orden** lo vamos a construir y **qué se podrá configurar**. Las tareas concretas están en los [issues](https://github.com/jonontheradio/melisma/issues) y en los [milestones](https://github.com/jonontheradio/melisma/milestones) del repo.

---

## 1. Qué es Melisma

Melisma es un editor de tono y tiempo por notas: abres una pista de voz (o de cualquier instrumento monofónico), Melisma detecta cada nota y la dibuja como un "blob" sobre un piano roll, y tú puedes:

- arrastrar una nota arriba o abajo para cambiar su tono,
- estirarla o moverla en el tiempo,
- corregir la afinación hacia la escala de forma suave (sin que suene robótico),
- reducir o exagerar el vibrato y la deriva de tono,
- cambiar formantes y volumen por nota.

Todo ello se renderiza sin destruir el audio original: el audio fuente nunca se modifica, solo cambian los parámetros de las notas.

## 2. Cómo funciona: el pipeline

```
 Audio fuente ──► ANÁLISIS ──► Modelo de notas ──► EDICIÓN (UI) ──► RESÍNTESIS ──► Audio de salida
  (WAV / captura    │             (Note, PitchCurve,     │               │
   en el DAW)       │              Segment)              │               │
                    ▼                                    ▼               ▼
            1. Curva de tono (f0)            El usuario cambia     PSOLA / vocoder
            2. Voiced / unvoiced             tono, tiempo,         reconstruye la señal
            3. Segmentación en notas         formantes, volumen    con los nuevos valores
            4. Marcas de pitch (épocas)
```

### 2.1 Análisis (`core/analysis`)
1. **Preprocesado**: pasar a mono y resamplear a una frecuencia interna fija (p. ej. 44.1 kHz).
2. **Detección de f0**: YIN como primera versión, pYIN después (más robusto, da probabilidad de voicing). Resultado: una curva de tono por frames (hop de ~5 ms).
3. **Voicing**: decidir qué frames tienen tono (vocales) y cuáles no (consonantes, respiraciones, silencio).
4. **Segmentación en notas**: agrupar frames en notas usando cambios de tono estable, onsets de energía y huecos sin voz. Cada nota guarda su tono central (media ponderada), su curva de deriva y sus límites.
5. **Marcas de pitch (épocas)**: localizar cada ciclo glotal; las necesita PSOLA para resintetizar.

### 2.2 Modelo de datos (`core/model`)
- `Note`: inicio, fin, tono central, desplazamiento de tono, factor de tiempo, desplazamiento de formantes, ganancia, cantidad de deriva y de vibrato.
- `PitchCurve`: la f0 original frame a frame.
- `Take`: audio fuente, resultado del análisis y lista de notas.
- Todas las ediciones son **comandos** (para deshacer y rehacer) y el estado se puede serializar (para guardarlo dentro del proyecto de Ableton).

### 2.3 Resíntesis (`core/synthesis`)
- **v1: TD-PSOLA**. Toma los ciclos de la señal alrededor de cada época, los recoloca con el nuevo periodo y los suma solapados. Es rápido, suena natural en voz y conserva los formantes de forma natural.
- **v2: vocoder (WORLD o similar)**. Separa envolvente espectral, f0 y aperiodicidad, lo que permite mover formantes de forma independiente y aguantar cambios grandes de tono.
- Las partes sin voz (consonantes) se copian tal cual, o solo se estiran en el tiempo.
- El render es **incremental**: al editar una nota solo se recalcula esa zona, con fundidos en los bordes.

### 2.4 Interfaz (`ui`)
- Piano roll vertical con teclas a la izquierda, rejilla de tiempo arriba y regla de escala.
- Blobs: la forma de onda de cada nota, dibujada a la altura de su tono, con la curva de deriva superpuesta.
- Herramientas: selección, tono, tiempo, formantes, volumen, cortar y unir notas.
- Zoom y scroll en ambos ejes; reproducción con cursor.

### 2.5 Plugin e integración con Ableton (`plugin`)

> **Importante:** que sepamos, Ableton Live **no soporta ARA2**, que es la tecnología que usa Melodyne en Logic, Cubase, Studio One o Reaper. Por eso, en Ableton también Melodyne funciona en "modo transferencia". Melisma hace lo mismo:

- **Modo captura (Ableton y cualquier DAW)**: insertas Melisma como VST3/AU en la pista, le das al play, el plugin graba el audio que le llega sincronizado con el transporte del DAW, lo analiza, y desde ese momento **reproduce su versión editada** siguiendo la posición de reproducción del DAW. La captura y las ediciones se guardan dentro del proyecto de Ableton.
- **Modo ARA2 (fase posterior)**: en los DAWs que lo soportan, Melisma lee el audio de la pista directamente, sin captura.
- **Standalone**: la misma app fuera del DAW, para abrir WAV y exportar.

## 3. Arquitectura del código

```
melisma/
├── core/               # Librería C++ pura, sin JUCE GUI. Testeable.
│   ├── analysis/       # f0, voicing, segmentación, épocas
│   ├── model/          # Note, Take, comandos de edición, serialización
│   ├── synthesis/      # PSOLA, vocoder, render incremental
│   └── dsp/            # utilidades: FFT, ventanas, resampleo, ring buffers
├── ui/                 # Componentes JUCE: piano roll, blobs, herramientas
├── plugin/             # AudioProcessor, captura, sincronía con el host, (ARA)
├── app/                # Standalone
├── tests/              # Catch2: tests unitarios y de precisión con audio sintético
├── tools/              # Scripts de evaluación y prototipos (Python opcional)
└── docs/
```

**Reglas de diseño:**
- `core` no depende de la GUI ni del host. Todo el DSP se prueba con tests.
- Nada que reserve memoria o bloquee en el hilo de audio. El análisis y el render se hacen en hilos de fondo; el hilo de audio solo lee buffers ya renderizados.
- Stack: **C++20, CMake, JUCE 9 (AGPLv3), Catch2**, GitHub Actions para CI en macOS (y después Windows).

## 4. Opciones configurables

### Análisis
| Opción | Por defecto | Qué hace |
|---|---|---|
| Algoritmo de f0 | pYIN | YIN (rápido) o pYIN (robusto) |
| Rango de tono | 70 – 1000 Hz | Limita la búsqueda; hay presets: voz grave, voz aguda, instrumento |
| Umbral de voicing | 0.5 | Qué cuenta como nota frente a ruido o consonante |
| Sensibilidad de separación de notas | media | Más alta = más notas cortas; más baja = notas largas unidas |
| Tamaño de hop | 5 ms | Resolución temporal del análisis |

### Afinación y rejilla
| Opción | Por defecto | Qué hace |
|---|---|---|
| Afinación de referencia | A4 = 440 Hz | 415–466 Hz |
| Tonalidad y escala | Cromática | Mayor, menor, modos, escala personalizada |
| Ajuste de tono | Semitono | Libre, semitono o escala |
| Ajuste de tiempo | Libre | Libre o rejilla del tempo del host |

### Corrección automática (macro)
| Opción | Por defecto | Qué hace |
|---|---|---|
| Corrección de tono central | 0 – 100 % | Lleva cada nota hacia la nota de la escala más cercana |
| Corrección de deriva | 0 – 100 % | Aplana las desviaciones lentas dentro de la nota |
| Conservar vibrato | sí | Excluye el vibrato de la corrección de deriva |
| Tiempo de transición | 30 ms | Suavidad del paso de una nota a la siguiente |

### Por nota (editables con herramientas)
Tono (cents), deriva (%), vibrato (%), formantes (semitonos), ganancia (dB), inicio, duración, tiempo de ataque.

### Resíntesis y calidad
| Opción | Por defecto | Qué hace |
|---|---|---|
| Algoritmo | PSOLA | PSOLA o vocoder (cuando exista) |
| Preservar formantes | sí | Evita la "voz de ardilla" en cambios grandes |
| Calidad de render | alta | Equilibrio entre CPU y artefactos |
| Duración de fundidos | 10 ms | Al unir zonas editadas con zonas originales |

### Plugin
| Opción | Por defecto | Qué hace |
|---|---|---|
| Modo | captura | Captura o ARA (si el host lo soporta) |
| Auto-captura al reproducir | activado | Grabar automáticamente en el primer play |
| Bypass de ediciones (A/B) | — | Compara el original con el editado |
| Monitorizar entrada durante la captura | sí | — |

### Interfaz
Tema claro u oscuro, mostrar u ocultar la curva de deriva, colores por nota o por tono, altura de las filas, nombres de notas en español o en inglés.

## 5. Hoja de ruta (milestones)

| # | Milestone | Resultado visible |
|---|---|---|
| M0 | Fundaciones | El proyecto compila: app standalone vacía y plugin VST3/AU que carga en Ableton, CI en verde |
| M1 | Análisis | Cargar un WAV y obtener la curva de tono y la lista de notas (tests de precisión) |
| M2 | Editor visual | Ver los blobs en un piano roll, seleccionarlos y moverlos (todavía sin sonido editado) |
| M3 | Resíntesis | Oír el resultado editado: cambiar tono y tiempo con PSOLA |
| M4 | Herramientas | Macro de corrección, deriva, vibrato, formantes, volumen, cortar y unir, escalas |
| M5 | Plugin y Ableton | Modo captura dentro de Ableton, sincronizado con el transporte y guardado en el proyecto |
| M6 | Calidad y vocoder | Vocoder, formantes independientes, rendimiento |
| M7 | ARA2 | Integración directa en Logic, Reaper, Cubase y Studio One |
| M8 | Release | Instaladores, firma y notarización en macOS, documentación de usuario |
| M9 | Polifonía (I+D) | Investigación: detección polifónica (por ejemplo con basic-pitch) |

**Primer gran hito:** completar M0 a M3 para tener una app standalone en la que abres una voz, mueves una nota y la oyes afinada.
**Hito "Ableton":** M5.

## 6. Cómo trabajamos

- Cada tarea es un issue con su milestone y sus etiquetas (`area:*`, `type:*`).
- Las épicas (un issue por milestone) agrupan sus tareas como sub-issues.
- Una rama y un PR por tarea; la CI tiene que pasar antes de fusionar.
- Para el DSP, primero un test con audio sintético (un seno con tono conocido) y después el código.
