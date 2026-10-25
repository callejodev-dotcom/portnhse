using System.Globalization;
using System.Text;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using static NHSE.Core.ItemKind;

namespace NHSE.Mac;

/// <summary>
/// Kinds of items as a player thinks of them (the game has ~200 internal kinds).
/// </summary>
public enum ItemCategory
{
    All,
    Furniture,
    Walls,
    Clothes,
    Tools,
    Materials,
    Food,
    Plants,
    Fish,
    Bugs,
    Sea,
    Fossils,
    Art,
    Recipes,
    Other,
}

/// <summary>
/// Item picker: category chips, a search box and a list of matching items with pictures.
/// Matching ignores case and accents and finds every typed word anywhere in the name.
/// The Recipes category lists DIY recipe cards, one per recipe.
/// </summary>
public sealed class ItemSearchBox : UserControl
{
    /// <param name="Id">Item to put in the space.</param>
    /// <param name="Count">Value of its count field (the recipe of a DIY card).</param>
    /// <param name="SpriteId">Item whose picture is shown.</param>
    public sealed record Entry(ushort Id, ushort Count, ushort SpriteId, string Name, string Key, ItemCategory Category);

    private static string? CachedLanguage;
    private static Entry[] CachedEntries = [];

    private readonly TextBox Search;
    private readonly ListBox Results;
    private ItemCategory Category = ItemCategory.All;

    /// <summary>
    /// Raised when an item is clicked (or Enter is pressed on it), with the item and its count value.
    /// </summary>
    public event Action<ushort, ushort>? ItemChosen;

    /// <param name="height">Height of the result list.</param>
    /// <param name="only">Show only this category, without the category chips (e.g. walls and floors for a room).</param>
    public ItemSearchBox(double height = 260, ItemCategory? only = null)
    {
        if (only is { } fixedCategory)
            Category = fixedCategory;
        Search = new TextBox { PlaceholderText = HomeStrings.SearchItem };
        Results = new ListBox
        {
            Height = height,
            Margin = new Thickness(0, 6, 0, 0),
            ItemTemplate = new FuncDataTemplate<Entry>((e, _) =>
            {
                var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
                if (e is null)
                    return row;
                row.Children.Add(new Image { Source = ItemSprite.GetItemSprite(e.SpriteId), Width = 28, Height = 28 });
                row.Children.Add(new TextBlock { Text = e.Name, VerticalAlignment = VerticalAlignment.Center, TextTrimming = TextTrimming.CharacterEllipsis });
                return row;
            }),
        };

        Search.TextChanged += (_, _) => Filter();
        Search.KeyDown += (_, e) =>
        {
            if (e.Key == Key.Down && Results.ItemCount > 0)
            {
                Results.SelectedIndex = 0;
                Results.Focus();
                e.Handled = true;
            }
            else if (e.Key == Key.Enter && Results.ItemCount > 0)
            {
                Choose(Results.Items[0] as Entry);
                e.Handled = true;
            }
        };
        Results.Tapped += (_, _) => Choose(Results.SelectedItem as Entry);
        Results.KeyDown += (_, e) =>
        {
            if (e.Key == Key.Enter)
            {
                Choose(Results.SelectedItem as Entry);
                e.Handled = true;
            }
        };

        var panel = new StackPanel();
        if (only is null)
            panel.Children.Add(Chips());
        panel.Children.Add(Search);
        panel.Children.Add(Results);
        Content = panel;
        Filter();
    }

    public void FocusSearch() => Search.Focus();

    private void Choose(Entry? e)
    {
        if (e is not null)
            ItemChosen?.Invoke(e.Id, e.Count);
    }

    private WrapPanel Chips()
    {
        var panel = new WrapPanel { ItemSpacing = 4, LineSpacing = 4, Margin = new Thickness(0, 0, 0, 8) };
        var chips = new List<ToggleButton>();
        foreach (var c in Enum.GetValues<ItemCategory>())
        {
            var chip = new ToggleButton { Content = GetName(c), IsChecked = c == Category, Classes = { "chip" } };
            chip.Click += (_, _) =>
            {
                Category = c;
                foreach (var other in chips)
                    other.IsChecked = other == chip;
                Filter();
            };
            chips.Add(chip);
            panel.Children.Add(chip);
        }
        return panel;
    }

    public static string GetName(ItemCategory c) => c switch
    {
        ItemCategory.All => HomeStrings.CatAll,
        ItemCategory.Furniture => HomeStrings.CatFurniture,
        ItemCategory.Walls => HomeStrings.CatWalls,
        ItemCategory.Clothes => HomeStrings.CatClothes,
        ItemCategory.Tools => HomeStrings.CatTools,
        ItemCategory.Materials => HomeStrings.CatMaterials,
        ItemCategory.Food => HomeStrings.CatFood,
        ItemCategory.Plants => HomeStrings.CatPlants,
        ItemCategory.Fish => HomeStrings.CatFish,
        ItemCategory.Bugs => HomeStrings.CatBugs,
        ItemCategory.Sea => HomeStrings.CatSea,
        ItemCategory.Fossils => HomeStrings.CatFossils,
        ItemCategory.Art => HomeStrings.CatArt,
        ItemCategory.Recipes => HomeStrings.CatRecipes,
        _ => HomeStrings.CatOther,
    };

