using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Text;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// NHSE updating itself: which release it takes, and the whole way with a local server — download, check, and the
/// script that puts the new app in place of the old one once NHSE has closed.
/// </summary>
public sealed class UpdaterTests : IDisposable
{
    private readonly string LatestUrl = Updater.LatestUrl;
    private readonly string? Bundle = Updater.Bundle;
    private readonly string Folder = Updater.Folder;
    private readonly Version Current = Updater.Current;
    private readonly string Temp = Path.Combine(Path.GetTempPath(), "nhse-mac-tests", "updater-" + Guid.NewGuid().ToString("N"));

    public void Dispose()
    {
        Updater.LatestUrl = LatestUrl;
        Updater.Bundle = Bundle;
        Updater.Folder = Folder;
        Updater.Current = Current;
        Updater.Reset();
        try
        {
            Directory.Delete(Temp, true);
        }
        catch (IOException)
        {
        }
    }

    private static string Release(string tag, string asset, bool draft = false) => $$"""
        { "tag_name": "{{tag}}", "draft": {{(draft ? "true" : "false")}},
          "assets": [ { "name": "{{asset}}", "browser_download_url": "https://example.com/{{asset}}" },
                      { "name": "NHSE-9.9.9-macOS-arm64.dmg", "browser_download_url": "https://example.com/dmg" } ] }
        """;

    [Fact]
    public void PicksOnlyANewerReleaseForThisMac()
    {
        var current = new Version(0, 1, 0);
        var newer = Updater.Pick(Release("v0.2.0", "NHSE-0.2.0-macOS-arm64.zip"), current, "arm64");
        Assert.Equal((new Version(0, 2, 0), "https://example.com/NHSE-0.2.0-macOS-arm64.zip"), newer);
        Assert.Null(Updater.Pick(Release("v0.1.0", "NHSE-0.1.0-macOS-arm64.zip"), current, "arm64"));
        Assert.Null(Updater.Pick(Release("v0.0.9", "NHSE-0.0.9-macOS-arm64.zip"), current, "arm64"));
        Assert.Null(Updater.Pick(Release("v0.2.0", "NHSE-0.2.0-macOS-x64.zip"), current, "arm64"));
        Assert.Null(Updater.Pick(Release("v0.2.0", "NHSE-0.2.0-macOS-arm64.zip", draft: true), current, "arm64"));
        Assert.Null(Updater.Pick(Release("nightly", "NHSE-0.2.0-macOS-arm64.zip"), current, "arm64"));
    }

    [Fact]
    public void FindsTheAppItRunsFrom()
    {
        Assert.Equal("/Applications/NHSE.app", Updater.FindBundle("/Applications/NHSE.app/Contents/MacOS/"));
        Assert.Null(Updater.FindBundle("/Users/someone/portnhse/src/NHSE.Mac/bin/Release/net10.0/"));
        Assert.Null(Updater.FindBundle("/Applications/NHSE/Contents/MacOS/"));
    }

    private static void Run(string tool, params string[] args)
    {
        var info = new ProcessStartInfo(tool) { RedirectStandardError = true, RedirectStandardOutput = true };
        foreach (var a in args)
            info.ArgumentList.Add(a);
        using var p = Process.Start(info)!;
        var error = p.StandardError.ReadToEnd();
        p.WaitForExit();
        Assert.True(p.ExitCode == 0, $"{tool}: {error}");
    }

    /// <summary> A small NHSE.app like the real one: a program, a file signed in its attributes (as the .dll), signed ad hoc. </summary>
    private static string MakeApp(string dir, string version)
    {
        var app = Path.Combine(dir, "NHSE.app");
        var macos = Path.Combine(app, "Contents", "MacOS");
        Directory.CreateDirectory(macos);
        File.WriteAllText(Path.Combine(app, "Contents", "Info.plist"), $"""
            <?xml version="1.0" encoding="UTF-8"?>
            <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
            <plist version="1.0"><dict>
                <key>CFBundleIdentifier</key><string>com.callejodev.nhse.test</string>
                <key>CFBundleExecutable</key><string>NHSE</string>
                <key>CFBundlePackageType</key><string>APPL</string>
                <key>CFBundleShortVersionString</key><string>{version}</string>
                <key>CFBundleVersion</key><string>{version}</string>
            </dict></plist>
            """);
        File.Copy("/usr/bin/true", Path.Combine(macos, "NHSE"));
        File.WriteAllText(Path.Combine(macos, "NHSE.dll"), "not really a library " + version);
        Run("/usr/bin/codesign", "--force", "--deep", "--sign", "-", app);
        return app;
    }

    private static int FreePort()
    {
        var l = new TcpListener(IPAddress.Loopback, 0);
        l.Start();
        var port = ((IPEndPoint)l.LocalEndpoint).Port;
        l.Stop();
        return port;
    }

