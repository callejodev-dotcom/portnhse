using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;
using static NHSE.Core.TerrainUnitModel;

namespace NHSE.Mac;

public partial class TerrainBrushEditor : NhseWindow
{
    public readonly PropertyGrid PG_TerrainTile;
    private readonly FieldItemEditor FIEWindow;

    public Slider Slider_thickness => slider_thickness;

    public bool IsBrushSelected;

    public bool RandomizeVariation;

    public TerrainBrushEditor(PropertyGrid pG_TerrainTile, FieldItemEditor fieWindow)
    {
        PG_TerrainTile = pG_TerrainTile;
        FIEWindow = fieWindow;
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);
        LoadImages();
        Closed += TerrainBrushEditor_FormClosed;
    }

    /// <summary>
    /// Button pictures (the "Map assets" images of the WinForms resources).
    /// </summary>
    private void LoadImages()
    {
        (Button Button, string File)[] tiles =
        [
            (nw_rounded_tile, "nw_brush_diagonal"), (n_tile, "n_brush"), (ne_rounded_tile, "ne_brush_diagonal"),
            (w_tile, "w_brush"), (middle_tile, "center_brush"), (e_tile, "e_brush"),
            (sw_rounded_tile, "sw_brush_diagonal"), (s_tile, "s_brush"), (se_rounded_tile, "se_brush_diagonal"),
            (nw_angular_tile, "nw_brush_angular"), (ne_angular_tile, "ne_brush_angular"),
            (sw_angular_tile, "sw_brush_angular"), (se_angular_tile, "se_brush_angular"),
            (tl_dirt_inside, "top_left_dirt_corner"), (tr_dirt_inside, "top_right_dirt_corner"),
            (dl_dirt_inside, "down_left_dirt_corner"), (dr_dirt_inside, "down_right_dirt_corner"),

            (nw_diagonal_water_tile, "nw_water_brush_diagonal"), (n_water_tile, "n_water_brush"), (ne_diagonal_water_tile, "ne_water_brush_diagonal"),
            (w_water_tile, "w_water_brush"), (center_water_tile, "center_water_brush"), (e_water_tile, "e_water_brush"),
            (sw_diagonal_water_tile, "sw_water_brush_diagonal"), (s_water_tile, "s_water_brush"), (se_diagonal_water_tile, "se_water_brush_diagonal"),
            (nw_angular_water_tile, "nw_water_brush_angular"), (ne_angular_water_tile, "ne_water_brush_angular"),
            (sw_angular_water_tile, "sw_water_brush_angular"), (se_angular_water_tile, "se_water_brush_angular"),
            (tl_water_inside, "top_left_water_corner"), (tr_water_inside, "top_right_water_corner"),
            (dl_water_inside, "down_left_water_corner"), (dr_water_inside, "down_right_water_corner"),
        ];
        foreach (var (button, file) in tiles)
            button.Content = new Image { Source = GetImage(file) };

        btn_normal_click.Content = new Image { Source = GetImage("mouse"), Width = 24, Height = 24 };
        btn_brush.Content = new Image { Source = GetImage("brush"), Width = 32, Height = 32 };

        // Pixel art: scaled up without smoothing (inherited by the buttons' images).
        RenderOptions.SetBitmapInterpolationMode(this, BitmapInterpolationMode.None);
    }

    private static Bitmap? GetImage(string name)
    {
        using var stream = EmbeddedResources.Open($"map/{name}.png");
        return stream is null ? null : new Bitmap(stream);
    }

    #region Tiles buttons

    #region Dirt tiles

    private void NW_Rounded_Tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff3B);
    private void N_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff5B, 3);
    private void NE_rounded_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff3B, 3);
    private void W_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff5B);
    private void Middle_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile();
    private void E_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff5B, 2);
    private void SW_rounded_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff3B, 1);
    private void S_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff5B, 1);
    private void SE_rounded_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff3B, 2);
    private void NW_angular_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff3C);
    private void NE_angular_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff3C, 3);
    private void SW_angular_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff3C, 1);
    private void SE_angular_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff3C, 2);

    private void TR_dirt_inside_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff7A, 2);
    private void DL_dirt_inside_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff7A);
    private void DR_dirt_inside_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff7A, 1);

    #endregion Dirt tiles

    #region Water tiles

    private void NW_diagonal_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River3B);
    private void N_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River5B, 3);
    private void NE_diagonal_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River3B, 3);
    private void W_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River5B);
    private void Center_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River8A);
    private void e_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River5B, 2);
    private void sw_diagonal_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River3B, 1);
    private void s_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River5B, 1);
    private void SE_diagonal_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River3B, 2);
    private void NW_angular_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River3C);
    private void NE_angular_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River3C, 3);
    private void SW_angular_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River3C, 1);
    private void SE_angular_water_tile_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River3C, 2);
    private void TL_dirt_inside_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(Cliff7A, 3);
    private void TL_water_inside_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River7A, 3);
    private void TR_water_inside_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River7A, 2);
    private void DL_water_inside_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River7A);
    private void DR_water_inside_Click(object? sender, RoutedEventArgs e) => SelectTerrainTile(River7A, 1);

    #endregion Water tiles

    #endregion Tiles buttons

    private void SliderThicknessValueChanged(object? sender, RangeBaseValueChangedEventArgs e) => lbl_size_count.Text = ((int)Math.Round(slider_thickness.Value)).ToString();

    private void SliderElevationValueChanged(object? sender, RangeBaseValueChangedEventArgs e)
    {
        if (PG_TerrainTile is null) // still being built
            return;
        lbl_elevation_count.Text = ((int)Math.Round(slider_elevation.Value)).ToString();
        var currentTile = (TerrainTile)PG_TerrainTile.SelectedObject!;
        currentTile.Elevation = (ushort)Math.Round(slider_elevation.Value);
        PG_TerrainTile.SelectedObject = currentTile;
    }

    private TerrainTile CreateTerrainTile(TerrainUnitModel unitModel = Base, ushort landMakingAngle = 0) => new()
    {
        Elevation = (ushort)Math.Round(slider_elevation.Value),
        UnitModel = unitModel,
        LandMakingAngle = landMakingAngle,
    };

    private void SelectTerrainTile(TerrainUnitModel unitModel = Base, ushort landMakingAngle = 0)
        => PG_TerrainTile.SelectedObject = CreateTerrainTile(unitModel, landMakingAngle);

    private void B_Brush_Click(object? sender, RoutedEventArgs e) => IsBrushSelected = true;
    private void TerrainBrushEditor_FormClosed(object? sender, EventArgs e) => IsBrushSelected = false;
    private void B_Normal_Click(object? sender, RoutedEventArgs e) => IsBrushSelected = false;
    private void CB_TileVariation_CheckedChanged(object? sender, RoutedEventArgs e) => RandomizeVariation = cb_tileVariation.IsChecked == true;
}
