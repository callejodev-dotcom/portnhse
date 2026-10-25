using Avalonia;
using Avalonia.Animation;
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

    /// <summary> When the state from the console that was put into the save was kept (until it is saved). </summary>
    private DateTime? FromConsole;

    /// <summary>
    /// The save was opened with a newer state of the game brought over Wi-Fi: it is not in the files until «Save».
    /// </summary>
    public void OpenedWithCopy(DateTime date)
    {
        FromConsole = date;
        Dirty = true;
        UpdateStatus();
    }

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
            L_Status.Text = Dirty ? HomeStrings.LiveStatusDirty
                : LastSent is { } sent ? string.Format(SentNothing ? HomeStrings.LiveStatusSame : HomeStrings.LiveStatusSent, sent.ToString("HH:mm"))
                : HomeStrings.LiveStatus;
            if (Dirty)
                L_Status.Foreground = Brushes.DarkOrange;
            else if (LastSent is not null)
                L_Status.Foreground = new SolidColorBrush(Color.FromRgb(0x2F, 0x9E, 0x62));
            else
                L_Status.ClearValue(TextBlock.ForegroundProperty);
            L_Backup.Text = HomeStrings.LiveStatusCopies;
            return;
        }
        B_Save.Content = HomeStrings.SaveButton;
        L_Status.Text = Dirty
            ? FromConsole is { } copy ? string.Format(HomeStrings.StatusFromConsole, copy.ToString("d MMM, HH:mm", System.Globalization.CultureInfo.CurrentCulture)) : HomeStrings.StatusDirty
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
        FromConsole = null;
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
        L_Status.Text = HomeStrings.LiveGamePushing;
        LiveGame.SendResult sent;
        try
        {
            sent = await Task.Run(() => Live!.Push(Ctx.SAV));
        }
        catch (Exception ex)
        {
            UpdateStatus();
            await Dialogs.Error(LiveGame.Explain(ex));
            return false;
        }
        finally
        {
            B_Save.IsEnabled = true;
        }
        if (sent.Moved)
        {
            UpdateStatus();
            await Dialogs.Error(HomeStrings.LiveGameMoved);
            return false;
        }
        if (sent.Wrong != 0)
        {
            // Still not sent: the next «Send» writes those places again.
            UpdateStatus();
            await Dialogs.Error(string.Format(HomeStrings.LiveVerifyFailed, sent.Wrong));
            return false;
        }
        Dirty = false;
        LastSent = DateTime.Now;
        SentNothing = sent.Bytes == 0;
        UpdateStatus();
        Sound.Asterisk();
        Toast(SentNothing ? HomeStrings.LiveToastSame : HomeStrings.LiveToastOk, SentNothing ? null : string.Format(HomeStrings.LiveToastDetail, sent.Places.ToString("N0")));
        return true;
    }

    /// <summary> When the last send was checked in the console, and whether there was nothing to send. </summary>
    private DateTime? LastSent;
    private bool SentNothing;

    private Border? ToastBox;
    private Avalonia.Threading.DispatcherTimer? ToastTimer;

    /// <summary> A message over the window for a few seconds (the send was checked…). </summary>
    private void Toast(string text, string? detail)
    {
        if (ToastBox is null)
        {
            ToastBox = new Border
            {
                CornerRadius = new CornerRadius(12),
                Padding = new Thickness(18, 12),
                Background = new SolidColorBrush(Color.FromRgb(0x2F, 0x9E, 0x62)),
                BoxShadow = new BoxShadows(new BoxShadow { Blur = 18, OffsetY = 4, Color = Color.FromArgb(0x50, 0, 0, 0) }),
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Bottom,
                Margin = new Thickness(0, 0, 0, 28),
                IsHitTestVisible = false,
                Transitions = [new DoubleTransition { Property = OpacityProperty, Duration = TimeSpan.FromMilliseconds(300) }],
            };
            if (Content is Grid root)
            {
                Grid.SetColumnSpan(ToastBox, Math.Max(1, root.ColumnDefinitions.Count));
                root.Children.Add(ToastBox);
            }
            ToastTimer = new Avalonia.Threading.DispatcherTimer { Interval = TimeSpan.FromSeconds(4) };
            ToastTimer.Tick += (_, _) =>
            {
                ToastTimer.Stop();
                ToastBox.Opacity = 0;
            };
        }
        var stack = new StackPanel { Spacing = 2 };
        stack.Children.Add(new TextBlock { Text = text, Foreground = Brushes.White, FontWeight = FontWeight.SemiBold, FontSize = 15 });
        if (detail is not null)
            stack.Children.Add(new TextBlock { Text = detail, Foreground = Brushes.White, FontSize = 12, Opacity = 0.9 });
        ToastBox.Child = stack;
        ToastBox.Opacity = 1;
        ToastTimer!.Stop();
        ToastTimer.Start();
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
