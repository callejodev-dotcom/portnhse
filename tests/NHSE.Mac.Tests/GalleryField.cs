using System.Reflection;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Headless;
using Avalonia.Headless.XUnit;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// Field item / terrain map editor (FieldItemEditor) and its helper windows.
/// </summary>
public class GalleryField
{
    private static HorizonSave Save()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        return TestSave.LoadCopy(out _);
    }

    private static T Find<T>(Window w, string name) where T : Control => w.FindControl<T>(name) ?? throw new InvalidOperationException(name);

    /// <summary>
    /// Shows the window and moves the mouse over it without keys held: the modifier keys are tracked globally
    /// (<see cref="UiUtil.ModifierKeys"/>), so a previous test could leave Shift "held".
    /// </summary>
    private static void ShowWithoutModifiers(Window w)
    {
        w.Show();
        Dispatcher.UIThread.RunJobs();
        w.MouseMove(new Point(1, 1), RawInputModifiers.None);
        Dispatcher.UIThread.RunJobs();
    }

    /// <summary> The map editor behind the window (private, like in WinForms). </summary>
    private static MapEditor GetMapEditor(FieldItemEditor w)
        => (MapEditor)typeof(FieldItemEditor).GetField("Editor", BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(w)!;

    [AvaloniaFact]
    public void FieldItemEditorTabs()
    {
        var w = new FieldItemEditor(Save().Main);
        var tabs = Find<TabControl>(w, "TC_Editor");

        Gallery.Capture(w, "40-field-items", false);

        tabs.SelectedIndex = 1;
        Gallery.Capture(w, "41-field-buildings", false);

        // Terrain tab with the tile names shown over the viewport.
        tabs.SelectedIndex = 2;
        Find<RadioButton>(w, "RB_Terrain").IsChecked = true;
        Find<Slider>(w, "TR_Terrain").Value = 255;
        Gallery.Capture(w, "42-field-terrain", false);

        tabs.SelectedIndex = 3;
        Gallery.Capture(w, "43-field-acres", false);
        w.Close();
    }

    [AvaloniaFact]
    public void FieldItemEditorLayer2()
    {
        var w = new FieldItemEditor(Save().Main);
        ShowWithoutModifiers(w);
        Find<NumericUpDown>(w, "NUD_Layer").Value = 2;
        Find<Button>(w, "B_Right").RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        Gallery.Capture(w, "44-field-layer2");
    }

    [AvaloniaFact]
    public void TerrainBrush()
    {
        var fie = new FieldItemEditor(Save().Main);
        var w = new TerrainBrushEditor(Find<PropertyGrid>(fie, "PG_TerrainTile"), fie);
        Gallery.Capture(w, "45-terrain-brush", false);
        Find<TabControl>(w, "tabControl1").SelectedIndex = 1;
        Gallery.Capture(w, "45-terrain-brush-water");
        fie.Close();
    }

    [AvaloniaFact]
    public void BulkSpawnWindow()
    {
        var fie = new FieldItemEditor(Save().Main);
        Gallery.Capture(new BulkSpawn(fie, 64, 64), "46-bulk-spawn");
        fie.Close();
    }

    [AvaloniaFact]
    public void BuildingHelpWindow()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        Gallery.Capture(new BuildingHelp(), "47-building-help");
    }

    [AvaloniaFact]
    public void BatchEditorWindow()
    {
        var sav = Save();
        var items = MapEditor.FromSaveFile(sav.Main).Mutator.CurrentLayer.Tiles;
        var w = new BatchEditor(items, new Item(Item.DIYRecipe));
        Find<Button>(w, "B_Add").RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        Gallery.Capture(w, "48-batch-editor");
    }

    /// <summary>
    /// ⇧ Click on the viewport sets the item of the editor (FieldItemEditor.SetTile), and Save writes the layer back.
    /// </summary>
    [AvaloniaFact]
    public void SetItemWithShiftClickAndSave()
    {
        var sav = Save();
        var w = new FieldItemEditor(sav.Main);
        ShowWithoutModifiers(w);

        var map = GetMapEditor(w);
        var cfg = map.Mutator.Manager.ConfigItems;
        const int viewX = 10, viewY = 10; // tile inside the viewport (even: root of a 2x2 item)
        var (relX, relY) = cfg.GetCoordinatesRelative(map.X + viewX, map.Y + viewY);
        Assert.True(cfg.IsCoordinateValidRelative(relX, relY));

        var apple = (ushort)Array.IndexOf(GameInfo.GetStrings("en").itemlist, "apple");
        Assert.NotEqual(apple, MapEditor.FromSaveFile(sav.Main).Mutator.CurrentLayer.GetTile(relX, relY).ItemId);

        Find<ItemEditor>(w, "ItemEdit").LoadItem(new Item(apple) { Count = 1 });
        Find<CheckBox>(w, "CHK_NoOverwrite").IsChecked = false;

        // Shift-click on the middle of the tile.
        var viewport = Find<MapPictureBox>(w, "PB_Viewport");
        var scale = map.ViewScale;
        var at = viewport.TranslatePoint(new Point((viewX * scale) + (scale / 2), (viewY * scale) + (scale / 2)), w)!.Value;
        w.MouseMove(at, RawInputModifiers.Shift);
        w.MouseDown(at, MouseButton.Left, RawInputModifiers.Shift);
        w.MouseUp(at, MouseButton.Left, RawInputModifiers.Shift);
        w.MouseMove(at, RawInputModifiers.None); // Shift released
        Dispatcher.UIThread.RunJobs();

        Assert.Equal(apple, map.Mutator.CurrentLayer.GetTile(relX, relY).ItemId);
        // Not in the save until saved.
        Assert.NotEqual(apple, MapEditor.FromSaveFile(sav.Main).Mutator.CurrentLayer.GetTile(relX, relY).ItemId);

        Find<Button>(w, "B_Save").RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        Dispatcher.UIThread.RunJobs();

        Assert.False(w.IsVisible);
        var saved = MapEditor.FromSaveFile(sav.Main).Mutator.CurrentLayer.GetTile(relX, relY);
        Assert.Equal(apple, saved.ItemId);
    }

    /// <summary>
    /// Dragging over the viewport (CHK_MoveOnDrag) moves the view, and the release is not taken as a click on a tile.
    /// </summary>
    [AvaloniaFact]
    public void DragMovesView()
    {
        var w = new FieldItemEditor(Save().Main);
        ShowWithoutModifiers(w);
        var map = GetMapEditor(w);
        var (x0, y0) = (map.X, map.Y);
        var tiles = map.Mutator.CurrentLayer.Tiles.Select(z => z.RawValue).ToArray();

        var viewport = Find<MapPictureBox>(w, "PB_Viewport");
        Point At(double x, double y) => viewport.TranslatePoint(new Point(x, y), w)!.Value;
        const RawInputModifiers left = RawInputModifiers.LeftMouseButton;
        w.MouseDown(At(256, 256), MouseButton.Left);
        w.MouseMove(At(260, 260), left);
        w.MouseMove(At(290, 290), left);
        w.MouseMove(At(320, 320), left); // 4 tiles right and down: the view goes 4 tiles left and up
        w.MouseUp(At(320, 320), MouseButton.Left);
        Dispatcher.UIThread.RunJobs();

        Assert.Equal((x0 - 4, y0 - 4), (map.X, map.Y));
        Assert.Equal(tiles, map.Mutator.CurrentLayer.Tiles.Select(z => z.RawValue));
        w.Close();
    }

    /// <summary>
    /// The terrain renders (map and viewport) are not blank, and the viewport has the building names to draw.
    /// </summary>
    [AvaloniaFact]
    public void TerrainRenderHasPixels()
    {
        var sav = Save();
        var editor = MapEditor.FromSaveFile(sav.Main);
        editor.ViewScale = 16;
        using var renderer = new MapRenderer(editor);

        var map = renderer.UpdateMapTerrain().GetPixels();
        Assert.Contains(map, z => z != 0 && z != TerrainSprite.ColorOcean);
        Assert.True(map.Distinct().Count() > 4);

        var b = editor.Buildings.Buildings.First(z => z.BuildingType != BuildingType.None);
        editor.Mutator.View.SetViewTo((b.X - 16) & 0xFFFE, (b.Y - 16) & 0xFFFE);
        var view = renderer.UpdateViewportTerrain(out var labels, 255, 255).GetPixels();
        Assert.Equal(512 * 512, view.Length);
        Assert.True(view.Distinct().Count() > 4);
        Assert.Contains(labels, z => z.Text == b.BuildingType.ToString());
        Assert.Contains(labels, z => z.Alpha == 255);

        var items = renderer.UpdateMapItemsReticle(-1).GetPixels();
        Assert.Contains(items, z => (z >>> 24) != 0);
    }
}
