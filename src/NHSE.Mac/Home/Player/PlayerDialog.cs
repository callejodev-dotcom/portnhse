using System.ComponentModel;
using System.Globalization;
using System.Runtime.CompilerServices;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Skeleton of the friendly player windows (recipes, reactions, records, catalog, profile, mailbox):
/// big title, a one-line explanation, the content, and at the bottom the extra tools on the left and
/// «Cancel» / «Save» on the right. Closing without saving (Cancel, Esc, ⌘W, the close button) calls <see cref="Discard"/>.
/// </summary>
public abstract class PlayerDialog : NhseWindow
{
    private bool Saved;

    protected PlayerDialog(string title, double width, double height)
    {
        Title = title;
        Width = width;
        Height = height;
        MinWidth = Math.Min(width, 620);
        MinHeight = Math.Min(height, 460);
        this.Bind(BackgroundProperty, this.GetResourceObservable("NhWindowBrush"));
    }

    /// <summary>
    /// Writes the changes back to the save. Called by «Save» just before the window closes with true.
    /// </summary>
    protected abstract void Save();

    /// <summary>
    /// Undoes what was edited in place, when the window closes without saving.
    /// </summary>
    protected virtual void Discard()
    {
    }

    /// <summary>
    /// Saves and closes, as the «Save» button does (used by the tests).
    /// </summary>
    public void SaveAndClose()
    {
        Save();
        Saved = true;
        Close(true);
    }

    protected override void OnClosed(EventArgs e)
    {
        if (!Saved)
            Discard();
        base.OnClosed(e);
    }

    /// <param name="title">Big title.</param>
    /// <param name="hint">One line saying what the window is for.</param>
    /// <param name="body">Content (fills the window).</param>
    /// <param name="tools">Buttons at the bottom left (e.g. a «More» menu).</param>
    protected void SetLayout(string title, string hint, Control body, params Control[] tools)
    {
        var header = new StackPanel { Margin = new Thickness(0, 0, 0, 14), Spacing = 2 };
        header.Children.Add(HomeUi.Text(title, "title"));
        header.Children.Add(HomeUi.Text(hint, "subtle"));

        var save = new Button { Content = HomeStrings.Done, Classes = { "accent" }, IsDefault = true, MinWidth = 100 };
        save.Click += (_, _) => SaveAndClose();
        var cancel = new Button { Content = HomeStrings.Cancel, IsCancel = true, MinWidth = 100 };
        cancel.Click += (_, _) => Close(false);

        var left = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        foreach (var t in tools)
            left.Children.Add(t);
        var right = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        right.Children.Add(cancel);
        right.Children.Add(save);
        var bar = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto"), Margin = new Thickness(0, 16, 0, 0) };
        bar.Children.Add(left);
        Grid.SetColumn(right, 1);
        bar.Children.Add(right);

        var root = new DockPanel { Margin = new Thickness(22, 18, 22, 18) };
        DockPanel.SetDock(header, Dock.Top);
        DockPanel.SetDock(bar, Dock.Bottom);
        root.Children.Add(header);
        root.Children.Add(bar);
        root.Children.Add(body);
        Content = root;
    }
}

/// <summary>
/// Small building blocks of the friendly player windows (private to them, so the shared HomeUi stays as it is).
/// </summary>
public static class PlayerUi
{
    /// <summary>
    /// Row of filter chips; exactly one is on.
    /// </summary>
    public static WrapPanel Chips(IReadOnlyList<string> names, int selected, Action<int> changed)
    {
        var panel = new WrapPanel { ItemSpacing = 4, LineSpacing = 4, VerticalAlignment = VerticalAlignment.Center };
        var chips = new List<ToggleButton>();
        for (int i = 0; i < names.Count; i++)
        {
            var index = i;
            var chip = new ToggleButton { Content = names[i], IsChecked = i == selected, Classes = { "chip" } };
            chip.Click += (_, _) =>
            {
                foreach (var other in chips)
                    other.IsChecked = other == chip;
                changed(index);
            };
            chips.Add(chip);
            panel.Children.Add(chip);
        }
        return panel;
    }

    /// <summary>
    /// Turns on one chip of a <see cref="Chips"/> row (when the filter is changed from code).
    /// </summary>
    public static void SelectChip(Panel chips, int index)
    {
        for (int i = 0; i < chips.Children.Count; i++)
        {
            if (chips.Children[i] is ToggleButton chip)
                chip.IsChecked = i == index;
        }
    }

    /// <summary>
    /// Search box that reports the typed words, without case or accents (see <see cref="ItemSearchBox.Normalize"/>).
    /// </summary>
    public static TextBox SearchBox(string placeholder, Action<string[]> changed)
    {
        var box = new TextBox { PlaceholderText = placeholder, MinWidth = 220 };
        box.TextChanged += (_, _) => changed(Words(box.Text));
        return box;
    }

    public static string[] Words(string? text) => ItemSearchBox.Normalize(text ?? string.Empty).Split(' ', StringSplitOptions.RemoveEmptyEntries);

    public static bool Matches(string key, string[] words) => words.All(w => key.Contains(w, StringComparison.Ordinal));

    private static readonly char[] NotLetter = [.. "¡!¿?()[]\"'«»-—–.,:;#*+ "];

    /// <summary>
    /// Sort key of a normalized name: names starting with ¡ ¿ ( are sorted by their first letter.
    /// </summary>
    public static string SortKey(string normalized) => normalized.TrimStart(NotLetter);

