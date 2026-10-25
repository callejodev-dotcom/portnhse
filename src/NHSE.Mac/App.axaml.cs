using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.Platform.Storage;
using NHSE.Core;

namespace NHSE.Mac;

public partial class App : Application
{
    /// <summary>
    /// Display names of <see cref="GameLanguage"/>'s language codes, in the same order.
    /// </summary>
    public static readonly string[] LanguageNames = ["English", "日本語", "Deutsch", "Español", "Français", "Italiano", "한국어", "简体中文", "繁體中文"];

    /// <summary>
    /// Raised after <see cref="SetLanguage"/> so open windows can translate themselves.
    /// </summary>
    public static event Action<string>? LanguageChanged;

    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
    }

    public override void OnFrameworkInitializationCompleted()
    {
        RequestedThemeVariant = Program.Settings.Theme.ToVariant();
        SetLanguage(GameLanguage.GetLanguageIndex(Program.Settings.Language));
        TrackModifierKeys();

        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            var main = new MainWindow();
            desktop.MainWindow = main;
            desktop.ShutdownMode = ShutdownMode.OnMainWindowClose;
            desktop.ShutdownRequested += (_, _) => Program.SaveSettings();
            TranslateAppMenu();
            LanguageChanged += _ => TranslateAppMenu();

            foreach (var path in Program.StartupPaths)
                main.OpenWhenShown(path);
        }

        // Folders/files dropped on the Dock icon or opened with "Open With".
        if (this.TryGetFeature<IActivatableLifetime>() is { } activatable)
        {
            activatable.Activated += (_, e) =>
            {
                if (e is not FileActivatedEventArgs f)
                    return;
                foreach (var item in f.Files)
                {
                    if (item.TryGetLocalPath() is { } path)
                        MainWindow.Instance?.OpenWhenShown(path);
                }
            };
        }

        base.OnFrameworkInitializationCompleted();
    }

    /// <summary>
    /// Changes the language of the game strings, the message strings and the interface.
    /// </summary>
    /// <param name="index">Index in <see cref="LanguageNames"/>.</param>
    public static void SetLanguage(int index)
    {
        if ((uint)index >= GameLanguage.LanguageCount)
            return;
        var lang = GameInfo.SetLanguage2Char(index);
        Program.Settings.Language = lang;

        ItemSprite.Initialize(GameInfo.GetStrings("en").itemlist);
        TranslationUtil.SetLocalization(typeof(MessageStrings), lang);
        TranslationUtil.SetLocalization(GameInfo.Strings.InternalNameTranslation, lang);
        TranslationUtil.SetLocalizationMac(typeof(MacStrings), lang);
        TranslationUtil.SetLocalizationMac(typeof(HomeStrings), lang);
        LanguageChanged?.Invoke(lang);
    }

    /// <summary>
    /// Keeps <see cref="UiUtil.ModifierKeys"/> current, since click events don't carry the modifier keys.
    /// </summary>
    private static void TrackModifierKeys()
    {
        const RoutingStrategies routes = RoutingStrategies.Tunnel;
        InputElement.KeyDownEvent.AddClassHandler<TopLevel>((_, e) => UiUtil.ModifierKeys = e.KeyModifiers, routes, true);
        InputElement.KeyUpEvent.AddClassHandler<TopLevel>((_, e) => UiUtil.ModifierKeys = e.KeyModifiers, routes, true);
        InputElement.PointerPressedEvent.AddClassHandler<TopLevel>((_, e) => UiUtil.ModifierKeys = e.KeyModifiers, routes, true);
        InputElement.PointerReleasedEvent.AddClassHandler<TopLevel>((_, e) => UiUtil.ModifierKeys = e.KeyModifiers, routes, true);
        InputElement.PointerMovedEvent.AddClassHandler<TopLevel>((_, e) => UiUtil.ModifierKeys = e.KeyModifiers, routes, true);
    }

    /// <summary>
    /// Items of the application menu (the one named after the app, on the left of the macOS menu bar).
    /// </summary>
    private void TranslateAppMenu()
    {
        if (NativeMenu.GetMenu(this)?.Items.FirstOrDefault() is NativeMenuItem about)
            about.Header = MacStrings.MenuAbout;
    }

    private async void About_Click(object? sender, EventArgs e)
        => await Dialogs.Alert(false, $"NHSE {GetVersion()}", MacStrings.MsgAbout, "https://github.com/kwsch/NHSE · https://github.com/callejodev-dotcom/portnhse");

    public static string GetVersion() => typeof(App).Assembly.GetName().Version?.ToString(3) ?? "0.0.0";
}
