using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using NHSE.Core;
using NHSE.Injection;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Pages made of cards that open the existing editors: island, museum, designs and the advanced tools.
/// </summary>
public static class ActionPages
{
    public static Control Island(HomeContext ctx)
    {
        var main = ctx.SAV.Main;

        var north = new RadioButton { Content = HomeStrings.HemisphereNorth, GroupName = "hemisphere", IsChecked = main.Hemisphere == Hemisphere.Northern };
        var south = new RadioButton { Content = HomeStrings.HemisphereSouth, GroupName = "hemisphere", IsChecked = main.Hemisphere == Hemisphere.Southern };
        north.IsCheckedChanged += (_, _) =>
        {
            var h = north.IsChecked == true ? Hemisphere.Northern : Hemisphere.Southern;
            if (main.Hemisphere == h)
                return;
            main.Hemisphere = h;
            ctx.MarkDirty();
        };

        string[] airportColors = [HomeStrings.ColorBlue, HomeStrings.ColorYellow, HomeStrings.ColorOrange, HomeStrings.ColorGreen]; // AirportColor order
        var airport = new ComboBox { ItemsSource = airportColors, SelectedIndex = (int)main.AirportThemeColor, MinWidth = 160, Margin = new Thickness(0, 6, 0, 0) };
        airport.SelectionChanged += (_, _) =>
        {
            if (airport.SelectedIndex < 0 || (int)main.AirportThemeColor == airport.SelectedIndex)
                return;
            main.AirportThemeColor = (AirportColor)airport.SelectedIndex;
            ctx.MarkDirty();
        };

        return Page(HomeStrings.IslandTitle, null,
            Tiles(2,
                Card(CardHeader("PltPalm", HomeStrings.Hemisphere, HomeStrings.HemisphereHint, Row(north, south))),
                Card(CardHeader("PlaneTicket", HomeStrings.AirportColor, null, airport))),
            Section(HomeStrings.MapTitle),
            Tiles(2,
                ActionCard("Fence", HomeStrings.MapTitle, HomeStrings.MapHint, () => ctx.Edit(new FieldItemEditor(main))),
                ActionCard("Porch", HomeStrings.PlayerHouses, HomeStrings.PlayerHousesHint, async () =>
                {
                    var houses = main.GetPlayerHouses();
                    if (await ctx.Edit(new PlayerHouseEditor(houses, ctx.SAV.Players, main, ctx.PlayerIndex)))
                        main.SetPlayerHouses(houses);
                })),
            Section(HomeStrings.IslandLife),
            Tiles(2,
                ActionCard("Peach", HomeStrings.FruitFlowers, HomeStrings.FruitFlowersHint, () => ctx.Edit(new FruitsFlowersEditor(ctx.Player, main))),
                ActionCard("Kabu", HomeStrings.Turnips, HomeStrings.TurnipsHint, async () =>
                {
                    var editor = new SingleObjectEditor<TurnipStonk>(main.Turnips, PropertySort.Categorized, false);
                    if (await ctx.Edit(editor))
                        main.Turnips = editor.Object;
                }),
                ActionCard("Tent", HomeStrings.Campsite, HomeStrings.CampsiteHint, () => ctx.Edit(new CampsiteEditor(ctx.SAV))),
                ActionCard("Balloon", HomeStrings.Visitors, HomeStrings.VisitorsHint, async () =>
                {
                    var editor = new SingleObjectEditor<object>(main.Visitor, PropertySort.NoSort, false);
                    if (await ctx.Edit(editor))
                        main.Visitor = (GSaveVisitorNpc)editor.Object;
                }),
                ActionCard("Megaphone", HomeStrings.Bulletin, HomeStrings.BulletinHint, async () =>
                {
                    var editor = new SingleObjectEditor<object>(main.Bulletin, PropertySort.NoSort, false);
                    if (await ctx.Edit(editor))
                        main.Bulletin = (GSaveBulletinBoard)editor.Object;
                })));
    }

    public static Control Museum(HomeContext ctx) => Page(HomeStrings.MuseumTitle, null,
        Tiles(2,
            ActionCard("Fossil", HomeStrings.MuseumTitle, HomeStrings.MuseumHint, async () =>
            {
                var museum = ctx.SAV.Main.Museum;
                if (await ctx.Edit(new MuseumEditor(museum)))
                    ctx.SAV.Main.Museum = museum;
            }),
            ActionCard("Book", HomeStrings.Catalog, HomeStrings.CatalogHint, () => ctx.Edit(new ItemReceivedEditor(ctx.Player)))));

