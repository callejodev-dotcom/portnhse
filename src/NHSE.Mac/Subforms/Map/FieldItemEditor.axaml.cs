using System.Buffers.Binary;
using System.Collections.ObjectModel;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

public sealed partial class FieldItemEditor : NhseWindow, IItemLayerEditor
{
    private readonly MainSave SAV;
    private readonly MapEditor Editor;
    private readonly MapRenderer Renderer;

    private MapViewState View => Editor.Mutator.View;
    private MapTileManager Map => Editor.Mutator.Manager;

    private bool Loading;
    private int SelectedBuildingIndex;

    /// <summary> Cached current hover X coordinate within the acre. </summary>
    private int HoverX;
    /// <summary> Cached current hover Y coordinate within the acre. </summary>
    private int HoverY;

    private int DragX = -1;
    private int DragY = -1;
    private bool IsDragOperationActive;

    /// <summary> Left button went down on the viewport (a release there is a click). </summary>
    private bool IsViewportPressed;

    private Tuple<int, int> MapDesignGridSize = new(7, 6);

    /// <summary> Building names shown in <see cref="LB_Items"/>. </summary>
    private readonly ObservableCollection<string> BuildingNames = [];
    /// <summary> Set while a building name is replaced, so the list selection change is not taken as the user's. </summary>
    private bool UpdatingBuildingNames;

    private string[] AcreNames = [];

    public ItemEditor ItemProvider => ItemEdit;

    /// <summary>
    /// Layer to spawn items into.
    /// </summary>
    public LayerItem Spawn => CurrentLayer;

    public LayerFieldItem CurrentLayer => Editor.Mutator.CurrentLayer;

    private TerrainBrushEditor? tbeForm;

