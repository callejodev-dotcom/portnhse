using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Friendly main window: a sidebar with the player(s), villagers, island, museum, designs and the advanced tools,
/// and a single "Save changes" button. Every value is written to the save data as soon as it changes;
/// the files are only written when saving.
/// </summary>
public sealed partial class HomeWindow : NhseWindow
{
    private readonly HomeContext Ctx;
    internal bool Dirty { get; private set; }
    private bool CloseConfirmed;
    private DateTime? LastSaved;

    private sealed record NavEntry(string Key, string Text, IImage? Icon, int Player = -1);

    /// <summary> The console the save was brought from, when it was opened over Wi-Fi: «Save» sends to it. </summary>
    private readonly LiveGame? Live;
    private readonly Button ReloadIsland = new() { HorizontalAlignment = HorizontalAlignment.Stretch, Height = 30 };

    public HomeWindow(HorizonSave sav, LiveGame? live = null)
    {
        InitializeComponent();
        Live = live;
        Ctx = new HomeContext(sav, this) { Live = live is not null };
        if (live is not null)
        {
            ReloadIsland.Content = HomeStrings.LiveReload;
            ReloadIsland.Click += async (_, _) => await ReloadFromConsole();
            if (B_Save.Parent is StackPanel bottom)
                bottom.Children.Insert(bottom.Children.IndexOf(B_Save) + 1, ReloadIsland);
            Closed += (_, _) => live.Dispose();
        }
        Ctx.Changed += () =>
        {
            Dirty = true;
            UpdateStatus();
        };

        BuildNav();
        BuildMenu();
        UpdateStatus();

        App.LanguageChanged += OnLanguageChanged;
        AppMenus.ThemeChanged += BuildMenu;
        Closed += (_, _) =>
        {
            App.LanguageChanged -= OnLanguageChanged;
            AppMenus.ThemeChanged -= BuildMenu;
        };
    }

    private void OnLanguageChanged(string lang)
    {
        var selected = LB_Nav.SelectedIndex;
        BuildNav();
        LB_Nav.SelectedIndex = Math.Max(0, selected);
        BuildMenu();
        UpdateStatus();
    }

    private void BuildNav()
    {
        var entries = new List<NavEntry>();
        for (int i = 0; i < Ctx.SAV.Players.Count; i++)
        {
            var pers = Ctx.SAV.Players[i].Personal;
            entries.Add(new NavEntry("player", pers.PlayerName, GetPhoto(pers), i));
        }
        entries.Add(new NavEntry("villagers", HomeStrings.NavVillagers, NavIcon("Leaf")));
        entries.Add(new NavEntry("island", HomeStrings.NavIsland, NavIcon("PltPalm")));
        entries.Add(new NavEntry("museum", HomeStrings.NavMuseum, NavIcon("Fossil")));
        entries.Add(new NavEntry("designs", HomeStrings.NavDesigns, NavIcon("MyDesignPro")));
        entries.Add(new NavEntry("advanced", HomeStrings.NavAdvanced, NavIcon("ToolChangeStick")));

        LB_Nav.ItemsSource = entries.Select(MakeNavItem).ToArray();
        LB_Nav.SelectedIndex = 0;
        Title = Live is null ? Ctx.SAV.GetSaveTitle("NHSE") : $"NHSE · {Ctx.SAV.Players[0].Personal.TownName} ({HomeStrings.LiveTitleSuffix})";
    }

    private static IImage? NavIcon(string name) => ItemSprite.GetResource($"sprites/MenuIcon/{name}.png");

    private static Bitmap? GetPhoto(Personal pers)
    {
        try
        {
            return new Bitmap(new MemoryStream(pers.GetPhotoData().ToArray()));
        }
        catch
        {
            return null;
        }
    }

