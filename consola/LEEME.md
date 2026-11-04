# NHSE para Switch

Editor de la partida de Animal Crossing: New Horizons (3.0.x) que se abre desde el Álbum, con el juego cerrado.
Copia `NHSE.nro` a `/switch/NHSE/` en la SD.

Al abrirse copia toda la partida a `/switch/NHSE/copias/<fecha>/`. Solo escribe al pulsar «Guardar», y después lee lo
escrito para comprobar que es exactamente lo que quería guardar y que la partida está sana.

## Qué se puede cambiar

- Dinero (monedero, banco) y millas Nook, de cualquier jugador.
- Bolsillos: poner cualquier objeto (buscándolo por su nombre, también recetas), cambiar la cantidad o vaciar un hueco.
- Aprender todas las recetas, completar el catálogo (con todos los colores) y aprender todas las reacciones.
- Flores: cambiar todas las de la isla por una o varias, alternando o al azar. Quitar las malas hierbas.
- Vecinos: cambiar uno por cualquiera de los 417 (en la misma casa), y ser mejores amigos de todos.
- Precios de los nabos de la semana.

## Carpetas

- `source/core`: el núcleo (descifrar, comprobaciones, cambios). Portable: se compila igual en el Mac.
  `tablas.hpp`, `vecinos.hpp`, `objetos.hpp`, `listas.hpp` y `romfs/vecinos.bin` salen de NHSE con
  `dotnet run --project consola/herramientas/Generar`.
- `source/app`: la app (SDL2). La compila la acción `consola.yml` con devkitPro.
- `prueba`: el núcleo desde la línea de órdenes. `tests/NHSE.Mac.Tests/ConsolaTests.cs` lo compila y comprueba que
  cada cambio deja la partida byte a byte igual que NHSE.
- `simulador`: la app entera en el Mac, sin consola (`brew install sdl2 sdl2_ttf`, luego `simulador/compilar.sh`). Lee
  una copia de la partida, pulsa los botones de un guion y saca fotos de la pantalla; ver `simulador/sim.cpp`.