    /// <summary>
    /// Button that opens a menu (e.g. «More ▾»).
    /// </summary>
    public static Button MenuButton(string text, params (string Text, Action Action)[] items)
    {
        var flyout = new MenuFlyout { Placement = PlacementMode.TopEdgeAlignedLeft };
        foreach (var (header, action) in items)
        {
            var m = new MenuItem { Header = header };
            m.Click += (_, _) => action();
            flyout.Items.Add(m);
        }
        return new Button { Content = text + " ▾", Flyout = flyout };
    }

    public static Button Button(string text, Action click, bool accent = false)
    {
        var b = new Button { Content = text, Padding = new Thickness(12, 5) };
        if (accent)
            b.Classes.Add("accent");
        b.Click += (_, _) => click();
        return b;
    }

    /// <summary>
    /// Label on the left, controls on the right.
    /// </summary>
    public static Grid Labeled(string label, double labelWidth, params Control[] controls)
    {
        var g = new Grid { ColumnDefinitions = new ColumnDefinitions($"{labelWidth.ToString(CultureInfo.InvariantCulture)},*"), ColumnSpacing = 10 };
        g.Children.Add(new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center, TextWrapping = TextWrapping.Wrap });
        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6 };
        foreach (var c in controls)
            row.Children.Add(c);
        Grid.SetColumn(row, 1);
        g.Children.Add(row);
        return g;
    }

    /// <summary>
    /// A colour of the home theme (NhGreenBrush, NhSubtleBrush…) for the current light/dark mode.
    /// </summary>
    public static IBrush ThemeBrush(string key)
    {
        var app = Application.Current;
        if (app is not null && app.TryGetResource(key, app.ActualThemeVariant, out var value) && value is IBrush brush)
            return brush;
        return Brushes.Gray;
    }

    /// <summary>
    /// Text in the theme's subtle colour, following light/dark changes.
    /// </summary>
    public static TextBlock Subtle(string text, double size = 12) => new()
    {
        Text = text,
        FontSize = size,
        TextWrapping = TextWrapping.Wrap,
        Classes = { "subtle" },
    };

    /// <summary>
    /// Culture of the game language, for month names and dates (es → es-ES, jp → ja-JP…).
    /// </summary>
    public static CultureInfo GameCulture
    {
        get
        {
            var name = GameInfo.CurrentLanguage switch
            {
                "jp" => "ja-JP",
                "zhs" => "zh-Hans",
                "zht" => "zh-Hant",
                "ko" => "ko-KR",
                "es" => "es-ES",
                "en" => "en-US",
                var other => other,
            };
            try
            {
                return CultureInfo.GetCultureInfo(name);
            }
            catch (CultureNotFoundException)
            {
                return CultureInfo.InvariantCulture;
            }
        }
    }

    /// <summary>
    /// A date of the save, or null when it is empty or not a real day.
    /// </summary>
    public static DateTime? ToDate(GSaveDate d)
    {
        if (d.IsEmpty || d.Month is < 1 or > 12 || d.Day < 1 || d.Year < 1 || d.Day > DateTime.DaysInMonth(d.Year, d.Month))
            return null;
        return new DateTime(d.Year, d.Month, d.Day);
    }

    /// <summary>
    /// Date picker that shows the date in words («7 de abril de 2026»).
    /// </summary>
    public static CalendarDatePicker DatePicker(DateTime? date, double width = 220) => new()
    {
        SelectedDate = date,
        SelectedDateFormat = CalendarDatePickerFormat.Custom,
        CustomDateFormatString = "d MMMM yyyy",
        PlaceholderText = PlayerStrings.NoDate,
        Width = width,
        HorizontalAlignment = HorizontalAlignment.Left,
    };

    /// <summary>
    /// A date as the player reads it (e.g. «12 abr 2020»).
    /// </summary>
    public static string FormatDate(DateTime date) => date.ToString("d MMM yyyy", GameCulture);

    /// <summary>
    /// "CatchFish" → "Catch fish", "Cold_Chill" → "Cold chill" (when there is no friendlier name).
    /// </summary>
    public static string SplitWords(string name)
    {
        var sb = new System.Text.StringBuilder(name.Length + 8);
        for (int i = 0; i < name.Length; i++)
        {
            var c = name[i];
            if (c == '_')
            {
                sb.Append(' ');
                continue;
            }
            if (i > 0 && char.IsUpper(c) && name[i - 1] != '_' && !char.IsUpper(name[i - 1]))
                sb.Append(' ').Append(char.ToLowerInvariant(c));
            else
                sb.Append(i == 0 ? c : char.ToLowerInvariant(c));
        }
        return sb.ToString();
    }

    /// <summary>
    /// Large round dots for the stamps of a record or similar progress: filled (green) and empty.
    /// </summary>
    public static StackPanel Dots(int filled, int total, double size = 11)
    {
        var p = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 4, VerticalAlignment = VerticalAlignment.Center };
        for (int i = 0; i < total; i++)
        {
            var dot = new Border { Width = size, Height = size, CornerRadius = new CornerRadius(size / 2), BorderThickness = new Thickness(1.5) };
            dot.Bind(Border.BorderBrushProperty, dot.GetResourceObservable(i < filled ? "NhGreenBrush" : "NhSubtleBrush"));
            if (i < filled)
                dot.Bind(Border.BackgroundProperty, dot.GetResourceObservable("NhGreenBrush"));
            p.Children.Add(dot);
        }
        return p;
    }
}

/// <summary>
/// Base of the row models shown in the virtualized lists.
/// </summary>
public abstract class RowModel : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;

    protected void Notify([CallerMemberName] string? name = null) => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    /// <summary>
    /// Tells the list that every value of the row may have changed.
    /// </summary>
    public void NotifyAll() => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(string.Empty));
}