    private static Control MakeNavItem(NavEntry e)
    {
        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 10, Tag = e };
        var icon = new Border { Width = 26, Height = 26, CornerRadius = new Avalonia.CornerRadius(e.Player >= 0 ? 13 : 0), ClipToBounds = true };
        icon.Child = new Image { Source = e.Icon, Stretch = e.Player >= 0 ? Stretch.UniformToFill : Stretch.Uniform };
        row.Children.Add(icon);
        row.Children.Add(new TextBlock { Text = e.Text, VerticalAlignment = VerticalAlignment.Center, FontSize = 14 });
        return row;
    }

    private void LB_Nav_SelectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (LB_Nav.SelectedItem is not Control { Tag: NavEntry entry })
            return;
        if (entry.Player >= 0)
            Ctx.PlayerIndex = entry.Player;
        PAN_Page.Content = entry.Key switch
        {
            "player" => new PlayerPage(Ctx),
            "villagers" => new VillagersPage(Ctx),
            "island" => ActionPages.Island(Ctx),
            "museum" => ActionPages.Museum(Ctx),
            "designs" => ActionPages.Designs(Ctx),
            _ => ActionPages.Advanced(Ctx, OpenClassicEditor, ReloadAll),
        };
    }

    /// <summary>
    /// Builds every page again from the save (after it was filled with what the game has), keeping the chosen page.
    /// </summary>
    private void ReloadAll()
    {
        var selected = LB_Nav.SelectedIndex;
        BuildNav();
        LB_Nav.SelectedIndex = Math.Max(0, selected);
    }

    /// <summary>
    /// The full WinForms-style editor, as a sheet: what it changes is copied into the save when it closes.
    /// </summary>
    private async Task OpenClassicEditor()
    {
        var editor = new Editor(Ctx.SAV) { WriteBackOnClose = true };
        await editor.ShowDialog(this);
        Ctx.MarkDirty();
        BuildNav();
    }

    private void UpdateStatus()
    {
        if (Live is not null)
        {
            B_Save.Content = HomeStrings.LiveSaveButton;
            L_Status.Text = Dirty ? HomeStrings.LiveStatusDirty : HomeStrings.LiveStatus;
            if (Dirty)
                L_Status.Foreground = Brushes.DarkOrange;
            else
                L_Status.ClearValue(TextBlock.ForegroundProperty);
            L_Backup.Text = HomeStrings.LiveStatusCopies;
            return;
        }
        B_Save.Content = HomeStrings.SaveButton;
        L_Status.Text = Dirty
            ? HomeStrings.StatusDirty
            : LastSaved is { } t ? string.Format(HomeStrings.StatusJustSaved, t.ToString("HH:mm")) : HomeStrings.StatusSaved;
        if (Dirty)
            L_Status.Foreground = Brushes.DarkOrange;
        else
            L_Status.ClearValue(TextBlock.ForegroundProperty);
        L_Backup.Text = Program.Settings.AutomaticBackup ? HomeStrings.StatusBackupOn : HomeStrings.StatusBackupOff;
    }

    private async void B_Save_Click(object? sender, RoutedEventArgs e) => await SaveToFiles();

    /// <summary>
    /// Encrypts and writes the save files; returns false if writing failed.
    /// </summary>
    internal async Task<bool> SaveToFiles()
    {
        if (Live is not null)
            return await SendToConsole();
        // Text boxes write their value when they lose focus.
        B_Save.Focus();
        try
        {
            Ctx.SAV.Save((uint)DateTime.Now.Ticks);
        }
        catch (Exception ex)
        {
            await Dialogs.Error(MessageStrings.MsgSaveDataExportFail, ex.Message);
            return false;
        }
        Dirty = false;
        LastSaved = DateTime.Now;
        UpdateStatus();
        Sound.Asterisk();
        return true;
    }

    /// <summary> Sends what changed to the console (the save opened over Wi-Fi). </summary>
    private async Task<bool> SendToConsole()
    {
        B_Save.Focus();
        B_Save.IsEnabled = false;
        try
        {
            var sent = await Task.Run(() => Live!.Push(Ctx.SAV));
            if (sent < 0)
            {
                await Dialogs.Error(HomeStrings.LiveGameMoved);
                return false;
            }
        }
        catch (Exception ex)
        {
            await Dialogs.Error(LiveGame.Explain(ex));
            return false;
        }
        finally
        {
            B_Save.IsEnabled = true;
        }
        Dirty = false;
        UpdateStatus();
        Sound.Asterisk();
        return true;
    }

    /// <summary> Saves the game in the console and loads the island again, then brings it again. </summary>
    private async Task ReloadFromConsole()
    {
        if (Dirty && await Dialogs.Prompt(MessageBoxButtons.YesNo, HomeStrings.LiveReloadUnsent) != DialogResult.Yes)
            return;
        if (await Dialogs.Prompt(MessageBoxButtons.YesNo, HomeStrings.LiveReloadAsk) != DialogResult.Yes)
            return;
        ReloadIsland.IsEnabled = false;
        B_Save.IsEnabled = false;
        try
        {
            var live = Live!;
            await live.Reload(text => Avalonia.Threading.Dispatcher.UIThread.Post(() => L_Status.Text = text));
            L_Status.Text = HomeStrings.LiveGamePulling;
            await Task.Delay(8000); // the island finishes loading
            var problem = await Task.Run(() => live.Pull(Ctx.SAV));
            if (problem is not null)
                await Dialogs.Error(problem);
            else
                ReloadAll();
            Dirty = false;
        }
        catch (Exception ex)
        {
            await Dialogs.Error(LiveGame.Explain(ex));
        }
        finally
        {
            ReloadIsland.IsEnabled = true;
            B_Save.IsEnabled = true;
            UpdateStatus();
        }
    }

    protected override async void OnClosing(WindowClosingEventArgs e)
    {
        base.OnClosing(e);
        if (!Dirty || CloseConfirmed)
            return;

        e.Cancel = true;
        var answer = await Dialogs.Prompt(MessageBoxButtons.YesNoCancel, HomeStrings.AskSaveBeforeClose);
        if (answer == DialogResult.Cancel)
            return;
        if (answer == DialogResult.Yes && !await SaveToFiles())
            return;
        CloseConfirmed = true;
        Close();
    }

    private void BuildMenu()
    {
        var cmd = UiUtil.CommandModifier;
        var file = AppMenus.Sub(AppMenus.FileHeader,
        [
            .. AppMenus.OpenItems(),
            new NativeMenuItemSeparator(),
            AppMenus.Item("Menu_Save", "Save", () => _ = SaveToFiles(), new KeyGesture(Key.S, cmd)),
            new NativeMenuItemSeparator(),
            AppMenus.Item(MacStrings.MenuCloseWindow, Close, new KeyGesture(Key.W, cmd)),
        ]);
        AppMenus.SetWindowMenu(this, file, AppMenus.Options(this));
    }
}