    /// <summary> Serves a release of 9.9.9 for this Mac, with <paramref name="zip"/> as its download. </summary>
    private static HttpListener Serve(byte[] zip, out string prefix)
    {
        prefix = $"http://127.0.0.1:{FreePort()}/";
        var listener = new HttpListener();
        listener.Prefixes.Add(prefix);
        listener.Start();
        var json = Encoding.UTF8.GetBytes($$"""
            { "tag_name": "v9.9.9", "draft": false,
              "assets": [ { "name": "NHSE-9.9.9-macOS-{{Updater.Architecture}}.zip", "browser_download_url": "{{prefix}}app.zip" } ] }
            """);
        _ = Task.Run(async () =>
        {
            while (listener.IsListening)
            {
                HttpListenerContext c;
                try
                {
                    c = await listener.GetContextAsync();
                }
                catch (Exception ex) when (ex is HttpListenerException or ObjectDisposedException or InvalidOperationException)
                {
                    return;
                }
                var body = c.Request.Url!.AbsolutePath == "/latest" ? json : zip;
                c.Response.ContentLength64 = body.Length;
                await c.Response.OutputStream.WriteAsync(body);
                c.Response.Close();
            }
        });
        return listener;
    }

    /// <summary> Points the updater at a fake installed NHSE.app 0.1.0 and the local server. </summary>
    private string Install(string prefix)
    {
        var installed = MakeApp(Path.Combine(Temp, "Applications"), "0.1.0");
        Updater.Reset();
        Updater.Current = new Version(0, 1, 0);
        Updater.Bundle = installed;
        Updater.Folder = Path.Combine(Temp, "actualizacion");
        Updater.LatestUrl = prefix + "latest";
        return installed;
    }

    [Fact]
    public async Task DownloadsChecksAndSwapsWhenNHSEHasClosed()
    {
        var made = MakeApp(Path.Combine(Temp, "release"), "9.9.9");
        var zip = Path.Combine(Temp, "NHSE.zip");
        Run("/usr/bin/ditto", "-c", "-k", "--keepParent", made, zip);
        using var server = Serve(File.ReadAllBytes(zip), out var prefix);
        var installed = Install(prefix);

        // Not while NHSE is open: the script waits for it (here a process that lasts a second).
        using var running = Process.Start("/bin/sleep", "1");
        var found = new List<Updater.Stage>();
        Updater.Changed += Seen;
        try
        {
            await Updater.Check(force: true);
        }
        finally
        {
            Updater.Changed -= Seen;
        }
        void Seen() => found.Add(Updater.State);
        Assert.True(Updater.State == Updater.Stage.Ready, Updater.Problem);
        Assert.Equal([Updater.Stage.Checking, Updater.Stage.Downloading, Updater.Stage.Ready], found);
        Assert.Equal(new Version(9, 9, 9), Updater.Found);

        // Check() already started a script for this process (the tests): this one waits for the short one.
        Updater.InstallWhenClosed(running.Id);
        Assert.Equal("0.1.0", Updater.ReadVersion(installed));
        var until = DateTime.UtcNow.AddSeconds(30);
        while (Updater.ReadVersion(installed) != "9.9.9" && DateTime.UtcNow < until)
            await Task.Delay(100, TestContext.Current.CancellationToken);
        Assert.Equal("9.9.9", Updater.ReadVersion(installed));
        Run("/usr/bin/codesign", "--verify", "--deep", "--strict", installed);
        Assert.False(Directory.Exists(Path.Combine(Path.GetDirectoryName(installed)!, ".NHSE-vieja.app")));
        Assert.False(Directory.Exists(Path.Combine(Path.GetDirectoryName(installed)!, ".NHSE-nueva.app")));
    }

    [Fact]
    public async Task ABrokenDownloadLeavesTheOldOne()
    {
        using var server = Serve(Encoding.ASCII.GetBytes("this is not a zip"), out var prefix);
        var installed = Install(prefix);
        await Updater.Check(force: true);
        Assert.Equal(Updater.Stage.Failed, Updater.State);
        Assert.Null(Updater.Staged);
        Assert.Equal("0.1.0", Updater.ReadVersion(installed));
    }

    [Fact]
    public async Task NothingNewerSaysSo()
    {
        var made = MakeApp(Path.Combine(Temp, "release"), "9.9.9");
        var zip = Path.Combine(Temp, "NHSE.zip");
        Run("/usr/bin/ditto", "-c", "-k", "--keepParent", made, zip);
        using var server = Serve(File.ReadAllBytes(zip), out var prefix);
        Install(prefix);
        Updater.Current = new Version(9, 9, 9);
        await Updater.Check(force: true);
        Assert.Equal(Updater.Stage.UpToDate, Updater.State);
        Assert.Null(Updater.Staged);
    }
}
