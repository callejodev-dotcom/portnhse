using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Friendly villager house: how it looks from outside (roof, walls and door, by name) and what hangs on the door,
/// with the house file and the raw values one step away. Works on a copy of the houses; «Save» writes them back.
/// </summary>
public sealed class VillagerHouseWindow : PlayerDialog
{
    private readonly MainSave Main;
    private readonly IVillagerHouse[] Houses;
    private readonly int Slot;
    private readonly string VillagerName;
    private readonly string Portrait;
    private readonly ContentControl Body = new();

    private IVillagerHouse House => Houses[Slot];

    /// <summary>
    /// Opens the house of the villager in <paramref name="villager"/> (index in the save); false if it has none or it wasn't saved.
    /// </summary>
    public static async Task<bool> Open(HomeContext ctx, int villager)
    {
        var v = ctx.SAV.Main.GetVillagers()[villager];
        var name = GameInfo.Strings.GetVillager(v.InternalName);
        var houses = ctx.SAV.Main.GetVillagerHouses();
        var slot = Array.FindIndex(houses, z => z.NPC1 == villager);
        if (slot < 0)
        {
            await Dialogs.Alert(string.Format(VillagerStrings.HouseNotFound, name));
            return false;
        }
        return await ctx.Edit(new VillagerHouseWindow(ctx.SAV.Main, houses, slot, name, v.InternalName));
    }

    public VillagerHouseWindow(MainSave main, IVillagerHouse[] houses, int slot, string name, string internalName)
        : base(string.Format(VillagerStrings.HouseTitle, name), 660, 640)
    {
        Main = main;
        Houses = houses;
        Slot = slot;
        VillagerName = name;
        Portrait = internalName;
        Build();

        var more = PlayerUi.MenuButton(HomeStrings.More,
            (MapStrings.ExportHouse, () => _ = Export()),
            (MapStrings.ImportHouse, () => _ = Import()));
        SetLayout(Title!, string.Format(VillagerStrings.HouseHint, name), new ScrollViewer { Content = Body }, more);
    }

    protected override void Save() => Main.SetVillagerHouses(Houses);

    private void Build()
    {
        var p = new StackPanel { Spacing = 12 };
        p.Children.Add(Outside());
        p.Children.Add(DoorDecoration());
        p.Children.Add(Advanced());
        Body.Content = p;
    }

