using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;
using NHSE.Injection;

namespace NHSE.Mac;

public sealed partial class Editor : NhseWindow
{
    private readonly HorizonSave SAV;
    private readonly VillagerEditor Villagers;

    /// <summary>
    /// Currently loaded player index.
    /// </summary>
    private int PlayerIndex = -1;

    private Bitmap? PlayerPhoto;

    /// <summary>
    /// Opened from the home window: copy the values into the save data when closing, without writing the files.
    /// </summary>
    public bool WriteBackOnClose { get; init; }

    protected override void OnClosing(WindowClosingEventArgs e)
    {
        base.OnClosing(e);
        if (WriteBackOnClose && !e.Cancel)
            SaveAll();
    }

    public Editor(HorizonSave file)
    {
        InitializeComponent();

        SAV = file;

        LoadPlayers();
        Villagers = LoadVillagers();

        LoadMain();
        BuildMenu();

        this.TranslateInterface(GameInfo.CurrentLanguage);
        App.LanguageChanged += OnLanguageChanged;
        AppMenus.ThemeChanged += BuildMenu;
        Closed += (_, _) =>
        {
            App.LanguageChanged -= OnLanguageChanged;
            AppMenus.ThemeChanged -= BuildMenu;
        };

        Title = SAV.GetSaveTitle("NHSE");
    }

    private void OnLanguageChanged(string lang)
    {
        this.TranslateInterface(lang);
        BuildMenu();
        Title = SAV.GetSaveTitle("NHSE");
    }

    #region Menu
    /// <summary>
    /// The WinForms menu strip becomes the macOS menu bar while this window is active.
    /// </summary>
    private void BuildMenu()
    {
        var cmd = UiUtil.CommandModifier;
        var file = AppMenus.Sub(AppMenus.FileHeader,
        [
            .. AppMenus.OpenItems(),
            new NativeMenuItemSeparator(),
            AppMenus.Item("Menu_Save", "Save", Menu_Save_Click, new KeyGesture(Key.S, cmd)),
            new NativeMenuItemSeparator(),
            AppMenus.Item(MacStrings.MenuCloseWindow, Close, new KeyGesture(Key.W, cmd)),
        ]);

        var tools = AppMenus.Sub(Translator.Tr("Editor.Menu_Tools", "Tools"),
            AppMenus.Item("Menu_DumpDecrypted", "Dump Decrypted", Menu_DumpDecrypted_Click),
            AppMenus.Item("Menu_VerifyHashes", "Verify Hashes", Menu_VerifyHashes_Click),
            AppMenus.Item("Menu_LoadDecrypted", "Load Decrypted", Menu_LoadDecrypted_Click),
            new NativeMenuItemSeparator(),
            AppMenus.Item("Menu_RAMEdit", "RAM Edit", Menu_RAMEdit_Click),
            AppMenus.Item("Menu_ItemImages", "Item Images", Menu_ItemImages_Click));

        AppMenus.SetWindowMenu(this, file, tools, AppMenus.Options(this));
    }

    private async void Menu_Save_Click()
    {
        try
        {
            SaveToFiles();
        }
        catch (Exception ex)
        {
            await Dialogs.Error(MessageStrings.MsgSaveDataExportFail, ex.Message);
            return;
        }
        await Dialogs.Alert(MessageStrings.MsgSaveDataExportSuccess);
    }

    private async void Menu_DumpDecrypted_Click()
    {
        var path = await Pickers.OpenFolder(this);
        if (path is null)
            return;
        SAV.Dump(path);
        Sound.Asterisk();
    }

    private async void Menu_LoadDecrypted_Click()
    {
        var path = await Pickers.OpenFile(this, "New Horizons Save File (main.dat)|main.dat", "main.dat", "Open main.dat ...");
        if (path is not null)
            await LoadDecryptedFromPath(path);
    }

    private async Task LoadDecryptedFromPath(string main)
    {
        var dir = Path.GetDirectoryName(main);
        if (dir is null || !Directory.Exists(dir))
        {
            await Dialogs.Alert(MessageStrings.MsgImportDirectoryDoesNotExist);
            return;
        }

        SAV.Load(dir);
        ReloadAll(); // reload all fields
        Sound.Asterisk();
    }

