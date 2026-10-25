using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// The passport: birthday (month by name, then the day), the passport fruit (with pictures) and the day the
/// player registered. The rarely needed «island founder» mark is under «Advanced options».
/// Saved to the player on «Save».
/// </summary>
public sealed class ProfileWindow : PlayerDialog
{
    private readonly Personal Pers;
    private readonly ComboBox Month;
    private readonly ComboBox Day;
    private readonly CalendarDatePicker Registered;
    private readonly CheckBox Founder;
    private readonly List<ToggleButton> FruitButtons = [];
    private ushort Fruit;
    private bool BirthdayChanged;

    public ProfileWindow(Player player) : base(HomeStrings.Profile, 640, 720)
    {
        Pers = player.Personal;
        var birthday = Pers.Birthday;
        Fruit = Pers.ProfileFruit;

        // Birthday: month names of the game language, and only the days that month has.
        var culture = PlayerUi.GameCulture;
        var months = Enumerable.Range(1, 12).Select(m => culture.DateTimeFormat.GetMonthName(m)).ToArray();
        Month = new ComboBox { ItemsSource = months, MinWidth = 170, MaxDropDownHeight = 420 };
        Day = new ComboBox { MinWidth = 80, MaxDropDownHeight = 420 };
        Month.SelectionChanged += (_, _) => FillDays();
        Month.SelectedIndex = birthday.Month is >= 1 and <= 12 ? birthday.Month - 1 : 0;
        FillDays();
        Day.SelectedIndex = Math.Clamp(birthday.Day - 1, 0, Day.ItemCount - 1);
        // A save without a birthday keeps it empty unless one is chosen.
        BirthdayChanged = birthday.Month is >= 1 and <= 12 && birthday.Day >= 1;
        Month.SelectionChanged += (_, _) => BirthdayChanged = true;
        Day.SelectionChanged += (_, _) => BirthdayChanged = true;

        // In the order of the language: «1 de julio», «July 1».
        var birthdayRow = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Margin = new Thickness(0, 8, 0, 0) };
        foreach (var part in PlayerStrings.BirthdayPattern.Split(' ', StringSplitOptions.RemoveEmptyEntries))
        {
            birthdayRow.Children.Add(part switch
            {
                "{d}" => Day,
                "{m}" => Month,
                _ => new TextBlock { Text = part, VerticalAlignment = VerticalAlignment.Center },
            });
        }

        // Passport fruit: one button per fruit, with its picture.
        var fruits = new WrapPanel { ItemSpacing = 8, LineSpacing = 8, Margin = new Thickness(0, 8, 0, 0) };
        foreach (var id in GameLists.Fruits)
            fruits.Children.Add(FruitButton(id));
        UpdateFruits();

        // Registration day
        Registered = PlayerUi.DatePicker(PlayerUi.ToDate(Pers.ProfileTimestamp));
        Registered.Margin = new Thickness(0, 8, 0, 0);

        Founder = new CheckBox { Content = PlayerStrings.Founder, IsChecked = Pers.ProfileIsMakeVillage };
        var advanced = new StackPanel { Spacing = 4 };
        advanced.Children.Add(Founder);
        advanced.Children.Add(PlayerUi.Subtle(PlayerStrings.FounderHint));

        var panel = new StackPanel { Spacing = 12 };
        panel.Children.Add(Passport(player));
        panel.Children.Add(HomeUi.Card(HomeUi.CardHeader("BdayCupcake", PlayerStrings.Birthday, PlayerStrings.BirthdayHint, birthdayRow)));
        panel.Children.Add(HomeUi.Card(HomeUi.CardHeader("Apple", PlayerStrings.PassportFruit, PlayerStrings.PassportFruitHint, fruits)));
        panel.Children.Add(HomeUi.Card(HomeUi.CardHeader("PlaneTicket", PlayerStrings.Registered, PlayerStrings.RegisteredHint, Registered)));
        panel.Children.Add(new Expander { Header = HomeStrings.AdvancedOptions, Content = advanced, HorizontalAlignment = HorizontalAlignment.Stretch });

