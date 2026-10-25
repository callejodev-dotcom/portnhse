using System.Globalization;
using System.IO.Compression;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Copies of the game as it was in the console, kept every time it is brought over Wi-Fi and before every change is
/// sent: the island and the first player as they are in the game's memory, zipped (about 150 KB each, they are mostly
/// empty space). One can be sent back to the game to leave everything as it was.
/// </summary>
public static class LiveBackups
{
    public static string Folder => Path.Combine(SaveFileLoader.BackupPath, "directo");

    public sealed record Copy(string Path, DateTime Date, string Title);

    /// <summary> Keeps a copy of the island and player data, named with the date, the island and why it was made. </summary>
    public static string Save(ReadOnlySpan<byte> main, ReadOnlySpan<byte> player, Personal personal, string why)
    {
        Directory.CreateDirectory(Folder);
        var date = DateTime.Now;
        var name = StringUtil.CleanFileName($"{date:yyyy-MM-dd HH.mm.ss} - {personal.TownName} ({personal.PlayerName}) - {why}");
        // Two in the same second (sending twice quickly) get a number.
        var path = Path.Combine(Folder, name + ".zip");
        for (int n = 2; File.Exists(path); n++)
            path = Path.Combine(Folder, $"{name} ({n}).zip");
        using (var zip = ZipFile.Open(path, ZipArchiveMode.Create))
        {
            Write(zip, "main.bin", main);
            Write(zip, "personal.bin", player);
        }
        return path;
    }

    /// <summary> Where the last known state of each island's game is kept (one file per island and player). </summary>
    private static string LatestFolder => Path.Combine(Folder, "ultima");

    private static string LatestPath(Personal personal) => Path.Combine(LatestFolder, $"{personal.TownID:X8}-{personal.PlayerID:X8}.zip");

    /// <summary>
    /// Keeps the game as it is now (after bringing it, or after sending changes), replacing the one kept before: it is
    /// what «Reopen the last one» opens when it is newer than the files of the save.
    /// </summary>
    public static void SaveLatest(ReadOnlySpan<byte> main, ReadOnlySpan<byte> player, Personal personal)
    {
        Directory.CreateDirectory(LatestFolder);
        var path = LatestPath(personal);
        var temp = path + ".tmp";
        using (var zip = ZipFile.Open(temp, ZipArchiveMode.Create))
        {
            Write(zip, "main.bin", main);
            Write(zip, "personal.bin", player);
        }
        File.Move(temp, path, true);
    }

    /// <summary>
    /// The newest state of this island's game known on the Mac (the last one brought or sent over Wi-Fi, or else the
    /// newest copy), if it is newer than <paramref name="than"/>: it goes into the save (only in memory).
    /// </summary>
    /// <returns>When that state was kept, or null if the save is already the newest.</returns>
    public static DateTime? UseNewest(HorizonSave sav, DateTime than)
    {
        var pers = sav.Players[0].Personal;
        var candidates = List().Select(c => (c.Path, c.Date)).ToList();
        var latest = LatestPath(pers);
        if (File.Exists(latest))
            candidates.Add((latest, File.GetLastWriteTime(latest)));
        var id = pers.Offsets.PersonalId;
        foreach (var (path, date) in candidates.Where(c => c.Date > than).OrderByDescending(c => c.Date))
        {
            (byte[] Main, byte[] Player)? copy;
            try
            {
                copy = Read(path, sav.Main.Data.Length, pers.Data.Length);
            }
            catch (Exception ex) when (ex is IOException or InvalidDataException)
            {
                continue;
            }
            // Only this island and this player.
            if (copy is not { } c || !c.Player.AsSpan(id, 0x38).SequenceEqual(pers.Data.Slice(id, 0x38)))
                continue;
            c.Main.CopyTo(sav.Main.Data);
            c.Player.CopyTo(pers.Data);
            return date;
        }
        return null;
    }

    private static void Write(ZipArchive zip, string name, ReadOnlySpan<byte> data)
    {
        using var s = zip.CreateEntry(name, CompressionLevel.SmallestSize).Open();
        s.Write(data);
    }

    /// <summary> The copies kept, newest first. </summary>
    public static IReadOnlyList<Copy> List()
    {
        if (!Directory.Exists(Folder))
            return [];
        return [.. Directory.GetFiles(Folder, "*.zip")
            .Select(p => new Copy(p, File.GetLastWriteTime(p), Path.GetFileNameWithoutExtension(p)))
            .OrderByDescending(z => z.Date)];
    }

    /// <summary> The island and player data of a copy, or null if it is not one of these copies or not the same size. </summary>
    public static (byte[] Main, byte[] Player)? Read(string path, int mainLength, int playerLength)
    {
        using var zip = ZipFile.OpenRead(path);
        var main = ReadEntry(zip, "main.bin");
        var player = ReadEntry(zip, "personal.bin");
        if (main?.Length != mainLength || player?.Length != playerLength)
            return null;
        return (main, player);
    }

    private static byte[]? ReadEntry(ZipArchive zip, string name)
    {
        if (zip.GetEntry(name) is not { } entry)
            return null;
        using var s = entry.Open();
        using var ms = new MemoryStream();
        s.CopyTo(ms);
        return ms.ToArray();
    }

    /// <summary> "2026-10-25 19.30.12 - Barro (Carli) - traída" → "25 oct 2026, 19:30 · traída". </summary>
    public static string Describe(Copy copy)
    {
        var why = copy.Title.Split(" - ").LastOrDefault() ?? string.Empty;
        return $"{copy.Date.ToString("d MMM yyyy, HH:mm", CultureInfo.CurrentCulture)} · {why}";
    }
}
