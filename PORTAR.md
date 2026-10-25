# Como se porta un formulario de NHSE.WinForms

Guía para pasar a Avalonia (macOS) os formularios de `NHSE/NHSE.WinForms`. O núcleo
(`NHSE.Core`, `NHSE.Villagers`, `NHSE.Injection`) úsase tal cal desde o submódulo; só se
reescribe a interface.

## Regras

1. **Unha xanela por formulario, co mesmo nome de clase** e no espazo de nomes `NHSE.Mac`.
   Os formularios van en `src/NHSE.Mac/Subforms/<Carpeta>/<Nome>.axaml` + `.axaml.cs`, na
   mesma carpeta que en WinForms. Se hai un esbozo `<Nome>.cs` (unha `PendingWindow`),
   bórrase ao portalo.
2. **Os controis conservan o nome de WinForms** (`x:Name="B_Save"`, `L_Count`…). As
   traducións saen dos `lang_xx.txt` de WinForms, cuxas claves son `Formulario.Control`, e
   só funcionan se os nomes coinciden. O título da xanela é o `Text` do formulario.
3. A raíz XAML é `<nh:NhseWindow xmlns:nh="using:NHSE.Mac" …>`. `NhseWindow` centra a
   xanela na súa dona e péchaa con ⌘W.
4. No construtor, despois de `InitializeComponent()`, chámase
   `this.TranslateInterface(GameInfo.CurrentLanguage);` no mesmo punto ca en WinForms.
5. O construtor ten os mesmos parámetros ca en WinForms (o editor principal xa os chama así).
6. Non se tocan os ficheiros compartidos (`Util/`, `Controls/`, `Sprites/`, `Styles.axaml`,
   `Themes.axaml`, `Editor.*`) sen necesidade; se fai falla un axudante novo, vai no
   ficheiro do propio formulario.
7. A compilación queda **sen avisos**: `dotnet build PortNHSE.slnx`.

## Equivalencias

| WinForms | Avalonia (neste proxecto) |
|---|---|
| `Form` | `NhseWindow` |
| `UserControl` | `UserControl` |
| `Label` | `TextBlock` (`Classes="label"` para aliñala á dereita dun editor) |
| `Button`, `CheckBox`, `RadioButton` | igual; o texto vai en `Content`; `Checked` → `IsChecked == true` |
| `TextBox`, `RichTextBox` | `TextBox` (`AcceptsReturn="True"` se é multiliña) |
| `NumericUpDown` | `NumericUpDown`; o valor é `decimal?`: `(uint)(NUD.Value ?? 0)` |
| `ComboBox` con `DataSource` de `ComboItem` | `cb.SetComboItems(lista)`, `UiUtil.GetIndex(cb)`, `cb.SelectedValue = (int)valor` |
| `ComboBox.Items.AddRange(textos)` | `cb.SetComboItems(textos)` + `SelectedIndex` |
| `ComboBox` con autocompletado | `IsEditable="True"` |
| `ListBox` | `ListBox` con `ItemsSource` (unha `ObservableCollection` se cambia) |
| `CheckedListBox` | `ListBox`/`ItemsControl` de `CheckBox` |
| `PictureBox` | `Image` (`Source` = `Bitmap`); para obxectos, `nh:ItemSlot` |
| `GroupBox` | `nh:GroupBox` (`Header`) |
| `TabControl`/`TabPage` | `TabControl`/`TabItem` (`Header`) |
| `FlowLayoutPanel`, `TableLayoutPanel`, `Panel` | `WrapPanel`/`StackPanel`, `Grid`, `Panel`/`Canvas` |
| `PropertyGrid` | `nh:PropertyGrid` (`SelectedObject`, `PropertySort`, `PropertyValueChanged`) |
| `ContextMenuStrip` | `ContextMenu` ou `Button.Flyout` con `MenuFlyout`; cada `MenuItem` co nome e `Header` |
| `ToolTip` | `ToolTip.Tip="…"` |
| `TrackBar`, `ProgressBar`, `DateTimePicker`, `Timer` | `Slider`, `ProgressBar`, `CalendarDatePicker`/`TimePicker`, `DispatcherTimer` |
| `MessageBox` / `WinFormsUtil.Alert/Error/Prompt` | `await Dialogs.Alert/Error/Prompt(...)` (devolven `DialogResult`) |
| `OpenFileDialog`, `SaveFileDialog`, `FolderBrowserDialog` | `await Pickers.OpenFile(this, filtro, nome)`, `SaveFile`, `OpenFolder` (o filtro co formato de WinForms) |
| `editor.ShowDialog() == DialogResult.OK` | `await editor.ShowDialog<bool>(this)` |
| `DialogResult = OK; Close();` | `Close(true)`; cancelar: `Close(false)` |
| `ModifierKeys`, `Keys.Control`, `Keys.Alt` | `UiUtil.ModifierKeys`, `UiUtil.CommandModifier` (⌘), `KeyModifiers.Alt` (⌥) |
| `Clipboard` | `await UiUtil.SetClipboardText(this, t)`, `await UiUtil.GetClipboardText(this)` |
| `System.Media.SystemSounds.Asterisk.Play()` | `Sound.Asterisk()` |
| `WinFormsUtil.GetIndex`, `FirstFormOfType` | `UiUtil.GetIndex`, `UiUtil.FirstFormOfType` |
| `NHSE.Sprites` (`ItemSprite`, `ImageUtil`, `MapRenderer`…) | os mesmos nomes en `src/NHSE.Mac/Sprites`, con `Bitmap`/`WriteableBitmap` de Avalonia |
| `MiscDumpHelper` | `MiscDumpHelper`, asíncrono e coa xanela dona como primeiro parámetro |

Os manexadores de eventos levan `(object? sender, RoutedEventArgs e)` (ou o tipo de evento
de Avalonia que toque) e son `async void` se esperan un diálogo.

## Deseño

Respéctase a disposición de WinForms con `Grid`/`StackPanel` en lugar de posicións
absolutas; o tamaño da xanela parte do `ClientSize` do deseñador cun 10–20 % máis, porque
as fontes de macOS son algo maiores. `SizeToContent` vale para os diálogos pequenos.

## Comprobar

As probas de `tests/NHSE.Mac.Tests` debuxan as xanelas sen abrir a aplicación e gardan
PNG en `galeria/`. Cada formulario portado engade un `[AvaloniaFact]` que o abre cunha
partida de proba e chama a `Gallery.Capture(xanela, "nome")`:

```sh
NHSE_TEST_SAVE=/ruta/a/unha/copia/da/partida dotnet test tests/NHSE.Mac.Tests
```

As partidas son datos persoais: nunca se suben ao repositorio. As probas traballan sobre
unha copia temporal (`TestSave.LoadCopy`).
