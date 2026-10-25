using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Friendly editor of the week's special visitors: who comes each day (with portraits and the game's names) and
/// K.K.'s concerts. Everything else the save keeps about them is under "Advanced options".
/// The visitor data is edited in place; the caller keeps it only if the window returns true.
/// </summary>
public sealed class VisitorsWindow : NhseWindow
{
    public sealed record Choice(VisitorNPC Value, string Name, string Code)
    {
        public override string ToString() => Name;
    }

    private readonly GSaveVisitorNpc Visitor;
    private readonly List<Choice> Choices;
    private readonly ComboBox[] Days = new ComboBox[7]; // by DayOfWeek
    private readonly CheckBox[] Normal = new CheckBox[7];
    private readonly CheckBox[] Birthday = new CheckBox[7];
    private PropertyGrid? Advanced;
    private bool Loading;

    public VisitorsWindow(GSaveVisitorNpc visitor)
    {
        Visitor = visitor;
        SizeToContent = SizeToContent.WidthAndHeight;
        CanResize = false;

        Choices = [new Choice(VisitorNPC.None, IslandStrings.Nobody, string.Empty),
            .. Enum.GetValues<VisitorNPC>().Where(z => z != VisitorNPC.None)
                .Select(z => new Choice(z, IslandUi.VisitorName(z), IslandUi.VisitorCode(z)))
                .OrderBy(z => z.Name, StringComparer.CurrentCultureIgnoreCase)];

        // Who comes each day
        var week = new Grid { ColumnDefinitions = new ColumnDefinitions("110,Auto,*"), ColumnSpacing = 12, RowSpacing = 6, Margin = new Thickness(0, 10, 0, 0) };
        int row = 0;
        foreach (var day in IslandUi.Week)
        {
            week.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
            var name = new TextBlock { Text = IslandUi.DayName(day), FontWeight = FontWeight.SemiBold, VerticalAlignment = VerticalAlignment.Center };
            Grid.SetRow(name, row);
            week.Children.Add(name);

            var d = (int)day;
            var combo = new ComboBox { ItemsSource = Choices, Width = 230, MaxDropDownHeight = 420, ItemTemplate = ChoiceTemplate() };
            combo.SelectionChanged += (_, _) =>
            {
                if (Loading || combo.SelectedItem is not Choice c)
                    return;
                Visitor.VisitorNPC[d] = c.Value;
                Visitor.InitSchedule[d] = c.Value;
                Advanced?.Refresh();
            };
            Days[d] = combo;
            Grid.SetRow(combo, row);
            Grid.SetColumn(combo, 1);
            week.Children.Add(combo);

            var note = day switch
            {
                DayOfWeek.Sunday => IslandStrings.SundayNote,
                DayOfWeek.Saturday => IslandStrings.SaturdayNote,
                _ => null,
            };
            if (note is not null)
            {
                var t = Text(note, "subtle");
                t.VerticalAlignment = VerticalAlignment.Center;
                t.FontSize = 12;
                t.MaxWidth = 220;
                Grid.SetRow(t, row);
                Grid.SetColumn(t, 2);
                week.Children.Add(t);
            }
            row++;
        }
        var weekCard = Card(CardHeader("Balloon", IslandStrings.ThisWeek, null, week));

        // K.K.'s concerts
        var concerts = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto" + string.Concat(Enumerable.Repeat(",52", 7))), RowDefinitions = new RowDefinitions("Auto,Auto,Auto"), ColumnSpacing = 2, Margin = new Thickness(0, 10, 0, 0) };
        AddConcertLabel(concerts, IslandStrings.ConcertNormal, 1);
        AddConcertLabel(concerts, IslandStrings.ConcertBirthday, 2);
        int col = 1;
        foreach (var day in IslandUi.Week)
        {
            var d = (int)day;
            var head = new TextBlock { Text = IslandUi.ShortDayName(day), HorizontalAlignment = HorizontalAlignment.Center, Classes = { "subtle" } };
            Grid.SetColumn(head, col);
            concerts.Children.Add(head);

            var normal = new CheckBox { HorizontalAlignment = HorizontalAlignment.Center, MinWidth = 0 };
            normal.IsCheckedChanged += (_, _) => SetFlag(Visitor.IsNormalLive, d, normal);
            ToolTip.SetTip(normal, $"{IslandStrings.ConcertNormal} · {IslandUi.DayName(day)}");
            Normal[d] = normal;
            Grid.SetRow(normal, 1);
            Grid.SetColumn(normal, col);
            concerts.Children.Add(normal);

            var birthday = new CheckBox { HorizontalAlignment = HorizontalAlignment.Center, MinWidth = 0 };
            birthday.IsCheckedChanged += (_, _) => SetFlag(Visitor.IsBirthdayLive, d, birthday);
            ToolTip.SetTip(birthday, $"{IslandStrings.ConcertBirthday} · {IslandUi.DayName(day)}");
            Birthday[d] = birthday;
            Grid.SetRow(birthday, 2);
            Grid.SetColumn(birthday, col);
            concerts.Children.Add(birthday);
            col++;
        }
        var concertCard = Card(TurnipsWindow.PortraitHeader("tkkA", IslandStrings.ConcertsTitle, IslandStrings.ConcertsHint, concerts));

        // Everything else, as the save has it (built when opened)
        var advanced = new StackPanel { Spacing = 6 };
        var expander = new Expander { Header = HomeStrings.AdvancedOptions, Content = advanced, HorizontalAlignment = HorizontalAlignment.Stretch };
        expander.PropertyChanged += (_, e) =>
        {
            if (e.Property != Expander.IsExpandedProperty || !expander.IsExpanded || Advanced is not null)
                return;
            advanced.Children.Add(Text(IslandStrings.VisitorsAdvancedHint, "subtle"));
            Advanced = new PropertyGrid { PropertySort = PropertySort.NoSort, Height = 260, SelectedObject = Visitor };
            Advanced.PropertyValueChanged += (_, _) => Load();
            advanced.Children.Add(Advanced);
        };

        var body = new StackPanel { Spacing = 12, Width = 600 };
        body.Children.Add(weekCard);
        body.Children.Add(concertCard);
        body.Children.Add(expander);

        IslandUi.Frame(this, IslandStrings.VisitorsTitle, IslandStrings.VisitorsHint, body, () => { });
        Load();
    }

