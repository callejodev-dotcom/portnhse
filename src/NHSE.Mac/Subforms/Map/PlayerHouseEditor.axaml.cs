using System.Collections.ObjectModel;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class PlayerHouseEditor : NhseWindow
{
    private readonly MainSave SAV;
    private readonly IPlayerHouse[] Houses;
    private readonly IReadOnlyList<Player> Players;
    private readonly ObservableCollection<string> Items = [];
    private RoomManager Manager;
    private const int scale = 24;

    private int Index = -1;
    private int RoomIndex = -1;

    public PlayerHouseEditor(IPlayerHouse[] houses, IReadOnlyList<Player> players, MainSave sav, int index)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);
        SAV = sav;
        Houses = houses;
        Players = players;
        Manager = new RoomManager(houses[0].GetRoom(0));

        var data = GameInfo.Strings.ItemDataSource;
        ItemEdit.Initialize(data, true);

        for (var i = 0; i < Houses.Length; i++)
        {
            var obj = Houses[i];
            Items.Add(MiscDumpHelper.GetHouseSummary(players, obj, i));
        }
        LB_Items.ItemsSource = Items;

        LB_Items.SelectedIndex = index;
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        SaveRoom();
        Close(true);
    }

    private void SaveRoom()
    {
        if (RoomIndex < 0)
            return;
        Manager.Save();
        var house = Houses[Index];
        house.SetRoom(RoomIndex, Manager.Room);
    }

    private bool Changing;

    private void LB_Items_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (LB_Items.SelectedIndex < 0 || Changing)
            return;
        SaveRoom();
        var house = Houses[Index = LB_Items.SelectedIndex];
        PG_Item.SelectedObject = house;

        ChangeRoom(house);
    }

    private void PG_Item_PropertyValueChanged(object? sender, EventArgs e)
    {
        var text = MiscDumpHelper.GetHouseSummary(Players, Houses[Index], Index);
        if (Items[Index] == text)
            return;

        // Replacing the text deselects the row in Avalonia: select it again, without reloading the house being edited.
        Changing = true;
        Items[Index] = text;
        LB_Items.SelectedIndex = Index;
        Changing = false;
    }

    private async void B_DumpHouse_Click(object? sender, RoutedEventArgs e)
    {
        await MiscDumpHelper.DumpHouse(this, Players, Houses, Index, UiUtil.ModifierKeys == KeyModifiers.Shift);
    }

    private async void B_LoadHouse_Click(object? sender, RoutedEventArgs e)
    {
        if (!await MiscDumpHelper.LoadHouse(this, SAV.Offsets, Players, Houses, Index))
            return;

        RoomIndex = -1;
        var house = Houses[Index];
        PG_Item.SelectedObject = house;
        ChangeRoom(house);
    }

    private async void B_EditFlags_Click(object? sender, RoutedEventArgs e)
    {
        var edit = Houses[Index].EventFlags.ToArray();
        var editor = new PlayerHouseFlagEditor(edit);
        if (await editor.ShowDialog<bool>(this))
            edit.CopyTo(Houses[Index].EventFlags);
    }

    private int HoverX;
    private int HoverY;

    private void PB_Room_MouseMove(object? sender, PointerEventArgs e)
    {
        var l = Current;
        var oldTile = l.GetTile(HoverX, HoverY);
        var tile = GetTile(l, e, out var x, out var y);
        if (ReferenceEquals(tile, oldTile))
            return;
        var str = GameInfo.Strings;
        var name = str.GetItemName(tile);
        ToolTip.SetTip(PB_Room, name);
        SetCoordinateText(x, y, name);
    }

    private void SetCoordinateText(int x, int y, string name) => L_Coordinates.Text = $"({x:000},{y:000}) = {name}";

    private Item GetTile(LayerItem layer, PointerEventArgs e, out int x, out int y)
    {
        SetHoveredItem(e);
        return layer.GetTile(x = HoverX, y = HoverY);
    }

    private void SetHoveredItem(PointerEventArgs e)
    {
        GetCoordinates(e, out HoverX, out HoverY);

        // Mouse event may fire with a slightly too large x/y; clamp just in case.
        Manager.Layers[0].TileInfo.ClampInside(ref HoverX, ref HoverY);
    }

    private void GetCoordinates(PointerEventArgs e, out int x, out int y)
    {
        var p = e.GetPosition(PB_Room);
        x = (int)p.X / scale;
        y = (int)p.Y / scale;
    }

    private void ChangeRoom(IPlayerHouse house)
    {
        RoomIndex = (int)(NUD_Room.Value ?? 1) - 1;
        ReloadManager(house);
        DrawLayer();
    }

    private void ReloadManager(IPlayerHouse house)
    {
        var unsupported = Manager.GetUnsupportedTiles();
        if (unsupported.Count != 0)
            _ = Dialogs.Alert(MessageStrings.MsgFieldItemUnsupportedLayer2Tile);
        var room = house.GetRoom(RoomIndex);
        Manager = new RoomManager(room);
    }

    private void DrawLayer() => DrawRoom(Current);

    private void DrawRoom(LayerItem layer)
    {
        var w = layer.TileInfo.TotalWidth;
        var h = layer.TileInfo.TotalHeight;
        Span<int> scale1 = stackalloc int[w * h];
        var scaleX = new int[scale * scale * scale1.Length];
        var bmp = ImageUtil.Create(scale * w, scale * h);

        // 10x10 items (2x2 tiles per item)
        var cfg = LayerPositionConfig.Create(1, 1, 20, 1, 0, 0);
        ItemLayerSprite.LoadViewport(bmp, layer, cfg, 0, 0, scale1, scaleX, scale, gridlineColor: 0x7F000000);
        PB_Room.Source = bmp;
    }

    private void NUD_Room_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        if (Index < 0)
            return;
        // WinForms switched rooms without keeping the changes made to the previous one.
        SaveRoom();
        ChangeRoom(Houses[Index]);
    }

    private void NUD_Layer_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        if (Index < 0)
            return;
        DrawLayer();
    }

    #region Item Editing

    private void PlayerHouseEditor_Click(object? sender, PointerReleasedEventArgs e)
    {
        if (e.InitialPressMouseButton != MouseButton.Left)
            return;
        var tile = GetTile(Current, e, out var x, out var y);
        OmniTile(tile, x, y, e.KeyModifiers);
    }

    private void OmniTile(Item tile, int x, int y, KeyModifiers modifiers)
    {
        switch (modifiers)
        {
            default:
                ViewTile(tile, x, y);
                return;
            case KeyModifiers.Shift:
                SetTile(tile, x, y);
                return;
            case KeyModifiers.Alt:
                DeleteTile(tile, x, y);
                return;
        }
    }

    private void Menu_View_Click(object? sender, RoutedEventArgs e)
    {
        var x = HoverX;
        var y = HoverY;

        var tile = Current.GetTile(x, y);
        ViewTile(tile, x, y);
    }

    private void Menu_Set_Click(object? sender, RoutedEventArgs e)
    {
        var x = HoverX;
        var y = HoverY;

        var tile = Current.GetTile(x, y);
        SetTile(tile, x, y);
    }

    private void Menu_Reset_Click(object? sender, RoutedEventArgs e)
    {
        var x = HoverX;
        var y = HoverY;

        var tile = Current.GetTile(x, y);
        DeleteTile(tile, x, y);
    }

    private LayerItem Current => Manager.Layers[(int)(NUD_Layer.Value ?? 1) - 1];

    private void ViewTile(Item tile, int x, int y)
    {
        if (CHK_RedirectExtensionLoad.IsChecked == true && tile.IsExtension)
        {
            var l = Current;
            var rx = Math.Max(0, Math.Min(l.TileInfo.TotalWidth - 1, x - tile.ExtensionX));
            var ry = Math.Max(0, Math.Min(l.TileInfo.TotalHeight - 1, y - tile.ExtensionY));
            var redir = l.GetTile(rx, ry);
            if (redir.IsRoot && redir.ItemId == tile.ExtensionItemId)
                tile = redir;
        }

        ViewTile(tile);
    }

    private void ViewTile(Item tile)
    {
        ItemEdit.LoadItem(tile);
    }

    private void SetTile(Item tile, int x, int y)
    {
        var l = Current;
        var pgt = new Item();
        ItemEdit.SetItem(pgt);
        var permission = l.IsOccupied(pgt, x, y);
        switch (permission)
        {
            case PlacedItemPermission.OutOfBounds:
            case PlacedItemPermission.Collision when CHK_NoOverwrite.IsChecked == true:
                Sound.Asterisk();
                return;
        }

        // Clean up original placed data
        if (tile.IsRoot && CHK_AutoExtension.IsChecked == true)
            l.DeleteExtensionTiles(tile, x, y);

        // Set new placed data
        if (pgt.IsRoot && CHK_AutoExtension.IsChecked == true)
            l.SetExtensionTiles(pgt, x, y);
        tile.CopyFrom(pgt);

        DrawLayer();
    }

    private void DeleteTile(Item tile, int x, int y)
    {
        if (CHK_AutoExtension.IsChecked == true)
        {
            if (!tile.IsRoot)
            {
                x -= tile.ExtensionX;
                y -= tile.ExtensionY;
                tile = Current.GetTile(x, y);
            }
            Current.DeleteExtensionTiles(tile, x, y);
        }

        tile.Delete();
        DrawLayer();
    }

    #endregion

    private async void B_LoadRoom_Click(object? sender, RoutedEventArgs e)
    {
        var room = await MiscDumpHelper.LoadRoom(this, SAV.Offsets, Manager.Room, RoomIndex);
        if (room is null)
            return;

        var house = Houses[Index];
        house.SetRoom(RoomIndex, room);
        ReloadManager(house);
        DrawLayer();
        Sound.Asterisk();
    }

    private async void B_DumpRoom_Click(object? sender, RoutedEventArgs e)
    {
        SaveRoom();
        await MiscDumpHelper.DumpRoom(this, Manager.Room, RoomIndex);
    }
}