        var body = new ScrollViewer { Content = panel, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled };
        SetLayout(HomeStrings.Profile, PlayerStrings.ProfileHint, body);
    }

    private static readonly int[] DaysInMonth = [31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];

    private void FillDays()
    {
        var month = Math.Max(0, Month.SelectedIndex);
        var current = Day.SelectedIndex;
        var days = Enumerable.Range(1, DaysInMonth[month]).Select(d => d.ToString()).ToArray();
        Day.ItemsSource = days;
        Day.SelectedIndex = Math.Clamp(current, 0, days.Length - 1);
    }

    /// <summary>
    /// Photo, name and island, as on the passport, so it is clear whose profile this is.
    /// </summary>
    private Border Passport(Player player)
    {
        var photo = new Border
        {
            Width = 72,
            Height = 72,
            CornerRadius = new CornerRadius(12),
            ClipToBounds = true,
            Background = new SolidColorBrush(Color.FromArgb(0x20, 0x80, 0x80, 0x80)),
        };
        try
        {
            var jpeg = player.Personal.GetPhotoData();
            if (jpeg.Length != 0)
                photo.Child = new Image { Source = new Bitmap(new MemoryStream(jpeg)), Stretch = Stretch.UniformToFill };
        }
        catch (Exception ex)
        {
            Console.WriteLine(ex.Message);
        }
        var text = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Spacing = 2 };
        var name = HomeUi.Text(Pers.PlayerName, "cardtitle");
        name.FontSize = 20;
        text.Children.Add(name);
        text.Children.Add(PlayerUi.Subtle(string.Format(PlayerStrings.IslandName, Pers.TownName), 14));
        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 14 };
        g.Children.Add(photo);
        Grid.SetColumn(text, 1);
        g.Children.Add(text);
        return HomeUi.Card(g);
    }

    private ToggleButton FruitButton(ushort id)
    {
        var content = new StackPanel { Spacing = 4, Width = 74 };
        content.Children.Add(new Image { Source = ItemSprite.GetItemSprite(id), Width = 40, Height = 40, HorizontalAlignment = HorizontalAlignment.Center });
        content.Children.Add(new TextBlock
        {
            Text = GameInfo.Strings.GetItemName(new Item(id)),
            HorizontalAlignment = HorizontalAlignment.Center,
            TextAlignment = TextAlignment.Center,
            TextWrapping = TextWrapping.Wrap,
        });
        var b = new ToggleButton { Content = content, Tag = id, Padding = new Thickness(6), CornerRadius = new CornerRadius(10) };
        b.Click += (_, _) =>
        {
            Fruit = id;
            UpdateFruits();
        };
        FruitButtons.Add(b);
        return b;
    }

    private void UpdateFruits()
    {
        foreach (var b in FruitButtons)
            b.IsChecked = b.Tag is ushort id && id == Fruit;
    }

    /// <summary>
    /// Sets the birthday (used by the tests).
    /// </summary>
    internal void SetBirthday(int month, int day)
    {
        Month.SelectedIndex = month - 1;
        Day.SelectedIndex = day - 1;
    }

    internal void SetFruit(ushort id)
    {
        Fruit = id;
        UpdateFruits();
    }

    protected override void Save()
    {
        if (BirthdayChanged)
        {
            var bd = Pers.Birthday;
            bd.Month = (byte)(Month.SelectedIndex + 1);
            bd.Day = (byte)(Day.SelectedIndex + 1);
            Pers.Birthday = bd;
            Pers.ProfileBirthday = bd;
        }
        Pers.ProfileFruit = Fruit;
        Pers.ProfileTimestamp = Registered.SelectedDate ?? DateTime.MinValue;
        Pers.ProfileIsMakeVillage = Founder.IsChecked == true;
    }
}
