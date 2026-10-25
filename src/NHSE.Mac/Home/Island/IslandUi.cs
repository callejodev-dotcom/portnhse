using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Pieces shared by the friendly island and museum windows: the window skeleton, day names in the game
/// language and the special characters' names and portraits.
/// </summary>
internal static class IslandUi
{
    /// <summary>
    /// Days of the week starting on Monday, as the game's weeks are shown in Spain.
    /// </summary>
    public static readonly DayOfWeek[] Week =
        [DayOfWeek.Monday, DayOfWeek.Tuesday, DayOfWeek.Wednesday, DayOfWeek.Thursday, DayOfWeek.Friday, DayOfWeek.Saturday, DayOfWeek.Sunday];

    /// <summary>
    /// Builds the usual layout of a friendly dialog: big title, one line of explanation, the content and
    /// Cancel + Save at the bottom right (with optional buttons at the bottom left).
    /// </summary>
    /// <returns>The Save button.</returns>
    public static Button Frame(Window w, string title, string hint, Control body, Action onSave, params Control[] bottomLeft)
    {
        w.Title = title;

        var header = new StackPanel { Margin = new Thickness(0, 0, 0, 14) };
        header.Children.Add(Text(title, "title"));
        header.Children.Add(Text(hint, "subtle"));

        var save = new Button { Content = HomeStrings.Done, Classes = { "accent" }, IsDefault = true, MinWidth = 100 };
        save.Click += (_, _) =>
        {
            onSave();
            w.Close(true);
        };
        var cancel = new Button { Content = HomeStrings.Cancel, IsCancel = true, MinWidth = 100 };
        cancel.Click += (_, _) => w.Close(false);

        var bottom = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto"), Margin = new Thickness(0, 16, 0, 0) };
        var left = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        foreach (var c in bottomLeft)
            left.Children.Add(c);
        bottom.Children.Add(left);
        var right = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        right.Children.Add(cancel);
        right.Children.Add(save);
        Grid.SetColumn(right, 1);
        bottom.Children.Add(right);

        var root = new DockPanel { Margin = new Thickness(22, 18, 22, 18) };
        DockPanel.SetDock(header, Dock.Top);
        DockPanel.SetDock(bottom, Dock.Bottom);
        root.Children.Add(header);
        root.Children.Add(bottom);
        // On a small screen the window stops at its edge (see NhseWindow): the content scrolls, the buttons stay.
        root.Children.Add(body is ScrollViewer or TabControl ? body
            : new ScrollViewer { Content = body, HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Disabled });
        w.Content = root;
        w.Bind(Window.BackgroundProperty, w.GetResourceObservable("NhWindowBrush"));
        return save;
    }

    /// <summary>
    /// Culture of the game language, for day names and dates.
    /// </summary>
    public static CultureInfo Culture
    {
        get
        {
            var lang = GameInfo.CurrentLanguage switch
            {
                "jp" => "ja",
                "zhs" => "zh-Hans",
                "zht" => "zh-Hant",
                var l => l,
            };
            try
            {
                return CultureInfo.GetCultureInfo(lang);
            }
            catch (CultureNotFoundException)
            {
                return CultureInfo.CurrentCulture;
            }
        }
    }

    public static string DayName(DayOfWeek day) => Capitalize(Culture.DateTimeFormat.GetDayName(day));

    public static string ShortDayName(DayOfWeek day) => Capitalize(Culture.DateTimeFormat.GetAbbreviatedDayName(day).TrimEnd('.'));

    public static string FormatDate(DateTime date) => date.ToString("d MMM yyyy", Culture).Replace(".", string.Empty);

    public static string Capitalize(string s) => s.Length == 0 ? s : char.ToUpper(s[0], Culture) + s[1..];

    /// <summary>
    /// Name of an item in the game language, starting with a capital letter.
    /// </summary>
    public static string ItemName(ushort id) => Capitalize(GameInfo.Strings.GetItemName(new Item(id)));

    /// <summary>
    /// Portrait of a villager or special character by internal name ("cat01", "boc" for Daisy Mae…).
    /// </summary>
    public static Image Portrait(string code, double size) => new()
    {
        Source = VillagerSprite.GetVillagerSprite(code),
        Width = size,
        Height = size,
        VerticalAlignment = VerticalAlignment.Center,
    };

    /// <summary>
    /// Internal name of each special visitor, to show its name and portrait from the game data.
    /// </summary>
    public static string VisitorCode(VisitorNPC v) => v switch
    {
        VisitorNPC.Gulliver => "gul",
        VisitorNPC.Label => "hgc",
        VisitorNPC.Saharah => "cml",
        VisitorNPC.Wisp => "gstA",
        VisitorNPC.Mabel => "hgh",
        VisitorNPC.CJ => "bey",
        VisitorNPC.Flick => "chy",
        VisitorNPC.Kicks => "skk",
        VisitorNPC.Leif => "slo",
        VisitorNPC.Redd => "fox",
        VisitorNPC.Gullivarrr => "gulB",
        _ => string.Empty,
    };

    public static string VisitorName(VisitorNPC v) => v == VisitorNPC.None ? IslandStrings.Nobody : GameInfo.Strings.GetVillager(VisitorCode(v));

    /// <summary>
    /// Resource-bound brush (follows light and dark mode).
    /// </summary>
    public static T Themed<T>(this T control, AvaloniaProperty property, string key) where T : Control
    {
        control.Bind(property, control.GetResourceObservable(key));
        return control;
    }

    /// <summary>
    /// Label on the left, controls on the right.
    /// </summary>
    public static Grid Labeled(string label, double labelWidth, params Control[] controls)
    {
        var g = new Grid { ColumnDefinitions = new ColumnDefinitions($"{labelWidth},*"), ColumnSpacing = 8 };
        g.Children.Add(new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center, TextWrapping = TextWrapping.Wrap });
        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6 };
        foreach (var c in controls)
            row.Children.Add(c);
        Grid.SetColumn(row, 1);
        g.Children.Add(row);
        return g;
    }
}
