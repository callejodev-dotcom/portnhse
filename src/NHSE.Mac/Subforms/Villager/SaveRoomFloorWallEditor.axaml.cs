using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class SaveRoomFloorWallEditor : NhseWindow
{
    public GSaveRoomFloorWall Entity { get; private set; }

    public SaveRoomFloorWallEditor(GSaveRoomFloorWall v)
    {
        Entity = v;
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);

        var ent = Entity;
        NUD_DesignAccent.Value = ent.AccentWallMyDesignID;
        NUD_DesignWall.Value = ent.WallMyDesignID;
        NUD_DesignFloor.Value = ent.FloorMyDesignID;

        NUD_DirAccent.Value = ent.AccentWallDirection;
        NUD_InfoBit.Value = ent.InfoBit;
        NUD_DirFloor.Value = ent.FloorDirection;
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        var ent = Entity;

        ent.AccentWallMyDesignID = (ushort)(NUD_DesignAccent.Value ?? 0);
        ent.WallMyDesignID = (ushort)(NUD_DesignWall.Value ?? 0);
        ent.FloorMyDesignID = (ushort)(NUD_DesignFloor.Value ?? 0);

        ent.AccentWallDirection = (byte)(NUD_DirAccent.Value ?? 0);
        ent.InfoBit = (byte)(NUD_InfoBit.Value ?? 0);
        ent.FloorDirection = (byte)(NUD_DirFloor.Value ?? 0);

        Entity = ent;

        Close(true);
    }

    private async void B_AccentWall_Click(object? sender, RoutedEventArgs e)
    {
        var ent = Entity;
        var accent = ent.AccentWallItem.ToBytes().ToClass<Item>();
        var editor = new SingleItemEditor(accent);
        if (await editor.ShowDialog<bool>(this))
            ent.AccentWallItem = editor.Item.ToBytesClass().ToStructure<GSaveItemName>();
        Entity = ent;
    }

    private async void B_Wall_Click(object? sender, RoutedEventArgs e)
    {
        var ent = Entity;
        var accent = ent.WallItem.ToBytes().ToClass<Item>();
        var editor = new SingleItemEditor(accent);
        if (await editor.ShowDialog<bool>(this))
            ent.WallItem = editor.Item.ToBytesClass().ToStructure<GSaveItemName>();
        Entity = ent;
    }

    private async void B_Floor_Click(object? sender, RoutedEventArgs e)
    {
        var ent = Entity;
        var accent = ent.FloorItem.ToBytes().ToClass<Item>();
        var editor = new SingleItemEditor(accent);
        if (await editor.ShowDialog<bool>(this))
            ent.FloorItem = editor.Item.ToBytesClass().ToStructure<GSaveItemName>();
        Entity = ent;
    }
}
