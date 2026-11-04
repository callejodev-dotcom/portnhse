# NHSE para Switch

Editor de la partida de Animal Crossing: New Horizons (3.0.x) que se abre desde el Álbum, con el juego cerrado.
Copia `NHSE.nro` a `/switch/NHSE/` en la SD.

Al abrirse copia toda la partida a `/switch/NHSE/copias/<fecha>/`. Solo escribe al pulsar «Guardar», y después lee lo
escrito para comprobar que es exactamente lo que quería guardar y que la partida está sana.

## Carpetas

- `source/core`: el núcleo (descifrar, comprobaciones, cambios). Portable: se compila igual en el Mac. No sabe nada
  de pantallas.
  - `acnh.hpp/.cpp`: cifrado, comprobaciones, números cifrados, objetos, bolsillos, flores, vecinos, nabos.
  - Los `.hpp` generados (`tablas`, `vecinos`, `objetos`, `listas`...) y `romfs/*.bin` salen de NHSE con
    `dotnet run --project consola/herramientas/Generar`. Cada tabla es una clase `ITabla` en su archivo de
    `herramientas/Generar`; el programa las encuentra solo.
- `source/app`: la app (SDL2).
  - `ui.hpp/.cpp`: dibujar, botones, pantallas apiladas (`abrir`, `volver`), teclado, avisos, y pantallas que se
    repiten: `lista` (en páginas), `elegir` (una opción), `confirmar`, `marcas` (cosas que se marcan, con buscar).
  - `objetos_ui.hpp/.cpp`: huecos de objetos (`todos`, `usados`), cambiar un hueco (`editor`: objeto, cantidad,
    color, tela, envoltorio) y `buscar` un objeto por su nombre.
  - `partida.hpp/.cpp`: la partida abierta (`principal()` es main.dat, `personal()` el del jugador elegido),
    `cambiaPrincipal()` / `cambiaPersonal()` tras cambiar algo, guardar, y leer/escribir en la SD (exportar).
  - `main.cpp`: inicio (secciones, guardar, salir).
  - Una sección por archivo (`jugador.cpp`, `vecinos.cpp`, `isla.cpp`...): cada una pone sus tarjetas con un
    `Registro` y no toca las demás.
- `prueba`: el núcleo desde la línea de órdenes, `prueba <carpeta> <orden> ...`. Cada archivo registra sus órdenes con
  `OrdenPrueba`. `tests/NHSE.Mac.Tests/Consola*Tests.cs` lo compila y comprueba que cada cambio deja la partida byte a
  byte igual que NHSE (con `ConsolaTests.Both`).
- `simulador`: la app entera en el Mac, sin consola (`brew install sdl2 sdl2_ttf`, luego `simulador/compilar.sh`). Lee
  una copia de la partida, pulsa los botones de un guion (también toques y lo que se escribe en el teclado) y saca
  fotos de la pantalla; ver `simulador/sim.cpp`.

## Añadir algo

1. El cambio en el núcleo (`source/core/<parte>.hpp/.cpp`), con las posiciones de NHSE (`NHSE/NHSE.Core`, versión 3.0:
   `MainSaveOffsets30`, `PersonalOffsets30`) y, si hacen falta nombres o listas, una tabla en `herramientas/Generar`.
2. Una orden de prueba (`prueba/<parte>.cpp`) y una prueba en C# que hace lo mismo con NHSE (mejor con el mismo código
   que usa la app del Mac) y compara la partida entera.
3. La pantalla en `source/app/<sección>.cpp`, con un `Registro` para su tarjeta.
4. Mirarla en el simulador (fotos) antes de darla por buena.

Textos en castellano, para Carla: claros y sin palabras técnicas.
