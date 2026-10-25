using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using NHSE.Villagers;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// The ten villagers as portraits; the selected one can be changed in plain terms (personality, catchphrase,
/// friendship, walls and floor, moving out), swapped for any other villager picked by name, or saved to a file.
/// The "More" menu has the actions for every villager at once.
/// </summary>
public sealed class VillagersPage : UserControl
{
    private const string VillagerFilter = "New Horizons Villager (*.nhv)|*.nhv|" +
                                          "New Horizons Villager (*.nhv2)|*.nhv2|" +
                                          "All files (*.*)|*.*";

    private readonly HomeContext Ctx;
    private IVillager[] Villagers = [];
    private int Selected;

    public VillagersPage(HomeContext ctx)
    {
        Ctx = ctx;
        Build();
    }

    /// <summary>
    /// Index of the villager whose details are shown.
    /// </summary>
    public int SelectedIndex
    {
        get => Selected;
        set
        {
            Selected = value;
            Build();
        }
    }

    /// <summary>
    /// The villagers as shown (read from the save on every rebuild).
    /// </summary>
    public IReadOnlyList<IVillager> Shown => Villagers;

    public void Build()
    {
        Villagers = Ctx.SAV.Main.GetVillagers();
        if (Selected >= Villagers.Length)
            Selected = 0;
        Content = Page(null, null, Header(), Portraits(), Details());
    }

    /// <summary>
    /// Writes the villager array back after an edit and marks the save as changed.
    /// </summary>
    private void Commit()
    {
        Ctx.SAV.Main.SetVillagers(Villagers);
        Ctx.MarkDirty();
    }

    private static string GetName(IVillager v) => VillagerTools.IsEmpty(v) ? HomeStrings.EmptyPlot : GameInfo.Strings.GetVillager(v.InternalName);

    #region Title and actions for everyone
    private Grid Header()
    {
        var text = new StackPanel();
        text.Children.Add(Text(HomeStrings.VillagersTitle, "title"));
        var hint = Text(HomeStrings.VillagersHint, "subtle");
        hint.Margin = new Thickness(0, 4, 0, 0);
        text.Children.Add(hint);

        MenuItem Item(string header, Func<Task> action)
        {
            var m = new MenuItem { Header = header };
            m.Click += async (_, _) => await action();
            return m;
        }

        var flyout = new MenuFlyout { Placement = PlacementMode.BottomEdgeAlignedRight };
        flyout.Items.Add(Item(VillagerStrings.EveryoneMovesOut, AskEveryoneMovesOut));
        flyout.Items.Add(Item(VillagerStrings.BestFriendsEveryVillager, AskBestFriendsEveryVillager));
        flyout.Items.Add(new Separator());
        flyout.Items.Add(Item(VillagerStrings.ExportAll, ExportAll));
        var more = new Button { Content = HomeStrings.More + " ▾", Flyout = flyout, VerticalAlignment = VerticalAlignment.Bottom };

        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto"), ColumnSpacing = 12 };
        g.Children.Add(text);
        Grid.SetColumn(more, 1);
        g.Children.Add(more);
        return g;
    }

    private async Task AskEveryoneMovesOut()
    {
        if (await Dialogs.Prompt(MessageBoxButtons.YesNo, VillagerStrings.EveryoneMovesOutAsk) != DialogResult.Yes)
            return;
        var count = EveryoneMovesOut();
        await Dialogs.Alert(string.Format(VillagerStrings.EveryoneMovesOutDone, count));
    }

    /// <summary>
    /// Every villager packs their things and leaves the island. Returns how many.
    /// </summary>
    public int EveryoneMovesOut()
    {
        var living = Villagers.Where(z => !VillagerTools.IsEmpty(z)).ToArray();
        foreach (var v in living)
            VillagerTools.SetMovingOut(v, true);
        Commit();
        Build();
        return living.Length;
    }

    private async Task AskBestFriendsEveryVillager()
    {
        if (await Dialogs.Prompt(MessageBoxButtons.YesNo, VillagerStrings.BestFriendsEveryVillagerAsk) != DialogResult.Yes)
            return;
        BestFriendsEveryVillager();
        await Dialogs.Alert(VillagerStrings.BestFriendsEveryVillagerDone);
    }

    /// <summary>
    /// Every villager gets the maximum friendship with every player they know.
    /// </summary>
    public void BestFriendsEveryVillager()
    {
        foreach (var v in Villagers.Where(z => !VillagerTools.IsEmpty(z)))
            v.SetFriendshipAll();
        Commit();
        Build();
    }

    private async Task ExportAll()
    {
        var folder = await Pickers.OpenFolder(Ctx.Owner);
        if (folder is null || !Directory.Exists(folder))
            return;
        var living = Villagers.Where(z => !VillagerTools.IsEmpty(z)).ToArray();
        living.Dump(folder);
        await Dialogs.Alert(string.Format(VillagerStrings.ExportAllDone, living.Length));
    }
    #endregion