    public FieldItemEditor(MainSave sav)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);

        // Read the expected scale from the control.
        var scale = (int)PB_Viewport.Width / LayerFieldItem.TilesPerAcreDim;
        SAV = sav;
        Editor = MapEditor.FromSaveFile(sav);
        Editor.MapScale = 1;
        Editor.ViewScale = scale;
        Renderer = new MapRenderer(Editor);

        Loading = true;

        LoadMapDesignSize();
        LoadComboBoxes();
        LoadBuildings(sav);
        ReloadMapBackground();
        LoadEditors();

        // Set initial states
        CB_Acre.SelectedIndex = 0;
        CB_MapAcre.SelectedIndex = 0;
        LB_Items.SelectedIndex = 0; // triggers a draw

        Loading = false;
        Closed += FieldItemEditor_FormClosed;
    }

    private void LoadComboBoxes()
    {
        // Snap viewport to acre; select acre type for current
        AcreNames = AcreCoordinate.Exterior.Select(z => z.Name).ToArray();
        CB_Acre.SetComboItems(AcreNames);
        CB_MapAcre.SetComboItems(AcreNames);

        CB_MapAcreSelect.SetComboItems(ComboItemUtil.GetArray<OutsideAcre>());

        NUD_MapAcreTemplateOutside.Value = SAV.OutsideFieldTemplateUniqueId;
        NUD_MapAcreTemplateField.Value = SAV.MainFieldParamUniqueID;
    }

    private void LoadBuildings(MainSave sav)
    {
        NUD_PlazaX.Value = sav.EventPlazaLeftUpX;
        NUD_PlazaY.Value = sav.EventPlazaLeftUpZ;

        foreach (var obj in Editor.Mutator.Manager.LayerBuildings.Buildings)
            BuildingNames.Add(obj.ToString());
        LB_Items.ItemsSource = BuildingNames;
    }

    private void LoadEditors()
    {
        var data = GameInfo.Strings.ItemDataSource.ToList();
        var field = FieldItemList.Items.Select(z => z.Value).ToList();
        data.Add(field, GameInfo.Strings.InternalNameTranslation);
        ItemEdit.Initialize(data, true);
        PG_TerrainTile.SelectedObject = new TerrainTile();
    }

    private void LoadMapDesignSize()
    {
        if (SAV.Info.GetKnownRevisionIndex() >= 31)
            MapDesignGridSize = new(9, 6);
    }

    private int ExteriorAcreIndex => CB_Acre.SelectedIndex;

    private void ChangeAcre(object? sender, SelectionChangedEventArgs e)
    {
        if (Loading || ExteriorAcreIndex < 0)
            return;
        ChangeViewToAcre(ExteriorAcreIndex);
        CB_MapAcre.SelectedIndex = CB_Acre.SelectedIndex;
    }

    private void ChangeViewToAcre(int acre)
    {
        View.SetViewToAcre(acre);
        LoadItemGridAcre();
    }

    private void LoadItemGridAcre()
    {
        ReloadItems();
        ReloadViewportBackground();
        UpdateArrowVisibility();
    }

    private int GetItemTransparency() => ((int)(0xFF * Math.Round(TR_Transparency.Value) / 100d) << 24) | 0x00FF_FFFF;

    private void ReloadMapBackground()
    {
        var img = Renderer.UpdateMapTerrain(SelectedBuildingIndex);
        PB_Map.SetBackgroundImage(img);
    }

    private void ReloadMapItemGrid() => PB_Map.SetImage(Renderer.UpdateMapItemsReticle(GetItemTransparency()));

    private void ReloadViewportBackground()
    {
        var tbuild = (byte)Math.Round(TR_BuildingTransparency.Value);
        var tterrain = (byte)Math.Round(TR_Terrain.Value);
        var img = Renderer.UpdateViewportTerrain(out var labels, tbuild, tterrain, SelectedBuildingIndex);
        PB_Viewport.SetBackgroundImage(img, labels);
    }

    private void ReloadViewportItems() => PB_Viewport.SetImage(Renderer.UpdateViewportItems(GetItemTransparency()));

    public void ReloadItems()
    {
        ReloadViewportItems();
        ReloadMapItemGrid();
    }

    private void ReloadBuildingsTerrain()
    {
        ReloadViewportBackground();
        ReloadMapBackground();
    }

    private void UpdateArrowVisibility()
    {
        B_Up.IsEnabled = View.CanUp;
        B_Down.IsEnabled = View.CanDown;
        B_Left.IsEnabled = View.CanLeft;
        B_Right.IsEnabled = View.CanRight;
    }

    #region Viewport mouse

    private void ViewportMouseDown(object? sender, PointerPressedEventArgs e)
    {
        ResetDrag();
        var point = e.GetCurrentPoint(PB_Viewport);
        IsViewportPressed = point.Properties.IsLeftButtonPressed;
        if (point.Properties.IsRightButtonPressed)
            UpdateHoveredCoordinates(point.Position); // the context menu acts on the clicked tile
    }

    private void ViewportMouseUp(object? sender, PointerReleasedEventArgs e)
    {
        if (e.InitialPressMouseButton != MouseButton.Left || !IsViewportPressed)
            return;
        IsViewportPressed = false;

        // A click is a release over the control.
        var pos = e.GetPosition(PB_Viewport);
        if (!new Rect(PB_Viewport.Bounds.Size).Contains(pos))
            return;
        ViewportMouseClick(pos);
    }

    private void ViewportMouseClick(Point e)
    {
        if (IsDragOperationActive)
        {
            ResetDrag();
            return;
        }

        if (RB_Item.IsChecked == true)
            OmniTile(e);
        else if (RB_Terrain.IsChecked == true)
            OmniTileTerrain(e);
    }

    private void ResetDrag()
    {
        DragX = -1;
        DragY = -1;
        IsDragOperationActive = false;
    }

    private void OmniTile(Point e)
    {
        if (!GetTile(e, CurrentLayer, out var tile))
            return;
        OmniTile(tile.Tile, tile.RelativeX, tile.RelativeY);
    }

    private void OmniTileTerrain(Point e)
    {
        if (!GetTile(e, Editor.Mutator.Manager.LayerTerrain, out var meta))
            return;

        var tile = meta.Tile;
        var relX = meta.RelativeX;
        var relY = meta.RelativeY;

        if (tbeForm?.IsBrushSelected != true)
        {
            OmniTileTerrain(tile);
            return;
        }

        if (tbeForm.Slider_thickness.Value <= 1)
        {
            SetTile(tile);
            return;
        }

        List<TerrainTile> selectedTiles = [];
        int radius = (int)Math.Round(tbeForm.Slider_thickness.Value);
        int threshold = (radius * radius) / 2;
        var cfg = Editor.Mutator.Manager.ConfigTerrain;
        for (int i = -radius; i < radius; i++)
        {
            for (int j = -radius; j < radius; j++)
            {
                if ((i * i) + (j * j) < threshold)
                {
                    var x = relX + i;
                    var y = relY + j;
                    if (cfg.IsCoordinateValidRelative(x, y))
                        selectedTiles.Add(Editor.Terrain.GetTile(x, y));
                }
            }
        }

        if (selectedTiles.Count != 0)
            SetTiles(selectedTiles);
    }

    private async void OmniTile(Item tile, int relX, int relY)
    {
        var mods = UiUtil.ModifierKeys;
        var cmd = UiUtil.CommandModifier;
        if (mods == (KeyModifiers.Alt | cmd) || mods == (KeyModifiers.Alt | cmd | KeyModifiers.Shift))
            await ReplaceTile(tile, relX, relY);
        else if (mods == KeyModifiers.Shift)
            SetTile(tile, relX, relY);
        else if (mods == KeyModifiers.Alt)
            DeleteTile(tile, relX, relY);
        else
            ViewTile(tile, relX, relY);
    }

    private void OmniTileTerrain(TerrainTile tile)
    {
        var mods = UiUtil.ModifierKeys;
        if (mods == (KeyModifiers.Shift | UiUtil.CommandModifier))
            RotateTile(tile);
        else if (mods == KeyModifiers.Shift)
            SetTile(tile);
        else if (mods == KeyModifiers.Alt)
            DeleteTile(tile);
        else
            ViewTile(tile);
    }

    private sealed record TileCheck<T>(T Tile, int AbsoluteX, int AbsoluteY, int RelativeX, int RelativeY);

    private bool GetTile(Point e, LayerFieldItem layerField, [NotNullWhen(true)] out TileCheck<Item>? item)
    {
        UpdateHoveredCoordinates(e);
        var (absX, absY) = GetAbsoluteCoordinatesHover();
        return GetTile(layerField, absX, absY, out item);
    }

    private (int X, int Y) GetAbsoluteCoordinatesHover()
    {
        var absX = View.X + HoverX;
        var absY = View.Y + HoverY;
        return (absX, absY);
    }

    private (int X, int Y) GetAbsoluteCoordinatesHoverTerrain()
    {
        // Terrain tiles are 16x16, but the view caters to 32x32 for field items.
        var absX = (View.X + HoverX) / 2;
        var absY = (View.Y + HoverY) / 2;
        return (absX, absY);
    }

    private (int X, int Y) GetViewCoordinates(Point e)
    {
        var x = (int)e.X / Editor.ViewScale;
        var y = (int)e.Y / Editor.ViewScale;
        return (x, y);
    }

    private bool GetTile(LayerFieldItem layerField, int absX, int absY, [NotNullWhen(true)] out TileCheck<Item>? item)
    {
        var cfg = Editor.Mutator.Manager.ConfigItems;
        var (relX, relY) = cfg.GetCoordinatesRelative(absX, absY);
        if (!cfg.IsCoordinateValidRelative(relX, relY))
        {
            item = null;
            return false;
        }

        var tile = layerField.GetTile(relX, relY);
        item = new TileCheck<Item>(tile, absX, absY, relX, relY);
        return true;
    }

    private bool GetTile(Point e, LayerTerrain layerField, [NotNullWhen(true)] out TileCheck<TerrainTile>? item)
    {
        UpdateHoveredCoordinates(e);
        var (absX, absY) = GetAbsoluteCoordinatesHoverTerrain();
        return GetTile(layerField, absX, absY, out item);
    }

    private bool GetTile(LayerTerrain layerField, int absX, int absY, [NotNullWhen(true)] out TileCheck<TerrainTile>? item)
    {
        var cfg = Editor.Mutator.Manager.ConfigTerrain;
        var (relX, relY) = cfg.GetCoordinatesRelative(absX, absY);
        if (!cfg.IsCoordinateValidRelative(relX, relY))
        {
            item = null;
            return false;
        }

        var tile = layerField.GetTile(relX, relY);
        item = new TileCheck<TerrainTile>(tile, absX, absY, relX, relY);
        return true;
    }

    private void UpdateHoveredCoordinates(Point e)
    {
        (HoverX, HoverY) = GetViewCoordinates(e);

        // Mouse event may fire with a slightly too large x/y; clamp just in case.
        HoverX &= 0x1F;
        HoverY &= 0x1F;
    }

    private void ViewportMouseMove(object? sender, PointerEventArgs e)
    {
        var point = e.GetCurrentPoint(PB_Viewport);
        if (point.Properties.IsLeftButtonPressed)
        {
            if (tbeForm?.IsBrushSelected == true)
            {
                OmniTileTerrain(point.Position);
                return;
            }
            if (CHK_MoveOnDrag.IsChecked == true)
            {
                MoveDrag(point.Position);
                return;
            }
        }

        var l = CurrentLayer;
        // Update hover tooltip if it is a different tile
        // Can't compare coordinates if redirection of extension tiles hijacks to return the root tile.
        // Just check the (root) tile returns for each.
        // Two different extension tiles can redirect to the same root tile, can skip updating in that case.
        if (!GetTile(l, View.X + HoverX, View.Y + HoverY, out var old))
            return;

        var oldTile = old.Tile;
        if (!GetTile(point.Position, l, out var dest))
            return;

        var tile = dest.Tile;
        var x = dest.RelativeX;
        var y = dest.RelativeY;
        if (ReferenceEquals(tile, oldTile))
            return; // same tile, no change

        // Regenerate tooltip text
        var str = GameInfo.Strings;
        var name = str.GetItemName(tile);
        var flagLayer = NUD_Layer.Value == 0 ? Map.LayerItemFlag0 : Map.LayerItemFlag1;
        var isActive = flagLayer.GetIsActive(x, y);
        if (isActive)
            name = $"{name} [Active]";
        ToolTip.SetTip(PB_Viewport, name);
        SetCoordinateText(x, y);
    }

    private void MoveDrag(Point e)
    {
        var (viewX, viewY) = GetViewCoordinates(e);

        if (DragX == -1)
        {
            DragX = viewX;
            DragY = viewY;
            return;
        }

        var dX = DragX - viewX;
        var dY = DragY - viewY;

        if (UiUtil.ModifierKeys == UiUtil.CommandModifier) // move in larger steps
        {
            dX *= 2;
            dY *= 2;
        }

        if ((dX & 1) == 1)
            dX ^= 1;
        if ((dY & 1) == 1)
            dY ^= 1;

        // Ensure movement is significant enough
        var aX = Math.Abs(dX);
        var aY = Math.Abs(dY);
        if (aX < 2 && aY < 2)
            return;

        DragX = viewX;
        DragY = viewY;
        if (!View.DragView(dX, dY))
            return;

        IsDragOperationActive = true;
        LoadItemGridAcre();
    }

    #endregion Viewport mouse

    #region Tile actions

    private void ViewTile(Item tile, int relX, int relY)
    {
        if (CHK_RedirectExtensionLoad.IsChecked == true && tile.IsExtension)
        {
            var l = CurrentLayer;
            relX -= tile.ExtensionX;
            relY -= tile.ExtensionY;
            l.TileInfo.ClampInside(ref relX, ref relY);
            var redirectTile = l.GetTile(relX, relY);

            if (redirectTile.IsRoot && redirectTile.ItemId == tile.ExtensionItemId)
                tile = redirectTile;
        }

        ViewTile(tile);
    }

    private void ViewTile(Item tile)
    {
        ItemEdit.LoadItem(tile);
        TC_Editor.SelectedItem = Tab_Item;
    }

    private void ViewTile(TerrainTile tile)
    {
        var pgt = (TerrainTile)PG_TerrainTile.SelectedObject!;
        pgt.CopyFrom(tile);
        PG_TerrainTile.SelectedObject = pgt;
        TC_Editor.SelectedItem = Tab_Terrain;
    }

    private void SetTile(Item tile, int relX, int relY)
    {
        var l = CurrentLayer;
        var pgt = new Item();
        ItemEdit.SetItem(pgt);

        if (pgt.IsFieldItem && CHK_FieldItemSnap.IsChecked == true)
        {
            // coordinates must be even (not odd-half)
            relX &= 0xFFFE;
            relY &= 0xFFFE;
            tile = l.GetTile(relX, relY);
        }

        var permission = l.IsOccupied(pgt, relX, relY);
        switch (permission)
        {
            case PlacedItemPermission.OutOfBounds:
            case PlacedItemPermission.Collision when CHK_NoOverwrite.IsChecked == true:
                Sound.Asterisk();
                return;
        }

        // Clean up original placed data
        if (tile.IsRoot && CHK_AutoExtension.IsChecked == true)
            l.DeleteExtensionTiles(tile, relX, relY);

        // Set new placed data
        if (pgt.IsRoot && CHK_AutoExtension.IsChecked == true)
            l.SetExtensionTiles(pgt, relX, relY);
        tile.CopyFrom(pgt);

        ReloadItems();
    }

    private async Task ReplaceTile(Item tile, int relX, int relY)
    {
        var l = CurrentLayer;
        var pgt = new Item();
        ItemEdit.SetItem(pgt);

        if (pgt.IsFieldItem && CHK_FieldItemSnap.IsChecked == true)
        {
            // coordinates must be even (not odd-half)
            relX &= 0xFFFE;
            relY &= 0xFFFE;
            tile = l.GetTile(relX, relY);
        }

        var permission = l.IsOccupied(pgt, relX, relY);
        switch (permission)
        {
            case PlacedItemPermission.OutOfBounds:
                Sound.Asterisk();
                return;
        }

        bool wholeMap = UiUtil.IsShift(UiUtil.ModifierKeys);
        var copy = new Item(tile.RawValue);
        var count = Editor.Mutator.ReplaceFieldItems(copy, pgt, wholeMap);
        if (count == 0)
        {
            await Dialogs.Alert(MessageStrings.MsgFieldItemModifyNone);
            return;
        }
        LoadItemGridAcre();
        await Dialogs.Alert(string.Format(MessageStrings.MsgFieldItemModifyCount, count));
    }

    private void RotateTile(TerrainTile tile)
    {
        bool wasRotated = tile.TryRotate();
        if (!wasRotated)
        {
            Sound.Asterisk();
            return;
        }
        ReloadBuildingsTerrain();
    }

    private void SetTile(TerrainTile tile)
    {
        var pgt = (TerrainTile)PG_TerrainTile.SelectedObject!;

        // Apply randomization if enabled
        if (tbeForm?.RandomizeVariation == true)
        {
            switch (pgt.UnitModel)
            {
                case TerrainUnitModel.Cliff5B:
                case TerrainUnitModel.River5B:
                    pgt.Variation = (ushort)Random.Shared.Next(4);
                    break;
            }
        }

        tile.CopyFrom(pgt);
        ReloadBuildingsTerrain();
    }

    private void SetTiles(IEnumerable<TerrainTile> tiles)
    {
        var pgt = (TerrainTile)PG_TerrainTile.SelectedObject!;

        foreach (var tile in tiles)
            tile.CopyFrom(pgt);
        ReloadBuildingsTerrain();
    }

    private void DeleteTile(Item tile, int relX, int relY)
    {
        if (CHK_AutoExtension.IsChecked == true)
        {
            var layer = CurrentLayer;
            if (!tile.IsRoot)
            {
                relX -= tile.ExtensionX;
                relY -= tile.ExtensionY;
                tile = layer.GetTile(relX, relY);
            }
            layer.DeleteExtensionTiles(tile, relX, relY);
        }

        tile.Delete();
        ReloadItems();
    }

    private void DeleteTile(TerrainTile tile)
    {
        tile.Clear();
        ReloadBuildingsTerrain();
    }

    #endregion Tile actions

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private async void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        var set = Map.FieldItems;
        var unsupported = set.GetUnsupportedTiles();
        if (unsupported.Count != 0)
        {
            var err = MessageStrings.MsgFieldItemUnsupportedLayer2Tile;
            var ask = MessageStrings.MsgAskContinue;
            var prompt = await Dialogs.Prompt(MessageBoxButtons.YesNo, err, ask);
            if (prompt != DialogResult.Yes)
                return;
        }

        Map.SetManager(SAV);
        SAV.OutsideFieldTemplateUniqueId = (ushort)(NUD_MapAcreTemplateOutside.Value ?? 0);
        SAV.MainFieldParamUniqueID = (ushort)(NUD_MapAcreTemplateField.Value ?? 0);
        Close(true);
    }

    #region Context menu (viewport)

    private bool TryGetHoveredItem(out Item tile, out int relX, out int relY)
    {
        var (absX, absY) = GetAbsoluteCoordinatesHover();
        var cfg = Editor.Mutator.Manager.ConfigItems;
        (relX, relY) = cfg.GetCoordinatesRelative(absX, absY);
        if (!cfg.IsCoordinateValidRelative(relX, relY))
        {
            Sound.Asterisk();
            tile = Item.NO_ITEM;
            return false;
        }

        tile = CurrentLayer.GetTile(relX, relY);
        return true;
    }

    private bool TryGetHoveredTerrain([NotNullWhen(true)] out TerrainTile? tile)
    {
        var (absX, absY) = GetAbsoluteCoordinatesHoverTerrain();
        var cfg = Editor.Mutator.Manager.ConfigTerrain;
        var (relX, relY) = cfg.GetCoordinatesRelative(absX, absY);
        if (!cfg.IsCoordinateValidRelative(relX, relY))
        {
            Sound.Asterisk();
            tile = null;
            return false;
        }

        tile = Editor.Terrain.GetTile(relX, relY);
        return true;
    }

    private void Menu_View_Click(object? sender, RoutedEventArgs e)
    {
        if (RB_Item.IsChecked == true)
        {
            if (TryGetHoveredItem(out var tile, out var relX, out var relY))
                ViewTile(tile, relX, relY);
        }
        else if (RB_Terrain.IsChecked == true)
        {
            if (TryGetHoveredTerrain(out var tile))
                ViewTile(tile);
        }
    }

    private void Menu_Set_Click(object? sender, RoutedEventArgs e)
    {
        if (RB_Item.IsChecked == true)
        {
            if (TryGetHoveredItem(out var tile, out var relX, out var relY))
                SetTile(tile, relX, relY);
        }
        else if (RB_Terrain.IsChecked == true)
        {
            if (TryGetHoveredTerrain(out var tile))
                SetTile(tile);
        }
    }

    private void Menu_Reset_Click(object? sender, RoutedEventArgs e)
    {
        if (RB_Item.IsChecked == true)
        {
            if (TryGetHoveredItem(out var tile, out var relX, out var relY))
                DeleteTile(tile, relX, relY);
        }
        else if (RB_Terrain.IsChecked == true)
        {
            if (TryGetHoveredTerrain(out var tile))
                DeleteTile(tile);
        }
    }

    private void CM_Click_Opening(object? sender, System.ComponentModel.CancelEventArgs e)
    {
        if (RB_Item.IsChecked != true) // not in Item edit mode, therefore no "Activate Flag" menu
        {
            Menu_Activate.IsVisible = false;
            return;
        }

        var (absX, absY) = GetAbsoluteCoordinatesHover();
        var cfg = Editor.Mutator.Manager.ConfigItems;
        var (relX, relY) = cfg.GetCoordinatesRelative(absX, absY);
        if (!cfg.IsCoordinateValidRelative(relX, relY))
            return;

        var flagLayer = NUD_Layer.Value == 0 ? Map.LayerItemFlag0 : Map.LayerItemFlag1;
        var isActive = flagLayer.GetIsActive(relX, relY);
        Menu_Activate.Header = isActive ? "Inactivate" : "Activate";
        Menu_Activate.IsVisible = true;
    }

    private void Menu_Activate_Click(object? sender, RoutedEventArgs e)
    {
        if (RB_Item.IsChecked != true) // not in Item edit mode, therefore no "Activate Flag" menu
            return;

        var (absX, absY) = GetAbsoluteCoordinatesHover();
        var cfg = Editor.Mutator.Manager.ConfigItems;
        var (relX, relY) = cfg.GetCoordinatesRelative(absX, absY);
        if (!cfg.IsCoordinateValidRelative(relX, relY))
            return;

        var flagLayer = NUD_Layer.Value == 0 ? Map.LayerItemFlag0 : Map.LayerItemFlag1;
        var isActive = flagLayer.GetIsActive(relX, relY);
        flagLayer.SetIsActive(relX, relY, !isActive);
    }

    #endregion Context menu (viewport)

    #region Navigation

    private void B_Up_Click(object? sender, RoutedEventArgs e)
    {
        if (UiUtil.ModifierKeys == KeyModifiers.Shift)
            CB_Acre.SelectedIndex = Math.Max(0, CB_Acre.SelectedIndex - Editor.Items.Layer0.TileInfo.Columns);
        else if (View.ArrowUp())
            LoadItemGridAcre();
    }

    private void B_Left_Click(object? sender, RoutedEventArgs e)
    {
        if (UiUtil.ModifierKeys == KeyModifiers.Shift)
            CB_Acre.SelectedIndex = Math.Max(0, CB_Acre.SelectedIndex - 1);
        else if (View.ArrowLeft())
            LoadItemGridAcre();
    }

    private void B_Right_Click(object? sender, RoutedEventArgs e)
    {
        if (UiUtil.ModifierKeys == KeyModifiers.Shift)
            CB_Acre.SelectedIndex = Math.Min(CB_Acre.SelectedIndex + 1, CB_Acre.ItemCount - 1);
        else if (View.ArrowRight())
            LoadItemGridAcre();
    }

    private void B_Down_Click(object? sender, RoutedEventArgs e)
    {
        if (UiUtil.ModifierKeys == KeyModifiers.Shift)
            CB_Acre.SelectedIndex = Math.Min(CB_Acre.SelectedIndex + Editor.Items.Layer0.TileInfo.Columns, CB_Acre.ItemCount - 1);
        else if (View.ArrowDown())
            LoadItemGridAcre();
    }

    #endregion Navigation

    #region Dump/Import

    private string AcreFileName => $"{View.X:000}-{View.Y:000}";

    private async void B_DumpAcreItem_Click(object? sender, RoutedEventArgs e)
    {
        var (relX, relY) = Editor.Mutator.Manager.ConfigItems.GetCoordinatesRelative(View.X, View.Y);
        await MapDumpHelper.DumpLayerAcreSingle(this, CurrentLayer, AcreFileName, (int)(NUD_Layer.Value ?? 1), relX, relY);
    }

    private async void B_DumpAllAcres_Click(object? sender, RoutedEventArgs e) => await MapDumpHelper.DumpLayerAcreAll(this, CurrentLayer);

    private async void B_ImportAcreItem_Click(object? sender, RoutedEventArgs e)
    {
        var (relX, relY) = Editor.Mutator.Manager.ConfigItems.GetCoordinatesRelative(View.X, View.Y);
        var layer = CurrentLayer;
        if (!await MapDumpHelper.ImportToLayerAcreSingle(this, layer, AcreFileName, (int)(NUD_Layer.Value ?? 1), relX, relY))
            return;
        ChangeViewToAcre(ExteriorAcreIndex);
        Sound.Asterisk();
    }

    private async void B_ImportAllAcres_Click(object? sender, RoutedEventArgs e)
    {
        if (!await MapDumpHelper.ImportToLayerAcreAll(this, CurrentLayer))
            return;
        ChangeViewToAcre(ExteriorAcreIndex);
        Sound.Asterisk();
    }

    private async void B_DumpAllAcresFlag_Click(object? sender, RoutedEventArgs e)
        => await MapDumpHelper.DumpLayerAllFlag(this, Editor.Mutator.CurrentLayerFlags, Editor.Mutator.ItemLayerIndex);

    private async void B_ImportAllAcresFlag_Click(object? sender, RoutedEventArgs e)
    {
        var layer = Editor.Mutator.CurrentLayerFlags;
        var index = Editor.Mutator.ItemLayerIndex;
        if (!await MapDumpHelper.ImportToLayerAllFlag(this, layer, index))
            return;
        ChangeViewToAcre(ExteriorAcreIndex);
        Sound.Asterisk();
    }

    private async void B_DumpBuildings_Click(object? sender, RoutedEventArgs e) => await MapDumpHelper.DumpBuildings(this, Editor.Buildings.Buildings);

    private async void B_ImportBuildings_Click(object? sender, RoutedEventArgs e)
    {
        if (!await MapDumpHelper.ImportBuildings(this, Editor.Buildings.Buildings))
            return;

        for (int i = 0; i < Editor.Buildings.Count; i++)
            SetBuildingName(i);
        LB_Items.SelectedIndex = 0;
        Sound.Asterisk();
        ReloadBuildingsTerrain();
    }

    private async void B_DumpTerrainAcre_Click(object? sender, RoutedEventArgs e)
    {
        var (relX, relY) = Editor.Mutator.Manager.ConfigTerrain.GetCoordinatesRelative(View.X / 2, View.Y / 2);
        await MapDumpHelper.DumpTerrainAcre(this, Editor.Terrain, AcreFileName, relX, relY);
    }

    private async void B_DumpTerrainAll_Click(object? sender, RoutedEventArgs e) => await MapDumpHelper.DumpTerrainAll(this, Editor.Terrain);

    private async void B_ImportTerrainAcre_Click(object? sender, RoutedEventArgs e)
    {
        var (relX, relY) = Editor.Mutator.Manager.ConfigTerrain.GetCoordinatesRelative(View.X / 2, View.Y / 2);
        if (!await MapDumpHelper.ImportTerrainAcre(this, Editor.Terrain, AcreFileName, relX, relY))
            return;
        ChangeViewToAcre(ExteriorAcreIndex);
        Sound.Asterisk();
    }

    private async void B_ImportTerrainAll_Click(object? sender, RoutedEventArgs e)
    {
        if (!await MapDumpHelper.ImportTerrainAll(this, Editor.Terrain))
            return;
        ChangeViewToAcre(ExteriorAcreIndex);
        Sound.Asterisk();
    }

    #endregion Dump/Import

    #region Map

    private async void Menu_SavePNG_Click(object? sender, RoutedEventArgs e)
    {
        if (PB_Map.Image is null)
        {
            await Dialogs.Alert(MessageStrings.MsgNoPictureLoaded);
            return;
        }

        CM_Picture.Close();

        const string name = "map";
        var path = await Pickers.SaveFile(this, "png file (*.png)|*.png|All files (*.*)|*.*", $"{name}.png");
        if (path is null)
            return;

        if (Menu_SavePNGTerrain.IsChecked != true)
        {
            PB_Map.Image.Save(path, new PngBitmapEncoderOptions());
        }
        else if (Menu_SavePNGItems.IsChecked != true)
        {
            PB_Map.BackgroundImage!.Save(path, new PngBitmapEncoderOptions());
        }
        else
        {
            using var img = PB_Map.GetComposite();
            img.Save(path, new PngBitmapEncoderOptions());
        }
    }

    /// <summary>
    /// CheckOnClick of the "Include" items (the menu stays open, like CM_Picture_Closing did).
    /// </summary>
    private void Menu_SavePNGCheck_Click(object? sender, RoutedEventArgs e)
    {
        if (sender is MenuItem m)
            m.IsChecked = !m.IsChecked;
    }

    private void PB_Map_MouseDown(object? sender, PointerPressedEventArgs e)
    {
        var point = e.GetCurrentPoint(PB_Map);
        if (!point.Properties.IsLeftButtonPressed)
            return;
        ClickMapAt(point.Position);
    }

    private void ClickMapAt(Point e)
    {
        var (absX, absY) = Editor.GetMapCoordinates((int)e.X, (int)e.Y, CHK_SnapToAcre.IsChecked == true ? MapViewCoordinateRequest.SnapAcre : MapViewCoordinateRequest.Centered);

        // Truncate to root-node coordinates. The map is only 1px per tile, and nobody is wanting to click on extension-tiles.
        absX &= 0xFFFE;
        absY &= 0xFFFE;

        if (View.SetViewTo(absX, absY))
            LoadItemGridAcre();
    }

    private void PB_Map_MouseMove(object? sender, PointerEventArgs e)
    {
        var point = e.GetCurrentPoint(PB_Map);
        var props = point.Properties;
        if (props.IsLeftButtonPressed)
        {
            ClickMapAt(point.Position);
        }
        else if (!props.IsRightButtonPressed && !props.IsMiddleButtonPressed)
        {
            var (absX, absY) = Editor.GetCursorCoordinates((int)point.Position.X, (int)point.Position.Y);
            SetCoordinateText(absX, absY);
        }
    }

    private void SetCoordinateText(int absX, int absY) => L_Coordinates.Text = $"({absX:000},{absY:000}) = (0x{absX:X2},0x{absY:X2})";

    private void NUD_Layer_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        if (Renderer is null || NUD_Layer.Value is not { } value) // still being built, or emptied
            return;
        View.ItemLayerIndex = (int)value - 1;
        LoadItemGridAcre();
    }

    #endregion Map

    #region Remove/Modify

    private async Task Remove(MenuItem sender, Func<int, int, int, int, int> removal)
    {
        var isModifyEntireMap = UiUtil.IsShift(UiUtil.ModifierKeys);
        var message = string.Format(MessageStrings.MsgFieldItemRemoveAsk, sender.Header);
        var question = await Dialogs.Prompt(MessageBoxButtons.YesNo, message);
        if (question != DialogResult.Yes)
            return;

        var count = Editor.Mutator.ModifyFieldItems(removal, isModifyEntireMap);
        if (count == 0)
        {
            await Dialogs.Alert(MessageStrings.MsgFieldItemRemoveNone);
            return;
        }
        LoadItemGridAcre();
        await Dialogs.Alert(string.Format(MessageStrings.MsgFieldItemRemoveCount, count));
    }

    private async Task Modify(MenuItem sender, Func<int, int, int, int, int> action)
    {
        var isModifyEntireMap = UiUtil.IsShift(UiUtil.ModifierKeys);
        var message = string.Format(MessageStrings.MsgFieldItemModifyAsk, sender.Header);
        var question = await Dialogs.Prompt(MessageBoxButtons.YesNo, message);
        if (question != DialogResult.Yes)
            return;

        var count = Editor.Mutator.ModifyFieldItems(action, isModifyEntireMap);
        if (count == 0)
        {
            await Dialogs.Alert(MessageStrings.MsgFieldItemModifyNone);
            return;
        }
        LoadItemGridAcre();
        await Dialogs.Alert(string.Format(MessageStrings.MsgFieldItemModifyCount, count));
    }

    private async void B_RemoveEditor_Click(object? sender, RoutedEventArgs e)
    {
        var item = ItemEdit.LoadFieldsToNewItem();
        var lambda = new Func<int, int, int, int, int>((min, max, x, y)
            => CurrentLayer.RemoveAllLike(min, max, x, y, item));
        await Remove(B_RemoveEditor, lambda);
    }

    private async void B_WaterFlowers_Click(object? sender, RoutedEventArgs e)
    {
        var all = UiUtil.IsCommand(UiUtil.ModifierKeys);
        var lambda = new Func<int, int, int, int, int>((xmin, ymin, width, height)
            => CurrentLayer.WaterAllFlowers(xmin, ymin, width, height, all));
        await Modify(B_WaterFlowers, lambda);
    }

    private async void B_RemoveAllWeeds_Click(object? sender, RoutedEventArgs e) => await Remove(B_RemoveAllWeeds, CurrentLayer.RemoveAllWeeds);
    private async void B_RemoveAllTrees_Click(object? sender, RoutedEventArgs e) => await Remove(B_RemoveAllTrees, CurrentLayer.RemoveAllTrees);
    private async void B_FillHoles_Click(object? sender, RoutedEventArgs e) => await Remove(B_FillHoles, CurrentLayer.RemoveAllHoles);
    private async void B_RemovePlants_Click(object? sender, RoutedEventArgs e) => await Remove(B_RemovePlants, CurrentLayer.RemoveAllPlants);
    private async void B_RemoveFences_Click(object? sender, RoutedEventArgs e) => await Remove(B_RemoveFences, CurrentLayer.RemoveAllFences);
    private async void B_RemoveObjects_Click(object? sender, RoutedEventArgs e) => await Remove(B_RemoveObjects, CurrentLayer.RemoveAllObjects);
    private async void B_RemoveAll_Click(object? sender, RoutedEventArgs e) => await Remove(B_RemoveAll, CurrentLayer.RemoveAll);
    private async void B_RemovePlacedItems_Click(object? sender, RoutedEventArgs e) => await Remove(B_RemovePlacedItems, CurrentLayer.RemoveAllPlacedItems);
    private async void B_RemoveShells_Click(object? sender, RoutedEventArgs e) => await Remove(B_RemoveShells, CurrentLayer.RemoveAllShells);
    private async void B_RemoveBranches_Click(object? sender, RoutedEventArgs e) => await Remove(B_RemoveBranches, CurrentLayer.RemoveAllBranches);
    private async void B_RemoveFlowers_Click(object? sender, RoutedEventArgs e) => await Remove(B_RemoveFlowers, CurrentLayer.RemoveAllFlowers);
    private async void B_RemoveBushes_Click(object? sender, RoutedEventArgs e) => await Remove(B_RemoveBushes, CurrentLayer.RemoveAllBushes);

    #endregion Remove/Modify

    private void TR_Transparency_Scroll(object? sender, RangeBaseValueChangedEventArgs e)
    {
        if (Renderer is null) // still being built
            return;
        ReloadItems();
    }

    private void TR_BuildingTransparency_Scroll(object? sender, RangeBaseValueChangedEventArgs e)
    {
        if (Renderer is null)
            return;
        ReloadBuildingsTerrain();
    }

    private void TR_Terrain_Scroll(object? sender, RangeBaseValueChangedEventArgs e)
    {
        if (Renderer is null)
            return;
        ReloadBuildingsTerrain();
    }

    #region Buildings

    private async void B_Help_Click(object? sender, RoutedEventArgs e)
    {
        var form = new BuildingHelp();
        await form.ShowDialog<bool>(this);
    }

    private void NUD_PlazaX_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        if (Loading || Renderer is null)
            return;
        Map.Plaza.X = (uint)(NUD_PlazaX.Value ?? 0);
        ReloadBuildingsTerrain();
    }

    private void NUD_PlazaY_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        if (Loading || Renderer is null)
            return;
        Map.Plaza.Z = (uint)(NUD_PlazaY.Value ?? 0);
        ReloadBuildingsTerrain();
    }

    private void LB_Items_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (UpdatingBuildingNames || LB_Items.SelectedIndex < 0)
            return;
        LoadBuildingIndex(LB_Items.SelectedIndex);

        // View location snap has changed the view. Reload everything
        LoadItemGridAcre();
        ReloadMapBackground();
    }

    private void LoadBuildingIndex(int index)
    {
        Loading = true;
        SelectedBuildingIndex = index;
        var b = Editor.Buildings[index];
        NUD_BuildingType.Value = (int)b.BuildingType;
        NUD_X.Value = b.X;
        NUD_Y.Value = b.Y;
        NUD_Angle.Value = b.Angle;
        NUD_Bit.Value = b.Bit;
        NUD_Type.Value = b.Type;
        NUD_TypeArg.Value = b.TypeArg;
        NUD_UniqueID.Value = b.UniqueID;
        Loading = false;

        // Jump the view to see the building
        // -16 to put it in the center of the view
        const int shift = 16;
        var x = (b.X - shift) & 0xFFFE;
        var y = (b.Y - shift) & 0xFFFE;
        View.SetViewTo(x, y);
    }

    private void NUD_BuildingType_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        if (Loading || Renderer is null || sender is not NumericUpDown n)
            return;

        var value = n.Value ?? 0;
        var b = Editor.Buildings[SelectedBuildingIndex];
        if (sender == NUD_BuildingType)
            b.BuildingType = (BuildingType)value;
        else if (sender == NUD_X)
            b.X = (ushort)value;
        else if (sender == NUD_Y)
            b.Y = (ushort)value;
        else if (sender == NUD_Angle)
            b.Angle = (byte)value;
        else if (sender == NUD_Bit)
            b.Bit = (sbyte)value;
        else if (sender == NUD_Type)
            b.Type = (ushort)value;
        else if (sender == NUD_TypeArg)
            b.TypeArg = (byte)value;
        else if (sender == NUD_UniqueID)
            b.UniqueID = (ushort)value;

        SetBuildingName(SelectedBuildingIndex);
        ReloadBuildingsTerrain();
    }

    /// <summary>
    /// Refreshes the list text of a building (LB_Items.Items[index] = ... in WinForms), keeping the selection.
    /// </summary>
    private void SetBuildingName(int index)
    {
        var selected = LB_Items.SelectedIndex;
        UpdatingBuildingNames = true;
        BuildingNames[index] = Editor.Buildings[index].ToString();
        LB_Items.SelectedIndex = selected;
        UpdatingBuildingNames = false;
    }

    #endregion Buildings

    #region Acres

    private void CB_MapAcre_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (CB_MapAcre.SelectedIndex < 0)
            return;
        var acre = Editor.Terrain.BaseAcres.Span[CB_MapAcre.SelectedIndex * 2];
        CB_MapAcreSelect.SelectedValue = (int)acre;

        // Jump view if available
        var index = Array.IndexOf(AcreNames, CB_MapAcre.SelectedItem as string);
        if (index >= 0)
            CB_Acre.SelectedIndex = index;
    }

    private void CB_MapAcreSelect_SelectedValueChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (Loading || CB_MapAcreSelect.SelectedItem is null)
            return;

        var index = CB_MapAcre.SelectedIndex;
        var value = UiUtil.GetIndex(CB_MapAcreSelect);

        // u16[], but values are at most u8 each.
        var span = Editor.Terrain.GetBaseAcreSpan(index);
        var oldValue = span[0];
        if (value == oldValue)
            return;

        BinaryPrimitives.WriteUInt16LittleEndian(span, (ushort)value);
        ReloadBuildingsTerrain();
    }

    private async void B_DumpMapAcres_Click(object? sender, RoutedEventArgs e)
    {
        if (!await MapDumpHelper.DumpMapAcresAll(this, Editor.Terrain.BaseAcres))
            return;
        ReloadBuildingsTerrain();
        Sound.Asterisk();
    }

    private async void B_ImportMapAcres_Click(object? sender, RoutedEventArgs e)
    {
        if (!await MapDumpHelper.ImportMapAcresAll(this, Editor.Terrain.BaseAcres))
            return;
        ReloadBuildingsTerrain();
        Sound.Asterisk();
    }

    #endregion Acres

    #region Terrain

    private async void B_ZeroElevation_Click(object? sender, RoutedEventArgs e)
    {
        if (DialogResult.Yes != await Dialogs.Prompt(MessageBoxButtons.YesNo, MessageStrings.MsgTerrainSetElevation0))
            return;
        foreach (var t in Editor.Terrain.Tiles)
            t.Elevation = 0;
        ReloadBuildingsTerrain();
        Sound.Asterisk();
    }

    private async void B_SetAllTerrain_Click(object? sender, RoutedEventArgs e)
    {
        if (DialogResult.Yes != await Dialogs.Prompt(MessageBoxButtons.YesNo, MessageStrings.MsgTerrainSetAll))
            return;

        var pgt = (TerrainTile)PG_TerrainTile.SelectedObject!;
        bool interiorOnly = DialogResult.Yes == await Dialogs.Prompt(MessageBoxButtons.YesNo, MessageStrings.MsgTerrainSetAllSkipExterior);
        Editor.Terrain.SetAll(pgt, interiorOnly);

        ReloadBuildingsTerrain();
        Sound.Asterisk();
    }

    private async void B_SetAllRoadTiles_Click(object? sender, RoutedEventArgs e)
    {
        if (DialogResult.Yes != await Dialogs.Prompt(MessageBoxButtons.YesNo, MessageStrings.MsgTerrainSetAll))
            return;

        var pgt = (TerrainTile)PG_TerrainTile.SelectedObject!;
        bool interiorOnly = DialogResult.Yes == await Dialogs.Prompt(MessageBoxButtons.YesNo, MessageStrings.MsgTerrainSetAllSkipExterior);
        Editor.Terrain.SetAllRoad(pgt, interiorOnly);

        ReloadBuildingsTerrain();
        Sound.Asterisk();
    }

    private void B_ClearPlacedDesigns_Click(object? sender, RoutedEventArgs e)
    {
        SAV.ClearDesignTiles(MapDesignGridSize.Item1, MapDesignGridSize.Item2);
        Sound.Asterisk();
    }

    private async void B_ExportPlacedDesigns_Click(object? sender, RoutedEventArgs e)
    {
        if (!await MapDumpHelper.DumpMapDesigns(this, SAV, MapDesignGridSize))
            return;
        Sound.Asterisk();
    }

    private async void B_ImportPlacedDesigns_Click(object? sender, RoutedEventArgs e)
    {
        if (!await MapDumpHelper.ImportMapDesigns(this, SAV, MapDesignGridSize))
            return;
        Sound.Asterisk();
    }

    #endregion Terrain

    private async void Menu_Spawn_Click(object? sender, RoutedEventArgs e) => await new BulkSpawn(this, View.X, View.Y).ShowDialog<bool>(this);

    private async void Menu_Bulk_Click(object? sender, RoutedEventArgs e)
    {
        var editor = new BatchEditor(Spawn.Tiles, ItemEdit.SetItem(new Item()));
        await editor.ShowDialog<bool>(this);
        Spawn.ClearDanglingExtensions(0, 0, Spawn.TileInfo.TotalWidth, Spawn.TileInfo.TotalHeight);
        LoadItemGridAcre();
    }

    private void B_TerrainBrush_Click(object? sender, RoutedEventArgs e)
    {
        if (tbeForm is null)
        {
            tbeForm = new TerrainBrushEditor(PG_TerrainTile, this);
            tbeForm.Closed += (_, _) => tbeForm = null;
            tbeForm.Show(this);
        }
        tbeForm.Activate();
    }

    private void FieldItemEditor_FormClosed(object? sender, EventArgs e) => tbeForm?.Close();
}

