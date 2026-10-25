# Interface amigable

A interface de WinForms portada (PORTAR.md) segue aí, pero quen abre unha partida
entra na ventá de inicio (`src/NHSE.Mac/Home/`), pensada para xente que non sabe nada
do formato da partida. Esta guía di como se fan as ventás novas dese estilo.

## Principios

1. **Linguaxe do xogo, non da partida.** «Cantidade», «Receitas aprendidas», «Vai
   mudarse»; nunca *Flag0*, *ManpuBit*, IDs en hexadecimal nin nomes internos (`cat01`).
   Os nomes de obxectos e veciños saen sempre de `GameInfo.Strings` no idioma escollido.
2. **Imaxes antes que números.** Obxectos con `ItemSprite`, veciños con
   `VillagerSprite`, iconas do xogo con `HomeUi.Icon("MoneyBag069")` (os nomes están en
   `NHSE/NHSE.Sprites/Resources/MenuIcon`).
3. **Só o que ten sentido.** Un control aparece só se se aplica ao que está escollido
   (a cantidade só nos obxectos que se amorean, a cor só nos mobles con variantes…).
4. **O técnico, escondido pero alcanzable.** Un `Expander` «Opcións avanzadas» co editor
   clásico, ou o apartado Avanzado do inicio. Non se perde ningunha función.
5. **Accións habituais nun clic.** «Aprender todas», «Marcar todo», «Máx.», buscadores
   que ignoran maiúsculas e tiles (`ItemSearchBox.Normalize`).
6. **Mesmo esqueleto en todas.** Título grande (`TextBlock.title`), unha liña que
   explica que se fai (`TextBlock.subtle`), o contido en tarxetas (`Border.card`) e abaixo á
   dereita «Cancelar» e «Gardar» (accent, `IsDefault`). Devolven `true` ao gardar, como as
   clásicas, e editan os datos no sitio.

## Pezas que xa hai

- `HomeUi`: `Page`, `Card`, `CardHeader(icona, título, explicación, …)`, `ActionCard`,
  `SmallButton`, `Tiles(columnas, …)`, `Row`, `Amount` (cifra con separador de miles),
  `Stat` (tarxeta dunha cifra con icona e explicación), `ItemStrip` (fila de miniaturas),
  `Icon`, `Text`, `Section`.
- `PlayerDialog`: esqueleto dunha ventá amigable (título, explicación, contido, «Cancelar» e
  «Gardar»); `Save()` escribe na partida e `Discard()` desfai o editado no sitio.
- `PlayerUi`: `Chips` (filtros), `SearchBox`, `MenuButton` («Máis ▾»), `Labeled`, `DatePicker`,
  `FormatDate`, `Dots`.
- `IslandSummaries.Summary`: o que unha tarxeta do inicio mostra debaixo e se refai ao gardar.
- `HomeContext`: a partida, o xogador escollido, `MarkDirty()` e `Edit(xanela)`, que abre
  un diálogo e marca a partida como cambiada se devolve `true`.
- `ItemsWindow`: editor de listas de obxectos (exemplo completo do estilo).
- `ItemSearchBox`: escoller un obxecto con categorías, buscador e imaxes.
- `VillagerPicker`: escoller un veciño polo nome, con retrato.
- `DesignsWindow` e `DesignSlot`: listas de deseños normais e PRO coa mesma ventá.
- Estilos en `Styles.axaml` (`card`, `title`, `section`, `cardtitle`, `subtle`, `amount`,
  `chip`) e cores claras/escuras en `Themes.axaml` (`NhWindowBrush`, `NhCardBrush`,
  `NhGreenBrush`…).

## Textos

Cada grupo de ventás ten a súa clase estática `XxxStrings` en `NHSE.Mac` co inglés por
defecto, e `src/NHSE.Mac/Resources/XxxStrings_es.txt` co castelán (`Propiedade=Texto`).
Rexístrase soa: `App.SetLanguage` traduce todas as clases `*Strings` do proxecto. O
castelán é o de España e o do propio xogo: «bayas», «Millas Nook», «muletilla»,
«Servicios a residentes», «Juliana», «Hermanas Manitas»…

## Comprobar

Unha proba de galería por ventá, e repasar as capturas en castelán e en escuro:

```sh
NHSE_TEST_SAVE=… NHSE_IDIOMA=es NHSE_GALERIA=$PWD/galeria-es dotnet test tests/NHSE.Mac.Tests
NHSE_TEST_SAVE=… NHSE_TEMA=Dark NHSE_IDIOMA=es NHSE_GALERIA=$PWD/galeria-escuro dotnet test tests/NHSE.Mac.Tests
```
