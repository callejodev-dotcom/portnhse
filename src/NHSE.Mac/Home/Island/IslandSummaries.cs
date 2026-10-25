using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// What the island and museum cards of the home window show under their explanation, so the player sees the
/// current state before opening them. Each one is rebuilt after its window is saved.
/// </summary>
internal static class IslandSummaries
{
    /// <summary>
    /// Holder of a summary that can be rebuilt.
    /// </summary>
    public sealed class Summary : ContentControl
    {
        private readonly Func<Control> Build;

        public Summary(Func<Control> build)
        {
            Build = build;
            Margin = new Thickness(0, 6, 0, 0);
            Refresh();
        }

        public void Refresh() => Content = Build();
    }

    private static StackPanel Line(params Control[] items)
    {
        var p = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6 };
        foreach (var i in items)
            p.Children.Add(i);
        return p;
    }

    private static TextBlock Label(string text) => new() { Text = text, FontSize = 12, FontWeight = Avalonia.Media.FontWeight.SemiBold, VerticalAlignment = VerticalAlignment.Center };

    private static Image Picture(Avalonia.Media.IImage? source) => new() { Source = source, Width = 24, Height = 24, VerticalAlignment = VerticalAlignment.Center };

    public static Summary Fruit(MainSave main) => new(() =>
    {
        var fruit = main.SpecialtyFruit;
        var flower = main.SpecialtyFlower;
        return Line(Picture(ItemSprite.GetItemSprite(fruit)), Picture(FruitsFlowersWindow.FlowerIcon(flower)),
            Label($"{IslandUi.ItemName(fruit)} · {FruitsFlowersWindow.FlowerName(flower)}"));
    });

    public static Summary Bulletin(MainSave main) => new(() =>
    {
        var notes = main.Bulletin.Stock.Buffer;
        var dates = notes.Where(z => z.UniqueId != 0 || !string.IsNullOrEmpty(z.Body)).Select(z => PlayerUi.ToDate(z.Date)).ToArray();
        if (dates.Length == 0)
            return Label(IslandStrings.BulletinSummaryEmpty);
        var latest = dates.Max() is { } d ? PlayerUi.FormatDate(d) : "?";
        return Label(string.Format(IslandStrings.BulletinSummary, dates.Length, latest));
    });

    public static Summary Turnips(MainSave main) => new(() =>
    {
        var t = main.Turnips;
        return Label(string.Format(IslandStrings.TurnipsSummary, t.BuyPrice, TurnipsWindow.PatternName(t.Pattern)));
    });

    public static Summary Campsite(MainSave main) => new(() =>
    {
        if (main.CampsiteStatus is not (2 or 3))
            return Label(IslandStrings.CampsiteSummaryLocked);
        var species = main.CampsiteVillagerSpeciesID;
        if (main.CampsiteStatus != 3 || species >= (int)VillagerSpecies.non)
            return Label(IslandStrings.CampsiteSummaryEmpty);
        var camper = VillagerUtil.GetInternalVillagerName((VillagerSpecies)species, main.CampsiteVillagerVariantID);
        return Line(IslandUi.Portrait(camper, 24), Label(string.Format(IslandStrings.CampsiteSummary, GameInfo.Strings.GetVillager(camper))));
    });

    public static Summary Visitors(MainSave main) => new(() =>
    {
        var week = main.Visitor.VisitorNPC;
        var faces = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 2 };
        foreach (var day in IslandUi.Week)
        {
            var v = (int)day < week.Length ? week[(int)day] : VisitorNPC.None;
            if (v == VisitorNPC.None)
                continue;
            var face = IslandUi.Portrait(IslandUi.VisitorCode(v), 26);
            ToolTip.SetTip(face, $"{IslandUi.DayName(day)}: {IslandUi.VisitorName(v)}");
            faces.Children.Add(face);
        }
        return faces.Children.Count == 0 ? Label(IslandStrings.VisitorsSummaryNone) : faces;
    });

    public static Summary Museum(MainSave main) => new(() =>
    {
        var donated = main.Museum.GetItems().Where(z => !z.IsNone).Select(z => z.ItemId).ToHashSet();
        var pieces = Enum.GetValues<MuseumWindow.Wing>().SelectMany(MuseumWindow.GetPieces).ToArray();
        return Label(string.Format(IslandStrings.MuseumSummary, pieces.Count(z => donated.Contains(z.Id)), pieces.Length));
    });

    public static string AirportName(AirportColor c) => c switch
    {
        AirportColor.Blue => IslandStrings.AirportBlue,
        AirportColor.Yellow => IslandStrings.AirportYellow,
        AirportColor.Orange => IslandStrings.AirportOrange,
        AirportColor.Green => IslandStrings.AirportGreen,
        _ => c.ToString(),
    };
}
