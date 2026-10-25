using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Friendly campsite editor: the camper's portrait and name, a button to choose another one from the villager
/// list (or nobody) and the day of the visit. Saves into the island data when Save is clicked.
/// </summary>
public sealed class CampsiteWindow : NhseWindow
{
    private const byte StatusEmpty = 2;
    private const byte StatusOccupied = 3;

    private readonly MainSave Main;
    private readonly HashSet<string> Residents;
    private readonly Image Portrait = new() { Width = 112, Height = 112, VerticalAlignment = VerticalAlignment.Top };
    private readonly TextBlock CamperName = new() { FontSize = 20, FontWeight = FontWeight.SemiBold, TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock Status = new() { Classes = { "subtle" }, TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock Warning = new() { TextWrapping = TextWrapping.Wrap, FontWeight = FontWeight.SemiBold };
    private readonly Button Empty = new() { Content = IslandStrings.NoCamper };
    private readonly CalendarDatePicker Date = new() { Width = 200 };
    private Control? DateCard;

    /// <summary>
    /// Internal name of the camper ("cat01"), or null if nobody is camping.
    /// </summary>
    internal string? Camper { get; private set; }

    /// <summary>
    /// False while the island has no campsite (nothing can be changed).
    /// </summary>
    public bool Unlocked { get; }

    public CampsiteWindow(MainSave main)
    {
        Main = main;
        Width = 560;
        SizeToContent = SizeToContent.Height;
        CanResize = false;
        Residents = [.. main.GetVillagers().Select(z => z.InternalName)];

        var status = main.CampsiteStatus;
        Unlocked = status is StatusEmpty or StatusOccupied;

        Control body;
        if (!Unlocked)
        {
            body = Card(CardHeader("Tent", IslandStrings.CampsiteLocked, IslandStrings.CampsiteLockedHint));
        }
        else
        {
            var species = main.CampsiteVillagerSpeciesID;
            if (status == StatusOccupied && species < (int)VillagerSpecies.non)
                Camper = VillagerUtil.GetInternalVillagerName((VillagerSpecies)species, main.CampsiteVillagerVariantID);

            var choose = new Button { Content = IslandStrings.ChooseCamper, Classes = { "accent" } };
            choose.Click += async (_, _) => await ChooseCamper();
            Empty.Click += (_, _) => SetCamper(null);
            Warning.Foreground = Brushes.DarkOrange;

            var info = new StackPanel { Spacing = 4, VerticalAlignment = VerticalAlignment.Center };
            info.Children.Add(CamperName);
            info.Children.Add(Status);
            info.Children.Add(Warning);
            info.Children.Add(Row(choose, Empty));

            var top = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 16 };
            top.Children.Add(Portrait);
            Grid.SetColumn(info, 1);
            top.Children.Add(info);

            DateTime date = main.CampTimestamp;
            Date.SelectedDate = date == DateTime.MinValue ? DateTime.Today : date;
            var dateHint = Text(IslandStrings.CampDateHint, "subtle");
            dateHint.FontSize = 12;
            var dateCard = Card(CardHeader("Timer", IslandStrings.CampDate, null, Date, dateHint));
            dateCard.Margin = new Thickness(0, 12, 0, 0);
            DateCard = dateCard;
            Date.Margin = new Thickness(0, 6, 0, 4);

            var panel = new StackPanel();
            panel.Children.Add(Card(top));
            panel.Children.Add(dateCard);
            body = panel;
            ShowCamper(Camper);
        }

        var save = IslandUi.Frame(this, IslandStrings.CampsiteTitle, IslandStrings.CampsiteHint, body, SaveCampsite);
        save.IsEnabled = Unlocked;
    }

    private async Task ChooseCamper()
    {
        var picker = new VillagerPicker();
        if (Camper is not null)
            picker.Select(Camper);
        if (await picker.ShowDialog<bool>(this) && picker.SelectedInternalName is { } name)
            SetCamper(name);
    }

    /// <summary>
    /// Puts a villager in the campsite (or nobody) and sets the visit to today.
    /// </summary>
    internal void SetCamper(string? internalName)
    {
        if (internalName is not null && !TryParse(internalName, out _, out _))
            return;
        Camper = internalName;
        if (internalName is not null)
            Date.SelectedDate = DateTime.Today;
        ShowCamper(internalName);
    }

    internal DateTime? VisitDate
    {
        get => Date.SelectedDate;
        set => Date.SelectedDate = value;
    }

    private void ShowCamper(string? camper)
    {
        Portrait.Source = camper is null ? ItemSprite.GetResource("sprites/MenuIcon/Tent.png") : VillagerSprite.GetVillagerSprite(camper);
        Portrait.Opacity = camper is null ? 0.5 : 1;
        CamperName.Text = camper is null ? IslandStrings.Nobody : GameInfo.Strings.GetVillager(camper);
        Status.Text = camper is null ? IslandStrings.NotCamping : IslandStrings.Camping;
        Warning.Text = camper is not null && Residents.Contains(camper) ? string.Format(IslandStrings.AlreadyResident, CamperName.Text) : null;
        Warning.IsVisible = Warning.Text is not null;
        Empty.IsVisible = camper is not null;
        if (DateCard is not null)
            DateCard.IsVisible = camper is not null; // the day only matters when someone is camping
    }

    private static bool TryParse(string internalName, out VillagerSpecies species, out int variant)
    {
        variant = 0;
        species = default;
        return internalName.Length == 5
               && Enum.TryParse(internalName[..3], out species)
               && int.TryParse(internalName[3..], out variant);
    }

    /// <summary>
    /// Same values as the classic editor: status 3 with the camper, or status 2 with the "nobody" species.
    /// </summary>
    private void SaveCampsite()
    {
        if (!Unlocked)
            return;
        if (Camper is not null && TryParse(Camper, out var species, out var variant))
        {
            Main.CampsiteStatus = StatusOccupied;
            Main.CampsiteVillagerSpeciesID = (byte)species;
            Main.CampsiteVillagerVariantID = (byte)variant;
            Main.CampTimestamp = Date.SelectedDate ?? DateTime.Today;
        }
        else
        {
            Main.CampsiteStatus = StatusEmpty;
            Main.CampsiteVillagerVariantID = 0;
            Main.CampsiteVillagerSpeciesID = (byte)VillagerSpecies.non;
        }
    }
}
