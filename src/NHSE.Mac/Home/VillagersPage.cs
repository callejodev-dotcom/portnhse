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
/// The ten villagers as portraits; the selected one can be changed in plain terms (personality, catchphrase, moving out)
/// or swapped for any other villager picked by name.
/// </summary>
public sealed class VillagersPage : UserControl
{
    private readonly HomeContext Ctx;
    private IVillager[] Villagers = [];
    private int Selected;

    public VillagersPage(HomeContext ctx)
    {
        Ctx = ctx;
        Build();
    }

    public void Build()
    {
        Villagers = Ctx.SAV.Main.GetVillagers();
        if (Selected >= Villagers.Length)
            Selected = 0;
        Content = Page(HomeStrings.VillagersTitle, HomeStrings.VillagersHint, Portraits(), Details());
    }

    /// <summary>
    /// Writes the villager array back after an edit and marks the save as changed.
    /// </summary>
    private void Commit()
    {
        Ctx.SAV.Main.SetVillagers(Villagers);
        Ctx.MarkDirty();
    }

    private static bool IsEmpty(IVillager v) => v.Species == (byte)VillagerSpecies.non;

    private Control Portraits()
    {
        var grid = new UniformGrid { Columns = 5, ColumnSpacing = 10, RowSpacing = 10, Margin = new Thickness(0, 18, 0, 0) };
        for (int i = 0; i < Villagers.Length; i++)
        {
            var index = i;
            var v = Villagers[i];
            var name = IsEmpty(v) ? HomeStrings.EmptyPlot : GameInfo.Strings.GetVillager(v.InternalName);
            var content = new StackPanel { Spacing = 4, HorizontalAlignment = HorizontalAlignment.Center };
            content.Children.Add(new Image { Source = IsEmpty(v) ? null : VillagerSprite.GetVillagerSprite(v.InternalName), Width = 72, Height = 72, HorizontalAlignment = HorizontalAlignment.Center });
            content.Children.Add(new TextBlock { Text = name, HorizontalAlignment = HorizontalAlignment.Center, FontWeight = FontWeight.SemiBold, TextTrimming = TextTrimming.CharacterEllipsis });
            var b = new Button { Content = content, Classes = { "card" }, Padding = new Thickness(6, 10) };
            if (index == Selected)
            {
                b.Bind(BorderBrushProperty, b.GetResourceObservable("NhGreenBrush"));
                b.BorderThickness = new Thickness(2);
            }
            b.Click += (_, _) =>
            {
                Selected = index;
                Build();
            };
            grid.Children.Add(b);
        }
        return grid;
    }

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
        var name = GameInfo.Strings.GetVillager(v.InternalName);

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

        // Catchphrase
        var phrase = new TextBox { Text = v.CatchPhrase, MinWidth = 200 };
        phrase.LostFocus += (_, _) =>
        {
            if (phrase.Text == v.CatchPhrase)
                return;
            v.CatchPhrase = phrase.Text ?? string.Empty;
            Commit();
        };
        var original = SmallButton(HomeStrings.CatchphraseOriginal, () =>
        {
            phrase.Text = v.CatchPhrase = GameInfo.Strings.GetVillagerDefaultPhrase(v.InternalName);
            Commit();
            return Task.CompletedTask;
        });

        // Moving out
        var moving = new CheckBox { Content = HomeStrings.MovingOut, IsChecked = v.MovingOut };
        var movingHint = Text(HomeStrings.MovingOutHint, "subtle");
        movingHint.IsVisible = v.MovingOut;
        moving.IsCheckedChanged += (_, _) =>
        {
            v.MovingOut = moving.IsChecked == true;
            movingHint.IsVisible = v.MovingOut;
            Commit();
        };

        var form = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*"),
            RowDefinitions = new RowDefinitions("Auto,Auto,Auto,Auto"),
            ColumnSpacing = 12,
            RowSpacing = 8,
            Margin = new Thickness(0, 10, 0, 0),
        };
        void Add(Control c, int row, int col)
        {
            Grid.SetRow(c, row);
            Grid.SetColumn(c, col);
            form.Children.Add(c);
        }
        Add(new TextBlock { Text = HomeStrings.Personality, VerticalAlignment = VerticalAlignment.Center }, 0, 0);
        Add(personality, 0, 1);
        Add(new TextBlock { Text = HomeStrings.Catchphrase, VerticalAlignment = VerticalAlignment.Center }, 1, 0);
        var phraseRow = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        phraseRow.Children.Add(phrase);
        phraseRow.Children.Add(original);
        Add(phraseRow, 1, 1);
        Add(moving, 2, 1);
        Add(movingHint, 3, 1);

        var actions = new WrapPanel { ItemSpacing = 8, LineSpacing = 8, Margin = new Thickness(0, 14, 0, 0) };
        actions.Children.Add(SmallButton(HomeStrings.ReplaceVillager, () => Replace(Selected), accent: true));
        actions.Children.Add(SmallButton(HomeStrings.VillagerFurniture, async () =>
        {
            var items = v.FtrStockList;
            if (await Ctx.Edit(new ItemsWindow(items, 8, 4, $"{GameInfo.Strings.GetVillager(v.InternalName)} · {HomeStrings.VillagerFurniture}")))
            {
                v.FtrStockList = items;
                Commit();
            }
        }));
        actions.Children.Add(SmallButton(HomeStrings.VillagerClothes, async () =>
        {
            var items = v.WearStockList;
            if (await Ctx.Edit(new ItemsWindow(items, 8, 3, $"{GameInfo.Strings.GetVillager(v.InternalName)} · {HomeStrings.VillagerClothes}")))
            {
                v.WearStockList = items;
                Commit();
            }
        }));
        actions.Children.Add(SmallButton(HomeStrings.VillagerFriendship, async () =>
        {
            if (await Ctx.Edit(new VillagerMemoryEditor(v)))
                Commit();
        }));
        actions.Children.Add(SmallButton(HomeStrings.VillagerRoom, async () =>
        {
            var editor = new SaveRoomFloorWallEditor(v.Room);
            if (await Ctx.Edit(editor))
            {
                v.Room = editor.Entity;
                Commit();
            }
        }));
        actions.Children.Add(SmallButton(HomeStrings.VillagerHouse, async () =>
        {
            var villagers = Ctx.SAV.Main.GetVillagers();
            var houses = Ctx.SAV.Main.GetVillagerHouses();
            if (await Ctx.Edit(new VillagerHouseEditor(houses, villagers, Ctx.SAV.Main, Selected)))
                Ctx.SAV.Main.SetVillagerHouses(houses);
        }));

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
}
