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
/// Every DIY recipe with the picture and name of what it makes: search, show learned or missing ones,
/// tick «Learned» (and «Made», «Favorite»), or learn / forget them all. Saved to the player on «Save».
/// </summary>
public sealed class RecipesWindow : PlayerDialog
{
    private enum ListFilter { All, Learned, Missing }

    private readonly Personal Pers;
    private readonly RecipeBook Book;
    private readonly RecipeRow[] All;
    private readonly ListBox List;
    private readonly TextBox SearchText;
    private readonly TextBlock Summary = new() { Classes = { "amount" } };
    private readonly TextBlock NothingFound = PlayerUi.Subtle(PlayerStrings.NothingFound, 14);
    private ListFilter Filter;
    private string[] Words = [];

    public RecipesWindow(Personal pers) : base(HomeStrings.Recipes, 840, 700)
    {
        Pers = pers;
        Book = pers.GetRecipeBook();
        var names = GameInfo.Strings.itemlist;
        All = [.. RecipeList.Recipes
            .Select(z => new RecipeRow(Book, z.Key, z.Value, GetName(names, z.Value)))
            .OrderBy(z => PlayerUi.SortKey(z.Key), StringComparer.Ordinal)];
        foreach (var row in All)
            row.KnownChanged += UpdateSummary;

        List = new ListBox
        {
            ItemTemplate = new FuncDataTemplate<RecipeRow>((r, _) => RowView(r)),
            Background = Brushes.Transparent,
        };

        var learnAll = PlayerUi.Button(HomeStrings.LearnAll, LearnAll, accent: true);
        var forgetAll = PlayerUi.Button(PlayerStrings.ForgetAll, () => _ = ForgetAll());
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, VerticalAlignment = VerticalAlignment.Center };
        buttons.Children.Add(learnAll);
        buttons.Children.Add(forgetAll);

        var top = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto"), RowDefinitions = new RowDefinitions("Auto,Auto"), RowSpacing = 12 };
        top.Children.Add(Summary);
        Grid.SetColumn(buttons, 1);
        top.Children.Add(buttons);

        var search = SearchText = PlayerUi.SearchBox(PlayerStrings.SearchRecipe, w =>
        {
            Words = w;
            Refilter();
        });
        var chips = PlayerUi.Chips([PlayerStrings.ShowAll, PlayerStrings.ShowLearned, PlayerStrings.ShowMissing], 0, i =>
        {
            Filter = (ListFilter)i;
            Refilter();
        });
        chips.Margin = new Thickness(12, 0, 0, 0);
        var filters = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto") };
        filters.Children.Add(search);
        Grid.SetColumn(chips, 1);
        filters.Children.Add(chips);
        Grid.SetRow(filters, 1);
        Grid.SetColumnSpan(filters, 2);
        top.Children.Add(filters);

        NothingFound.HorizontalAlignment = HorizontalAlignment.Center;
        NothingFound.VerticalAlignment = VerticalAlignment.Center;
        var listArea = new Grid();
        listArea.Children.Add(List);
        listArea.Children.Add(NothingFound);
        var listCard = new Border { Child = listArea, Classes = { "card" }, Padding = new Thickness(6), Margin = new Thickness(0, 12, 0, 0) };

        var body = new DockPanel();
        var topCard = HomeUi.Card(top);
        DockPanel.SetDock(topCard, Dock.Top);
        body.Children.Add(topCard);
        body.Children.Add(listCard);

        var more = PlayerUi.MenuButton(HomeStrings.More, (PlayerStrings.MarkLearnedAsMade, MarkAllMade));
        SetLayout(HomeStrings.Recipes, PlayerStrings.RecipesHint, body, more);

