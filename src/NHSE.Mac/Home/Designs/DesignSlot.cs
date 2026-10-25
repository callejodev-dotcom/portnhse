using System.Security.Cryptography;
using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// A design of either kind (normal or PRO) with what the designs window needs, so one window shows both.
/// </summary>
public abstract class DesignSlot
{
    public abstract IVillagerOrigin Origin { get; }
    public abstract Span<byte> Data { get; }
    public abstract string Name { get; set; }
    public abstract string Town { get; }
    public abstract string Maker { get; }
    public abstract uint MakerID { get; }

    /// <summary>
    /// Number of 32x32 pictures: 1 for a normal design, 4 for a PRO one (front, back and sleeves).
    /// </summary>
    public abstract int SheetCount { get; }

    public abstract WriteableBitmap GetSheet(int sheet);

    /// <summary>
    /// The 15 colours, as ARGB.
    /// </summary>
    public abstract int[] GetColors();

    /// <summary>
    /// Sets the usage value the game expects after a design is brought from a file (see the classic editors).
    /// </summary>
    public abstract void FixUsage();

    /// <summary>
    /// Made by the game, not by a player: the sample designs it comes with and the blank slots.
    /// </summary>
    public bool IsFromGame => MakerID == uint.MaxValue;

    /// <summary>
    /// Start of the SHA-1 of the colours and pixels of the game's blank slot (the grey square with dashed lines),
    /// which does not change with the language as the name does.
    /// </summary>
    protected abstract string BlankHash { get; }

    private const int PictureOffset = 0x78; // palette, then pixels

    /// <summary>
    /// A slot never used (the game's blank design).
    /// </summary>
    public bool IsEmpty => IsFromGame && Convert.ToHexString(SHA1.HashData(Data[PictureOffset..])).StartsWith(BlankHash, StringComparison.Ordinal);

    public bool IsMadeBy(IVillagerOrigin player) => Origin.IsOriginatedFrom(player);

    public void MakeMine(IVillagerOrigin player) => Origin.ChangeOrigins(player, Data);

    /// <summary>
    /// All the sheets side by side (2x2 for PRO designs), enlarged without blurring the pixels.
    /// </summary>
    public WriteableBitmap GetPicture(int scale)
    {
        if (SheetCount == 1)
            return PatternEditor.ResizeImage(GetSheet(0), scale);

        const int size = 32;
        var pixels = new int[size * 2 * size * 2];
        for (int s = 0; s < SheetCount; s++)
        {
            using var sheet = GetSheet(s);
            var src = sheet.GetPixels();
            var (ox, oy) = ((s % 2) * size, (s / 2) * size);
            for (int y = 0; y < size; y++)
                src.AsSpan(y * size, size).CopyTo(pixels.AsSpan(((oy + y) * size * 2) + ox, size));
        }
        return PatternEditor.ResizeImage(ImageUtil.GetBitmap(pixels, size * 2, size * 2), scale);
    }
}

/// <summary>
/// What changes between the normal and the PRO designs: the file, its size and how to read one.
/// </summary>
public sealed record DesignKind(string Extension, int Size, Func<byte[], DesignSlot> Read)
{
    public static readonly DesignKind Normal = new(".nhd", DesignPattern.SIZE, data => new NormalDesign(new DesignPattern(data)));
    public static readonly DesignKind Pro = new(".nhpd", DesignPatternPRO.SIZE, data => new ProDesign(new DesignPatternPRO(data)));

    public string Description => this == Pro ? DesignStrings.FilePro : DesignStrings.FileNormal;

    public string Filter => $"{Description} (*{Extension})|*{Extension}";

    /// <summary>
    /// True if the file has the size of this kind of design (the files have no other mark).
    /// </summary>
    public bool CanRead(string path) => File.Exists(path) && new FileInfo(path).Length == Size;
}

public sealed class NormalDesign(DesignPattern p) : DesignSlot
{
    public DesignPattern Pattern { get; } = p;

    public override IVillagerOrigin Origin => Pattern;
    public override Span<byte> Data => Pattern.Data;
    public override string Name { get => Pattern.DesignName; set => Pattern.DesignName = value; }
    public override string Town => Pattern.TownName;
    public override string Maker => Pattern.PlayerName;
    public override uint MakerID => Pattern.PlayerID;
    protected override string BlankHash => "F6B081A947A1";
    public override int SheetCount => 1;
    public override WriteableBitmap GetSheet(int sheet) => Pattern.GetImage();

    public override int[] GetColors()
    {
        using var palette = Pattern.GetPalette();
        return palette.GetPixels();
    }

    public override void FixUsage()
    {
        if (Pattern.UsageCompatibility is (0xEE00 or 0xEE02)) // known valid values (00=non-transparent, 02=transparent)
            return;
        using var image = Pattern.GetImage();
        Pattern.UsageCompatibility = ImageUtil.ContainsTransparentPixels(image) ? 0xEE02u : 0xEE00u;
    }
}

public sealed class ProDesign(DesignPatternPRO p) : DesignSlot
{
    public DesignPatternPRO Pattern { get; } = p;

    public override IVillagerOrigin Origin => Pattern;
    public override Span<byte> Data => Pattern.Data;
    public override string Name { get => Pattern.DesignName; set => Pattern.DesignName = value; }
    public override string Town => Pattern.TownName;
    public override string Maker => Pattern.PlayerName;
    public override uint MakerID => Pattern.PlayerID;
    protected override string BlankHash => "0447131569D5";
    public override int SheetCount => 4;
    public override WriteableBitmap GetSheet(int sheet) => Pattern.GetImage(sheet);

    public override int[] GetColors()
    {
        using var palette = Pattern.GetPalette();
        return palette.GetPixels();
    }

    public override void FixUsage()
    {
        if (Pattern.UsageCompatibility is not (0xEE01 or 0xEE05)) // known valid values (01=pro, 05=default_unused)
            Pattern.UsageCompatibility = 0xEE01; // reset to default pro design
    }
}
