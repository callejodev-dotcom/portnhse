using Avalonia;
using Avalonia.Headless;
using NHSE.Core;
using NHSE.Mac;
using NHSE.Mac.Tests;

[assembly: AvaloniaTestApplication(typeof(TestApp))]

namespace NHSE.Mac.Tests;

public static class TestApp
{
    /// <summary>
    /// Headless app with Skia rendering, so the gallery tests can capture real frames.
    /// </summary>
    /// <remarks>
    /// Settings, backups and images go to a temporary folder, never to ~/Library/Application Support/NHSE.
    /// NHSE_TEMA=Dark|Light and NHSE_IDIOMA=es|de|… render the gallery in another theme or language.
    /// </remarks>
    public static AppBuilder BuildAvaloniaApp()
    {
        TestSave.DeleteCopies();
        AppDomain.CurrentDomain.ProcessExit += (_, _) => TestSave.DeleteCopies();

        var appData = Path.Combine(Path.GetTempPath(), "nhse-mac-tests", "appdata");
        Directory.CreateDirectory(appData);
        Environment.SetEnvironmentVariable("NHSE_APPDATA", appData);
        Environment.SetEnvironmentVariable("NHSE_LOGS", Path.Combine(Path.GetTempPath(), "nhse-mac-tests", "logs"));
        Environment.SetEnvironmentVariable("NHSE_OFFLINE", "1"); // no downloads (e.g. the house pictures) while testing

        if (Enum.TryParse<ThemeSetting>(Environment.GetEnvironmentVariable("NHSE_TEMA"), true, out var theme))
            Program.Settings.Theme = theme;
        if (Environment.GetEnvironmentVariable("NHSE_IDIOMA") is { Length: not 0 } lang)
            Program.Settings.Language = lang;

        return AppBuilder.Configure<App>()
            .UseSkia()
            .UseHeadless(new AvaloniaHeadlessPlatformOptions { UseHeadlessDrawing = false });
    }
}