    public static Control Designs(HomeContext ctx)
    {
        var main = ctx.SAV.Main;
        return Page(HomeStrings.DesignsTitle, null,
            Tiles(2,
                ActionCard("MyDesignPro", HomeStrings.Patterns, HomeStrings.PatternsHint, async () =>
                {
                    var patterns = main.GetDesigns();
                    if (await ctx.Edit(new PatternEditor(patterns, ctx.Player)))
                        main.SetDesigns(patterns);
                }),
                ActionCard("MyDesignPro", HomeStrings.ProDesigns, HomeStrings.ProDesignsHint, async () =>
                {
                    var patterns = main.GetDesignsPRO();
                    if (await ctx.Edit(new PatternEditorPRO(patterns, ctx.Player)))
                        main.SetDesignsPRO(patterns);
                }),
                ActionCard("Flowershower", HomeStrings.FlagDesign, HomeStrings.FlagDesignHint, async () =>
                {
                    var patterns = new[] { main.FlagMyDesign };
                    if (await ctx.Edit(new PatternEditor(patterns, ctx.Player)))
                        main.FlagMyDesign = patterns[0];
                }),
                ActionCard("TailorTicket", HomeStrings.TailorDesigns, HomeStrings.TailorDesignsHint, async () =>
                {
                    var patterns = main.GetDesignsTailor();
                    if (await ctx.Edit(new PatternEditorPRO(patterns, ctx.Player)))
                        main.SetDesignsTailor(patterns);
                })));
    }

    public static Control Advanced(HomeContext ctx, Func<Task> classicEditor)
    {
        var main = ctx.SAV.Main;

        NumericUpDown Seed(uint value, Action<uint> set)
        {
            var n = new NumericUpDown { Value = value, Minimum = 0, Maximum = uint.MaxValue, FormatString = "0", HorizontalAlignment = HorizontalAlignment.Stretch, Margin = new Thickness(0, 4, 0, 0) };
            n.ValueChanged += (_, _) =>
            {
                set((uint)(n.Value ?? 0));
                ctx.MarkDirty();
            };
            return n;
        }

        return Page(HomeStrings.AdvancedTitle, HomeStrings.AdvancedHint,
            Section(HomeStrings.ClassicEditor),
            Tiles(2,
                ActionCard("ToolChangeStick", HomeStrings.ClassicEditor, HomeStrings.ClassicEditorHint, classicEditor),
                ActionCard("Timer", HomeStrings.VerifyHashes, HomeStrings.VerifyHashesHint, async () =>
                {
                    var result = ctx.SAV.GetInvalidHashes().ToArray();
                    if (result.Length == 0)
                        await Dialogs.Alert(MessageStrings.MsgSaveDataHashesValid);
                    else
                        await Dialogs.Error(MessageStrings.MsgSaveDataHashMismatch, string.Join(Environment.NewLine, result.Select(z => z.ToString())));
                })),
            Section(HomeStrings.WeatherSeed),
            Tiles(2,
                Card(CardHeader("Syabon", HomeStrings.WeatherSeed, HomeStrings.WeatherSeedHint, Seed(main.WeatherSeed, v => main.WeatherSeed = v))),
                Card(CardHeader("Syabon", "Tour", null, Seed(main.TourWeatherSeed, v => main.TourWeatherSeed = v)))),
            Section("Flags"),
            Tiles(2,
                ActionCard("Book", HomeStrings.PlayerFlags, null, async () =>
                {
                    var pers = ctx.Player.Personal;
                    var flags = pers.GetEventFlagsPlayer();
                    if (await ctx.Edit(new FlagEditor(flags)))
                        pers.SetEventFlagsPlayer(flags);
                }),
                ActionCard("Book", HomeStrings.IslandFlags, null, async () =>
                {
                    var edit = main.EventFlagLand.ToArray();
                    if (await ctx.Edit(new LandFlagEditor(edit)))
                        edit.CopyTo(main.EventFlagLand);
                }),
                ActionCard("Book", HomeStrings.FieldGoods, null, async () =>
                {
                    var editor = new SingleObjectEditor<object>(main.SaveFg, PropertySort.NoSort, false);
                    if (await ctx.Edit(editor))
                        main.SaveFg = (GSaveFg)editor.Object;
                })),
            Section(HomeStrings.AdvancedTitle),
            Tiles(2,
                ActionCard("Cardboard", HomeStrings.DumpDecrypted, null, async () =>
                {
                    var path = await Pickers.OpenFolder(ctx.Owner);
                    if (path is null)
                        return;
                    ctx.SAV.Dump(path);
                    Sound.Asterisk();
                }),
                ActionCard("Cardboard", HomeStrings.LoadDecrypted, null, async () =>
                {
                    var path = await Pickers.OpenFile(ctx.Owner, "New Horizons Save File (main.dat)|main.dat", "main.dat");
                    var dir = path is null ? null : Path.GetDirectoryName(path);
                    if (dir is null || !Directory.Exists(dir))
                        return;
                    ctx.SAV.Load(dir);
                    ctx.MarkDirty();
                    Sound.Asterisk();
                }),
                ActionCard("GlowStick", HomeStrings.RamEdit, HomeStrings.RamEditHint, () =>
                {
                    var exist = UiUtil.FirstFormOfType<SysBotRAMEdit>();
                    if (exist is not null)
                        exist.Activate();
                    else
                        new SysBotRAMEdit(InjectionType.Generic).Show(ctx.Owner);
                    return Task.CompletedTask;
                }),
                ActionCard("CameraAppPro", HomeStrings.ItemImages, HomeStrings.ItemImagesHint, () =>
                {
                    var exist = UiUtil.FirstFormOfType<ImageFetcher>();
                    if (exist is not null)
                        exist.Activate();
                    else
                        new ImageFetcher().Show(ctx.Owner);
                    return Task.CompletedTask;
                })));
    }
}
