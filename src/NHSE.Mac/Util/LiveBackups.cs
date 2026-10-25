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
        var name = StringUtil.CleanFileName($"{date:yyyy-MM-dd HH.mm.ss} - {personal.TownName} ({personal.PlayerName}) - {why}.zip");
        var path = Path.Combine(Folder, name);
        using (var zip = ZipFile.Open(path, ZipArchiveMode.Create))
        {
            Write(zip, "main.bin", main);
            Write(zip, "personal.bin", player);
        }
        return path;
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