    private async void Menu_VerifyHashes_Click()
    {
        var result = SAV.GetInvalidHashes().ToArray();
        if (result.Length == 0)
        {
            await Dialogs.Alert(MessageStrings.MsgSaveDataHashesValid);
            return;
        }

        if (await Dialogs.Prompt(MessageBoxButtons.YesNo, MessageStrings.MsgAskExportResultToClipboard) != DialogResult.Yes)
            return;

        var lines = result.Select(z => z.ToString());
        await UiUtil.SetClipboardText(this, string.Join(Environment.NewLine, lines));
    }

    private void Menu_RAMEdit_Click()
    {
        var exist = UiUtil.FirstFormOfType<SysBotRAMEdit>();
        if (exist != null)
        {
            exist.Activate();
            return;
        }

        var sysbot = new SysBotRAMEdit(InjectionType.Generic);
        sysbot.Show(this);
    }

    private void Menu_ItemImages_Click()
    {
        var exist = UiUtil.FirstFormOfType<ImageFetcher>();
        if (exist != null)
        {
            exist.Activate();
            return;
        }

        var imgfetcher = new ImageFetcher();
        imgfetcher.Show(this);
    }
    #endregion

    /// <summary>
    /// Writes the values of the open editors into the save data, then encrypts and writes the files.
    /// </summary>
    internal void SaveToFiles()
    {
        SaveAll();
        SAV.Save((uint)DateTime.Now.Ticks);
    }

    private void ReloadAll()
    {
        Villagers.Villagers = SAV.Main.GetVillagers();
        Villagers.Origin = SAV.Players[0].Personal;
        Villagers.Reload();
        LoadPlayers();
    }

    private VillagerEditor LoadVillagers()
    {
        var p0 = SAV.Players[0].Personal;
        var villagers = SAV.Main.GetVillagers();
        var v = new VillagerEditor(villagers, p0, SAV, true, PlayerIndex);
        PAN_Villagers.Content = v;
        return v;
    }

    private void SaveAll()
    {
        SavePlayer(PlayerIndex);
        Villagers.Save();
        SAV.Main.SetVillagers(Villagers.Villagers);
        SaveMain();
    }

    private void LoadMain()
    {
        var m = SAV.Main;
        var names = Enum.GetNames<Hemisphere>();
        CB_Hemisphere.SetComboItems(names);
        CB_Hemisphere.SelectedIndex = (int)m.Hemisphere;
        CB_Tour_Hemisphere.SetComboItems(names);
        CB_Tour_Hemisphere.SelectedIndex = (int)m.TourHemisphere;

        names = Enum.GetNames<AirportColor>();
        CB_AirportColor.SetComboItems(names);
        CB_AirportColor.SelectedIndex = (int)m.AirportThemeColor;
        NUD_WeatherSeed.Value = m.WeatherSeed;
        NUD_Tour_WeatherSeed.Value = m.TourWeatherSeed;
    }

    private void SaveMain()
    {
        var m = SAV.Main;
        m.Hemisphere = (Hemisphere)CB_Hemisphere.SelectedIndex;
        m.TourHemisphere = (Hemisphere)CB_Tour_Hemisphere.SelectedIndex;
        m.AirportThemeColor = (AirportColor)CB_AirportColor.SelectedIndex;
        m.WeatherSeed = (uint)(NUD_WeatherSeed.Value ?? 0);
        m.TourWeatherSeed = (uint)(NUD_Tour_WeatherSeed.Value ?? 0);
    }

    #region Player Editing
    private void LoadPlayers()
    {
        // A valid save file has at least one player (main player).
        if (SAV.Players.Count == 0)
            throw new Exception("No players found in the loaded directory.");

        // Load the player names to the selection box.
        PlayerIndex = -1;
        CB_Players.SetComboItems(SAV.Players.Select(z => z.DirectoryName));

        // Trigger a load.
        CB_Players.SelectedIndex = 0;
        LoadPlayer(0);
    }

