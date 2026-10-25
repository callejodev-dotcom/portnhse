using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Data;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Nook Miles+ records: what each one counts, how far the player is and the next stamp,
/// «Max» for one record and «Complete all». The dates of the stamps are optional details.
/// Saved to the player on «Save».
/// </summary>
public sealed class AchievementsWindow : PlayerDialog
{
    private enum ListFilter { All, InProgress, Completed }

    private readonly Personal Pers;
    private readonly AchievementList Records;
    private readonly RecordRow[] All;
    private readonly ListBox List;
    private readonly TextBox SearchText;
    private readonly ContentControl Details = new();
    private readonly TextBlock Summary = new() { Classes = { "cardtitle" }, VerticalAlignment = VerticalAlignment.Center };
    private ListFilter Filter;
    private string[] Words = [];

    public AchievementsWindow(Personal pers) : base(HomeStrings.Achievements, 980, 720)
    {
        Pers = pers;
        Records = pers.Achievements;
        All = [.. LifeSupportAchievement.List.Values.OrderBy(z => z.Index).Select(z => new RecordRow(Records, z))];

        List = new ListBox
        {
            ItemTemplate = new FuncDataTemplate<RecordRow>((r, _) => RowView(r)),
            Background = Brushes.Transparent,
        };
        List.SelectionChanged += (_, _) => BuildDetails();

        var search = SearchText = PlayerUi.SearchBox(PlayerStrings.SearchRecord, w =>
        {
            Words = w;
            Refilter();
        });
        var chips = PlayerUi.Chips([PlayerStrings.ShowAll, PlayerStrings.ShowInProgress, PlayerStrings.ShowCompleted], 0, i =>
        {
            Filter = (ListFilter)i;
            Refilter();
        });
        chips.Margin = new Thickness(10, 0, 0, 0);
        var filters = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto"), Margin = new Thickness(0, 0, 0, 10) };
        filters.Children.Add(search);
        Grid.SetColumn(chips, 1);
        filters.Children.Add(chips);

        var listCard = new Border { Child = List, Classes = { "card" }, Padding = new Thickness(6) };
        var left = new DockPanel();
        DockPanel.SetDock(filters, Dock.Top);
        left.Children.Add(filters);
        left.Children.Add(listCard);

        var detailsCard = new Border { Child = Details, Classes = { "card" }, Width = 360, Margin = new Thickness(16, 0, 0, 0) };
        var body = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto") };
        body.Children.Add(left);
        Grid.SetColumn(detailsCard, 1);
        body.Children.Add(detailsCard);

        var completeAll = PlayerUi.Button(PlayerStrings.CompleteAll, () => _ = CompleteAll());
        var more = PlayerUi.MenuButton(HomeStrings.More, (PlayerStrings.ResetAll, () => _ = ResetAll()));
        SetLayout(HomeStrings.Achievements, PlayerStrings.RecordsHint, body, completeAll, more, Summary);

        UpdateSummary();
        Refilter();
        List.SelectedIndex = 0;
        Opened += (_, _) => search.Focus();
    }

    /// <summary>
    /// Records with every stamp earned, and how many records there are.
    /// </summary>
    public static (int Completed, int Total) CountCompleted(AchievementList records)
    {
        var rows = LifeSupportAchievement.List.Values.Select(z => new RecordRow(records, z)).ToArray();
        return (rows.Count(z => z.IsComplete), rows.Length);
    }

    public static string GetName(LifeSupportAchievement a) => PlayerStrings.GetText($"Record{a.Name}") ?? PlayerUi.SplitWords(a.Name);

    private void Refilter()
    {
        var selected = List.SelectedItem;
        IEnumerable<RecordRow> rows = All;
        rows = Filter switch
        {
            ListFilter.InProgress => rows.Where(z => !z.IsComplete),
            ListFilter.Completed => rows.Where(z => z.IsComplete),
            _ => rows,
        };
        if (Words.Length != 0)
            rows = rows.Where(z => PlayerUi.Matches(z.Key, Words));
        var list = rows.ToArray();
        List.ItemsSource = list;
        if (selected is RecordRow r && list.Contains(r))
            List.SelectedItem = r;
        else if (list.Length != 0)
            List.SelectedIndex = 0;
        else
            BuildDetails();
    }

    private void UpdateSummary() => Summary.Text = string.Format(PlayerStrings.RecordsDone, All.Count(z => z.IsComplete), All.Length);

    private void RefreshAll()
    {
        foreach (var row in All)
            row.NotifyAll();
        UpdateSummary();
        BuildDetails();
    }

    internal void Search(string text)
    {
        SearchText.Text = text;
        Words = PlayerUi.Words(text);
        Refilter();
    }

    internal void Select(int index) => List.SelectedItem = All.FirstOrDefault(z => z.Index == index);

    internal void CompleteAllNow()
    {
        Records.GiveAll(DateTime.Now);
        foreach (var row in All)
            row.CompleteSteps();
        RefreshAll();
    }

