using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Presenters;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Styling;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Friendly museum editor: one tab per wing (fish, bugs, sea creatures, fossils, art) listing every piece with
/// its picture and a tick for "donated", with the date and the player who donated it, plus one-click donations.
/// The game keeps the donations as a list in donation order; removing one moves the later ones up.
/// The museum is edited in place; the caller keeps it only if the window returns true.
/// </summary>
public sealed class MuseumWindow : NhseWindow
{
    public enum Wing { Fish, Bugs, Sea, Fossils, Art }

    internal sealed record Piece(ushort Id, string Name, string Key, Wing Wing);

    private enum Filter { All, Missing, Donated }

    private sealed class WingTab(Wing wing, Piece[] pieces)
    {
        public Wing Wing { get; } = wing;
        public Piece[] Pieces { get; } = pieces;
        public TextBlock HeaderCount { get; } = new() { Classes = { "subtle" }, FontSize = 12, VerticalAlignment = VerticalAlignment.Center };
        public TextBlock Count { get; } = new() { FontWeight = FontWeight.SemiBold, FontSize = 15 };
        public ProgressBar Bar { get; } = new() { Height = 6, MinHeight = 6, Width = 280, HorizontalAlignment = HorizontalAlignment.Left, Margin = new Thickness(0, 6, 0, 0) };
        public Button DonateAll { get; } = new() { Content = IslandStrings.DonateAll, VerticalAlignment = VerticalAlignment.Center };
        public TextBox Search { get; } = new() { PlaceholderText = IslandStrings.SearchPiece, Width = 220 };
        public ListBox List { get; } = new();
        public TextBlock Empty { get; } = new() { Text = IslandStrings.NothingFound, Classes = { "subtle" }, HorizontalAlignment = HorizontalAlignment.Center, Margin = new Thickness(0, 30, 0, 0), IsVisible = false };
        public Filter Filter { get; set; }
    }

    private readonly Museum Museum;
    private readonly IReadOnlyList<Player> Players;
    private readonly int PlayerIndex;
    private readonly Dictionary<ushort, int> Donations = []; // item → its entry in the list
    private readonly WingTab[] Wings;
    private readonly TabControl Tabs = new();
    private Core.MuseumEditor Edit;

    public MuseumWindow(Museum museum, IReadOnlyList<Player> players, int playerIndex)
    {
        Museum = museum;
        Players = players;
        PlayerIndex = playerIndex;
        Edit = new Core.MuseumEditor(museum);
        Width = 760;
        Height = 720;
        MinWidth = 620;
        MinHeight = 480;

        // Ticks, not selection: a chosen row looks like the others.
        Styles.Add(new Style(x => x.OfType<ListBoxItem>().Class(":selected").Template().OfType<ContentPresenter>())
        {
            Setters = { new Setter(ContentPresenter.BackgroundProperty, Brushes.Transparent) },
        });

        Wings = [.. Enum.GetValues<Wing>().Select(z => new WingTab(z, GetPieces(z)))];
        foreach (var w in Wings)
            Tabs.Items.Add(new TabItem { Header = TabHeader(w), Content = TabContent(w) });

        var everything = new Button { Content = IslandStrings.DonateEverything };
        everything.Click += async (_, _) => await DonateEverything();
        var classic = new Button { Content = IslandStrings.ClassicMuseum };
        ToolTip.SetTip(classic, IslandStrings.ClassicMuseumHint);
        classic.Click += async (_, _) => await OpenClassic();

        IslandUi.Frame(this, IslandStrings.MuseumTitle, IslandStrings.MuseumHint, Tabs, () => Edit.Save(), everything, classic);

        Reindex();
        foreach (var w in Wings)
            Refresh(w);
        UpdateCounts();
    }

