using System.Collections.Concurrent;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Pictures and game names of the house roofs, walls and doors (the core only has their internal names, like
/// "HouseRoofPA04StandardE"). The list comes with the app (<c>Resources/exterior.tsv</c>); the pictures are downloaded the
/// first time they are needed into <c>img/exterior</c> of the app data folder, as the item pictures are.
/// </summary>
public static class ExteriorPictures
{
    private sealed record Entry(string Url, string[] Names);

    private static readonly string[] Languages = ["en", "es", "de", "fr", "it", "jp", "ko", "zhs", "zht"];

    private static readonly Dictionary<string, Entry> Entries = Load();
    private static readonly ConcurrentDictionary<string, Bitmap?> Cache = new();
    private static Task<int>? Downloading;

    public static string Folder => Path.Combine(Program.AppDataPath, "img", "exterior");

    /// <summary> Raised on the UI thread when downloaded pictures are ready to be shown. </summary>
    public static event Action? PicturesChanged;

    private static Dictionary<string, Entry> Load()
    {
        var result = new Dictionary<string, Entry>(StringComparer.Ordinal);
        var text = EmbeddedResources.GetText("mac/exterior.tsv") ?? string.Empty;
        foreach (var line in text.Split('\n'))
        {
            if (line.Length == 0 || line[0] == '#')
                continue;
            var parts = line.TrimEnd('\r').Split('\t');
            if (parts.Length < 2)
                continue;
            result[parts[0]] = new Entry(parts[1], parts[2..]);
        }
        return result;
    }

    /// <summary>
    /// The entry with the picture of a look: square doors use the picture of the arched one (only those are in the list),
    /// and the roofs of the smaller houses the one of the finished house.
    /// </summary>
    private static string? Resolve(string name)
    {
        if (Entries.ContainsKey(name))
            return name;
        if (name.StartsWith("HouseDoor", StringComparison.Ordinal) && name.EndsWith('S') && Entries.ContainsKey(name[..^1] + "R"))
            return name[..^1] + "R";
        if (name.StartsWith("HouseRoofPA0", StringComparison.Ordinal) && name.Length > 12)
        {
            var finished = "HouseRoofPA04" + name[13..];
            if (Entries.ContainsKey(finished))
                return finished;
        }
        return null;
    }

    /// <summary> True if there is a picture for this look (downloaded or not yet). </summary>
    public static bool Has(string name) => Resolve(name) is not null;

    /// <summary>
    /// Name of a look in the game language (e.g. «tejado pizarra rojo»), or null when the game list has none (walls).
    /// </summary>
    public static string? GetName(string name)
    {
        if (Resolve(name) is not { } key)
            return null;
        // Only in the game language: a missing translation falls back to the name made from the internal one.
        var names = Entries[key].Names;
        var index = Array.IndexOf(Languages, GameInfo.CurrentLanguage);
        if (index < 0)
            index = 0;
        return index < names.Length && names[index].Length != 0 ? names[index] : null;
    }

    private static string FilePath(string key) => Path.Combine(Folder, key + ".png");

    /// <summary>
    /// The picture of a look, if it has been downloaded; otherwise null (and the download starts).
    /// </summary>
    public static Bitmap? Get(string name)
    {
        if (Resolve(name) is not { } key)
            return null;
        if (Cache.TryGetValue(key, out var cached))
            return cached;
        var path = FilePath(key);
        if (!File.Exists(path))
        {
            EnsureDownloaded();
            return null;
        }
        Bitmap? bmp;
        try
        {
            bmp = new Bitmap(path);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"{path}: {ex.Message}");
            bmp = null;
        }
        Cache[key] = bmp;
        return bmp;
    }

    /// <summary>
    /// Downloads the pictures not downloaded yet, once, in the background. Does nothing without network
    /// or when NHSE_OFFLINE is set (the tests).
    /// </summary>
    public static Task<int> EnsureDownloaded()
    {
        if (Environment.GetEnvironmentVariable("NHSE_OFFLINE") is { Length: not 0 })
            return Task.FromResult(0);
        return Downloading ??= Task.Run(DownloadMissing);
    }

    private static async Task<int> DownloadMissing()
    {
        var missing = Entries.Where(z => !File.Exists(FilePath(z.Key))).ToList();
        if (missing.Count == 0)
            return 0;
        Directory.CreateDirectory(Folder);
        using var client = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };
        client.DefaultRequestHeaders.UserAgent.ParseAdd("NHSE-Mac");
        int done = 0;
        await Parallel.ForEachAsync(missing, new ParallelOptions { MaxDegreeOfParallelism = 6 }, async (e, ct) =>
        {
            try
            {
                var data = await client.GetByteArrayAsync(e.Value.Url, ct);
                var temp = FilePath(e.Key) + ".part";
                await File.WriteAllBytesAsync(temp, data, ct);
                File.Move(temp, FilePath(e.Key), true);
                Interlocked.Increment(ref done);
            }
            catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException or IOException)
            {
                Console.WriteLine($"{e.Value.Url}: {ex.Message}");
            }
        });
        if (done != 0)
        {
            // Pictures that were missing before are now on disk: forget the empty answers.
            foreach (var key in Cache.Where(z => z.Value is null).Select(z => z.Key).ToList())
                Cache.TryRemove(key, out _);
            Dispatcher.UIThread.Post(() => PicturesChanged?.Invoke());
        }
        if (done != missing.Count)
            Downloading = null; // try again next time for the rest
        return done;
    }
}
