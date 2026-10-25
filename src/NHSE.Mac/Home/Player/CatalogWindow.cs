using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Data;
using Avalonia.Data.Converters;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// The catalog of received items (what the player can order from Nook Shopping): every item with its picture,
/// by category, with a search box and a «Received» tick. Marking an item also marks all its colours.
/// The flags are edited on a copy and written to the player on «Save».
/// </summary>
public sealed class CatalogWindow : PlayerDialog
{
    private static readonly ItemCategory[] Categories = [.. Enum.GetValues<ItemCategory>().Where(z => z != ItemCategory.Recipes)];

    private readonly Personal Pers;
    private readonly byte[] Received;
    private readonly byte[] Variants;
    private readonly CatalogRow[] All;
    private readonly ListBox List;
    private readonly TextBox SearchText;
    private readonly TextBlock Summary = new() { Classes = { "cardtitle" }, VerticalAlignment = VerticalAlignment.Center };
    private readonly TextBlock NothingFound = PlayerUi.Subtle(PlayerStrings.NothingFound, 14);
    private readonly WrapPanel CategoryChips;
    private ItemCategory Category = ItemCategory.All;
    private string[] Words = [];

    public CatalogWindow(Player player) : base(HomeStrings.Catalog, 820, 720)
    {
        Pers = player.Personal;
        var ofs = Pers.Offsets;
        var data = Pers.Data;
        // The received flags end where the colour flags start.
        Received = data.Slice(ofs.ItemCollectBit, ofs.ItemRemakeCollectBit - ofs.ItemCollectBit).ToArray();
        Variants = data.Slice(ofs.ItemRemakeCollectBit, (ofs.MaxRemakeBitFlag + 7) / 8).ToArray();

        var max = Received.Length * 8;
        var skip = GameLists.NoCheckReceived.ToArray();
        All = [.. GameInfo.Strings.ItemDataSource
            .Where(z => z.Value is > 0 and < Item.NONE && z.Value < max && IsRealName(z.Text) && Array.IndexOf(skip, (ushort)z.Value) < 0)
            .Select(z => new CatalogRow(this, (ushort)z.Value, z.Text, ItemSearchBox.GetCategory((ushort)z.Value)))
            .Where(z => z.Category != ItemCategory.Recipes)
            .OrderBy(z => PlayerUi.SortKey(z.Key), StringComparer.Ordinal)];

        List = new ListBox
        {
            ItemTemplate = new FuncDataTemplate<CatalogRow>((r, _) => RowView(r, Category == ItemCategory.All)),
            Background = Brushes.Transparent,
        };

        var search = SearchText = PlayerUi.SearchBox(HomeStrings.SearchItem, w =>
        {
            Words = w;
            Refilter();
        });
        var chips = CategoryChips = PlayerUi.Chips([.. Categories.Select(ItemSearchBox.GetName)], 0, i =>
        {
            Category = Categories[i];
            Refilter();
        });
        chips.Margin = new Thickness(0, 0, 0, 8);

        var markAll = PlayerUi.Button(PlayerStrings.MarkShown, () => MarkShown(true), accent: true);
        var unmarkAll = PlayerUi.Button(PlayerStrings.UnmarkShown, () => _ = UnmarkShown());
        var actions = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        actions.Children.Add(markAll);
        actions.Children.Add(unmarkAll);
        var summaryRow = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto"), Margin = new Thickness(0, 10, 0, 0) };
        summaryRow.Children.Add(Summary);
        Grid.SetColumn(actions, 1);
        summaryRow.Children.Add(actions);

        var top = new StackPanel();
        top.Children.Add(chips);
        top.Children.Add(search);
        top.Children.Add(summaryRow);
        var topCard = HomeUi.Card(top);

        NothingFound.HorizontalAlignment = HorizontalAlignment.Center;
        NothingFound.VerticalAlignment = VerticalAlignment.Center;
        var listArea = new Grid();
        listArea.Children.Add(List);
        listArea.Children.Add(NothingFound);
        var listCard = new Border { Child = listArea, Classes = { "card" }, Padding = new Thickness(6), Margin = new Thickness(0, 12, 0, 0) };

        var body = new DockPanel();
        DockPanel.SetDock(topCard, Dock.Top);
        body.Children.Add(topCard);
        body.Children.Add(listCard);

        SetLayout(HomeStrings.Catalog, PlayerStrings.CatalogHint, body);
        Refilter();
        Opened += (_, _) => search.Focus();
    }

    private static bool IsRealName(string text)
    {
        if (string.IsNullOrWhiteSpace(text) || text.StartsWith("(Item #", StringComparison.Ordinal))
            return false;
        return text.Any(char.IsLetterOrDigit);
    }

    internal bool IsReceived(ushort id) => FlagUtil.GetFlag(Received, 0, id);

