using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// The reactions the player knows, by name and grouped as the game shows them, plus the eight on the reaction wheel.
/// Saved to the player on «Save».
/// </summary>
public sealed class ReactionsWindow : PlayerDialog
{
    private readonly Personal Pers;
    private readonly IReactionStore Store;
    private readonly Dictionary<Reaction, CheckBox> Boxes = [];
    private readonly ComboBox[] Wheel = new ComboBox[8];
    private readonly TextBlock Summary = new() { Classes = { "amount" } };
    private bool Loading;

    /// <summary>
    /// Reactions the game really has (without the empty value and the unused ones).
    /// </summary>
    public static Reaction[] Valid { get; } = [.. Enum.GetValues<Reaction>().Where(z => z != Reaction.None && !z.ToString().StartsWith("UNUSED", StringComparison.Ordinal))];

    private sealed record WheelChoice(Reaction Value, string Name)
    {
        public override string ToString() => Name;
    }

    public ReactionsWindow(Personal pers) : base(HomeStrings.Reactions, 940, 700)
    {
        Pers = pers;
        Store = pers.Reactions;

        // Left: every reaction, in three groups.
        var groups = new StackPanel { Spacing = 4 };
        groups.Children.Add(Group(PlayerStrings.ReactionGroupFeelings, Valid.Where(z => z < Reaction.Scare)));
        groups.Children.Add(Group(PlayerStrings.ReactionGroupGestures, Valid.Where(z => z is >= Reaction.Scare and < Reaction.PostureWarmUp)));
        groups.Children.Add(Group(PlayerStrings.ReactionGroupMoves, Valid.Where(z => z >= Reaction.PostureWarmUp)));
        var left = new ScrollViewer { Content = groups, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled, Padding = new Thickness(0, 0, 14, 0) };

        // Right: count, learn all and the wheel.
        var learnAll = PlayerUi.Button(HomeStrings.LearnAll, LearnAll, accent: true);
        var summary = new StackPanel { Spacing = 8 };
        summary.Children.Add(Summary);
        summary.Children.Add(learnAll);

        var wheel = new StackPanel { Spacing = 6 };
        wheel.Children.Add(HomeUi.Text(PlayerStrings.Wheel, "cardtitle"));
        wheel.Children.Add(PlayerUi.Subtle(PlayerStrings.WheelHint));
        for (int i = 0; i < Wheel.Length; i++)
        {
            var slot = i;
            var cb = new ComboBox { HorizontalAlignment = HorizontalAlignment.Stretch, MaxDropDownHeight = 420 };
            cb.SelectionChanged += (_, _) =>
            {
                if (Loading || cb.SelectedItem is not WheelChoice choice)
                    return;
                Store.UIList[slot] = choice.Value;
            };
            Wheel[i] = cb;
            var row = new Grid { ColumnDefinitions = new ColumnDefinitions("22,*") };
            row.Children.Add(new TextBlock { Text = $"{i + 1}", VerticalAlignment = VerticalAlignment.Center, Classes = { "subtle" } });
            Grid.SetColumn(cb, 1);
            row.Children.Add(cb);
            wheel.Children.Add(row);
        }

        var right = new StackPanel { Spacing = 12, Width = 270 };
        right.Children.Add(HomeUi.Card(summary));
        right.Children.Add(HomeUi.Card(wheel));

        var body = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto") };
        body.Children.Add(left);
        Grid.SetColumn(right, 1);
        body.Children.Add(right);

        SetLayout(HomeStrings.Reactions, PlayerStrings.ReactionsHint, body);
        Refresh();
    }

    public static string GetName(Reaction r) => PlayerStrings.GetText($"Reaction{r}") ?? PlayerUi.SplitWords(r.ToString());

    private Control Group(string title, IEnumerable<Reaction> reactions)
    {
        var grid = new UniformGrid { Columns = 3, ColumnSpacing = 6, RowSpacing = 2 };
        foreach (var r in reactions)
        {
            var reaction = r;
            var box = new CheckBox { Content = GetName(r) };
            box.IsCheckedChanged += (_, _) =>
            {
                if (Loading)
                    return;
                if (box.IsChecked == true)
                    Learn(reaction);
                else
                    Forget(reaction);
                Refresh();
            };
            Boxes[r] = box;
            grid.Children.Add(box);
        }
        var panel = new StackPanel { Spacing = 6 };
        panel.Children.Add(HomeUi.Text(title, "cardtitle"));
        panel.Children.Add(grid);
        var card = HomeUi.Card(panel);
        card.Margin = new Thickness(0, 0, 0, 8);
        return card;
    }

    private bool IsKnown(Reaction r) => Array.IndexOf(Store.ManpuBit, r) >= 0;

    private void Learn(Reaction r)
    {
        if (IsKnown(r))
            return;
        Store.TryAddReaction(r);
        if (!IsKnown(r))
            _ = Dialogs.Alert(PlayerStrings.ReactionsFull);
    }

    /// <summary>
    /// Removes the reaction from the learned list (the ones after it move up, as the list has no gaps)
    /// and from the wheel.
    /// </summary>
    private void Forget(Reaction r)
    {
        var list = Store.ManpuBit;
        var news = Store.NewFlag;
        int w = 0;
        for (int i = 0; i < list.Length; i++)
        {
            if (list[i] == r)
                continue;
            list[w] = list[i];
            if (i < news.Length && w < news.Length)
                news[w] = news[i];
            w++;
        }
        for (; w < list.Length; w++)
        {
            list[w] = Reaction.None;
            if (w < news.Length)
                news[w] = false;
        }

        var wheel = Store.UIList;
        for (int i = 0; i < wheel.Length; i++)
        {
            if (wheel[i] == r)
                wheel[i] = Reaction.None;
        }
    }

    internal void LearnAll()
    {
        Store.AddMissingReactions();
        Refresh();
        Sound.Asterisk();
    }

    /// <summary>
    /// Ticks the boxes of the known reactions, updates the count and the choices of the wheel.
    /// </summary>
    private void Refresh()
    {
        Loading = true;
        foreach (var (r, box) in Boxes)
            box.IsChecked = IsKnown(r);
        Summary.Text = string.Format(HomeStrings.ReactionsHint, Valid.Count(IsKnown), Valid.Length);

        var known = Valid.Where(IsKnown).Select(z => new WheelChoice(z, GetName(z))).OrderBy(z => z.Name, StringComparer.CurrentCultureIgnoreCase);
        var choices = new List<WheelChoice> { new(Reaction.None, PlayerStrings.WheelEmpty) };
        choices.AddRange(known);
        var ui = Store.UIList;
        for (int i = 0; i < Wheel.Length && i < ui.Length; i++)
        {
            var current = ui[i];
            var source = choices;
            if (choices.All(z => z.Value != current))
                source = [.. choices, new WheelChoice(current, GetName(current))];
            Wheel[i].ItemsSource = source;
            Wheel[i].SelectedItem = source.First(z => z.Value == current);
        }
        Loading = false;
    }

    protected override void Save() => Pers.Reactions = Store;
}