        UpdateSummary();
        Refilter();
        Opened += (_, _) => search.Focus();
    }

    private static string GetName(string[] names, ushort item)
    {
        var name = item < names.Length ? names[item] : string.Empty;
        return string.IsNullOrWhiteSpace(name) ? $"#{item}" : name;
    }

    /// <summary>
    /// Learned and total recipes, without the two one-time story recipes (bridge and campsite kits),
    /// which «Learn all» leaves alone as the game gives them during the story.
    /// </summary>
    public static (int Known, int Total) Count(RecipeBook book)
    {
        int known = 0, total = 0;
        foreach (var index in RecipeList.Recipes.Keys)
        {
            if (RecipeRow.IsStoryRecipe(index))
                continue;
            total++;
            if (book.GetIsKnown(index))
                known++;
        }
        return (known, total);
    }

    private void UpdateSummary()
    {
        var (known, total) = Count(Book);
        Summary.Text = string.Format(HomeStrings.RecipesHint, known.ToString("N0"), total.ToString("N0"));
    }

    private void Refilter()
    {
        IEnumerable<RecipeRow> rows = All;
        rows = Filter switch
        {
            ListFilter.Learned => rows.Where(z => z.Known),
            ListFilter.Missing => rows.Where(z => !z.Known),
            _ => rows,
        };
        if (Words.Length != 0)
            rows = rows.Where(z => PlayerUi.Matches(z.Key, Words));
        var list = rows.ToArray();
        List.ItemsSource = list;
        NothingFound.IsVisible = list.Length == 0;
    }

    private void RefreshAll()
    {
        foreach (var row in All)
            row.NotifyAll();
        UpdateSummary();
    }

    internal void LearnAll()
    {
        Book.GiveAll(RecipeList.Recipes);
        RefreshAll();
        Sound.Asterisk();
    }

    private async Task ForgetAll()
    {
        if (await Dialogs.Prompt(MessageBoxButtons.YesNo, PlayerStrings.ForgetAllAsk) != DialogResult.Yes)
            return;
        Book.ClearAll();
        RefreshAll();
    }

    private void MarkAllMade()
    {
        Book.CraftAll();
        RefreshAll();
        Sound.Asterisk();
    }

    /// <summary>
    /// Rows shown with the current search and filter (used by the tests).
    /// </summary>
    internal IReadOnlyList<RecipeRow> Shown => (IReadOnlyList<RecipeRow>?)List.ItemsSource ?? [];

    /// <summary>
    /// Types in the search box (used by the tests).
    /// </summary>
    internal void Search(string text)
    {
        SearchText.Text = text;
        Words = PlayerUi.Words(text);
        Refilter();
    }

    protected override void Save() => Pers.SetRecipeBook(Book);

    private static readonly FuncValueConverter<bool, double> Faded = new(known => known ? 1 : 0.5);

    private static Control RowView(RecipeRow? r)
    {
        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*,130,120,110"), ColumnSpacing = 10, Margin = new Thickness(2, 1) };
        if (r is null)
            return g;

        var picture = new Image { Width = 34, Height = 34, VerticalAlignment = VerticalAlignment.Center };
        picture.Bind(Image.SourceProperty, new Binding(nameof(RecipeRow.Picture)));
        picture.Bind(OpacityProperty, new Binding(nameof(RecipeRow.Known)) { Converter = Faded });
        g.Children.Add(picture);

        var text = new StackPanel { VerticalAlignment = VerticalAlignment.Center };
        var name = new TextBlock { Text = r.Name, FontSize = 14, TextTrimming = TextTrimming.CharacterEllipsis };
        text.Children.Add(name);
        if (r.IsStory)
            text.Children.Add(PlayerUi.Subtle(PlayerStrings.StoryRecipe, 11));
        Grid.SetColumn(text, 1);
        g.Children.Add(text);

        var known = new CheckBox { Content = PlayerStrings.Learned, VerticalAlignment = VerticalAlignment.Center };
        known.Bind(ToggleButton.IsCheckedProperty, new Binding(nameof(RecipeRow.Known)) { Mode = BindingMode.TwoWay });
        Grid.SetColumn(known, 2);
        g.Children.Add(known);

        var made = new CheckBox { Content = PlayerStrings.Made, VerticalAlignment = VerticalAlignment.Center };
        made.Bind(ToggleButton.IsCheckedProperty, new Binding(nameof(RecipeRow.Made)) { Mode = BindingMode.TwoWay });
        made.Bind(IsVisibleProperty, new Binding(nameof(RecipeRow.Known)));
        ToolTip.SetTip(made, PlayerStrings.MadeHint);
        Grid.SetColumn(made, 3);
        g.Children.Add(made);

        var favorite = new CheckBox { Content = PlayerStrings.Favorite, VerticalAlignment = VerticalAlignment.Center };
        favorite.Bind(ToggleButton.IsCheckedProperty, new Binding(nameof(RecipeRow.Favorite)) { Mode = BindingMode.TwoWay });
        favorite.Bind(IsVisibleProperty, new Binding(nameof(RecipeRow.Known)));
        Grid.SetColumn(favorite, 4);
        g.Children.Add(favorite);
        return g;
    }
}

/// <summary>
/// One recipe of the list, read from and written to the recipe book directly.
/// </summary>
public sealed class RecipeRow(RecipeBook book, ushort index, ushort itemId, string name) : RowModel
{
    public ushort Index { get; } = index;
    public ushort ItemId { get; } = itemId;
    public string Name { get; } = name;
    public string Key { get; } = ItemSearchBox.Normalize(name);

    /// <summary>
    /// Raised when the recipe is learned or forgotten.
    /// </summary>
    public event Action? KnownChanged;

    public static bool IsStoryRecipe(int index) => index is RecipeList.BridgeConstructionKit or RecipeList.CampsiteConstructionKit;

    public bool IsStory => IsStoryRecipe(Index);

    public Bitmap? Picture => ItemSprite.GetItemSprite(ItemId);

    public bool Known
    {
        get => book.GetIsKnown(Index);
        set
        {
            if (value == Known)
                return;
            book.SetIsKnown(Index, value);
            book.SetIsNew(Index, value); // shown as new in the phone, as when the game teaches it
            if (!value)
            {
                book.SetIsMade(Index, false);
                book.SetIsFavorite(Index, false);
            }
            NotifyAll();
            KnownChanged?.Invoke();
        }
    }

    public bool Made
    {
        get => book.GetIsMade(Index);
        set
        {
            book.SetIsMade(Index, value);
            Notify();
        }
    }

    public bool Favorite
    {
        get => book.GetIsFavorite(Index);
        set
        {
            book.SetIsFavorite(Index, value);
            Notify();
        }
    }
}