    private void LoadPlayer(object? sender, SelectionChangedEventArgs e)
    {
        if (CB_Players.SelectedIndex != PlayerIndex)
            LoadPlayer(CB_Players.SelectedIndex);
    }

    private async void B_EditPlayerItems_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[PlayerIndex];
        var pers = player.Personal;
        var bag = pers.Bag;
        var pocket = pers.Pocket;
        var items = pocket.Concat(bag).ToArray();
        var editor = new PlayerItemEditor(items, 10, 4, sysbot: true);
        if (!await editor.ShowDialog<bool>(this))
            return;

        pers.Pocket = items.AsSpan(0, pocket.Count).ToArray();
        pers.Bag = items.AsSpan(pocket.Count, bag.Count).ToArray();
    }

    private async void B_Storage_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[PlayerIndex];
        var pers = player.Personal;
        var p1 = pers.ItemChest;
        var editor = new PlayerItemEditor(p1, 10, 5);
        if (await editor.ShowDialog<bool>(this))
            pers.ItemChest = p1;
    }

    private async void B_RecycleBin_Click(object? sender, RoutedEventArgs e)
    {
        var items = SAV.Main.RecycleBin;
        var editor = new PlayerItemEditor(items, 10, 4);
        if (await editor.ShowDialog<bool>(this))
            SAV.Main.RecycleBin = items;
    }

    private async void B_EditPlayerRecipes_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[PlayerIndex];
        var editor = new RecipeListEditor(player);
        await editor.ShowDialog<bool>(this);
    }

    private async void B_EditPlayerReceivedItems_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[PlayerIndex];
        var editor = new ItemReceivedEditor(player);
        await editor.ShowDialog<bool>(this);
    }

    private async void B_EditPlayerReactions_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[PlayerIndex];
        var editor = new ReactionEditor(player.Personal);
        await editor.ShowDialog<bool>(this);
    }

    private async void B_EditPlayerMisc_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[PlayerIndex];
        var save = SAV.Main;
        var editor = new MiscPlayerEditor(player, save);
        await editor.ShowDialog<bool>(this);
    }

    private async void B_EditPlayerPostBox_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[PlayerIndex];
        var pb = SAV.Players[PlayerIndex].PostBox;
        var editor = new PostBoxEditor(player, pb);
        await editor.ShowDialog<bool>(this);
    }

    private void LoadPlayer(int index)
    {
        if (PlayerIndex >= 0)
            SavePlayer(PlayerIndex);

        if (index < 0)
            return;

        var player = SAV.Players[index];

        var pers = player.Personal;
        TB_Name.Text = pers.PlayerName;
        TB_TownName.Text = pers.TownName;
        NUD_BankBells.Value = Math.Min(int.MaxValue, pers.Bank);
        NUD_NookMiles.Value = Math.Min(int.MaxValue, pers.NookMiles);
        NUD_TotalNookMiles.Value = Math.Min(int.MaxValue, pers.TotalNookMiles);
        NUD_Wallet.Value = Math.Min(int.MaxValue, pers.Wallet);

        // swapped on purpose -- first count is the first two rows of items
        NUD_PocketCount1.Value = Math.Min(int.MaxValue, pers.PocketCount);
        NUD_PocketCount2.Value = Math.Min(int.MaxValue, pers.BagCount);
        NUD_StorageCount.Value = Math.Min(int.MaxValue, pers.ItemChestCount);

        if (pers.Data30 is { IsInitialized30: true } addition)
        {
            L_HotelTickets.IsVisible = NUD_HotelTickets.IsVisible = true;
            NUD_HotelTickets.Value = Math.Min(int.MaxValue, addition.HotelTickets);
        }
        else
        {
            L_HotelTickets.IsVisible = NUD_HotelTickets.IsVisible = false;
        }

        if (player.WhereAreN is not null)
        {
            L_Poki.IsVisible = NUD_Poki.IsVisible = true;
            NUD_Poki.Value = Math.Min(int.MaxValue, player.WhereAreN.Poki);
        }
        else
        {
            L_Poki.IsVisible = NUD_Poki.IsVisible = false;
        }

        try
        {
            var photo = pers.GetPhotoData();
            PlayerPhoto = new Bitmap(new MemoryStream(photo.ToArray()));
            PB_Player.Source = PlayerPhoto;
        }
        catch (Exception e)
        {
            PB_Player.Source = PlayerPhoto = null;
            Console.WriteLine(e);
        }

        PlayerIndex = index;
    }

    private void SavePlayer(int index)
    {
        if (index < 0)
            return;

        var player = SAV.Players[index];
        var pers = player.Personal;

        if (pers.PlayerName != TB_Name.Text)
        {
            var orig = pers.GetPlayerIdentity().ToArray();
            pers.PlayerName = TB_Name.Text ?? string.Empty;
            var updated = pers.GetPlayerIdentity();
            SAV.ChangeIdentity(orig, updated);
        }
        if (pers.TownName != TB_TownName.Text)
        {
            var orig = pers.GetTownIdentity().ToArray();
            pers.TownName = TB_TownName.Text ?? string.Empty;
            var updated = pers.GetTownIdentity();
            SAV.ChangeIdentity(orig, updated);
        }

        pers.Bank = pers.Bank with { Value = (uint)(NUD_BankBells.Value ?? 0) };
        pers.NookMiles = pers.NookMiles with { Value = (uint)(NUD_NookMiles.Value ?? 0) };
        pers.TotalNookMiles = pers.TotalNookMiles with { Value = (uint)(NUD_TotalNookMiles.Value ?? 0) };
        pers.Wallet = pers.Wallet with { Value = (uint)(NUD_Wallet.Value ?? 0) };

        // swapped on purpose -- first count is the first two rows of items
        pers.PocketCount = (uint)(NUD_PocketCount1.Value ?? 0);
        pers.BagCount = (uint)(NUD_PocketCount2.Value ?? 0);

        pers.ItemChestCount = (uint)(NUD_StorageCount.Value ?? 0);

        if (player.Personal.Data30 is { IsInitialized30: true } addition)
        {
            addition.HotelTickets = addition.HotelTickets with { Value = (uint)(NUD_HotelTickets.Value ?? 0) };
        }

        if (player.WhereAreN is { } x)
        {
            x.Poki = x.Poki with { Value = (uint)(NUD_Poki.Value ?? 0) };
        }
    }

    private async void B_EditAchievements_Click(object? sender, RoutedEventArgs e)
    {
        var pers = SAV.Players[PlayerIndex].Personal;
        var editor = new AchievementEditor(pers);
        await editor.ShowDialog<bool>(this);
    }

    private async void B_EditPlayerFlags_Click(object? sender, RoutedEventArgs e)
    {
        var pers = SAV.Players[PlayerIndex].Personal;
        var flags = pers.GetEventFlagsPlayer();
        var editor = new FlagEditor(flags);
        if (await editor.ShowDialog<bool>(this))
            pers.SetEventFlagsPlayer(flags);
    }

    #endregion

    private async void Menu_SavePNG_Click(object? sender, RoutedEventArgs e)
    {
        if (PlayerPhoto is null)
        {
            await Dialogs.Alert(MessageStrings.MsgNoPictureLoaded);
            return;
        }

        string name = SAV.Players[PlayerIndex].Personal.PlayerName;
        var path = await Pickers.SaveFile(this, "png file (*.png)|*.png|All files (*.*)|*.*", $"{name}.png");
        if (path is null)
            return;

        PlayerPhoto.Save(path, new PngBitmapEncoderOptions());
    }

    private async void B_EditTurnipExchange_Click(object? sender, RoutedEventArgs e)
    {
        var turnips = SAV.Main.Turnips;
        var editor = new SingleObjectEditor<TurnipStonk>(turnips, PropertySort.Categorized, false);
        if (await editor.ShowDialog<bool>(this))
            SAV.Main.Turnips = editor.Object;
    }

    private async void B_EditFieldItems_Click(object? sender, RoutedEventArgs e)
    {
        var editor = new FieldItemEditor(SAV.Main);
        await editor.ShowDialog<bool>(this);
    }

    private async void B_EditLandFlags_Click(object? sender, RoutedEventArgs e)
    {
        var flags = SAV.Main.EventFlagLand;
        var edit = flags.ToArray();
        var editor = new LandFlagEditor(edit);
        if (await editor.ShowDialog<bool>(this))
            edit.CopyTo(SAV.Main.EventFlagLand);
    }

    private async void B_EditPatterns_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[PlayerIndex];
        var patterns = SAV.Main.GetDesigns();
        var editor = new PatternEditor(patterns, player);
        if (await editor.ShowDialog<bool>(this))
            SAV.Main.SetDesigns(patterns);
    }

    private async void B_EditPRODesigns_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[PlayerIndex];
        var patterns = SAV.Main.GetDesignsPRO();
        var editor = new PatternEditorPRO(patterns, player);
        if (await editor.ShowDialog<bool>(this))
            SAV.Main.SetDesignsPRO(patterns);
    }

    private async void B_EditPatternFlag_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[PlayerIndex];
        var patterns = new[] { SAV.Main.FlagMyDesign };
        var editor = new PatternEditor(patterns, player);
        if (await editor.ShowDialog<bool>(this))
            SAV.Main.FlagMyDesign = patterns[0];
    }

    private async void B_EditDesignsTailor_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[PlayerIndex];
        var patterns = SAV.Main.GetDesignsTailor();
        var editor = new PatternEditorPRO(patterns, player);
        if (await editor.ShowDialog<bool>(this))
            SAV.Main.SetDesignsTailor(patterns);
    }

    private async void B_EditPlayerHouses_Click(object? sender, RoutedEventArgs e)
    {
        var houses = SAV.Main.GetPlayerHouses();
        var editor = new PlayerHouseEditor(houses, SAV.Players, SAV.Main, PlayerIndex);
        if (await editor.ShowDialog<bool>(this))
            SAV.Main.SetPlayerHouses(houses);
    }

    private async void B_EditCampsite_Click(object? sender, RoutedEventArgs e)
    {
        var editor = new CampsiteEditor(SAV);
        await editor.ShowDialog<bool>(this);
    }

    private async void B_EditFruitFlower_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[PlayerIndex];
        var save = SAV.Main;
        var editor = new FruitsFlowersEditor(player, save);
        await editor.ShowDialog<bool>(this);
    }

    private async void B_EditBulletin_Click(object? sender, RoutedEventArgs e)
    {
        var editor = new SingleObjectEditor<object>(SAV.Main.Bulletin, PropertySort.NoSort, false);
        if (await editor.ShowDialog<bool>(this))
            SAV.Main.Bulletin = (GSaveBulletinBoard)editor.Object;
    }

    private async void B_EditFieldGoods_Click(object? sender, RoutedEventArgs e)
    {
        var editor = new SingleObjectEditor<object>(SAV.Main.SaveFg, PropertySort.NoSort, false);
        if (await editor.ShowDialog<bool>(this))
            SAV.Main.SaveFg = (GSaveFg)editor.Object;
    }

    private async void B_EditMuseum_Click_Click(object? sender, RoutedEventArgs e)
    {
        var museum = SAV.Main.Museum;
        var editor = new MuseumEditor(museum);
        if (await editor.ShowDialog<bool>(this))
            SAV.Main.Museum = museum;
    }

    private async void B_EditVisitors_Click(object? sender, RoutedEventArgs e)
    {
        var editor = new SingleObjectEditor<object>(SAV.Main.Visitor, PropertySort.NoSort, false);
        if (await editor.ShowDialog<bool>(this))
            SAV.Main.Visitor = (GSaveVisitorNpc)editor.Object;
    }

    private static readonly IBrush Warning = new SolidColorBrush(Color.FromArgb(0x60, 0xFF, 0x00, 0x00));

    private void NUD_PocketCount_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        if (sender is NumericUpDown nud)
            SetWarning(nud, (uint)(nud.Value ?? 0) > 20);
    }

    private void NUD_Wallet_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e) => SetWarning(NUD_Wallet, (ulong)(NUD_Wallet.Value ?? 0) > 99_999);

    private static void SetWarning(NumericUpDown nud, bool warn)
    {
        if (warn)
            nud.Background = Warning;
        else
            nud.ClearValue(BackgroundProperty);
    }
}
