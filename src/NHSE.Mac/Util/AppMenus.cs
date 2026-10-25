using Avalonia.Controls;
using Avalonia.Input;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Pieces of the macOS menu bar shared by the launcher and the editor windows.
/// </summary>
public static class AppMenus
{
    /// <summary>
    /// Raised after the theme changes, so every window rebuilds its menu with the new tick.
    /// </summary>
    public static event Action? ThemeChanged;

    /// <summary>
    /// Menu item translated with the <c>Editor.{key}</c> entry of the WinForms language files.
    /// </summary>
    public static NativeMenuItem Item(string key, string text, Action action, KeyGesture? gesture = null)
    {
        var item = new NativeMenuItem(Translator.Tr($"Editor.{key}", text)) { Gesture = gesture };
        item.Click += (_, _) => action();
        return item;
    }

    public static NativeMenuItem Item(string text, Action action, KeyGesture? gesture = null)
    {
        var item = new NativeMenuItem(text) { Gesture = gesture };
        item.Click += (_, _) => action();
        return item;
    }

    public static NativeMenuItem Sub(string header, params NativeMenuItemBase[] items)
    {
        var menu = new NativeMenu();
        foreach (var i in items)
            menu.Items.Add(i);
        return new NativeMenuItem(header) { Menu = menu };
    }

    /// <summary>
    /// Sets the menu bar items of a window. The window keeps the same <see cref="NativeMenu"/> and only its items change:
    /// giving an exported window a new NativeMenu fails in Avalonia.Native ("The menu being updated does not match").
    /// </summary>
    public static void SetWindowMenu(Window window, params NativeMenuItemBase[] items)
    {
        var menu = NativeMenu.GetMenu(window);
        if (menu is null)
        {
            menu = new NativeMenu();
            foreach (var i in items)
                menu.Items.Add(i);
            NativeMenu.SetMenu(window, menu);
            return;
        }
        menu.Items.Clear();
        foreach (var i in items)
            menu.Items.Add(i);
    }

    public static string FileHeader => Translator.Tr("Editor.Menu_File", "File");

    /// <summary>
    /// Open Save Folder… (⌘O) and Open main.dat or .zip… (⇧⌘O).
    /// </summary>
    public static NativeMenuItemBase[] OpenItems()
    {
        var cmd = UiUtil.CommandModifier;
        return
        [
            Item(MacStrings.MenuOpenFolder, () => _ = MainWindow.Instance?.MenuOpenFolder(), new KeyGesture(Key.O, cmd)),
            Item(MacStrings.MenuOpenFile, () => _ = MainWindow.Instance?.MenuOpenFile(), new KeyGesture(Key.O, cmd | KeyModifiers.Shift)),
        ];
    }

    /// <summary>
    /// Options: language, theme and settings (⌘,).
    /// </summary>
    public static NativeMenuItem Options(Window owner)
    {
        var languages = new NativeMenuItemBase[App.LanguageNames.Length];
        var current = GameLanguage.GetLanguageIndex(GameInfo.CurrentLanguage);
        for (int i = 0; i < languages.Length; i++)
        {
            var index = i;
            var item = new NativeMenuItem(App.LanguageNames[i]) { ToggleType = MenuItemToggleType.Radio, IsChecked = i == current };
            item.Click += (_, _) => SetLanguage(index);
            languages[i] = item;
        }

        var theme = Program.Settings.Theme;
        NativeMenuItem ThemeItem(string key, string text, ThemeSetting t)
        {
            var item = Item(key, text, () => SetTheme(t));
            item.ToggleType = MenuItemToggleType.Radio;
            item.IsChecked = theme == t;
            return item;
        }

        return Sub(Translator.Tr("Editor.Menu_Options", "Options"),
            Sub(MacStrings.MenuLanguage, languages),
            Sub(Translator.Tr("Editor.Menu_Theme", "Theme"),
                ThemeItem("Menu_Theme_System", "System Theme", ThemeSetting.System),
                ThemeItem("Menu_Theme_Classic", "Light (Classic)", ThemeSetting.Light),
                ThemeItem("Menu_Theme_Dark", "Dark", ThemeSetting.Dark)),
            new NativeMenuItemSeparator(),
            Item("Menu_Settings", "Settings", () => _ = new SettingsEditor().ShowDialog(owner), new KeyGesture(Key.OemComma, UiUtil.CommandModifier)));
    }

    private static void SetLanguage(int index)
    {
        App.SetLanguage(index); // every open window translates itself and rebuilds its menu
        Program.SaveSettings();
    }

    private static void SetTheme(ThemeSetting theme)
    {
        // Avalonia switches themes live, no restart needed (WinForms asked to save and restart).
        Program.Settings.Theme = theme;
        Program.SaveSettings();
        if (Avalonia.Application.Current is { } app)
            app.RequestedThemeVariant = theme.ToVariant();
        ThemeChanged?.Invoke();
    }
}
