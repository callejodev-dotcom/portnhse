using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// The player at a glance: name and island, money, items and collections, each with a plain explanation.
/// Values are written to the save as soon as they change.
/// </summary>
public sealed class PlayerPage : UserControl
{
    private readonly HomeContext Ctx;
    private Personal Pers => Ctx.Player.Personal;

    public PlayerPage(HomeContext ctx)
    {
        Ctx = ctx;
        Build();
    }

    /// <summary>
    /// Rebuilds the page from the save (after another editor changed it).
    /// </summary>
    public void Build() => Content = Page(null, null, Header(), Money(), Items(), Collections());

    private Control Header()
    {
        var photo = new Border
        {
            Width = 96,
            Height = 96,
            CornerRadius = new CornerRadius(14),
            ClipToBounds = true,
            Background = new SolidColorBrush(Color.FromArgb(0x20, 0x80, 0x80, 0x80)),
            VerticalAlignment = VerticalAlignment.Top,
        };
        try
        {
            photo.Child = new Image { Source = new Bitmap(new MemoryStream(Pers.GetPhotoData().ToArray())), Stretch = Stretch.UniformToFill };
            ToolTip.SetTip(photo, HomeStrings.ChangePhotoHint);
        }
        catch (Exception ex)
        {
            Console.WriteLine(ex.Message);
        }

        var name = new TextBox { Text = Pers.PlayerName, MaxLength = 10, Classes = { "titlebox" }, MinWidth = 200, HorizontalAlignment = HorizontalAlignment.Left };
        ToolTip.SetTip(name, HomeStrings.RenameHint);
        CommitOnLeave(name, value => Rename(value, isTown: false));

        var town = new TextBox { Text = Pers.TownName, MaxLength = 10, MinWidth = 140, FontSize = 16, Classes = { "inline" }, HorizontalAlignment = HorizontalAlignment.Left };
        ToolTip.SetTip(town, HomeStrings.RenameHint);
        CommitOnLeave(town, value => Rename(value, isTown: true));

        var island = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        island.Children.Add(new TextBlock { Text = HomeStrings.IslandOf, VerticalAlignment = VerticalAlignment.Center, FontSize = 15, Classes = { "subtle" } });
        island.Children.Add(town);

        var text = new StackPanel { Spacing = 6, VerticalAlignment = VerticalAlignment.Center };
        text.Children.Add(name);
        text.Children.Add(island);
        if (Ctx.SAV.Players.Count > 1)
            text.Children.Add(Text(string.Format(HomeStrings.PlayerOf, Ctx.PlayerIndex + 1, Ctx.SAV.Players.Count), "subtle"));

        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 18 };
        g.Children.Add(photo);
        Grid.SetColumn(text, 1);
        g.Children.Add(text);
        return g;
    }

    private static void CommitOnLeave(TextBox box, Action<string> commit)
    {
        void Commit()
        {
            var value = (box.Text ?? string.Empty).Trim();
            if (value.Length != 0)
                commit(value);
        }
        box.LostFocus += (_, _) => Commit();
        box.KeyDown += (_, e) =>
        {
            if (e.Key == Key.Enter)
                Commit();
        };
    }

    /// <summary>
    /// Same as the classic editor: the name is also stored in letters, memories… so every copy is replaced.
    /// </summary>
    private void Rename(string value, bool isTown)
    {
        var current = isTown ? Pers.TownName : Pers.PlayerName;
        if (current == value)
            return;
        var orig = (isTown ? Pers.GetTownIdentity() : Pers.GetPlayerIdentity()).ToArray();
        if (isTown)
            Pers.TownName = value;
        else
            Pers.PlayerName = value;
        var updated = isTown ? Pers.GetTownIdentity() : Pers.GetPlayerIdentity();
        Ctx.SAV.ChangeIdentity(orig, updated);
        Ctx.MarkDirty();
    }

    private Control Money()
    {
        var wallet = Amount(Pers.Wallet.Value);
        void CheckWallet()
        {
            // The game only holds 99,999 in the wallet.
            if ((wallet.Value ?? 0) > 99_999)
                wallet.Foreground = Brushes.IndianRed;
            else
                wallet.ClearValue(TemplatedControl.ForegroundProperty);
        }
        CheckWallet();
        wallet.ValueChanged += (_, _) =>
        {
            Pers.Wallet = Pers.Wallet with { Value = (uint)(wallet.Value ?? 0) };
            CheckWallet();
            Ctx.MarkDirty();
        };

        var bank = Amount(Pers.Bank.Value);
        bank.ValueChanged += (_, _) =>
        {
            Pers.Bank = Pers.Bank with { Value = (uint)(bank.Value ?? 0) };
            Ctx.MarkDirty();
        };

        var miles = Amount(Pers.NookMiles.Value);
        miles.ValueChanged += (_, _) =>
        {
            Pers.NookMiles = Pers.NookMiles with { Value = (uint)(miles.Value ?? 0) };
            Ctx.MarkDirty();
        };

        var cards = new List<Control>
        {
            Stat("MoneyBag039", HomeStrings.Wallet, wallet, HomeStrings.WalletHint),
            Stat("MoneyBag069", HomeStrings.Bank, bank, HomeStrings.BankHint),
            Stat("Coin", HomeStrings.NookMiles, miles, string.Format(HomeStrings.NookMilesEarned, Pers.TotalNookMiles.Value.ToString("N0"))),
        };

        if (Ctx.Player.WhereAreN is { } wan)
        {
            var poki = Amount(wan.Poki.Value);
            poki.ValueChanged += (_, _) =>
            {
                wan.Poki = wan.Poki with { Value = (uint)(poki.Value ?? 0) };
                Ctx.MarkDirty();
            };
            cards.Add(Stat("Coin", HomeStrings.Poki, poki, HomeStrings.PokiHint));
        }
        if (Pers.Data30 is { IsInitialized30: true } addition)
        {
            var tickets = Amount(addition.HotelTickets.Value);
            tickets.ValueChanged += (_, _) =>
            {
                addition.HotelTickets = addition.HotelTickets with { Value = (uint)(tickets.Value ?? 0) };
                Ctx.MarkDirty();
            };
            cards.Add(Stat("RollanTicket", HomeStrings.HotelTickets, tickets, HomeStrings.HotelTicketsHint));
        }

        var panel = new StackPanel();
        panel.Children.Add(Section(HomeStrings.SectionMoney));
        panel.Children.Add(Tiles(3, [.. cards]));
        return panel;
    }

    private Control Items()
    {
        var pocket = Pers.Pocket;
        var bag = Pers.Bag;
        var pockets = pocket.Concat(bag).ToArray();
        var chest = Pers.ItemChest;
        var recycle = Ctx.SAV.Main.RecycleBin;

        var panel = new StackPanel();
        panel.Children.Add(Section(HomeStrings.SectionItems));
        panel.Children.Add(Tiles(3,
            ActionCard("Bag", HomeStrings.Pockets, string.Format(HomeStrings.PocketsHint, pockets.Count(z => !z.IsNone)), async () =>
            {
                var items = Pers.Pocket.Concat(Pers.Bag).ToArray();
                var count = Pers.Pocket.Count;
                if (await Ctx.Edit(new ItemsWindow(items, 10, 4, HomeStrings.Pockets)))
                {
                    Pers.Pocket = items.AsSpan(0, count).ToArray();
                    Pers.Bag = items.AsSpan(count).ToArray();
                    Build();
                }
            }, ItemStrip(pockets)),
            ActionCard("Cardboard", HomeStrings.Storage, string.Format(HomeStrings.StorageHint, chest.Count(z => !z.IsNone), Pers.ItemChestCount), async () =>
            {
                // Only the spaces the player has unlocked (the save keeps room for many more).
                var items = Pers.ItemChest;
                var unlocked = items.Take((int)Math.Clamp(Pers.ItemChestCount, 50, (uint)items.Count)).ToArray();
                if (await Ctx.Edit(new ItemsWindow(unlocked, 10, 5, HomeStrings.Storage)))
                {
                    Pers.ItemChest = items;
                    Build();
                }
            }, ItemStrip(chest)),
            ActionCard("Can", HomeStrings.RecycleBin, HomeStrings.RecycleBinHint, async () =>
            {
                var items = Ctx.SAV.Main.RecycleBin;
                if (await Ctx.Edit(new ItemsWindow(items, 10, 4, HomeStrings.RecycleBin, HomeStrings.RecycleBinHint)))
                {
                    Ctx.SAV.Main.RecycleBin = items;
                    Build();
                }
            }, ItemStrip(recycle))));
        return panel;
    }

    private Control Collections()
    {
        var book = Pers.GetRecipeBook();
        var recipes = RecipeList.Recipes;
        var known = recipes.Keys.Count(z => book.GetIsKnown(z));

        var reactions = Pers.Reactions;
        var allReactions = Enum.GetValues<Reaction>().Count(z => z != 0 && !z.ToString().StartsWith("UNUSED", StringComparison.Ordinal));
        var knownReactions = reactions.ManpuBit.Count(z => z != 0);

        var mail = Ctx.Player.PostBox.GetUsedPostBoxSlots().Count;

        var recipeCard = Card(CardHeader("BookRecipe", HomeStrings.Recipes, string.Format(HomeStrings.RecipesHint, known, recipes.Count),
            Row(SmallButton(HomeStrings.LearnAll, () =>
                {
                    var b = Pers.GetRecipeBook();
                    b.GiveAll(RecipeList.Recipes);
                    Pers.SetRecipeBook(b);
                    Ctx.MarkDirty();
                    Sound.Asterisk();
                    Build();
                    return Task.CompletedTask;
                }),
                SmallButton(HomeStrings.Open, async () =>
                {
                    if (await Ctx.Edit(new RecipeListEditor(Ctx.Player)))
                        Build();
                }))));

        var reactionCard = Card(CardHeader("Love", HomeStrings.Reactions, string.Format(HomeStrings.ReactionsHint, knownReactions, allReactions),
            Row(SmallButton(HomeStrings.LearnAll, () =>
                {
                    var r = Pers.Reactions;
                    r.AddMissingReactions();
                    Pers.Reactions = r;
                    Ctx.MarkDirty();
                    Sound.Asterisk();
                    Build();
                    return Task.CompletedTask;
                }),
                SmallButton(HomeStrings.Open, async () =>
                {
                    if (await Ctx.Edit(new ReactionEditor(Pers)))
                        Build();
                }))));

        var panel = new StackPanel();
        panel.Children.Add(Section(HomeStrings.SectionCollections));
        panel.Children.Add(Tiles(3,
            recipeCard,
            reactionCard,
            ActionCard("StarPiece", HomeStrings.Achievements, HomeStrings.AchievementsHint, () => Ctx.Edit(new AchievementEditor(Pers))),
            ActionCard("Post", HomeStrings.Mail, string.Format(HomeStrings.MailHint, mail), async () =>
            {
                await Ctx.Edit(new PostBoxEditor(Ctx.Player, Ctx.Player.PostBox));
                Build();
            }),
            ActionCard("Book", HomeStrings.Catalog, HomeStrings.CatalogHint, () => Ctx.Edit(new ItemReceivedEditor(Ctx.Player))),
            ActionCard("BdayCupcake", HomeStrings.Profile, HomeStrings.ProfileHint, () => Ctx.Edit(new MiscPlayerEditor(Ctx.Player, Ctx.SAV.Main)))));
        return panel;
    }
}
