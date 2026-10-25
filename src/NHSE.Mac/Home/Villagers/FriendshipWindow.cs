using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Friendly editor of how a villager gets on with each player: the friendship as hearts (the game's levels, explained),
/// a bar for the exact points and the nickname the villager uses. Everything else the villager remembers of each
/// player is in the classic memory editor, under "Advanced options".
/// Works on a copy of the memories: the villager changes only when the window is saved (returns true).
/// </summary>
public sealed class FriendshipWindow : NhseWindow
{
    /// <summary>
    /// Lowest points of each friendship level, from 0 to 7 hearts (the levels the game uses).
    /// </summary>
    public static readonly byte[] LevelStart = [0, 25, 30, 70, 100, 130, 160, 200];

    public const int MaxLevel = 7;

    private readonly IVillager Villager;
    private readonly IVillagerOrigin? Host;
    private readonly string VillagerName;
    private readonly StackPanel Players = new() { Spacing = 10 };
    private readonly List<Slider> Bars = [];
    private GSaveMemory[] Work;

    /// <param name="villager">Villager whose memories are edited.</param>
    /// <param name="host">A player of this island, to tell the island's players from visitors.</param>
    public FriendshipWindow(IVillager villager, IVillagerOrigin? host = null)
    {
        Villager = villager;
        Host = host;
        VillagerName = GameInfo.Strings.GetVillager(villager.InternalName);
        Work = Copy(villager.GetMemories());

        Title = string.Format(VillagerStrings.FriendshipTitle, VillagerName);
        Width = 600;
        SizeToContent = SizeToContent.Height;
        CanResize = false;

        var header = new StackPanel { Margin = new Thickness(0, 0, 0, 14) };
        header.Children.Add(Text(Title, "title"));
        header.Children.Add(Text(string.Format(VillagerStrings.FriendshipHint, VillagerName), "subtle"));

        var how = Card(CardHeader("Love", VillagerStrings.FriendshipHowTitle, VillagerStrings.FriendshipHow));

        var body = new StackPanel { Spacing = 12 };
        body.Children.Add(how);
        body.Children.Add(Players);
        body.Children.Add(Advanced());

        var bestFriends = new Button { Content = VillagerStrings.BestFriendsAll, Padding = new Thickness(12, 4) };
        bestFriends.Click += (_, _) => SetAll(byte.MaxValue);
        bestFriends.IsVisible = Shown.Any();

        var save = new Button { Content = HomeStrings.Done, Classes = { "accent" }, IsDefault = true, MinWidth = 100 };
        save.Click += (_, _) => Save();
        var cancel = new Button { Content = HomeStrings.Cancel, IsCancel = true, MinWidth = 100 };
        cancel.Click += (_, _) => Close(false);
        var right = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, HorizontalAlignment = HorizontalAlignment.Right };
        right.Children.Add(cancel);
        right.Children.Add(save);
        var buttons = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), Margin = new Thickness(0, 16, 0, 0) };
        buttons.Children.Add(bestFriends);
        Grid.SetColumn(right, 1);
        buttons.Children.Add(right);

        var root = new DockPanel { Margin = new Thickness(22, 18, 22, 18) };
        DockPanel.SetDock(header, Dock.Top);
        DockPanel.SetDock(buttons, Dock.Bottom);
        root.Children.Add(header);
        root.Children.Add(buttons);
        root.Children.Add(new ScrollViewer { Content = body, MaxHeight = 600, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled });
        Content = root;
        this.Bind(BackgroundProperty, this.GetResourceObservable("NhWindowBrush"));

        BuildPlayers();
    }

    #region Levels
    /// <summary>
    /// Friendship level (number of hearts, 0-7) of some friendship points.
    /// </summary>
    public static int GetLevel(byte points)
    {
        for (int level = MaxLevel; level > 0; level--)
        {
            if (points >= LevelStart[level])
                return level;
        }
        return 0;
    }

    public static string GetLevelName(int level) => level switch
    {
        0 => VillagerStrings.FriendshipLevel0,
        1 => VillagerStrings.FriendshipLevel1,
        2 => VillagerStrings.FriendshipLevel2,
        3 => VillagerStrings.FriendshipLevel3,
        4 => VillagerStrings.FriendshipLevel4,
        5 => VillagerStrings.FriendshipLevel5,
        6 => VillagerStrings.FriendshipLevel6,
        _ => VillagerStrings.FriendshipLevel7,
    };

    /// <summary>
    /// Row of the game's heart icons: as many full ones as the level, the rest faded.
    /// </summary>
    public static StackPanel HeartStrip(int level, double size = 18)
    {
        var p = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 1, VerticalAlignment = VerticalAlignment.Center };
        for (int i = 1; i <= MaxLevel; i++)
        {
            var heart = Icon("Love", size);
            heart.VerticalAlignment = VerticalAlignment.Center;
            heart.Opacity = i <= level ? 1 : 0.22;
            p.Children.Add(heart);
        }
        return p;
    }
    #endregion

    #region Players
    private static GSaveMemory[] Copy(IEnumerable<GSaveMemory> memories) => [.. memories.Select(m => new GSaveMemory(m.Data.ToArray()))];

    /// <summary>
    /// Indexes of the players the villager remembers (the other spaces are empty).
    /// </summary>
    private IEnumerable<int> Shown => Enumerable.Range(0, Work.Length).Where(i => !string.IsNullOrWhiteSpace(Work[i].PlayerName));

    private bool IsFromThisIsland(GSaveMemory m) => Host is null || m.IsSameTown(Host);

    private void BuildPlayers()
    {
        Players.Children.Clear();
        Bars.Clear();
        foreach (var i in Shown)
            Players.Children.Add(PlayerCard(Work[i]));
        if (Players.Children.Count == 0)
            Players.Children.Add(Card(Text(string.Format(VillagerStrings.NobodyMet, VillagerName), "subtle")));
    }

    private Border PlayerCard(GSaveMemory m)
    {
        // Who
        var who = new StackPanel { Spacing = 2 };
        who.Children.Add(Text(m.PlayerName, "cardtitle"));
        who.Children.Add(Text(IsFromThisIsland(m) ? VillagerStrings.FromYourIsland : string.Format(VillagerStrings.FromOtherIsland, m.TownName), "subtle"));

        // Hearts: a click sets the lowest points of that level (the last one, the maximum)
        var hearts = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 0 };
        var heartImages = new Image[MaxLevel];
        var bar = new Slider { Minimum = 0, Maximum = 255, TickFrequency = 1, IsSnapToTickEnabled = true, Value = m.Friendship };
        for (int i = 1; i <= MaxLevel; i++)
        {
            var level = i;
            var img = Icon("Love", 28);
            heartImages[i - 1] = img;
            var b = new Button
            {
                Content = img,
                Background = Brushes.Transparent,
                BorderThickness = new Thickness(0),
                Padding = new Thickness(2),
                MinWidth = 0,
                MinHeight = 0,
            };
            ToolTip.SetTip(b, string.Format(VillagerStrings.HeartTip, GetLevelName(level), LevelStart[level]));
            b.Click += (_, _) => bar.Value = level == MaxLevel ? byte.MaxValue : LevelStart[level];
            hearts.Children.Add(b);
        }

        var levelName = new TextBlock { FontWeight = FontWeight.SemiBold, FontSize = 15, VerticalAlignment = VerticalAlignment.Center };
        var points = Text(string.Empty, "subtle");
        points.VerticalAlignment = VerticalAlignment.Center;
        void Show()
        {
            var level = GetLevel(m.Friendship);
            for (int i = 0; i < heartImages.Length; i++)
                heartImages[i].Opacity = i < level ? 1 : 0.22;
            levelName.Text = GetLevelName(level);
            points.Text = string.Format(VillagerStrings.FriendshipPoints, m.Friendship);
        }
        bar.ValueChanged += (_, _) =>
        {
            m.Friendship = (byte)Math.Clamp(Math.Round(bar.Value), 0, 255);
            Show();
        };
        Bars.Add(bar);
        Show();

        var levelText = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Margin = new Thickness(4, 0, 0, 0) };
        levelText.Children.Add(levelName);
        levelText.Children.Add(points);
        var heartRow = new WrapPanel { ItemSpacing = 8, LineSpacing = 4 };
        heartRow.Children.Add(hearts);
        heartRow.Children.Add(levelText);

        // The name the villager calls the player
        var nick = new TextBox { Text = m.NickName, PlaceholderText = m.PlayerName, MaxLength = 10, Width = 170 };
        nick.PropertyChanged += (_, e) =>
        {
            // TextChanged comes later from the dispatcher: a click on Save right after typing would miss it.
            if (e.Property == TextBox.TextProperty)
                m.NickName = nick.Text ?? string.Empty;
        };
        var nickHint = Text(VillagerStrings.NicknameHint, "subtle");
        nickHint.VerticalAlignment = VerticalAlignment.Center;
        var nickRow = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 10, Margin = new Thickness(0, 4, 0, 0) };
        nickRow.Children.Add(new TextBlock { Text = VillagerStrings.Nickname, VerticalAlignment = VerticalAlignment.Center, FontWeight = FontWeight.SemiBold });
        nickRow.Children.Add(nick);
        nickRow.Children.Add(nickHint);

        var panel = new StackPanel { Spacing = 6 };
        panel.Children.Add(who);
        panel.Children.Add(heartRow);
        panel.Children.Add(bar);
        panel.Children.Add(nickRow);
        return Card(panel);
    }

    /// <summary>
    /// Sets the friendship of every player the villager knows (shown at once; kept only if saved).
    /// </summary>
    public void SetAll(byte points)
    {
        foreach (var bar in Bars)
            bar.Value = points;
    }

    private void Save()
    {
        Villager.SetMemories(Work);
        Close(true);
    }
    #endregion

    #region Advanced
    private Expander Advanced()
    {
        var open = new Button { Content = VillagerStrings.ClassicMemories, Margin = new Thickness(0, 6, 0, 0) };
        open.Click += async (_, _) => await OpenClassic();
        var panel = new StackPanel();
        panel.Children.Add(Text(string.Format(VillagerStrings.ClassicMemoriesHint, VillagerName), "subtle"));
        panel.Children.Add(open);
        return new Expander { Header = HomeStrings.AdvancedOptions, Content = panel, HorizontalAlignment = HorizontalAlignment.Stretch };
    }

    /// <summary>
    /// The classic editor works on a copy of the villager with the changes made here; what it saves comes back here.
    /// </summary>
    private async Task OpenClassic()
    {
        var copy = Clone(Villager);
        copy.SetMemories(Work);
        if (!await new VillagerMemoryEditor(copy).ShowDialog<bool>(this))
            return;
        Work = Copy(copy.GetMemories());
        BuildPlayers();
    }

    private static IVillager Clone(IVillager v) => v switch
    {
        Villager2 => new Villager2(v.Write()),
        _ => new Villager1(v.Write()),
    };
    #endregion
}