    #region Portraits
    private UniformGrid Portraits()
    {
        var grid = new UniformGrid { Columns = 5, ColumnSpacing = 10, RowSpacing = 10, Margin = new Thickness(0, 18, 0, 0) };
        for (int i = 0; i < Villagers.Length; i++)
        {
            var index = i;
            var v = Villagers[i];
            var empty = VillagerTools.IsEmpty(v);

            var picture = new Grid { Width = 72, Height = 72, HorizontalAlignment = HorizontalAlignment.Center };
            picture.Children.Add(new Image { Source = empty ? null : VillagerSprite.GetVillagerSprite(v.InternalName) });
            if (!empty && v.MovingOut)
            {
                // Packed boxes on the portrait of those who are leaving
                var boxes = Icon("Cardboard", 26);
                boxes.HorizontalAlignment = HorizontalAlignment.Right;
                boxes.VerticalAlignment = VerticalAlignment.Bottom;
                boxes.Margin = new Thickness(0, 0, -12, -2);
                ToolTip.SetTip(boxes, VillagerStrings.MovingOutBadge);
                picture.Children.Add(boxes);
            }

            var content = new StackPanel { Spacing = 4, HorizontalAlignment = HorizontalAlignment.Center };
            content.Children.Add(picture);
            content.Children.Add(new TextBlock { Text = GetName(v), HorizontalAlignment = HorizontalAlignment.Center, FontWeight = FontWeight.SemiBold, TextTrimming = TextTrimming.CharacterEllipsis });
            var b = new Button { Content = content, Classes = { "card" }, Padding = new Thickness(6, 10) };
            if (index == Selected)
            {
                b.Bind(BorderBrushProperty, b.GetResourceObservable("NhGreenBrush"));
                b.BorderThickness = new Thickness(2);
            }
            b.Click += (_, _) => SelectedIndex = index;
            grid.Children.Add(b);
        }
        return grid;
    }
    #endregion

    #region The selected villager
    private static string PersonalityName(VillagerPersonality p) => p switch
    {
        VillagerPersonality.Lazy => HomeStrings.PersonalityLazy,
        VillagerPersonality.Jock => HomeStrings.PersonalityJock,
        VillagerPersonality.Cranky => HomeStrings.PersonalityCranky,
        VillagerPersonality.Smug => HomeStrings.PersonalitySmug,
        VillagerPersonality.Normal => HomeStrings.PersonalityNormal,
        VillagerPersonality.Peppy => HomeStrings.PersonalityPeppy,
        VillagerPersonality.Snooty => HomeStrings.PersonalitySnooty,
        VillagerPersonality.Uchi => HomeStrings.PersonalitySisterly,
        _ => p.ToString(),
    };