    #region Pieces
    /// <summary>
    /// Every piece of a wing that can be donated (no forgeries, trash or seasonal items), sorted by name.
    /// </summary>
    internal static Piece[] GetPieces(Wing wing)
    {
        ushort[] ids = wing switch
        {
            Wing.Fish => GameLists.Fish.ToArray(),
            Wing.Bugs => GameLists.Bugs.ToArray(),
            Wing.Sea => GameLists.Dive.ToArray(),
            Wing.Fossils => GameLists.Fossils.ToArray(),
            _ => GameLists.Art.ToArray(),
        };
        return [.. ids.Distinct()
            .Where(id => Belongs(wing, ItemInfo.GetItemKind(id)))
            .Select(id =>
            {
                var name = IslandUi.ItemName(id);
                return new Piece(id, name, ItemSearchBox.Normalize(name), wing);
            })
            .OrderBy(z => z.Key, StringComparer.Ordinal)];
    }

    private static bool Belongs(Wing wing, ItemKind k) => wing switch
    {
        Wing.Fish => k == ItemKind.Kind_Fish,
        Wing.Bugs => k == ItemKind.Kind_Insect,
        Wing.Sea => k == ItemKind.Kind_DiveFish,
        Wing.Fossils => k == ItemKind.Kind_Fossil,
        _ => k is ItemKind.Kind_Picture or ItemKind.Kind_Sculpture,
    };

    private static string WingName(Wing wing) => wing switch
    {
        Wing.Fish => HomeStrings.CatFish,
        Wing.Bugs => HomeStrings.CatBugs,
        Wing.Sea => HomeStrings.CatSea,
        Wing.Fossils => HomeStrings.CatFossils,
        _ => HomeStrings.CatArt,
    };

    private static string CountText(Wing wing) => wing switch
    {
        Wing.Fish => IslandStrings.FishCount,
        Wing.Bugs => IslandStrings.BugsCount,
        Wing.Sea => IslandStrings.SeaCount,
        Wing.Fossils => IslandStrings.FossilsCount,
        _ => IslandStrings.ArtCount,
    };

    private static Image WingIcon(Wing wing)
    {
        var icon = wing switch
        {
            Wing.Fish => "FishTai",
            Wing.Bugs => "InsMonshirocho",
            Wing.Sea => "Tako",
            Wing.Fossils => "Fossil",
            _ => "Haniwa",
        };
        return new Image { Source = ItemSprite.GetResource($"sprites/MenuIcon/{icon}.png"), Width = 22, Height = 22, VerticalAlignment = VerticalAlignment.Center };
    }

    internal bool IsDonated(ushort id) => Donations.ContainsKey(id);

    /// <summary>
    /// Number of donated pieces of a wing, and how many it has.
    /// </summary>
    internal (int Donated, int Total) Progress(Wing wing)
    {
        var w = Wings[(int)wing];
        return (w.Pieces.Count(z => IsDonated(z.Id)), w.Pieces.Length);
    }
    #endregion