    private void Filter()
    {
        var words = Normalize(Search.Text ?? string.Empty).Split(' ', StringSplitOptions.RemoveEmptyEntries);
        IEnumerable<Entry> matches = GetEntries();
        matches = Category == ItemCategory.All
            ? matches.Where(z => z.Category != ItemCategory.Recipes)
            : matches.Where(z => z.Category == Category);
        if (words.Length != 0)
        {
            matches = matches.Where(z => words.All(w => z.Key.Contains(w, StringComparison.Ordinal)))
                .OrderBy(z => z.Key.StartsWith(words[0], StringComparison.Ordinal) ? 0 : 1);
        }
        Results.ItemsSource = matches.Take(600).ToArray();
    }

    /// <summary>
    /// Every named item of the current language, sorted by name, plus one DIY card per recipe.
    /// </summary>
    private static Entry[] GetEntries()
    {
        if (CachedLanguage == GameInfo.CurrentLanguage)
            return CachedEntries;
        CachedLanguage = GameInfo.CurrentLanguage;

        var items = GameInfo.Strings.ItemDataSource
            .Where(z => z.Value != Item.NONE && IsRealName(z.Text))
            .Select(z => new Entry((ushort)z.Value, 0, (ushort)z.Value, z.Text, Normalize(z.Text), GetCategory((ushort)z.Value)));

        var names = GameInfo.Strings.itemlist;
        var recipes = RecipeList.Recipes
            .Where(z => z.Value < names.Length && !string.IsNullOrWhiteSpace(names[z.Value]))
            .Select(z => new Entry(Item.DIYRecipe, z.Key, z.Value, names[z.Value], Normalize(names[z.Value]), ItemCategory.Recipes));

        // Sort by the letters, so names starting with ¡ ¿ ( don't all come first.
        return CachedEntries = [.. items.Concat(recipes).OrderBy(z => z.Key.TrimStart(NotLetter), StringComparer.Ordinal)];
    }

    private static readonly char[] NotLetter = [.. "¡!¿?()[]\"'«»-—–.,:;#*+ "];

    /// <summary>
    /// Skips the unnamed internal entries ("(Item #123)", "—").
    /// </summary>
    private static bool IsRealName(string text)
    {
        if (string.IsNullOrWhiteSpace(text) || text.StartsWith("(Item #", StringComparison.Ordinal))
            return false;
        return text.Any(char.IsLetterOrDigit);
    }

    public static ItemCategory GetCategory(ushort id)
    {
        var k = ItemInfo.GetItemKind(id);
        if (k == Kind_Fish) return ItemCategory.Fish;
        if (k == Kind_Insect) return ItemCategory.Bugs;
        if (k == Kind_DiveFish) return ItemCategory.Sea;
        if (k is Kind_Fossil or Kind_FossilUnknown) return ItemCategory.Fossils;
        if (k is Kind_Picture or Kind_PictureFake or Kind_Sculpture or Kind_SculptureFake) return ItemCategory.Art;
        if (k is Kind_RoomWall or Kind_RoomFloor or Kind_Rug or Kind_RugMyDesign) return ItemCategory.Walls;
        if (k.IsClothing || k is Kind_Umbrella or Kind_Shoes) return ItemCategory.Clothes;
        if (k.IsFurniture || k is Kind_FtrWall or Kind_Counter or Kind_Pillar or Kind_Fence or Kind_Lantern or Kind_Windmill or Kind_HousePost or Kind_DoorDeco or Kind_Poster or Kind_Gyroid or Kind_XmasDeco)
            return ItemCategory.Furniture;
        if (k is Kind_FishingRod or Kind_Net or Kind_Shovel or Kind_Axe or Kind_Watering or Kind_Slingshot or Kind_Ladder or Kind_ChangeStick
            or Kind_WoodenStickTool or Kind_GroundMaker or Kind_RiverMaker or Kind_CliffMaker or Kind_Timer or Kind_Megaphone or Kind_SmartPhone)
            return ItemCategory.Tools;
        if (k.IsCrafting || k is Kind_Feather or Kind_RainbowFeather or Kind_StarPiece or Kind_SnowCrystal or Kind_Sakurapetal or Kind_AutumnLeaf
            or Kind_ShellDrift or Kind_LoveCrystal or Kind_Honeycomb or Kind_DreamGold)
            return ItemCategory.Materials;
        if (k is Kind_Fruit or Kind_Vegetable or Kind_Mushroom or Kind_CookingMaterial or Kind_Dishes or Kind_Drink or Kind_HarvestDish or Kind_Candy
            or Kind_Turnip or Kind_Icecandy or Kind_Tapioca or Kind_SouvenirChocolate or Kind_JuiceFuzzyapple or Kind_BdayCupcake or Kind_Candyfloss)
            return ItemCategory.Food;
        if (k.IsFlower || k is Kind_FlowerSeed or Kind_Bush or Kind_BushSeedling or Kind_Tree or Kind_TreeSeedling or Kind_VegeSeedling or Kind_VegeTree
            or Kind_Weed or Kind_WeedLight or Kind_Vine)
            return ItemCategory.Plants;
        if (k == Kind_DIYRecipe) return ItemCategory.Recipes;
        return ItemCategory.Other;
    }

    /// <summary>
    /// Lower case, without accents, so "sofa" finds "sofá".
    /// </summary>
    public static string Normalize(string s)
    {
        var d = s.ToLowerInvariant().Normalize(NormalizationForm.FormD);
        var sb = new StringBuilder(d.Length);
        foreach (var c in d)
        {
            if (CharUnicodeInfo.GetUnicodeCategory(c) != UnicodeCategory.NonSpacingMark)
                sb.Append(c);
        }
        return sb.ToString();
    }
}
