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

        var airport = new ComboBox { ItemsSource = Enum.GetValues<AirportColor>().Select(IslandSummaries.AirportName).ToArray(), SelectedIndex = (int)main.AirportThemeColor, MinWidth = 160, Margin = new Thickness(0, 6, 0, 0) };
        airport.SelectionChanged += (_, _) =>
        {
            if (airport.SelectedIndex < 0 || (int)main.AirportThemeColor == airport.SelectedIndex)
                return;
            main.AirportThemeColor = (AirportColor)airport.SelectedIndex;
            ctx.MarkDirty();
        };

        var fruit = IslandSummaries.Fruit(main);
        var turnips = IslandSummaries.Turnips(main);
        var campsite = IslandSummaries.Campsite(main);
        var visitors = IslandSummaries.Visitors(main);
        var bulletin = IslandSummaries.Bulletin(main);

        return Page(HomeStrings.IslandTitle, null,
            Tiles(2,
                Card(CardHeader("PltPalm", HomeStrings.Hemisphere, HomeStrings.HemisphereHint, Row(north, south))),
                Card(CardHeader("PlaneTicket", HomeStrings.AirportColor, null, airport))),
            Section(HomeStrings.MapTitle),
            Tiles(2,
                ActionCard("Fence", HomeStrings.MapTitle, HomeStrings.MapHint, () => IslandMapWindow.Open(ctx)),
                ActionCard("Porch", HomeStrings.PlayerHouses, HomeStrings.PlayerHousesHint, () => HouseWindow.Open(ctx))),
            Section(HomeStrings.IslandLife),
            Tiles(2,
                ActionCard("Peach", IslandStrings.FruitTitle, IslandStrings.FruitHint, async () =>
                {
                    if (await ctx.Edit(new FruitsFlowersWindow(ctx.SAV.Players, ctx.PlayerIndex, main)))
                        fruit.Refresh();
                }, fruit),
                ActionCard("Kabu", HomeStrings.Turnips, HomeStrings.TurnipsHint, async () =>
                {
                    var t = main.Turnips;
                    if (!await ctx.Edit(new TurnipsWindow(t)))
                        return;
                    main.Turnips = t;
                    turnips.Refresh();
                }, turnips),
                ActionCard("Tent", HomeStrings.Campsite, HomeStrings.CampsiteHint, async () =>
                {
                    if (await ctx.Edit(new CampsiteWindow(main)))
                        campsite.Refresh();
                }, campsite),
                ActionCard("Balloon", HomeStrings.Visitors, HomeStrings.VisitorsHint, async () =>
                {
                    var v = main.Visitor;
                    if (!await ctx.Edit(new VisitorsWindow(v)))
                        return;
                    main.Visitor = v;
                    visitors.Refresh();
                }, visitors),
                ActionCard("Megaphone", HomeStrings.Bulletin, HomeStrings.BulletinHint, async () =>
                {
                    if (await ctx.Edit(new BulletinWindow(main)))
                        bulletin.Refresh();
                }, bulletin)));
    }

    public static Control Museum(HomeContext ctx)
    {
        var summary = IslandSummaries.Museum(ctx.SAV.Main);
        return Page(HomeStrings.MuseumTitle, null,
            Tiles(2,
                ActionCard("Fossil", HomeStrings.MuseumTitle, HomeStrings.MuseumHint, async () =>
                {
                    var museum = ctx.SAV.Main.Museum;
                    if (!await ctx.Edit(new MuseumWindow(museum, ctx.SAV.Players, ctx.PlayerIndex)))
                        return;
                    ctx.SAV.Main.Museum = museum;
                    summary.Refresh();
                }, summary),
                ActionCard("Book", HomeStrings.Catalog, HomeStrings.CatalogHint, () => ctx.Edit(new CatalogWindow(ctx.Player)))));
    }

    public static Control Designs(HomeContext ctx)
    {
        var main = ctx.SAV.Main;
        var me = ctx.Player.Personal;

        // Each card shows how many slots are used and the first designs; it is rebuilt after saving.
        Button DesignCard(string icon, string title, string hint, Func<DesignSlot[]> read, Func<DesignsWindow> open)
        {
            var summary = new StackPanel { Spacing = 4 };
            void Refresh()
            {
                var slots = read();
                summary.Children.Clear();
                if (slots.Length > 1)
                    summary.Children.Add(Text(string.Format(DesignStrings.SlotsUsed, slots.Count(z => !z.IsEmpty), slots.Length), "subtle"));
                var strip = new WrapPanel { ItemSpacing = 4, LineSpacing = 4, Margin = new Thickness(0, 4, 0, 0) };
                foreach (var d in slots.Where(z => !z.IsEmpty).Take(8))
                {
                    var img = new Image { Source = d.GetPicture(1), Width = 34, Height = 34 };
                    ToolTip.SetTip(img, d.Name);
                    strip.Children.Add(img);
                }
                summary.Children.Add(strip);
            }
            Refresh();
            return ActionCard(icon, title, hint, async () =>
            {
                if (await ctx.Edit(open()))
                    Refresh();
            }, summary);
        }

        return Page(HomeStrings.DesignsTitle, null,
            Tiles(2,
                DesignCard("MyDesignPro", HomeStrings.Patterns, DesignStrings.PatternsHint,
                    () => [.. main.GetDesigns().Select(z => new NormalDesign(z))],
                    () =>
                    {
                        var patterns = main.GetDesigns();
                        return DesignsWindow.Normal(HomeStrings.Patterns, patterns, me, () => main.SetDesigns(patterns));
                    }),
                DesignCard("MyDesignPro", HomeStrings.ProDesigns, DesignStrings.ProHint,
                    () => [.. main.GetDesignsPRO().Select(z => new ProDesign(z))],
                    () =>
                    {
                        var patterns = main.GetDesignsPRO();
                        return DesignsWindow.Pro(HomeStrings.ProDesigns, patterns, me, () => main.SetDesignsPRO(patterns));
                    }),
                DesignCard("Flowershower", HomeStrings.FlagDesign, DesignStrings.FlagHint,
                    () => [new NormalDesign(main.FlagMyDesign)],
                    () =>
                    {
                        var patterns = new[] { main.FlagMyDesign };
                        return DesignsWindow.Normal(HomeStrings.FlagDesign, patterns, me, () => main.FlagMyDesign = patterns[0]);
                    }),
                DesignCard("TailorTicket", HomeStrings.TailorDesigns, DesignStrings.TailorHint,
                    () => [.. main.GetDesignsTailor().Select(z => new ProDesign(z))],
                    () =>
                    {
                        var patterns = main.GetDesignsTailor();
                        return DesignsWindow.Pro(HomeStrings.TailorDesigns, patterns, me, () => main.SetDesignsTailor(patterns));
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
                ActionCard("Book", HomeStrings.PlayerFlags, FlagStrings.PlayerHint, async () =>
                {
                    var pers = ctx.Player.Personal;
                    var flags = pers.GetEventFlagsPlayer();
                    if (await ctx.Edit(new EventFlagsWindow(flags, false, string.Format(FlagStrings.PlayerTitle, pers.PlayerName))))
                        pers.SetEventFlagsPlayer(flags);
                }),
                ActionCard("Book", HomeStrings.IslandFlags, FlagStrings.IslandHint, async () =>
                {
                    var edit = main.EventFlagLand.ToArray();
                    if (await ctx.Edit(new EventFlagsWindow(edit, true, FlagStrings.IslandTitle)))
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
