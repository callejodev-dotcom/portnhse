# NHSE para Mac

Versión para macOS de [NHSE](https://github.com/kwsch/NHSE), o editor de partidas de
*Animal Crossing: New Horizons* de Kaphotics e colaboradores.

O núcleo de NHSE (lectura e cifrado das partidas, obxectos, aldeáns, mapa…) úsase tal
cal: vai como submódulo en `NHSE/`, fixado nunha versión concreta. O que se reescribiu é a
interface, que en Windows é WinForms e aquí é [Avalonia](https://avaloniaui.net), con
aspecto e atallos de Mac. Os textos da interface son os da versión de Windows, así que
está nos mesmos nove idiomas (español incluído).

> Trae a túa propia partida: este programa non a extrae da consola. **Garda sempre unha
> copia antes de editar.** Non apoiamos facer trampas a costa doutras persoas.

## Instalar

1. Descarga o `.dmg` da [última versión](https://github.com/callejodev-dotcom/portnhse/releases):
   `arm64` para Mac con Apple Silicon (M1 en diante), `x64` para Mac con Intel.
2. Arrastra **NHSE** a Aplicacións.
3. A primeira vez macOS avisa de que non pode comprobar o programador (a app non está
   notarizada por Apple). Ábrea igual desde **Axustes do Sistema → Privacidade e
   seguranza → Abrir igualmente**, ou cun comando:
   ```sh
   xattr -dr com.apple.quarantine /Applications/NHSE.app
   ```

Require macOS 14 (Sonoma) ou posterior. Non fai falta instalar .NET: vai dentro da app.

## Usar

1. Copia a partida da consola ao Mac (con JKSV ou Checkpoint nunha Switch modificada, ou
   desde a carpeta de partidas dun emulador). É a carpeta que contén `main.dat`,
   `mainHeader.dat` e as carpetas `Villager0`, `Villager1`…
2. Abre NHSE e solta esa carpeta na xanela (ou na icona do Dock), ou usa **Abrir carpeta
   de partida…** (⌘O). Tamén abre un `.zip` coa partida.
3. Edita e garda con **⌘S**. NHSE recalcula os hashes e volve cifrar os ficheiros no
   mesmo sitio.
4. Copia a carpeta de volta á consola.

Cada partida que abres queda copiada en `~/Library/Application Support/NHSE/bak`
(o botón **Ver copias de seguridad** do lanzador abre esa carpeta).

### Diferenzas coa versión de Windows

- Onde NHSE di Ctrl, aquí é **⌘**; Alt é **⌥**.
- Nas cuadrículas de obxectos un clic normal carga o obxecto no editor; ⇧ clic pono,
  ⌥ clic bórrao, ⌘⌥ clic cópiao a todas as casas. O clic dereito ten as mesmas accións.
- O menú vai na barra de menús de macOS. O tema claro/escuro cambia ao momento, sen
  reiniciar.
- As imaxes de obxectos descargadas gárdanse en `~/Library/Application Support/NHSE/img`.
- A inxección por USB (sys-botbase) necesita libusb: `brew install libusb`. A inxección
  pola rede funciona sen nada máis.

## Compilar

Fai falta o SDK de [.NET 10](https://dotnet.microsoft.com/download).

```sh
git clone --recursive https://github.com/callejodev-dotcom/portnhse.git
cd portnhse
dotnet build PortNHSE.slnx
dotnet run --project src/NHSE.Mac
```

- **Probas:** `dotnet test tests/NHSE.Mac.Tests`. As que necesitan unha partida real
  usan a carpeta de `NHSE_TEST_SAVE` (sempre sobre unha copia temporal) e gardan capturas
  de cada xanela en `galeria/`. As partidas son datos persoais e non se suben ao repo.
- **Empaquetar:** `tools/empaquetar.sh` crea `dist/NHSE-<versión>-macOS-<arq>.dmg`.
- **Actualizar NHSE:** `git -C NHSE fetch && git -C NHSE checkout <commit>`, compilar,
  pasar as probas e facer commit do submódulo.
- **Portar formularios:** [PORTAR.md](PORTAR.md) explica como se pasa un formulario de
  WinForms a Avalonia neste proxecto.

## Licenza

GPLv3, coma NHSE. Ver [LICENSE](LICENSE).

---

*English:* macOS port of [NHSE](https://github.com/kwsch/NHSE) (Animal Crossing: New
Horizons save editor). NHSE's core is used unchanged as a pinned submodule; the WinForms
interface was rewritten with Avalonia, keeping NHSE's translations. Download the `.dmg`
for your Mac from Releases (arm64 = Apple Silicon, x64 = Intel), drag NHSE to
Applications, and open it from *System Settings → Privacy & Security → Open Anyway* the
first time (the app is not notarized). Drop your save folder on the window, edit, and
save with ⌘S. Always keep a backup.
