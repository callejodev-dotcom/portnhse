using System.ComponentModel;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Data;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using NHSE.Core;

namespace NHSE.Mac;

public partial class ItemReceivedEditor : NhseWindow
{
    private readonly Player Player;

    // Rows of the two CheckedListBoxes of WinForms.
    private readonly List<Entry> Items = [];
    private readonly List<Entry> Remake = [];

    public ItemReceivedEditor(Player player)
    {
        Player = player;
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);
        FillCheckBoxes();
        Initialize(GameInfo.Strings.ItemDataSource);
        CLB_Remake.SelectedIndex = 0;
        CLB_Items.SelectedIndex = 0x50; // clackercart
    }

    public void Initialize(List<ComboItem> items)
    {
        CB_Item.SetComboItems(items);
        CB_Item.SelectedIndex = 0; // WinForms selects the first item of a bound data source
    }

    private void FillCheckBoxes()
    {
        var items = GameInfo.Strings.itemlistdisplay;
        FillCollect(items);
        FillRemake(items);
        SetupCheckedList(CLB_Items, Items);
        SetupCheckedList(CLB_Remake, Remake);
    }

    private void FillCollect(ReadOnlySpan<string> items)
    {
        var ofs = Player.Personal.Offsets.ItemCollectBit;
        var data = Player.Personal.Data;
        for (int i = 0; i < items.Length; i++)
        {
            var flag = FlagUtil.GetFlag(data, ofs, i);
            string value = items[i];
            string name = $"0x{i:X3} - {value}";
            Items.Add(new Entry(name, flag));
        }
    }

    private void FillRemake(ReadOnlySpan<string> items)
    {
        var str = GameInfo.Strings;
        var invert = ItemRemakeUtil.GetInvertedDictionary();
        var ofs = Player.Personal.Offsets.ItemRemakeCollectBit;
        var max = Player.Personal.Offsets.MaxRemakeBitFlag;
        var data = Player.Personal.Data;
        for (int i = 0; i < max; i++)
        {
            var remakeIndex = i >> 3; // ItemRemakeInfo.BodyColorCountMax
            var variant = i & 7;

            ushort itemId = invert.GetValueOrDefault((short)remakeIndex, (ushort)0);
            var itemName = remakeIndex == 0652 ? "photo" : items[itemId];

            var flag = FlagUtil.GetFlag(data, ofs, i);
            string name = $"{remakeIndex:0000} V{variant:0} - {itemName}";

            if (ItemRemakeInfoData.List.TryGetValue((short)remakeIndex, out var info))
                name = $"{name} ({info.GetBodyDescription(variant, str)})";

            Remake.Add(new Entry(name, flag));
        }
    }

    public void GiveAll(ReadOnlySpan<ushort> indexes, bool value = true)
    {
        foreach (var item in indexes)
            GiveItem(item, value, CB_VariantBodiesOnly.IsChecked == true);
        Sound.Asterisk();
    }

    private void GiveEverything(ReadOnlySpan<string> items, bool value = true)
    {
        if (!value)
        {
            for (ushort i = 0; i < Items.Count; i++)
                GiveItem(i, false, CB_VariantBodiesOnly.IsChecked == true);
            return;
        }

        var skip = GameLists.NoCheckReceived;
        for (ushort i = 1; i < Items.Count; i++)
        {
            if (string.IsNullOrEmpty(items[i]))
                continue;
            if (skip.Contains(i))
                continue;
            GiveItem(i, remakeOnly: CB_VariantBodiesOnly.IsChecked == true);
        }
        Sound.Asterisk();
    }

    private void GiveAllFurniture(ReadOnlySpan<string> items, bool value = true)
    {
        var skip = new List<ushort>();
        skip.AddRange(GameLists.NoCheckReceived);
        skip.AddRange(GameLists.Bugs);
        skip.AddRange(GameLists.Fish);
        skip.AddRange(GameLists.Art);
        skip.AddRange(GameLists.Dive);

        skip = skip.Distinct().ToList();

        for (ushort i = 1; i < Items.Count; i++)
        {
            if (string.IsNullOrEmpty(items[i]))
                continue;
            if (skip.Contains(i))
                continue;
            GiveItem(i, value, CB_VariantBodiesOnly.IsChecked == true);
        }
        Sound.Asterisk();
    }

    private void GiveItem(ushort item, bool value = true, bool remakeOnly = false)
    {
        if (!remakeOnly)
            Items[item].IsChecked = value;

        var remakeIndex = ItemRemakeUtil.GetRemakeIndex(item);
        if (!ItemRemakeInfoData.List.TryGetValue(remakeIndex, out var info))
            return;

        for (int i = 0; i < ItemRemakeInfo.BodyColorCountMax; i++)
        {
            if (!info.HasBodyColor(i))
                continue;
            int rIndex = (remakeIndex * ItemRemakeInfo.BodyColorCountMax) + i;
            Remake[rIndex].IsChecked = value;
        }
    }

    // B_GiveAll opens its menu (CM_Buttons in WinForms) by itself.
    private static bool NotAlt => UiUtil.ModifierKeys != KeyModifiers.Alt;
    private void B_AllBugs_Click(object? sender, RoutedEventArgs e) => GiveAll(GameLists.Bugs, NotAlt);
    private void B_AllFish_Click(object? sender, RoutedEventArgs e) => GiveAll(GameLists.Fish, NotAlt);
    private void B_AllArt_Click(object? sender, RoutedEventArgs e) => GiveAll(GameLists.Art, NotAlt);
    private void B_AllDive_Click(object? sender, RoutedEventArgs e) => GiveAll(GameLists.Dive, NotAlt);
    private void B_AllFurniture_Click(object? sender, RoutedEventArgs e) => GiveAllFurniture(GameInfo.Strings.itemlist, NotAlt);
    private void B_GiveEverything_Click(object? sender, RoutedEventArgs e) => GiveEverything(GameInfo.Strings.itemlist, NotAlt);
    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        SetCollect();
        SetRemake();
        Close(true);
    }

    private void SetCollect()
    {
        var ofs = Player.Personal.Offsets.ItemCollectBit;
        var data = Player.Personal.Data;
        for (int i = 0; i < Items.Count; i++)
            FlagUtil.SetFlag(data, ofs, i, Items[i].IsChecked);
    }

    private void SetRemake()
    {
        var ofs = Player.Personal.Offsets.ItemRemakeCollectBit;
        var data = Player.Personal.Data;
        for (int i = 0; i < Remake.Count; i++)
            FlagUtil.SetFlag(data, ofs, i, Remake[i].IsChecked);
    }

    private void CB_Item_SelectedValueChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (CB_Item.SelectedItem is null)
            return; // typing a name that matches nothing yet

        var index = UiUtil.GetIndex(CB_Item);
        if ((uint)index >= Items.Count)
            index = 0;
        CLB_Items.SelectedIndex = index;

        var remake = ItemRemakeUtil.GetRemakeIndex((ushort)index);
        if (remake > 0)
            CLB_Remake.SelectedIndex = remake * ItemRemakeInfo.BodyColorCountMax;
    }

    private void CLB_Items_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        var index = CLB_Items.SelectedIndex;
        CB_Item.SelectedValue = index;
    }

    /// <summary>
    /// Shows a list of <see cref="Entry"/> like a WinForms CheckedListBox: a box to tick, and Space toggles the selected row.
    /// </summary>
    private static void SetupCheckedList(ListBox lb, List<Entry> entries)
    {
        lb.ItemTemplate = CheckedTemplate;
        lb.ItemsSource = entries;
        lb.KeyDown += (_, e) =>
        {
            if (e.Key != Key.Space || lb.SelectedItem is not Entry entry)
                return;
            entry.IsChecked = !entry.IsChecked;
            e.Handled = true;
        };
    }

    private static readonly FuncDataTemplate<Entry> CheckedTemplate = new((_, _) =>
    {
        var chk = new CheckBox
        {
            MinWidth = 0,
            MinHeight = 0,
            Padding = new Thickness(0),
            Margin = new Thickness(0, 0, 6, 0),
            VerticalAlignment = VerticalAlignment.Center,
        };
        chk.Bind(ToggleButton.IsCheckedProperty, new Binding(nameof(Entry.IsChecked)) { Mode = BindingMode.TwoWay });
        var text = new TextBlock { VerticalAlignment = VerticalAlignment.Center };
        text.Bind(TextBlock.TextProperty, new Binding(nameof(Entry.Text)));
        return new StackPanel { Orientation = Orientation.Horizontal, Children = { chk, text } };
    }, true);

    /// <summary>
    /// One row of a checked list.
    /// </summary>
    public sealed class Entry(string text, bool isChecked) : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string Text { get; } = text;

        public bool IsChecked
        {
            get;
            set
            {
                if (field == value)
                    return;
                field = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsChecked)));
            }
        } = isChecked;
    }
}