    private async Task CompleteAll()
    {
        if (await Dialogs.Prompt(MessageBoxButtons.YesNo, PlayerStrings.CompleteAllAsk) != DialogResult.Yes)
            return;
        CompleteAllNow();
        Sound.Asterisk();
    }

    private async Task ResetAll()
    {
        if (await Dialogs.Prompt(MessageBoxButtons.YesNo, PlayerStrings.ResetAllAsk) != DialogResult.Yes)
            return;
        Records.ClearAll();
        RefreshAll();
    }

    protected override void Save() => Pers.Achievements = Records;

    private static Control RowView(RecordRow? r)
    {
        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto"), Margin = new Thickness(4, 3) };
        if (r is null)
            return g;
        var text = new StackPanel { Spacing = 1 };
        text.Children.Add(new TextBlock { Text = r.Name, FontSize = 14, FontWeight = FontWeight.SemiBold, TextTrimming = TextTrimming.CharacterEllipsis });
        var progress = PlayerUi.Subtle(string.Empty);
        progress.Bind(TextBlock.TextProperty, new Binding(nameof(RecordRow.Progress)));
        text.Children.Add(progress);
        g.Children.Add(text);

        // The stamps, redrawn when the count changes.
        var dots = new ContentControl { VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(10, 0, 4, 0) };
        dots.Bind(ContentProperty, new Binding(nameof(RecordRow.Earned)));
        dots.ContentTemplate = new FuncDataTemplate<int>((earned, _) => PlayerUi.Dots(earned, r.StampCount));
        Grid.SetColumn(dots, 1);
        g.Children.Add(dots);
        return g;
    }

    #region Details of the chosen record
    private void BuildDetails()
    {
        if (List.SelectedItem is not RecordRow row)
        {
            Details.Content = PlayerUi.Subtle(PlayerStrings.NothingFound, 14);
            return;
        }

        var panel = new StackPanel { Spacing = 10 };
        var title = HomeUi.Text(row.Name, "cardtitle");
        title.FontSize = 19;
        panel.Children.Add(title);

        var progress = PlayerUi.Subtle(row.Progress, 13);
        var stamps = new WrapPanel { ItemSpacing = 6, LineSpacing = 6 };

        if (!row.IsSteps)
        {
            // How many, big, with «Max»
            var count = new NumericUpDown
            {
                Minimum = 0,
                Maximum = uint.MaxValue,
                Value = row.Count,
                FormatString = "N0",
                ShowButtonSpinner = false,
                FontSize = 26,
                FontWeight = FontWeight.Bold,
                Width = 200,
            };
            count.ValueChanged += (_, _) =>
            {
                row.Count = (uint)Math.Clamp(count.Value ?? 0, 0, uint.MaxValue);
                progress.Text = row.Progress;
                FillStamps(stamps, row);
                UpdateSummary();
            };
            var max = PlayerUi.Button(HomeStrings.MaxButton, () => Max(row));
            max.VerticalAlignment = VerticalAlignment.Center;
            var line = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
            line.Children.Add(count);
            line.Children.Add(max);
            panel.Children.Add(line);
        }
        else
        {
            var steps = HomeUi.Text(string.Format(PlayerStrings.StepsDone, row.Earned, row.StampCount), "amount");
            var max = PlayerUi.Button(HomeStrings.MaxButton, () => Max(row));
            max.VerticalAlignment = VerticalAlignment.Center;
            var line = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 12 };
            line.Children.Add(steps);
            line.Children.Add(max);
            panel.Children.Add(line);
        }
        if (!row.IsSteps)
            panel.Children.Add(progress);

        // The stamps as a track: the reached ones in green
        panel.Children.Add(new TextBlock { Text = PlayerStrings.Stamps, FontWeight = FontWeight.SemiBold, Margin = new Thickness(0, 8, 0, 0) });
        if (row.IsSteps)
            panel.Children.Add(PlayerUi.Subtle(PlayerStrings.RecordStepsHint));
        FillStamps(stamps, row);
        panel.Children.Add(stamps);

        var reset = PlayerUi.Button(PlayerStrings.ResetRecord, () =>
        {
            Records.ClearAll(row.Index);
            row.NotifyAll();
            UpdateSummary();
            BuildDetails();
        });
        reset.HorizontalAlignment = HorizontalAlignment.Left;
        reset.Margin = new Thickness(0, 10, 0, 0);
        panel.Children.Add(reset);

        panel.Children.Add(Dates(row));