    /// <summary>
    /// Marks the item, and every colour of it, as received (or not), as the classic editor does.
    /// </summary>
    internal void SetReceived(ushort id, bool value)
    {
        FlagUtil.SetFlag(Received, 0, id, value);
        var remake = ItemRemakeUtil.GetRemakeIndex(id);
        if (!ItemRemakeInfoData.List.TryGetValue(remake, out var info))
            return;
        for (int i = 0; i < ItemRemakeInfo.BodyColorCountMax; i++)
        {
            if (!info.HasBodyColor(i))
                continue;
            var bit = (remake * ItemRemakeInfo.BodyColorCountMax) + i;
            if (bit < Variants.Length * 8)
                FlagUtil.SetFlag(Variants, 0, bit, value);
        }
    }

    internal void UpdateSummary()
    {
        var shown = Shown;
        Summary.Text = string.Format(PlayerStrings.CatalogCount, shown.Count(z => z.Received).ToString("N0"), shown.Count.ToString("N0"));
    }

    /// <summary>
    /// Rows shown with the current category and search.
    /// </summary>
    internal IReadOnlyList<CatalogRow> Shown => (IReadOnlyList<CatalogRow>?)List.ItemsSource ?? [];

    internal void Search(string text)
    {
        SearchText.Text = text;
        Words = PlayerUi.Words(text);
        Refilter();
    }

    internal void ShowCategory(ItemCategory c)
    {
        Category = c;
        PlayerUi.SelectChip(CategoryChips, Array.IndexOf(Categories, c));
        Refilter();
    }

    private void Refilter()
    {
        IEnumerable<CatalogRow> rows = All;
        if (Category != ItemCategory.All)
            rows = rows.Where(z => z.Category == Category);
        if (Words.Length != 0)
        {
            rows = rows.Where(z => PlayerUi.Matches(z.Key, Words))
                .OrderBy(z => z.Key.StartsWith(Words[0], StringComparison.Ordinal) ? 0 : 1);
        }
        var list = rows.ToArray();
        List.ItemsSource = list;
        NothingFound.IsVisible = list.Length == 0;
        UpdateSummary();
    }

    internal void MarkShown(bool value)
    {
        foreach (var row in Shown)
        {
            SetReceived(row.Id, value);
            row.NotifyAll();
        }
        UpdateSummary();
        if (value)
            Sound.Asterisk();
    }

    private async Task UnmarkShown()
    {
        if (await Dialogs.Prompt(MessageBoxButtons.YesNo, string.Format(PlayerStrings.UnmarkShownAsk, Shown.Count.ToString("N0"))) != DialogResult.Yes)
            return;
        MarkShown(false);
    }

    protected override void Save()
    {
        var ofs = Pers.Offsets;
        var data = Pers.Data;
        Received.CopyTo(data[ofs.ItemCollectBit..]);
        Variants.CopyTo(data[ofs.ItemRemakeCollectBit..]);
    }

    private static readonly FuncValueConverter<bool, double> Faded = new(received => received ? 1 : 0.55);

    /// <param name="showCategory">Shows the category next to the name (only useful when every category is listed).</param>
    private static Control RowView(CatalogRow? r, bool showCategory)
    {
        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto,120"), ColumnSpacing = 10, Margin = new Thickness(2, 1) };
        if (r is null)
            return g;
        var picture = new Image { Width = 34, Height = 34, VerticalAlignment = VerticalAlignment.Center };
        picture.Bind(Image.SourceProperty, new Binding(nameof(CatalogRow.Picture)));
        picture.Bind(OpacityProperty, new Binding(nameof(CatalogRow.Received)) { Converter = Faded });
        HomeUi.SetItemTip(picture, () => picture.DataContext is CatalogRow row ? new Item(row.Id) : null);
        g.Children.Add(picture);

        var name = new TextBlock { Text = r.Name, FontSize = 14, VerticalAlignment = VerticalAlignment.Center, TextTrimming = TextTrimming.CharacterEllipsis };
        Grid.SetColumn(name, 1);
        g.Children.Add(name);

        var category = PlayerUi.Subtle(showCategory ? ItemSearchBox.GetName(r.Category) : string.Empty, 12);
        category.VerticalAlignment = VerticalAlignment.Center;
        Grid.SetColumn(category, 2);
        g.Children.Add(category);

        var box = new CheckBox { Content = PlayerStrings.Received, VerticalAlignment = VerticalAlignment.Center };
        box.Bind(ToggleButton.IsCheckedProperty, new Binding(nameof(CatalogRow.Received)) { Mode = BindingMode.TwoWay });
        Grid.SetColumn(box, 3);
        g.Children.Add(box);
        return g;
    }
}

/// <summary>
/// One item of the catalog.
/// </summary>
public sealed class CatalogRow(CatalogWindow owner, ushort id, string name, ItemCategory category) : RowModel
{
    public ushort Id { get; } = id;
    public string Name { get; } = name;
    public string Key { get; } = ItemSearchBox.Normalize(name);
    public ItemCategory Category { get; } = category;

    public Bitmap? Picture => ItemSprite.GetItemSprite(Id);

    public bool Received
    {
        get => owner.IsReceived(Id);
        set
        {
            if (value == Received)
                return;
            owner.SetReceived(Id, value);
            Notify();
            owner.UpdateSummary();
        }
    }
}
