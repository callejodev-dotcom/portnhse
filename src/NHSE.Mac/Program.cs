using Avalonia;

namespace NHSE.Mac;

internal static class Program
{
    /// <summary>
    /// Folder for settings, backups and downloaded item images: ~/Library/Application Support/NHSE,
    /// or the folder in NHSE_APPDATA (the tests use a temporary one).
    /// </summary>
    public static string AppDataPath { get; } = Environment.GetEnvironmentVariable("NHSE_APPDATA") is { Length: not 0 } custom
        ? custom
        : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "NHSE");

    /// <summary>
    /// Settings file name.
    /// </summary>
    private const string ConfigFileName = "settings.json";

    /// <summary>
    /// Full path to the settings file.
    /// </summary>
    private static string PathConfig => Path.Combine(AppDataPath, ConfigFileName);

    /// <summary>
    /// Application settings, loaded at startup.
    /// </summary>
    public static ApplicationSettings Settings { get; private set; } = new();

    /// <summary>
    /// Save folders or files passed on the command line.
    /// </summary>
    public static string[] StartupPaths { get; private set; } = [];

    [STAThread]
    public static void Main(string[] args)
    {
        Directory.CreateDirectory(AppDataPath);
        Settings = ApplicationSettings.Load(PathConfig);
        StartupPaths = args.Where(z => !z.StartsWith("-psn_", StringComparison.Ordinal) && (Directory.Exists(z) || File.Exists(z))).ToArray();
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    // Avalonia configuration, don't remove; also used by visual designer.
    public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
            .UsePlatformDetect()
#if DEBUG
            .WithDeveloperTools()
#endif
            .LogToTrace();

    public static void SaveSettings() => Settings.Save(PathConfig);
}