public interface IItemLayerEditor
{
    void ReloadItems();

    ItemEditor ItemProvider { get; }
    LayerItem Spawn { get; }
}

/// <summary>
/// The WinForms PictureBox of the map editor: an <see cref="Image"/> drawn over a <see cref="BackgroundImage"/>, pixel for pixel
/// (nearest neighbour when the screen scales it), plus the building/tile names that GDI+ wrote into the background bitmap.
/// The bitmaps are the renderer's reusable ones: after updating their pixels, set them again so the control repaints.
/// </summary>
public sealed class MapPictureBox : Control
{
    private static readonly Typeface LabelFont = new("Courier New, Menlo, monospace");
    private const double LabelSize = 11; // Courier New 8.25pt, the font of L_Coordinates

    private (FormattedText Text, Point Origin)[] Labels = [];

    public MapPictureBox() => ClipToBounds = true; // labels of buildings near the edge stay inside

    public WriteableBitmap? BackgroundImage { get; private set; }
    public WriteableBitmap? Image { get; private set; }

    public void SetBackgroundImage(WriteableBitmap img, IReadOnlyList<MapLabel>? labels = null)
    {
        var resized = BackgroundImage?.PixelSize != img.PixelSize;
        BackgroundImage = img;
        Labels = labels is null ? [] : CreateLabels(labels, img.PixelSize);
        if (resized)
            InvalidateMeasure();
        InvalidateVisual();
    }