    #region Layout
    private static StackPanel TabHeader(WingTab w)
    {
        var p = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6 };
        p.Children.Add(WingIcon(w.Wing));
        p.Children.Add(new TextBlock { Text = WingName(w.Wing), VerticalAlignment = VerticalAlignment.Center });
        p.Children.Add(w.HeaderCount);
        return p;
    }

    private DockPanel TabContent(WingTab w)
    {
        w.Bar.Themed(ProgressBar.ForegroundProperty, "NhGreenBrush");
        var progress = new StackPanel();
        progress.Children.Add(w.Count);
        progress.Children.Add(w.Bar);
        w.DonateAll.Click += (_, _) => DonateAll(w.Wing);
        var top = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto"), Margin = new Thickness(0, 12, 0, 10) };
        top.Children.Add(progress);
        Grid.SetColumn(w.DonateAll, 1);
        top.Children.Add(w.DonateAll);

        // Search and show all / missing / donated
        w.Search.TextChanged += (_, _) => Refresh(w);
        var filters = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 4, VerticalAlignment = VerticalAlignment.Center };
        var chips = new List<ToggleButton>();
        foreach (var (f, text) in new[] { (Filter.All, IslandStrings.FilterAll), (Filter.Missing, IslandStrings.FilterMissing), (Filter.Donated, IslandStrings.FilterDonated) })
        {
            var chip = new ToggleButton { Content = text, IsChecked = f == w.Filter, Classes = { "chip" }, VerticalAlignment = VerticalAlignment.Center, VerticalContentAlignment = VerticalAlignment.Center };
            chip.Click += (_, _) =>
            {
                w.Filter = f;
                foreach (var other in chips)
                    other.IsChecked = other == chip;
                Refresh(w);
            };
            chips.Add(chip);
            filters.Children.Add(chip);
        }
        var filterRow = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 12, Margin = new Thickness(0, 0, 0, 8) };
        filterRow.Children.Add(w.Search);
        filterRow.Children.Add(filters);

        w.List.ItemTemplate = new FuncDataTemplate<Piece>((p, _) => p is null ? new Panel() : PieceRow(p));
        w.List.Themed(ListBox.BackgroundProperty, "NhCardBrush");
        w.List.CornerRadius = new CornerRadius(10);
        w.List.Padding = new Thickness(4);
        var list = new Panel();
        list.Children.Add(w.List);
        list.Children.Add(w.Empty);

        var dock = new DockPanel();
        DockPanel.SetDock(top, Dock.Top);
        DockPanel.SetDock(filterRow, Dock.Top);
        dock.Children.Add(top);
        dock.Children.Add(filterRow);
        dock.Children.Add(list);
        return dock;
    }

    /// <summary>
    /// One piece: tick, picture, name and when and who donated it. The whole row is the tick.
    /// </summary>
    private CheckBox PieceRow(Piece p)
    {
        var detail = new TextBlock { Text = Detail(p.Id), Classes = { "subtle" }, FontSize = 12 };
        var text = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Spacing = 1 };
        text.Children.Add(new TextBlock { Text = p.Name, FontWeight = FontWeight.SemiBold, TextTrimming = TextTrimming.CharacterEllipsis });
        text.Children.Add(detail);

        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 10, Margin = new Thickness(6, 0, 0, 0) };
        g.Children.Add(new Image { Source = ItemSprite.GetItemSprite(p.Id), Width = 36, Height = 36 });
        Grid.SetColumn(text, 1);
        g.Children.Add(text);

        var check = new CheckBox
        {
            Content = g,
            IsChecked = IsDonated(p.Id),
            HorizontalAlignment = HorizontalAlignment.Stretch,
            HorizontalContentAlignment = HorizontalAlignment.Stretch,
            VerticalContentAlignment = VerticalAlignment.Center,
        };
        check.IsCheckedChanged += (_, _) =>
        {
            SetDonated(p, check.IsChecked == true);
            detail.Text = Detail(p.Id);
        };
        return check;
    }

    private string Detail(ushort id)
    {
        if (!Donations.TryGetValue(id, out var index))
            return IslandStrings.NotDonated;
        DateTime date = Edit.Dates[index];
        var player = Edit.Players[index];
        var name = player < Players.Count ? Players[player].Personal.PlayerName : null;
        if (date == DateTime.MinValue)
            return name is null ? IslandStrings.Donated : string.Format(IslandStrings.DonatedBy, name);
        var when = IslandUi.FormatDate(date);
        return name is null ? string.Format(IslandStrings.DonatedOn, when) : string.Format(IslandStrings.DonatedOnBy, when, name);
    }

    private void Refresh(WingTab w)
    {
        var words = ItemSearchBox.Normalize(w.Search.Text ?? string.Empty).Split(' ', StringSplitOptions.RemoveEmptyEntries);
        var shown = w.Pieces.Where(p => w.Filter switch
            {
                Filter.Missing => !IsDonated(p.Id),
                Filter.Donated => IsDonated(p.Id),
                _ => true,
            })
            .Where(p => words.All(z => p.Key.Contains(z, StringComparison.Ordinal)))
            .ToArray();
        w.List.ItemsSource = shown;
        w.Empty.IsVisible = shown.Length == 0;
    }

    private void UpdateCounts()
    {
        foreach (var w in Wings)
        {
            var (done, total) = Progress(w.Wing);
            w.HeaderCount.Text = $"{done}/{total}";
            w.Count.Text = string.Format(CountText(w.Wing), done, total);
            w.Bar.Maximum = total;
            w.Bar.Value = done;
            w.DonateAll.IsEnabled = done < total;
        }
    }
    #endregion

    #region Donations
    private void Reindex()
    {
        Donations.Clear();
        for (int i = 0; i < Edit.Items.Length; i++)
        {
            var item = Edit.Items[i];
            if (!item.IsNone)
                Donations.TryAdd(item.ItemId, i);
        }
    }

    /// <summary>
    /// Donates a piece today in the name of the current player, or takes it out of the museum, and updates the counts.
    /// </summary>
    internal void SetDonated(Piece p, bool donated)
    {
        Change(p, donated);
        UpdateCounts();
    }

    private void Change(Piece p, bool donated)
    {
        if (donated == IsDonated(p.Id))
            return;
        if (donated)
        {
            var index = Array.FindIndex(Edit.Items, z => z.IsNone);
            if (index < 0)
                return;
            Edit.Items[index] = new Item(p.Id);
            Edit.Dates[index] = DateTime.Today;
            Edit.Players[index] = (byte)PlayerIndex;
            Donations[p.Id] = index;
            return;
        }

        int at;
        while ((at = Array.FindIndex(Edit.Items, z => z.ItemId == p.Id)) >= 0)
            RemoveEntry(at);
        Reindex();
    }

    /// <summary>
    /// Takes an entry out of the donation list, moving the later ones up like the game keeps them.
    /// </summary>
    private void RemoveEntry(int index)
    {
        var last = Edit.Items.Length - 1;
        for (int i = index; i < last; i++)
        {
            Edit.Items[i] = Edit.Items[i + 1];
            Edit.Dates[i] = Edit.Dates[i + 1];
            Edit.Players[i] = Edit.Players[i + 1];
        }
        Edit.Items[last] = new Item(Item.NONE);
        Edit.Dates[last] = default;
        Edit.Players[last] = byte.MaxValue; // empty entries have no player
    }

    internal Piece[] Pieces(Wing wing) => Wings[(int)wing].Pieces;

    internal void SelectWing(Wing wing) => Tabs.SelectedIndex = (int)wing;

    internal void DonateAll(Wing wing)
    {
        var w = Wings[(int)wing];
        foreach (var p in w.Pieces)
            Change(p, true);
        Refresh(w);
        UpdateCounts();
    }

    private async Task DonateEverything()
    {
        var missing = Wings.SelectMany(z => z.Pieces).Count(z => !IsDonated(z.Id));
        if (missing == 0)
            return;
        var player = PlayerIndex < Players.Count ? Players[PlayerIndex].Personal.PlayerName : string.Empty;
        var answer = await Dialogs.Prompt(MessageBoxButtons.YesNo, IslandStrings.DonateEverythingAsk, string.Format(IslandStrings.DonateEverythingAsk2, missing, player));
        if (answer != DialogResult.Yes)
            return;
        foreach (var w in Wings)
        {
            foreach (var p in w.Pieces)
                Change(p, true);
            Refresh(w);
        }
        UpdateCounts();
    }

    /// <summary>
    /// The classic editor on the same museum (dates, donors, export and import), then back here.
    /// </summary>
    private async Task OpenClassic()
    {
        Edit.Save();
        var classic = new MuseumEditor(Museum) { Title = IslandStrings.MuseumTitle };
        if (!await classic.ShowDialog<bool>(this))
            return;
        Edit = new Core.MuseumEditor(Museum);
        Reindex();
        foreach (var w in Wings)
            Refresh(w);
        UpdateCounts();
    }
    #endregion
}
