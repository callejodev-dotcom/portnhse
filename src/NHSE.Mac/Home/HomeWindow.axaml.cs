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

    public HomeWindow(HorizonSave sav)
    {
        InitializeComponent();
        Ctx = new HomeContext(sav, this);
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
        Title = Ctx.SAV.GetSaveTitle("NHSE");
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
            _ => ActionPages.Advanced(Ctx, OpenClassicEditor),
        };
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
        var menu = new NativeMenu();
        menu.Items.Add(file);
        menu.Items.Add(AppMenus.Options(this));
        NativeMenu.SetMenu(this, menu);
    }
}