    public void SetImage(WriteableBitmap img)
    {
        var resized = Image?.PixelSize != img.PixelSize;
        Image = img;
        if (resized)
            InvalidateMeasure();
        InvalidateVisual();
    }

    private static (FormattedText, Point)[] CreateLabels(IReadOnlyList<MapLabel> labels, PixelSize size)
    {
        const int margin = 200; // text centered further away can't reach the image
        var result = new List<(FormattedText, Point)>(labels.Count);
        var brushes = new Dictionary<byte, IBrush>();
        foreach (var l in labels)
        {
            if (l.X < -margin || l.Y < -margin || l.X > size.Width + margin || l.Y > size.Height + margin)
                continue;
            if (!brushes.TryGetValue(l.Alpha, out var brush))
                brushes[l.Alpha] = brush = new SolidColorBrush(Color.FromArgb(l.Alpha, 0, 0, 0)).ToImmutable();
            var text = new FormattedText(l.Text, CultureInfo.InvariantCulture, FlowDirection.LeftToRight, LabelFont, LabelSize, brush);
            // Centered on (X, Y), like the GDI+ StringFormat with centered alignments.
            result.Add((text, new Point(l.X - (text.Width / 2), l.Y - (text.Height / 2))));
        }
        return [.. result];
    }

    /// <summary>
    /// The image drawn over the background, as one bitmap (for saving what is shown).
    /// </summary>
    public WriteableBitmap GetComposite()
    {
        var back = BackgroundImage!;
        var bg = back.GetPixels();
        var fg = Image!.GetPixels();
        for (int i = 0; i < bg.Length; i++)
            bg[i] = BlendOver(fg[i], bg[i]);
        return ImageUtil.GetBitmap(bg, back.PixelSize.Width, back.PixelSize.Height);
    }