        Details.Content = new ScrollViewer { Content = panel, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled };
    }

    private void Max(RecordRow row)
    {
        Records.GiveAll(row.Index, DateTime.Now);
        row.CompleteSteps();
        row.NotifyAll();
        UpdateSummary();
        BuildDetails();
    }

    /// <summary>
    /// One pill per stamp: what it takes (e.g. «1.000»), green with a tick when reached. Step records
    /// (each kind of fruit, each golden tool…) have numbered pills that are clicked on and off.
    /// </summary>
    private void FillStamps(WrapPanel stamps, RecordRow row)
    {
        stamps.Children.Clear();
        for (int i = 0; i < row.StampCount; i++)
        {
            var stamp = i;
            var reached = row.IsReached(i);
            var label = row.IsSteps ? $"{i + 1}" : row.Info.GetThresholdValue(i).ToString("N0");
            var text = new TextBlock { Text = reached ? $"✓ {label}" : label, FontWeight = reached ? FontWeight.SemiBold : FontWeight.Normal };
            var pill = new Border
            {
                Child = text,
                CornerRadius = new CornerRadius(13),
                Padding = new Thickness(8, 3),
                BorderThickness = new Thickness(1.5),
                MinWidth = 40,
            };
            text.HorizontalAlignment = HorizontalAlignment.Center;
            pill.Bind(Border.BorderBrushProperty, pill.GetResourceObservable(reached ? "NhGreenBrush" : "NhCardBorderBrush"));
            if (reached)
                pill.Bind(Border.BackgroundProperty, pill.GetResourceObservable("NhSelectedBrush"));
            var tip = row.IsSteps ? string.Format(PlayerStrings.StampStep, i + 1) : string.Format(PlayerStrings.StampAt, label);
            if (PlayerUi.ToDate(Records.Date[row.Index][i]) is { } date)
                tip += " · " + string.Format(PlayerStrings.StampEarnedOn, PlayerUi.FormatDate(date));
            ToolTip.SetTip(pill, tip);

            if (row.IsSteps)
            {
                pill.Cursor = new Avalonia.Input.Cursor(Avalonia.Input.StandardCursorType.Hand);
                pill.PointerPressed += (_, e) =>
                {
                    row.SetStep(stamp, !row.IsReached(stamp));
                    UpdateSummary();
                    BuildDetails();
                    e.Handled = true;
                };
            }
            stamps.Children.Add(pill);
        }
    }

    /// <summary>
    /// The day each stamp was earned, for those who care (hidden by default).
    /// </summary>
    private Expander Dates(RecordRow row)
    {
        var panel = new StackPanel { Spacing = 6 };
        panel.Children.Add(PlayerUi.Subtle(PlayerStrings.StampDatesHint));
        for (int i = 0; i < row.StampCount; i++)
        {
            var stamp = i;
            var picker = PlayerUi.DatePicker(PlayerUi.ToDate(Records.Date[row.Index][i]), 190);
            picker.SelectedDateChanged += (_, _) =>
            {
                Records.Date[row.Index][stamp] = picker.SelectedDate ?? DateTime.MinValue;
            };
            var label = row.IsSteps ? string.Format(PlayerStrings.StampStep, i + 1) : row.Info.GetThresholdValue(i).ToString("N0");
            panel.Children.Add(PlayerUi.Labeled(label, 80, picker));
        }
        return new Expander { Header = PlayerStrings.StampDates, Content = panel, HorizontalAlignment = HorizontalAlignment.Stretch, Margin = new Thickness(0, 8, 0, 0) };
    }
    #endregion
}

/// <summary>
/// One Nook Miles+ record of the list.
/// </summary>
public sealed class RecordRow(AchievementList records, LifeSupportAchievement info) : RowModel
{
    public LifeSupportAchievement Info { get; } = info;
    public int Index => Info.Index;
    public string Name { get; } = AchievementsWindow.GetName(info);
    public string Key => ItemSearchBox.Normalize(Name);
    public int StampCount => Info.AchievementCount;

    /// <summary>
    /// Records whose stamps are separate steps (each fruit, each golden tool…) instead of a count to reach.
    /// </summary>
    public bool IsSteps => Info.GetThresholdValue(0) == 0;

    public uint Count
    {
        get => records.Counts[Index];
        set
        {
            records.Counts[Index] = value;
            NotifyAll();
        }
    }

    public bool IsReached(int stamp) => Info.GetIsSatisfied(stamp, Count);

    /// <summary>
    /// Ticks every step of a step record (the game's «give all» only raises counts).
    /// </summary>
    public void CompleteSteps()
    {
        if (IsSteps)
            Count |= (1u << StampCount) - 1;
    }

    public void SetStep(int stamp, bool value)
    {
        var bit = 1u << stamp;
        Count = value ? Count | bit : Count & ~bit;
    }

    public int Earned => Enumerable.Range(0, StampCount).Count(IsReached);

    public bool IsComplete => Earned == StampCount;

    public string Progress
    {
        get
        {
            if (IsSteps)
                return string.Format(PlayerStrings.StepsDone, Earned, StampCount);
            var count = Count;
            for (int i = 0; i < StampCount; i++)
            {
                var threshold = Info.GetThresholdValue(i);
                if (count < threshold)
                    return string.Format(PlayerStrings.NextStamp, count.ToString("N0"), threshold.ToString("N0"));
            }
            return string.Format(PlayerStrings.AllStamps, count.ToString("N0"));
        }
    }
}
