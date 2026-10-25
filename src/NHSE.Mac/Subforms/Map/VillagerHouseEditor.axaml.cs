using System.Collections.ObjectModel;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class VillagerHouseEditor : NhseWindow
{
    private readonly MainSave SAV;
    private readonly IVillagerHouse[] Houses;
    private readonly IReadOnlyList<IVillager> Villagers;
    private readonly ObservableCollection<string> Items = [];

    private int Index;

    public VillagerHouseEditor(IVillagerHouse[] houses, IReadOnlyList<IVillager> villagers, MainSave sav, int index)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);
        SAV = sav;
        Houses = houses;
        Villagers = villagers;

        foreach (var obj in Houses)
            Items.Add(GetHouseSummary(obj));
        LB_Items.ItemsSource = Items;

        var hIndex = Array.FindIndex(houses, z => z.NPC1 == index);
        if (hIndex < 0)
            hIndex = 0;
        LB_Items.SelectedIndex = hIndex;
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e) => Close(true);

    private void LB_Items_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (LB_Items.SelectedIndex < 0 || Changing)
            return;
        PG_Item.SelectedObject = Houses[Index = LB_Items.SelectedIndex];
    }

    private bool Changing;

    private void PG_Item_PropertyValueChanged(object? sender, EventArgs e)
    {
        var text = GetHouseSummary(Houses[Index]);
        if (Items[Index] == text)
            return;

        // Replacing the text deselects the row in Avalonia: select it again, without reloading the grid being edited.
        Changing = true;
        Items[Index] = text;
        LB_Items.SelectedIndex = Index;
        Changing = false;
    }

    private string GetHouseSummary(IVillagerHouse house) => $"{GetVillagerName(house)}'s House";

    private string GetVillagerName(IVillagerHouse house)
    {
        var villagerIndex = house.NPC1;
        var v = (uint)villagerIndex >= Villagers.Count ? "???" : Villagers[villagerIndex].InternalName;
        var name = GameInfo.Strings.GetVillager(v);
        return name;
    }

    private async void B_DumpHouse_Click(object? sender, RoutedEventArgs e)
    {
        if (UiUtil.ModifierKeys == KeyModifiers.Shift)
        {
            var folder = await Pickers.OpenFolder(this);
            if (folder is null || !Directory.Exists(folder))
                return;
            SAV.DumpVillagerHouses(folder);
            return;
        }

        var h = Houses[Index];
        var name = GetVillagerName(h);
        const string filter = "New Horizons Villager House (*.nhvh)|*.nhvh|" +
                              "New Horizons Villager House (*.nhvh2)|*.nhvh2|" +
                              "All files (*.*)|*.*";
        var path = await Pickers.SaveFile(this, filter, $"{name}.{h.Extension}");
        if (path is null)
            return;

        var data = h.Write();
        await File.WriteAllBytesAsync(path, data);
    }

    private async void B_LoadHouse_Click(object? sender, RoutedEventArgs e)
    {
        var h = Houses[Index];
        var name = GetVillagerName(Houses[Index]);
        if (name == "???")
            name = "*";
        const string filter = "New Horizons Villager House (*.nhvh)|*.nhvh|" +
                              "New Horizons Villager House (*.nhvh2)|*.nhvh2|" +
                              "All files (*.*)|*.*";
        var path = await Pickers.OpenFile(this, filter, $"{name}.{h.Extension}");
        if (path is null)
            return;

        var expectLength = SAV.Offsets.VillagerHouseSize;
        var fi = new FileInfo(path);
        if (!VillagerHouseConverter.IsCompatible((int)fi.Length, expectLength))
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expectLength), path);
            return;
        }

        Memory<byte> data = await File.ReadAllBytesAsync(path);
        data = VillagerHouseConverter.GetCompatible(data, expectLength);
        if (data.Length != expectLength)
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expectLength), path);
            return;
        }

        h = SAV.Offsets.ReadVillagerHouse(data);
        var current = Houses[Index];
        h.NPC1 = current.NPC1;
        Houses[Index] = h;
        PG_Item.SelectedObject = h;
    }
}