    private static int BlendOver(int front, int back)
    {
        var fa = (front >>> 24) / 255d;
        if (fa >= 1)
            return front;
        var ba = (back >>> 24) / 255d;
        var oa = fa + (ba * (1 - fa));
        if (oa <= 0)
            return 0;

        int Channel(int shift)
        {
            var f = (front >> shift) & 0xFF;
            var b = (back >> shift) & 0xFF;
            return (int)Math.Round(((f * fa) + (b * ba * (1 - fa))) / oa);
        }
        return ((int)Math.Round(oa * 255) << 24) | (Channel(16) << 16) | (Channel(8) << 8) | Channel(0);
    }

    protected override Size MeasureOverride(Size availableSize)
    {
        var img = BackgroundImage ?? Image;
        return img is null ? default : img.Size;
    }

    public override void Render(DrawingContext context)
    {
        // Transparent fill: the whole box takes the pointer, also where nothing is drawn.
        context.FillRectangle(Brushes.Transparent, new Rect(Bounds.Size));

        using (context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.None }))
        {
            if (BackgroundImage is { } bg)
                context.DrawImage(bg, new Rect(bg.Size));
            if (Image is { } fg)
                context.DrawImage(fg, new Rect(fg.Size));
        }

        foreach (var (text, origin) in Labels)
            context.DrawText(text, origin);
    }
}
