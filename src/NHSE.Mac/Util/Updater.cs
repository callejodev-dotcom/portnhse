using System.Diagnostics;
using System.Net.Http.Headers;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace NHSE.Mac;

/// <summary>
/// Keeps NHSE up to date by itself: looks for a newer version among the releases of the port on GitHub, downloads it
/// in the background, and a small script puts it in place of this one once NHSE closes (or at once, restarting it).
/// Only when running from NHSE.app, not while developing or testing.
/// </summary>
public static partial class Updater
{
    public enum Stage { Idle, Checking, Downloading, Ready, UpToDate, Failed }

    /// <summary> Where the newest release is described (GitHub's API). </summary>
    internal static string LatestUrl { get; set; } = "https://api.github.com/repos/callejodev-dotcom/portnhse/releases/latest";

    public const string ReleasesPage = "https://github.com/callejodev-dotcom/portnhse/releases";

    /// <summary> The NHSE.app this is running from, or null (running from a build folder). </summary>
    internal static string? Bundle { get; set; } = FindBundle(AppContext.BaseDirectory);

    /// <summary> Where the new version is downloaded and kept until it is put in place. </summary>
    internal static string Folder { get; set; } = Path.Combine(Program.AppDataPath, "actualizacion");

    public static Version Current { get; internal set; } = Normalize(typeof(Updater).Assembly.GetName().Version);

    public static Stage State { get; private set; }
    public static Version? Found { get; private set; }
    public static string? Problem { get; private set; }

    /// <summary> Raised (on any thread) when <see cref="State"/> changes. </summary>
    public static event Action? Changed;

    /// <summary> The downloaded NHSE.app, ready to go in place of this one. </summary>
    internal static string? Staged { get; private set; }

    /// <summary> Whether this copy can update itself (it is an installed NHSE.app and the tests are not running). </summary>
    public static bool CanUpdate => Bundle is not null && Environment.GetEnvironmentVariable("NHSE_OFFLINE") is null;

    private static int Busy;

    /// <summary> Forgets what was found (tests). </summary>
    internal static void Reset()
    {
        State = Stage.Idle;
        Found = null;
        Staged = null;
        Problem = null;
    }

    private static void Set(Stage stage, string? problem = null)
    {
        State = stage;
        Problem = problem;
        Changed?.Invoke();
    }

