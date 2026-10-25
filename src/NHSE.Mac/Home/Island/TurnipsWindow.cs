using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Shapes;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Friendly turnip editor: Daisy Mae's price, the week's selling prices as a table with a small bar chart,
/// and the week's trend explained in plain words. The prices are edited in place; the caller keeps them only if
/// the window returns true.
/// </summary>
public sealed class TurnipsWindow : NhseWindow
{
    private const double PriceWidth = 78;
    private const double ChartHeight = 84;
    private const uint MaxPrice = 9_999;

    private readonly TurnipStonk Turnips;
    private readonly NumericUpDown Buy;
    private readonly NumericUpDown[] Prices = new NumericUpDown[14]; // half days, Sunday morning = 0
    private readonly List<Control> ChartParts = [];
    private readonly TextBlock Best = new() { TextWrapping = TextWrapping.Wrap, FontWeight = FontWeight.SemiBold };
    private readonly Grid Table;
    private readonly RadioButton[] Patterns = new RadioButton[4];
    private readonly ComboBox SpikeStart = new() { MinWidth = 200 };
    private readonly Control SpikeRow;
    private readonly List<int> SpikeValues = [];

    public TurnipsWindow(TurnipStonk turnips)
    {
        Turnips = turnips;
        SizeToContent = SizeToContent.WidthAndHeight;
        CanResize = false;

        // Daisy Mae's price
        Buy = Amount(turnips.BuyPrice, Math.Max(MaxPrice, turnips.BuyPrice));
        Buy.Width = 150;
        Buy.HorizontalAlignment = HorizontalAlignment.Left;
        Buy.ValueChanged += (_, _) =>
        {
            Turnips.BuyPrice = (uint)(Buy.Value ?? 0);
            DrawChart();
        };
        Best.Margin = new Thickness(0, 10, 0, 0);
        Best.Themed(TextBlock.ForegroundProperty, "NhGreenBrush");
        var buyCard = Card(PortraitHeader("boc", IslandStrings.BuyTitle, IslandStrings.BuyHint, Buy, Best));

        // The week
        Table = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions($"Auto,{string.Join(',', Enumerable.Repeat(PriceWidth, 6))}"),
            RowDefinitions = new RowDefinitions($"Auto,Auto,Auto,{ChartHeight + 4}"),
            ColumnSpacing = 6,
            RowSpacing = 6,
            Margin = new Thickness(0, 10, 0, 0),
        };
        AddCell(new TextBlock { Text = IslandStrings.Morning, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 0, 6, 0) }, 1, 0);
        AddCell(new TextBlock { Text = IslandStrings.Afternoon, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 0, 6, 0) }, 2, 0);
        for (int d = 0; d < 6; d++)
        {
            var day = (DayOfWeek)(d + 1);
            AddCell(new TextBlock { Text = IslandUi.DayName(day), FontWeight = FontWeight.SemiBold, HorizontalAlignment = HorizontalAlignment.Center }, 0, d + 1);
            for (int pm = 0; pm < 2; pm++)
            {
                var half = ((d + 1) * 2) + pm;
                var value = GetPrice(turnips, half);
                var box = new NumericUpDown
                {
                    Minimum = 0,
                    Maximum = Math.Max(MaxPrice, value),
                    Value = value,
                    FormatString = "0",
                    ShowButtonSpinner = false,
                    TextAlignment = TextAlignment.Center,
                    HorizontalAlignment = HorizontalAlignment.Stretch,
                };
                ToolTip.SetTip(box, string.Format(pm == 0 ? IslandStrings.DayMorning : IslandStrings.DayAfternoon, IslandUi.DayName(day)));
                box.ValueChanged += (_, _) =>
                {
                    SetPrice(Turnips, half, (uint)(box.Value ?? 0));
                    DrawChart();
                };
                Prices[half] = box;
                AddCell(box, pm + 1, d + 1);
            }
        }

        var all = new NumericUpDown { Minimum = 0, Maximum = MaxPrice, Value = 500, FormatString = "0", ShowButtonSpinner = false, Width = 90, TextAlignment = TextAlignment.Center };
        var apply = new Button { Content = HomeStrings.Apply };
        apply.Click += (_, _) =>
        {
            for (int half = 2; half < Prices.Length; half++)
                Prices[half].Value = all.Value ?? 0;
        };
        var allRow = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Margin = new Thickness(0, 12, 0, 0) };
        allRow.Children.Add(new TextBlock { Text = IslandStrings.AllPrices, VerticalAlignment = VerticalAlignment.Center });
        allRow.Children.Add(all);
        allRow.Children.Add(apply);

        var legend = Text(IslandStrings.ChartLegend, "subtle");
        legend.FontSize = 12;
        legend.Margin = new Thickness(0, 4, 0, 0);
        var weekCard = Card(CardHeader("Kabu", IslandStrings.WeekTitle, IslandStrings.WeekHint, Table, legend, allRow));

        // The trend
        var patterns = new UniformGrid { Columns = 4, ColumnSpacing = 14, Margin = new Thickness(0, 10, 0, 0) };
        for (int i = 0; i < Patterns.Length; i++)
        {
            var pattern = (TurnipPattern)i;
            var radio = new RadioButton
            {
                GroupName = "pattern",
                Content = PatternContent(pattern),
                IsChecked = turnips.Pattern == pattern,
                HorizontalAlignment = HorizontalAlignment.Stretch,
                VerticalAlignment = VerticalAlignment.Top,
                VerticalContentAlignment = VerticalAlignment.Top,
            };
            radio.IsCheckedChanged += (_, _) =>
            {
                if (radio.IsChecked != true)
                    return;
                Turnips.Pattern = pattern;
                UpdateSpikeRow();
            };
            Patterns[i] = radio;
            patterns.Children.Add(radio);
        }

        FillSpikeStart(turnips.FeverStart);
        SpikeStart.SelectionChanged += (_, _) =>
        {
            if (SpikeStart.SelectedIndex >= 0)
                Turnips.FeverStart = (uint)SpikeValues[SpikeStart.SelectedIndex];
        };
        SpikeRow = IslandUi.Labeled(IslandStrings.SpikeStart, 130, SpikeStart);
        SpikeRow.Margin = new Thickness(0, 8, 0, 0);

        var patternCard = Card(CardHeader("BadKabu", IslandStrings.PatternTitle, IslandStrings.PatternHint, patterns, SpikeRow));

        // Daisy Mae and the week side by side, the trend below
        var body = new Grid { ColumnDefinitions = new ColumnDefinitions("290,660"), RowDefinitions = new RowDefinitions("Auto,Auto"), ColumnSpacing = 12, RowSpacing = 12 };
        body.Children.Add(buyCard);
        Grid.SetColumn(weekCard, 1);
        body.Children.Add(weekCard);
        Grid.SetRow(patternCard, 1);
        Grid.SetColumnSpan(patternCard, 2);
        body.Children.Add(patternCard);

        IslandUi.Frame(this, IslandStrings.TurnipsTitle, IslandStrings.TurnipsHint, body, () => { });

        DrawChart();
        UpdateSpikeRow();
    }

    private void AddCell(Control c, int row, int column)
    {
        Grid.SetRow(c, row);
        Grid.SetColumn(c, column);
        Table.Children.Add(c);
    }

    /// <summary>
    /// Card header with a character's portrait instead of a menu icon.
    /// </summary>
    internal static Grid PortraitHeader(string code, string title, string hint, params Control[] below)
    {
        var g = CardHeader("Leaf", title, hint, below);
        g.Children.RemoveAt(0);
        var portrait = IslandUi.Portrait(code, 56);
        portrait.VerticalAlignment = VerticalAlignment.Top;
        g.Children.Insert(0, portrait);
        return g;
    }

    #region Prices
    /// <summary>
    /// Selling price of a half day (0 = Sunday morning, 13 = Saturday afternoon).
    /// </summary>
    public static uint GetPrice(TurnipStonk t, int half) => half switch
    {
        0 => t.SellSundayAM,
        1 => t.SellSundayPM,
        2 => t.SellMondayAM,
        3 => t.SellMondayPM,
        4 => t.SellTuesdayAM,
        5 => t.SellTuesdayPM,
        6 => t.SellWednesdayAM,
        7 => t.SellWednesdayPM,
        8 => t.SellThursdayAM,
        9 => t.SellThursdayPM,
        10 => t.SellFridayAM,
        11 => t.SellFridayPM,
        12 => t.SellSaturdayAM,
        13 => t.SellSaturdayPM,
        _ => throw new ArgumentOutOfRangeException(nameof(half)),
    };

    public static void SetPrice(TurnipStonk t, int half, uint value)
    {
        switch (half)
        {
            case 0: t.SellSundayAM = value; break;
            case 1: t.SellSundayPM = value; break;
            case 2: t.SellMondayAM = value; break;
            case 3: t.SellMondayPM = value; break;
            case 4: t.SellTuesdayAM = value; break;
            case 5: t.SellTuesdayPM = value; break;
            case 6: t.SellWednesdayAM = value; break;
            case 7: t.SellWednesdayPM = value; break;
            case 8: t.SellThursdayAM = value; break;
            case 9: t.SellThursdayPM = value; break;
            case 10: t.SellFridayAM = value; break;
            case 11: t.SellFridayPM = value; break;
            case 12: t.SellSaturdayAM = value; break;
            case 13: t.SellSaturdayPM = value; break;
            default: throw new ArgumentOutOfRangeException(nameof(half));
        }
    }

    /// <summary>
    /// Price box of a half day (tests).
    /// </summary>
    internal NumericUpDown PriceBox(DayOfWeek day, bool afternoon) => Prices[((int)day * 2) + (afternoon ? 1 : 0)];

    internal NumericUpDown BuyBox => Buy;

    internal RadioButton PatternButton(TurnipPattern p) => Patterns[(int)p];

    /// <summary>
    /// Two bars per day (morning and afternoon) under the price boxes; green when selling earns more than the buy price.
    /// </summary>
    private void DrawChart()
    {
        foreach (var c in ChartParts)
            Table.Children.Remove(c);
        ChartParts.Clear();

        var buy = Turnips.BuyPrice;
        uint max = buy;
        for (int half = 2; half < 14; half++)
            max = Math.Max(max, GetPrice(Turnips, half));

        // When to sell
        var best = Enumerable.Range(2, 12).MaxBy(z => GetPrice(Turnips, z));
        var bestPrice = GetPrice(Turnips, best);
        Best.Text = bestPrice == 0 ? null : string.Format(IslandStrings.BestPrice, HalfDayName(best, false), bestPrice);
        Best.IsVisible = Best.Text is not null;

        if (max == 0)
            return;

        double Height(uint v) => Math.Max(2, ChartHeight * v / max);

        for (int d = 0; d < 6; d++)
        {
            var bars = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 4, HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Bottom };
            for (int pm = 0; pm < 2; pm++)
            {
                var half = ((d + 1) * 2) + pm;
                var v = GetPrice(Turnips, half);
                var bar = new Border
                {
                    Width = 24,
                    Height = Height(v),
                    CornerRadius = new CornerRadius(4, 4, 0, 0),
                    VerticalAlignment = VerticalAlignment.Bottom,
                    Opacity = v > buy ? (pm == 0 ? 1 : 0.75) : 0.35,
                };
                bar.Themed(Border.BackgroundProperty, v > buy ? "NhGreenBrush" : "NhSubtleBrush");
                ToolTip.SetTip(bar, string.Format(IslandStrings.Bells, v));
                bars.Children.Add(bar);
            }
            Grid.SetRow(bars, 3);
            Grid.SetColumn(bars, d + 1);
            ChartParts.Add(bars);
        }

        // Daisy Mae's price as a line across the chart
        if (buy != 0)
        {
            var line = new Border { Height = 1.5, VerticalAlignment = VerticalAlignment.Bottom, Margin = new Thickness(0, 0, 0, Height(buy)), Opacity = 0.8 };
            line.Themed(Border.BackgroundProperty, "NhSubtleBrush");
            Grid.SetRow(line, 3);
            Grid.SetColumn(line, 1);
            Grid.SetColumnSpan(line, 6);
            ChartParts.Add(line);

            var label = new TextBlock
            {
                Text = buy.ToString(),
                FontSize = 11,
                VerticalAlignment = VerticalAlignment.Bottom,
                HorizontalAlignment = HorizontalAlignment.Right,
                Margin = new Thickness(0, 0, 6, Math.Max(0, Height(buy) - 7)),
                Classes = { "subtle" },
            };
            Grid.SetRow(label, 3);
            ChartParts.Add(label);
        }

        foreach (var c in ChartParts)
            Table.Children.Add(c);
    }
    #endregion

    #region Trend
    internal static string PatternName(TurnipPattern p) => p switch
    {
        TurnipPattern.Fluctuating => IslandStrings.PatternFluctuating,
        TurnipPattern.LargeSpike => IslandStrings.PatternLargeSpike,
        TurnipPattern.Decreasing => IslandStrings.PatternDecreasing,
        TurnipPattern.SmallSpike => IslandStrings.PatternSmallSpike,
        _ => p.ToString(),
    };

    private static Control PatternContent(TurnipPattern p)
    {
        var (name, hint, points) = p switch
        {
            TurnipPattern.Fluctuating => (PatternName(p), IslandStrings.PatternFluctuatingHint,
                new[] { 0, 12, 7, 6, 14, 16, 21, 8, 28, 18, 35, 7, 44, 13 }),
            TurnipPattern.LargeSpike => (PatternName(p), IslandStrings.PatternLargeSpikeHint,
                new[] { 0, 12, 8, 14, 16, 17, 22, 19, 27, 1, 32, 17, 38, 20, 44, 21 }),
            TurnipPattern.Decreasing => (PatternName(p), IslandStrings.PatternDecreasingHint,
                new[] { 0, 4, 11, 8, 22, 12, 33, 16, 44, 20 }),
            _ => (PatternName(p), IslandStrings.PatternSmallSpikeHint,
                new[] { 0, 12, 8, 14, 16, 17, 22, 11, 27, 7, 32, 11, 38, 17, 44, 19 }),
        };

        var line = new Polyline { StrokeThickness = 2, StrokeJoin = PenLineJoin.Round, StrokeLineCap = PenLineCap.Round };
        for (int i = 0; i < points.Length; i += 2)
            line.Points.Add(new Point(points[i], points[i + 1]));
        line.Themed(Shape.StrokeProperty, "NhGreenBrush");
        var spark = new Border { Child = line, Width = 46, Height = 24, Margin = new Thickness(0, 0, 8, 0), VerticalAlignment = VerticalAlignment.Center };

        var top = new StackPanel { Orientation = Orientation.Horizontal };
        top.Children.Add(new TextBlock { Text = name, FontWeight = FontWeight.SemiBold, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(0, 0, 8, 0) });
        top.Children.Add(spark);

        var text = new StackPanel { Spacing = 2 };
        text.Children.Add(top);
        var caption = Text(hint, "subtle");
        caption.FontSize = 12;
        text.Children.Add(caption);
        return text;
    }

    private void FillSpikeStart(uint current)
    {
        var names = new List<string>();
        for (int half = 2; half < 14; half++)
        {
            SpikeValues.Add(half);
            names.Add(HalfDayName(half));
        }
        if (!SpikeValues.Contains((int)current))
        {
            SpikeValues.Add((int)current);
            names.Add(current < 14 ? HalfDayName((int)current) : current.ToString());
        }
        SpikeStart.ItemsSource = names;
        SpikeStart.SelectedIndex = SpikeValues.IndexOf((int)current);
    }

    /// <param name="half">Half day, 0 = Sunday morning.</param>
    /// <param name="start">At the start of a sentence (capital letter) or in the middle (as the language writes day names).</param>
    private static string HalfDayName(int half, bool start = true)
    {
        var day = (DayOfWeek)(half / 2);
        var name = start ? IslandUi.DayName(day) : IslandUi.Culture.DateTimeFormat.GetDayName(day);
        return string.Format(half % 2 == 0 ? IslandStrings.DayMorning : IslandStrings.DayAfternoon, name);
    }

    private void UpdateSpikeRow() => SpikeRow.IsVisible = Turnips.Pattern is TurnipPattern.LargeSpike or TurnipPattern.SmallSpike;
    #endregion
}