    private Control Details()
    {
        if (Villagers.Length == 0)
            return new Panel();
        var v = Villagers[Selected];
        if (VillagerTools.IsEmpty(v))
        {
            var empty = Card(CardHeader("Leaf", HomeStrings.EmptyPlot, VillagerStrings.NobodyLivesHere));
            empty.Margin = new Thickness(0, 16, 0, 0);
            return empty;
        }
        var name = GetName(v);

        // Personality
        var personalities = Enum.GetValues<VillagerPersonality>().Where(z => z != VillagerPersonality.None).ToArray();
        var personality = new ComboBox
        {
            ItemsSource = personalities.Select(PersonalityName).ToArray(),
            SelectedIndex = Array.IndexOf(personalities, v.Personality),
            MinWidth = 180,
        };
        personality.SelectionChanged += (_, _) =>
        {
            if (personality.SelectedIndex < 0)
                return;
            v.Personality = personalities[personality.SelectedIndex];
            Commit();
        };

        // Catchphrase (empty: the villager says their own)
        var original = GameInfo.Strings.GetVillagerDefaultPhrase(v.InternalName);
        var phrase = new TextBox { Text = v.CatchPhrase, PlaceholderText = original, MinWidth = 200 };
        phrase.LostFocus += (_, _) =>
        {
            if (phrase.Text == v.CatchPhrase)
                return;
            v.CatchPhrase = phrase.Text ?? string.Empty;
            Commit();
        };
        var restore = SmallButton(HomeStrings.CatchphraseOriginal, () =>
        {
            phrase.Text = v.CatchPhrase = original;
            Commit();
            return Task.CompletedTask;
        });

        // Moving out
        var moving = new CheckBox { Content = HomeStrings.MovingOut, IsChecked = v.MovingOut };
        var movingHint = Text(VillagerStrings.MovingOutHint, "subtle");
        movingHint.IsVisible = v.MovingOut;
        moving.IsCheckedChanged += (_, _) =>
        {
            VillagerTools.SetMovingOut(v, moving.IsChecked == true);
            movingHint.IsVisible = v.MovingOut;
            Commit();
            Build();
        };

        var form = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*"),
            ColumnSpacing = 14,
            RowSpacing = 10,
            Margin = new Thickness(0, 12, 0, 0),
        };
        void AddRow(string? label, Control c)
        {
            var row = form.RowDefinitions.Count;
            form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
            if (label is not null)
            {
                var l = new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center };
                Grid.SetRow(l, row);
                form.Children.Add(l);
            }
            Grid.SetRow(c, row);
            Grid.SetColumn(c, 1);
            form.Children.Add(c);
        }
        AddRow(HomeStrings.Personality, personality);
        AddRow(HomeStrings.Catchphrase, Line(phrase, restore));
        AddRow(HomeStrings.VillagerFriendship, FriendshipLine(v));
        AddRow(HomeStrings.VillagerRoom, RoomLine(v, name));
        if (VillagerTools.GetCraftingText(v) is { } crafting)
        {
            var icon = Icon("DIYRecipePlus", 22);
            icon.VerticalAlignment = VerticalAlignment.Center;
            AddRow(VillagerStrings.Crafting, Line(icon, new TextBlock { Text = crafting, VerticalAlignment = VerticalAlignment.Center, TextWrapping = TextWrapping.Wrap }));
        }
        var export = SmallButton(HomeStrings.ExportVillager, () => PickExport(v));
        ToolTip.SetTip(export, VillagerStrings.ExportHint);
        var import = SmallButton(HomeStrings.ImportVillager, PickImport);
        ToolTip.SetTip(import, VillagerStrings.ImportHint);
        AddRow(null, moving);
        AddRow(null, movingHint);
        var fileHint = Text(VillagerStrings.FileHint, "subtle");
        fileHint.VerticalAlignment = VerticalAlignment.Center;
        AddRow(VillagerStrings.VillagerFile, Line(export, import, fileHint));

        var actions = new WrapPanel { ItemSpacing = 8, LineSpacing = 8, Margin = new Thickness(0, 16, 0, 0) };
        actions.Children.Add(SmallButton(HomeStrings.ReplaceVillager, () => Replace(Selected), accent: true));
        actions.Children.Add(SmallButton(HomeStrings.VillagerFurniture, async () =>
        {
            var items = v.FtrStockList;
            if (await Ctx.Edit(new ItemsWindow(items, 8, 4, $"{name} · {HomeStrings.VillagerFurniture}")))
            {
                v.FtrStockList = items;
                Commit();
            }
        }));
        actions.Children.Add(SmallButton(HomeStrings.VillagerClothes, async () =>
        {
            var items = v.WearStockList;
            if (await Ctx.Edit(new ItemsWindow(items, 8, 3, $"{name} · {HomeStrings.VillagerClothes}")))
            {
                v.WearStockList = items;
                Commit();
            }
        }));
        actions.Children.Add(SmallButton(HomeStrings.VillagerHouse, () => VillagerHouseWindow.Open(Ctx, Selected)));

        var portrait = new Image { Source = VillagerSprite.GetVillagerSprite(v.InternalName), Width = 120, Height = 120, VerticalAlignment = VerticalAlignment.Top };
        var text = new StackPanel();
        text.Children.Add(Text(name, "title"));
        text.Children.Add(form);
        text.Children.Add(actions);

        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 18 };
        g.Children.Add(portrait);
        Grid.SetColumn(text, 1);
        g.Children.Add(text);

        var card = Card(g);
        card.Margin = new Thickness(0, 16, 0, 0);
        return card;
    }

    private static StackPanel Line(params Control[] items)
    {
        var p = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        foreach (var c in items)
            p.Children.Add(c);
        return p;
    }

    /// <summary>
    /// Hearts with the player chosen in the sidebar, and the button that opens the friendship window.
    /// </summary>
    private StackPanel FriendshipLine(IVillager v)
    {
        var player = Ctx.Player.Personal;
        var line = Line();
        if (VillagerTools.GetMemoryOf(v, player) is { } memory)
        {
            var level = FriendshipWindow.GetLevel(memory.Friendship);
            line.Children.Add(FriendshipWindow.HeartStrip(level));
            line.Children.Add(new TextBlock { Text = FriendshipWindow.GetLevelName(level), FontWeight = FontWeight.SemiBold, VerticalAlignment = VerticalAlignment.Center });
            var with = Text(string.Format(VillagerStrings.FriendshipWith, player.PlayerName), "subtle");
            with.VerticalAlignment = VerticalAlignment.Center;
            line.Children.Add(with);
        }
        else
        {
            var none = Text(string.Format(VillagerStrings.NotMetYet, player.PlayerName), "subtle");
            none.VerticalAlignment = VerticalAlignment.Center;
            line.Children.Add(none);
        }
        line.Children.Add(SmallButton(HomeStrings.Edit, async () =>
        {
            if (await Ctx.Edit(new FriendshipWindow(v, Ctx.SAV.Players[0].Personal)))
            {
                Commit();
                Build();
            }
        }));
        return line;
    }

    /// <summary>
    /// Pictures and names of the wallpaper and the flooring, and the button that opens the room window.
    /// </summary>
    private StackPanel RoomLine(IVillager v, string name)
    {
        var room = v.Room;
        var line = Line();
        foreach (var part in new[] { room.WallItem, room.FloorItem })
        {
            var item = part.ToItem();
            var picture = new Image { Source = ItemSprite.GetItemSprite(item), Width = 26, Height = 26, VerticalAlignment = VerticalAlignment.Center };
            SetItemTip(picture, () => item);
            line.Children.Add(picture);
            line.Children.Add(new TextBlock { Text = GameInfo.Strings.GetItemName(item), VerticalAlignment = VerticalAlignment.Center });
        }
        line.Children.Add(SmallButton(HomeStrings.Edit, async () =>
        {
            var w = new RoomWindow(v.Room, name);
            if (await Ctx.Edit(w))
            {
                v.Room = w.Room;
                Commit();
                Build();
            }
        }));
        return line;
    }
    #endregion

    #region Swap, export and import
    /// <summary>
    /// Swap the villager for another one picked by name (the classic editor needs the internal name in the clipboard).
    /// </summary>
    private async Task Replace(int index)
    {
        if (Villagers[index] is not Villager2 v2)
        {
            await Dialogs.Error(HomeStrings.SwapNeedsUpdate);
            return;
        }

        var picker = new VillagerPicker();
        if (!await picker.ShowDialog<bool>(Ctx.Owner) || picker.SelectedInternalName is not { } internalName)
            return;

        var oldName = GameInfo.Strings.GetVillager(v2.InternalName);
        var houses = Ctx.SAV.Main.GetVillagerHouses();
        var houseIndex = Array.FindIndex(houses, z => z.NPC1 == index);
        if (houseIndex < 0)
            return;
        var exist = new VillagerInfo(v2, houses[houseIndex]);
        var replace = VillagerSwap.GetReplacementVillager(exist, internalName);

        Ctx.SAV.Main.SetVillagerHouse(new VillagerHouse1(replace.House), houseIndex);
        Villagers[index] = new Villager2(replace.Villager);
        Commit();
        Build();
        await Dialogs.Alert(string.Format(HomeStrings.SwapDone, GameInfo.Strings.GetVillager(internalName), oldName));
    }

    private async Task PickExport(IVillager v)
    {
        var path = await Pickers.SaveFile(Ctx.Owner, VillagerFilter, $"{GetName(v)}.{v.Extension}");
        if (path is not null)
            await ExportVillager(path);
    }

    /// <summary>
    /// Saves the selected villager to a file (.nhv2), as the classic editor does.
    /// </summary>
    public Task ExportVillager(string path) => File.WriteAllBytesAsync(path, Villagers[Selected].Write());

    private async Task PickImport()
    {
        var v = Villagers[Selected];
        var path = await Pickers.OpenFile(Ctx.Owner, VillagerFilter, $"{GetName(v)}.{v.Extension}");
        if (path is not null && await ImportVillager(path))
            await Dialogs.Alert(string.Format(VillagerStrings.ImportDone, GetName(Villagers[Selected])));
    }

    /// <summary>
    /// Replaces the selected villager with the one in a file. A villager from another island can be made to remember
    /// this island and its first player instead, as the classic editor offers. Returns false if nothing was imported.
    /// </summary>
    public async Task<bool> ImportVillager(string path)
    {
        var data = await File.ReadAllBytesAsync(path);
        if (VillagerTools.Read(Ctx.SAV.Main, data) is not { } v)
        {
            await Dialogs.Error(VillagerStrings.ImportWrongFile, path);
            return false;
        }

        var host = Ctx.SAV.Players[0].Personal;
        if (!v.IsOriginatedFrom(host))
        {
            var answer = await Dialogs.Prompt(MessageBoxButtons.YesNoCancel, string.Format(VillagerStrings.ImportOtherIsland, v.TownName));
            if (answer == DialogResult.Cancel)
                return false;
            if (answer == DialogResult.Yes)
                v = VillagerTools.AdaptToIsland(v, host, Ctx.SAV.Main);
        }

        Villagers[Selected] = v;
        Commit();
        Build();
        return true;
    }
    #endregion
}