    private Control Outside()
    {
        var house = House;
        var fields = new StackPanel { Spacing = 8, VerticalAlignment = VerticalAlignment.Center };
        fields.Children.Add(Text(VillagerStrings.HouseOutside, "cardtitle"));
        // The "order" is what the house turns into the next day: set it too, so the change stays.
        // Villager roofs have no pictures in the game list: they are chosen by name.
        var preview = new HousePreview { HorizontalAlignment = HorizontalAlignment.Left, Margin = new Thickness(80, 4, 0, 4) };
        void ShowPreview() => preview.Set(house.RoofUniqueID.ToString(), house.WallUniqueID.ToString(), house.DoorUniqueID.ToString());
        ShowPreview();
        fields.Children.Add(preview);
        fields.Children.Add(PlayerUi.Labeled(MapStrings.Roof, 80, Picker(house.RoofUniqueID, v => { house.RoofUniqueID = v; house.OrderRoofUniqueID = v; ShowPreview(); })));
        var walls = new HousePartPicker<WallType>(HouseStyleNames.Offered<WallType>("HouseWallN"), house.WallUniqueID) { Width = 300 };
        walls.Changed += v => { house.WallUniqueID = v; house.OrderWallUniqueID = v; ShowPreview(); };
        fields.Children.Add(PlayerUi.Labeled(MapStrings.Walls, 80, walls));
        var door = new DoorPicker(house.DoorUniqueID) { Width = 300 };
        door.Changed += v => { house.DoorUniqueID = v; house.OrderDoorUniqueID = v; ShowPreview(); };
        fields.Children.Add(PlayerUi.Labeled(MapStrings.Door, 80, door));
        ExteriorPictures.EnsureDownloaded();

        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 18 };
        g.Children.Add(new Image { Source = VillagerSprite.GetVillagerSprite(Portrait), Width = 104, Height = 104, VerticalAlignment = VerticalAlignment.Top });
        Grid.SetColumn(fields, 1);
        g.Children.Add(fields);
        return Card(g);
    }

    private static ComboBox Picker<T>(T current, Action<T> set) where T : struct, Enum
    {
        var values = Enum.GetValues<T>().Where(v => HouseStyleNames.IsForVillagers(v) || EqualityComparer<T>.Default.Equals(v, current)).ToList();
        if (!values.Contains(current))
            values.Add(current);
        var cb = new ComboBox { ItemsSource = HouseStyleNames.GetNames(values), SelectedIndex = values.IndexOf(current), Width = 300, MaxDropDownHeight = 400 };
        cb.SelectionChanged += (_, _) =>
        {
            if (cb.SelectedIndex >= 0)
                set(values[cb.SelectedIndex]);
        };
        return cb;
    }

    private Control DoorDecoration()
    {
        var deco = House.DoorDecoItemName;
        var current = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 10 };
        if (deco.IsNone)
        {
            current.Children.Add(PlayerUi.Subtle(VillagerStrings.NoDoorDecoration, 13));
        }
        else
        {
            var picture = new Image { Source = ItemSprite.GetItemSprite(deco), Width = 44, Height = 44 };
            SetItemTip(picture, () => House.DoorDecoItemName);
            current.Children.Add(picture);
            current.Children.Add(new TextBlock { Text = GameInfo.Strings.GetItemName(deco), FontWeight = FontWeight.SemiBold, VerticalAlignment = VerticalAlignment.Center });
            current.Children.Add(PlayerUi.Button(VillagerStrings.RemoveDoorDecoration, () =>
            {
                House.DoorDecoItemName = new Item(Item.NONE);
                Build();
            }));
        }

        var search = new ItemSearchBox(180);
        search.ItemChosen += (id, count) =>
        {
            House.DoorDecoItemName = new Item(id) { Count = count };
            Build();
        };

        var p = new StackPanel { Spacing = 8 };
        p.Children.Add(current);
        p.Children.Add(Text(VillagerStrings.ChangeDoorDecoration, "subtle"));
        p.Children.Add(search);
        return Card(CardHeader("Present", VillagerStrings.DoorDecoration, VillagerStrings.DoorDecorationHint, p));
    }

    private Expander Advanced()
    {
        var panel = new StackPanel { Spacing = 8 };
        var expander = new Expander { Header = HomeStrings.AdvancedOptions, Content = panel, HorizontalAlignment = HorizontalAlignment.Stretch };
        expander.PropertyChanged += (_, e) =>
        {
            if (e.Property != Expander.IsExpandedProperty || !expander.IsExpanded || panel.Children.Count != 0)
                return;
            panel.Children.Add(PlayerUi.Subtle(VillagerStrings.HouseAdvancedHint));
            var grid = new PropertyGrid { SelectedObject = House, PropertySort = PropertySort.NoSort, HelpVisible = false, Height = 320 };
            panel.Children.Add(grid);
        };
        return expander;
    }

    private async Task Export()
    {
        const string filter = "New Horizons Villager House (*.nhvh)|*.nhvh|New Horizons Villager House (*.nhvh2)|*.nhvh2";
        var path = await Pickers.SaveFile(this, filter, $"{VillagerName}.{House.Extension}");
        if (path is not null)
            await File.WriteAllBytesAsync(path, House.Write());
    }

    private async Task Import()
    {
        const string filter = "New Horizons Villager House (*.nhvh, *.nhvh2)|*.nhvh;*.nhvh2|All files (*.*)|*.*";
        var path = await Pickers.OpenFile(this, filter);
        if (path is null)
            return;

        var expect = Main.Offsets.VillagerHouseSize;
        var length = (int)new FileInfo(path).Length;
        if (!VillagerHouseConverter.IsCompatible(length, expect))
        {
            await Dialogs.Error(string.Format(VillagerStrings.HouseWrongFile, Path.GetFileName(path)));
            return;
        }
        Memory<byte> data = await File.ReadAllBytesAsync(path);
        data = VillagerHouseConverter.GetCompatible(data, expect);
        if (data.Length != expect)
        {
            await Dialogs.Error(string.Format(VillagerStrings.HouseWrongFile, Path.GetFileName(path)));
            return;
        }

        // The villager living in it stays the same.
        var h = Main.Offsets.ReadVillagerHouse(data);
        h.NPC1 = House.NPC1;
        Houses[Slot] = h;
        Build();
    }
}