    /// <summary>
    /// Looks for a newer version and downloads it. Without <paramref name="force"/>, only when automatic updates are
    /// on; with it (the menu), also says when this one is already the newest.
    /// </summary>
    public static async Task Check(bool force = false)
    {
        if (Bundle is null || (!force && (!CanUpdate || !Program.Settings.AutoUpdate)) || State == Stage.Ready)
            return;
        if (Interlocked.Exchange(ref Busy, 1) == 1)
            return;
        try
        {
            Set(Stage.Checking);
            using var http = new HttpClient { Timeout = TimeSpan.FromMinutes(10) };
            http.DefaultRequestHeaders.UserAgent.Add(new ProductInfoHeaderValue("NHSE-Mac", Current.ToString(3)));
            using var request = new HttpRequestMessage(HttpMethod.Get, LatestUrl);
            request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("application/vnd.github+json"));
            using var response = await http.SendAsync(request);
            if (!response.IsSuccessStatusCode)
            {
                // No release published yet.
                Set(response.StatusCode == System.Net.HttpStatusCode.NotFound ? Stage.UpToDate : Stage.Failed, response.ReasonPhrase);
                return;
            }
            var release = Pick(await response.Content.ReadAsStringAsync(), Current, Architecture);
            if (release is not { } r)
            {
                Set(Stage.UpToDate);
                return;
            }
            Found = r.Version;
            Set(Stage.Downloading);
            Staged = await Download(http, r.Version, r.Url);
            Set(Stage.Ready);
            InstallWhenClosed();
        }
        catch (Exception ex) when (ex is HttpRequestException or IOException or TaskCanceledException or JsonException or InvalidOperationException or UnauthorizedAccessException)
        {
            Set(Stage.Failed, ex.Message);
        }
        finally
        {
            Interlocked.Exchange(ref Busy, 0);
        }
    }

    /// <summary> "arm64" or "x64", as the downloads are named. </summary>
    internal static string Architecture => RuntimeInformation.ProcessArchitecture == System.Runtime.InteropServices.Architecture.Arm64 ? "arm64" : "x64";

    /// <summary>
    /// The version and download of a release (GitHub's JSON) when it is newer than <paramref name="current"/> and has
    /// NHSE.app zipped for this kind of Mac.
    /// </summary>
    internal static (Version Version, string Url)? Pick(string json, Version current, string arch)
    {
        using var doc = JsonDocument.Parse(json);
        var root = doc.RootElement;
        var tag = root.GetProperty("tag_name").GetString() ?? string.Empty;
        if (root.TryGetProperty("draft", out var draft) && draft.GetBoolean())
            return null;
        if (!Version.TryParse(tag.TrimStart('v', 'V'), out var parsed))
            return null;
        var version = Normalize(parsed);
        if (version <= current)
            return null;
        var name = $"NHSE-{version.ToString(3)}-macOS-{arch}.zip";
        foreach (var asset in root.GetProperty("assets").EnumerateArray())
        {
            if (asset.GetProperty("name").GetString() == name)
                return (version, asset.GetProperty("browser_download_url").GetString()!);
        }
        return null;
    }

    private static Version Normalize(Version? v) => v is null ? new Version(0, 0, 0) : new Version(v.Major, v.Minor, Math.Max(0, v.Build));

    /// <summary> Downloads and unzips the new NHSE.app, and checks it is whole, signed and the version it says. </summary>
    private static async Task<string> Download(HttpClient http, Version version, string url)
    {
        if (Directory.Exists(Folder))
            Directory.Delete(Folder, true);
        var dir = Path.Combine(Folder, version.ToString(3));
        Directory.CreateDirectory(dir);
        var zip = Path.Combine(Folder, "NHSE.zip");
        await using (var source = await http.GetStreamAsync(url))
        await using (var file = File.Create(zip))
            await source.CopyToAsync(file);
        await Run("/usr/bin/ditto", "-x", "-k", zip, dir);
        File.Delete(zip);
        var app = Path.Combine(dir, "NHSE.app");
        if (!Directory.Exists(app))
            throw new InvalidOperationException("The download has no NHSE.app.");
        await Run("/usr/bin/codesign", "--verify", "--deep", "--strict", app);
        if (ReadVersion(app) != version.ToString(3))
            throw new InvalidOperationException("The download is not the version it says.");
        return app;
    }

    /// <summary> CFBundleShortVersionString of an app. </summary>
    internal static string? ReadVersion(string app)
    {
        var plist = Path.Combine(app, "Contents", "Info.plist");
        if (!File.Exists(plist))
            return null;
        var m = ShortVersion().Match(File.ReadAllText(plist));
        return m.Success ? m.Groups[1].Value : null;
    }

    [GeneratedRegex(@"<key>CFBundleShortVersionString</key>\s*<string>([^<]*)</string>")]
    private static partial Regex ShortVersion();

    private static async Task Run(string tool, params string[] args)
    {
        var info = new ProcessStartInfo(tool) { RedirectStandardError = true, RedirectStandardOutput = true };
        foreach (var a in args)
            info.ArgumentList.Add(a);
        using var p = Process.Start(info) ?? throw new InvalidOperationException($"{tool} did not start.");
        var error = p.StandardError.ReadToEndAsync();
        await p.StandardOutput.ReadToEndAsync();
        await p.WaitForExitAsync();
        if (p.ExitCode != 0)
            throw new InvalidOperationException($"{Path.GetFileName(tool)}: {(await error).Trim()}");
    }

    /// <summary> The NHSE.app holding a folder (…/NHSE.app/Contents/MacOS/), or null. </summary>
    internal static string? FindBundle(string baseDirectory)
    {
        var dir = new DirectoryInfo(baseDirectory.TrimEnd('/'));
        if (dir.Name != "MacOS" || dir.Parent is not { Name: "Contents" } contents || contents.Parent is not { } app || !app.Name.EndsWith(".app", StringComparison.Ordinal))
            return null;
        return app.FullName;
    }

    /// <summary> Asks for NHSE to be opened again after putting the new version in place. </summary>
    private static string ReopenFlag => Path.Combine(Folder, "reabrir");

    /// <summary>
    /// Starts the script that waits for NHSE to close and then puts the new version in place of this one.
    /// </summary>
    internal static void InstallWhenClosed(int? pid = null)
    {
        if (Staged is null || Bundle is null)
            return;
        var script = Path.Combine(Folder, "instalar.sh");
        File.WriteAllText(script, InstallScript);
        var info = new ProcessStartInfo("/bin/bash") { UseShellExecute = false };
        foreach (var a in new[] { script, (pid ?? Environment.ProcessId).ToString(), Staged, Bundle, ReopenFlag, Path.Combine(Folder, "instalar.log") })
            info.ArgumentList.Add(a);
        Process.Start(info)?.Dispose();
    }

    /// <summary> Closes NHSE and opens the new version once it is in place. </summary>
    public static void RestartNow()
    {
        if (State != Stage.Ready)
            return;
        File.WriteAllText(ReopenFlag, string.Empty);
        if (Avalonia.Application.Current?.ApplicationLifetime is Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime desktop)
            desktop.Shutdown();
    }

    /// <summary>
    /// Waits for NHSE (its process) to end, copies the new app beside the old one, checks it, swaps them and, if asked,
    /// opens it. If anything fails the old one stays.
    /// </summary>
    private const string InstallScript = """
        #!/bin/bash
        PID="$1"; NEW="$2"; APP="$3"; REOPEN="$4"; LOG="$5"
        exec >>"$LOG" 2>&1
        while kill -0 "$PID" 2>/dev/null; do sleep 0.5; done
        DIR="$(dirname "$APP")"
        TMP="$DIR/.NHSE-nueva.app"; OLD="$DIR/.NHSE-vieja.app"
        rm -rf "$TMP" "$OLD"
        ditto "$NEW" "$TMP" || { echo "$(date) copy failed"; exit 1; }
        codesign --verify --deep --strict "$TMP" || { echo "$(date) not signed"; rm -rf "$TMP"; exit 1; }
        if mv "$APP" "$OLD" && mv "$TMP" "$APP"; then
            rm -rf "$OLD" "$(dirname "$NEW")"
            echo "$(date) installed"
        else
            [ -d "$APP" ] || mv "$OLD" "$APP"
            rm -rf "$TMP"
            echo "$(date) swap failed"
            exit 1
        fi
        if [ -f "$REOPEN" ]; then rm -f "$REOPEN"; open "$APP"; fi
        """;
}
