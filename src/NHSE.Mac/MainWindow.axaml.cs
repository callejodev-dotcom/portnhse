using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Simple launcher for opening a save file (the "Main" form of NHSE.WinForms).
/// </summary>
public partial class MainWindow : Window
{
    public static MainWindow? Instance { get; private set; }

    private readonly List<string> Pending = [];

    public MainWindow()
    {
        Instance = this;
        InitializeComponent();
        LoadText();
        App.LanguageChanged += _ => LoadText();
        AppMenus.ThemeChanged += BuildMenu;

        DragDrop.SetAllowDrop(this, true);
        AddHandler(DragDrop.DragOverEvent, Main_DragOver);
        AddHandler(DragDrop.DragLeaveEvent, (_, _) => DropZone.Classes.Set("over", false));
        AddHandler(DragDrop.DropEvent, Main_DragDrop);

        Opened += async (_, _) =>
        {
            foreach (var path in Pending.ToArray())
                await TryOpenSaveFile(path);
            Pending.Clear();
        };
    }

    private void LoadText()
    {
        L_Drop.Text = MacStrings.LauncherDrop;
        L_DropHint.Text = MacStrings.LauncherHint;
        B_OpenFolder.Content = MacStrings.LauncherOpenFolder;
        B_OpenFile.Content = MacStrings.LauncherOpenFile;
        B_Reopen.Content = MacStrings.LauncherReopen;
        B_Backups.Content = MacStrings.LauncherBackups;
        L_Warning.Text = MacStrings.LauncherWarning;
        B_Reopen.IsEnabled = Directory.Exists(Program.Settings.LastFilePath);
        B_Backups.IsVisible = Directory.Exists(SaveFileLoader.BackupPath);
        B_Images.Content = MacStrings.LauncherImages;
        B_Images.IsVisible = !ItemSprite.SingleSpriteExists;
        BuildMenu();
    }

    /// <summary>
    /// Menu bar while the launcher is the active window (the editors have their own).
    /// </summary>
    private void BuildMenu()
    {
        var reopen = AppMenus.Item(MacStrings.LauncherReopen, () => B_Reopen_Click(null, new RoutedEventArgs()));
        reopen.IsEnabled = B_Reopen.IsEnabled;
        var backups = AppMenus.Item(MacStrings.LauncherBackups, () => UiUtil.OpenInFinder(SaveFileLoader.BackupPath));
        backups.IsEnabled = B_Backups.IsVisible;

        var menu = new NativeMenu();
        menu.Items.Add(AppMenus.Sub(AppMenus.FileHeader, [.. AppMenus.OpenItems(), reopen, new NativeMenuItemSeparator(), backups]));
        menu.Items.Add(AppMenus.Options(this));
        NativeMenu.SetMenu(this, menu);
    }

    /// <summary>
    /// Opens a save as soon as the window is shown (command line, Dock icon, Finder "Open With").
    /// </summary>
    public void OpenWhenShown(string path)
    {
        if (IsLoaded)
            _ = TryOpenSaveFile(path);
        else
            Pending.Add(path);
    }

    private void Main_DragOver(object? sender, DragEventArgs e)
    {
        var any = e.DataTransfer.TryGetFiles()?.Length > 0;
        e.DragEffects = any ? DragDropEffects.Copy : DragDropEffects.None;
        DropZone.Classes.Set("over", any);
    }

    private async void Main_DragDrop(object? sender, DragEventArgs e)
    {
        DropZone.Classes.Set("over", false);
        var files = e.DataTransfer.TryGetFiles();
        if (files is not { Length: not 0 })
            return;
        if (files[0].TryGetLocalPath() is not { } path)
            return;
        e.DragEffects = DragDropEffects.Copy;
        await TryOpenSaveFile(path);
    }

    private async void B_OpenFolder_Click(object? sender, RoutedEventArgs e) => await MenuOpenFolder();
    private async void B_OpenFile_Click(object? sender, RoutedEventArgs e) => await MenuOpenFile();

    private async void B_Reopen_Click(object? sender, RoutedEventArgs e)
    {
        var path = Program.Settings.LastFilePath;
        if (Directory.Exists(path))
            await TryOpenSaveFile(path);
    }

    private void B_Backups_Click(object? sender, RoutedEventArgs e) => UiUtil.OpenInFinder(SaveFileLoader.BackupPath);

    /// <summary>
    /// Without the item image pack the grids only show category icons, so the launcher offers it until it is installed.
    /// </summary>
    private void B_Images_Click(object? sender, RoutedEventArgs e)
    {
        var fetcher = UiUtil.FirstFormOfType<ImageFetcher>() ?? new ImageFetcher();
        fetcher.Closed += (_, _) => LoadText();
        fetcher.Show(this);
        fetcher.Activate();
    }

    public async Task MenuOpenFolder()
    {
        var path = await Pickers.OpenFolder(this, MacStrings.LauncherOpenFolder);
        if (path is not null)
            await TryOpenSaveFile(path);
    }

    public async Task MenuOpenFile()
    {
        const string filter = "New Horizons Save File (main.dat)|main.dat;*.dat|Zip (*.zip)|*.zip";
        var path = await Pickers.OpenFile(this, filter, "main.dat", "Open main.dat ...");
        if (path is not null)
            await TryOpenSaveFile(path);
    }

    public async Task<bool> TryOpenSaveFile(string path)
    {
        var sav = await SaveFileLoader.TryGetSaveFile(path);
        if (sav is null)
            return false;
        Program.SaveSettings();

        var editor = new HomeWindow(sav);
        editor.Closed += (_, _) =>
        {
            if (!UiUtil.GetWindows().OfType<HomeWindow>().Any(z => z != editor))
            {
                LoadText();
                Show();
                Activate();
            }
        };
        editor.Show();
        Hide();
        return true;
    }

    protected override void OnClosing(WindowClosingEventArgs e)
    {
        Program.SaveSettings();
        base.OnClosing(e);
    }
}