    private static void AddConcertLabel(Grid g, string text, int row)
    {
        var t = new TextBlock { Text = text, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 0, 12, 0) };
        Grid.SetRow(t, row);
        g.Children.Add(t);
    }

    private void SetFlag(bool[] flags, int day, CheckBox box)
    {
        if (Loading)
            return;
        flags[day] = box.IsChecked == true;
        Advanced?.Refresh();
    }

    /// <summary>
    /// Shows the values of the visitor data (at the start and after editing them in Advanced options).
    /// </summary>
    private void Load()
    {
        Loading = true;
        for (int d = 0; d < 7; d++)
        {
            var v = d < Visitor.VisitorNPC.Length ? Visitor.VisitorNPC[d] : VisitorNPC.None;
            Days[d].SelectedItem = Choices.FirstOrDefault(z => z.Value == v) ?? Choices[0];
            Normal[d].IsChecked = d < Visitor.IsNormalLive.Length && Visitor.IsNormalLive[d];
            Birthday[d].IsChecked = d < Visitor.IsBirthdayLive.Length && Visitor.IsBirthdayLive[d];
        }
        Loading = false;
    }

    private static FuncDataTemplate<Choice> ChoiceTemplate() => new((c, _) =>
    {
        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        if (c is null)
            return row;
        if (c.Code.Length != 0)
            row.Children.Add(IslandUi.Portrait(c.Code, 28));
        else
            row.Children.Add(new Border { Width = 28, Height = 28 });
        row.Children.Add(new TextBlock { Text = c.Name, VerticalAlignment = VerticalAlignment.Center });
        return row;
    });

    /// <summary>
    /// Visitor box of a day (tests).
    /// </summary>
    internal ComboBox DayBox(DayOfWeek day) => Days[(int)day];
}
