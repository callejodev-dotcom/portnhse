using System.Text.Json;
using System.Text.Json.Serialization;
using Avalonia.Styling;

namespace NHSE.Mac;

/// <summary>
/// Application settings stored as JSON (port of NHSE.WinForms' ApplicationSettings).
/// </summary>
public sealed class ApplicationSettings
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        Converters = { new JsonStringEnumConverter() },
    };

    /// <summary>
    /// Last opened save file path.
    /// </summary>
    public string LastFilePath { get; set; } = string.Empty;

    /// <summary>
    /// SysBot connection and RAM editing settings.
    /// </summary>
    public SysBotSettings SysBot { get; set; } = new();

    /// <summary>
    /// Whether to automatically backup save files.
    /// </summary>
    public bool AutomaticBackup { get; set; } = true;

    /// <summary>
    /// Whether the backup prompt has been shown.
    /// </summary>
    public bool BackupPrompted { get; set; }

    /// <summary>
    /// UI language code (e.g., "en", "de", "jp").
    /// </summary>
    public string Language { get; set; } = "en";

    /// <summary>
    /// Light/dark appearance.
    /// </summary>
    public ThemeSetting Theme { get; set; } = ThemeSetting.System;

    /// <summary>
    /// Loads settings from disk, or creates default settings if the file doesn't exist.
    /// </summary>
    /// <param name="path">Full path to the settings file.</param>
    public static ApplicationSettings Load(string path)
    {
        if (!File.Exists(path))
            return new ApplicationSettings { Language = GetDefaultLanguage() };

        try
        {
            var json = File.ReadAllText(path);
            return JsonSerializer.Deserialize<ApplicationSettings>(json, JsonOptions) ?? new ApplicationSettings();
        }
        catch
        {
            // If deserialization fails, return defaults
            return new ApplicationSettings();
        }
    }

    /// <summary>
    /// Saves the current settings to disk.
    /// </summary>
    /// <param name="path">Full path to the settings file.</param>
    public void Save(string path)
    {
        try
        {
            var json = JsonSerializer.Serialize(this, JsonOptions);
            File.WriteAllText(path, json);
        }
        catch
        {
            // Silently fail if we can't save settings
        }
    }

    /// <summary>
    /// First run: use the system language if NHSE has it.
    /// </summary>
    private static string GetDefaultLanguage()
    {
        var name = GetSystemLanguage() ?? System.Globalization.CultureInfo.CurrentUICulture.Name;
        var code = name.Split('-', '_')[0].ToLowerInvariant();
        return code switch
        {
            "de" or "en" or "es" or "fr" or "it" or "ko" => code,
            "gl" or "ca" or "eu" => "es",
            "ja" => "jp",
            "zh" => name.Contains("Hant") || name.EndsWith("TW") || name.EndsWith("HK") ? "zht" : "zhs",
            _ => "en",
        };
    }

    /// <summary>
    /// First language in System Settings → Language &amp; Region (apps opened from Finder have no LANG for .NET to read).
    /// </summary>
    private static string? GetSystemLanguage()
    {
        if (!OperatingSystem.IsMacOS())
            return null;
        try
        {
            var psi = new System.Diagnostics.ProcessStartInfo("/usr/bin/defaults", ["read", "-g", "AppleLanguages"])
            {
                RedirectStandardOutput = true,
                UseShellExecute = false,
            };
            using var p = System.Diagnostics.Process.Start(psi);
            if (p is null)
                return null;
            var output = p.StandardOutput.ReadToEnd();
            p.WaitForExit(2000);
            var start = output.IndexOf('"');
            var end = start < 0 ? -1 : output.IndexOf('"', start + 1);
            return end > start ? output[(start + 1)..end] : null;
        }
        catch
        {
            return null;
        }
    }
}

public enum ThemeSetting
{
    System,
    Light,
    Dark,
}

public static class ThemeSettingExtensions
{
    public static ThemeVariant ToVariant(this ThemeSetting t) => t switch
    {
        ThemeSetting.Light => ThemeVariant.Light,
        ThemeSetting.Dark => ThemeVariant.Dark,
        _ => ThemeVariant.Default,
    };
}

/// <summary>
/// SysBot connection and RAM editing settings.
/// </summary>
public sealed class SysBotSettings
{
    /// <summary>
    /// SysBot IP address.
    /// </summary>
    public string IP { get; set; } = "192.168.0.1";

    /// <summary>
    /// SysBot port number.
    /// </summary>
    public int Port { get; set; } = 6000;

    /// <summary>
    /// Whether the SysBot prompt has been shown.
    /// </summary>
    public bool Prompted { get; set; }

    /// <summary>
    /// RAM offset for pouch editing.
    /// </summary>
    public uint PouchOffset { get; set; }

    /// <summary>
    /// RAM offset for generic editing.
    /// </summary>
    public uint GenericOffset { get; set; }

    public override string ToString() => $"{IP}:{Port}";
}
