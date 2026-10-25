using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Port of NHSE.Sprites' ItemSprite. The item info (count, uses, flags) that WinForms drew into a bitmap
/// is drawn as text by the controls that show the sprite (see <see cref="ItemSlot"/>).
/// </summary>
public static class ItemSprite
{
    private static string[] ItemNames = []; // currently only used as length check for FieldItem

    /// <summary>
    /// Folder with the downloaded item images (~/Library/Application Support/NHSE/img).
    /// </summary>
    public static string PlatformAppDataPath => Program.AppDataPath;
    public static string PlatformAppDataImagePath { get; } = Path.Combine(Program.AppDataPath, "img");
    public static bool SingleSpriteExists => Directory.Exists(PlatformAppDataImagePath) && Directory.EnumerateFileSystemEntries(PlatformAppDataImagePath).Any();

    private static readonly Dictionary<string, Bitmap?> Cache = [];

    public static void Initialize(string[] itemNames)
    {
        ItemNames = itemNames;

        if (!Directory.Exists(PlatformAppDataImagePath))
            Directory.CreateDirectory(PlatformAppDataImagePath);
    }

    /// <summary>
    /// Forgets the decoded images, after downloading a new set of item images.
    /// </summary>
    public static void ClearCache()
    {
        lock (Cache)
            Cache.Clear();
    }

    public static Bitmap? GetItemSprite(Item item) => GetItemSprite(item.ItemId, item.Count);

    public static Bitmap? GetItemSprite(ushort id, ushort count = 0)
    {
        if (id == Item.NONE)
            return null;

        if (!TryGetItemImageSprite(id, out var path, count))
            return TryGetMenuIconSprite(id, out var img) ? img : Leaf;

        return GetCached(path, () => new Bitmap(path)) ?? Leaf;
    }

    public static Bitmap? Leaf => GetResource("sprites/Misc/leaf.png");

    private static bool TryGetMenuIconSprite(ushort id, out Bitmap? img)
    {
        id = TryGetFieldItemId(id, ItemNames.Length);
        var iconType = ItemInfo.GetMenuIcon(id);

        img = GetResource($"sprites/MenuIcon/{iconType}.png");
        return img != null;
    }

    private static bool TryGetItemImageSprite(ushort id, out string path, ushort count = 0)
    {
        id = TryGetFieldItemId(id, ItemNames.Length);

        var name = $"{id:00000}_{count}";
        if (SpriteFileExists(name, out path))
            return true;

        name = $"{id:00000}_0"; // fallback to no variation
        return SpriteFileExists(name, out path);
    }

    private static bool SpriteFileExists(string filename, out string path)
    {
        path = Path.Combine(PlatformAppDataImagePath, filename + ".png");
        return File.Exists(path);
    }

    private static ushort TryGetFieldItemId(ushort id, int length)
    {
        if (id < length)
            return id;
        if (!FieldItemList.Items.TryGetValue(id, out var definition))
            return id;

        var remap = definition.HeldItemId;
        return remap >= length ? id : remap;
    }

    /// <summary>
    /// Gets an image embedded from the NHSE.Sprites resources.
    /// </summary>
    public static Bitmap? GetResource(string name)
    {
        if (!EmbeddedResources.Exists(name))
            return null;
        return GetCached(name, () =>
        {
            using var s = EmbeddedResources.Open(name)!;
            return new Bitmap(s);
        });
    }

    private static Bitmap? GetCached(string key, Func<Bitmap> load)
    {
        lock (Cache)
        {
            if (Cache.TryGetValue(key, out var bmp))
                return bmp;
            try
            {
                bmp = load();
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                bmp = null;
            }
            Cache[key] = bmp;
            return bmp;
        }
    }

    /// <summary>
    /// Texts drawn over an item slot, as the WinForms DrawInfo did: count (top left), uses (center),
    /// system param (top right) and additional param (bottom left). Empty when the value is 0.
    /// </summary>
    public static (string Count, string Uses, string System, string Additional) GetItemMarkup(Item item) =>
    (
        item.Count != 0 ? item.Count.ToString() : string.Empty,
        item.UseCount != 0 ? item.UseCount.ToString() : string.Empty,
        item.SystemParam != 0 ? item.SystemParam.ToString() : string.Empty,
        item.AdditionalParam != 0 ? item.AdditionalParam.ToString() : string.Empty
    );
}

/// <summary>
/// Provides sprites for Villagers.
/// </summary>
public static class VillagerSprite
{
    /// <summary>
    /// Gets the Villager's 128x128 sprite based on the input parameters.
    /// </summary>
    /// <param name="species">Species of Villager</param>
    /// <param name="variant">Variant of Villager</param>
    public static Bitmap? GetVillagerSprite(VillagerSpecies species, int variant)
    {
        var asset = VillagerUtil.GetInternalVillagerName(species, variant);
        return GetVillagerSprite(asset);
    }

    /// <summary>
    /// Gets the sprite for the Villager based on its internal name.
    /// </summary>
    /// <param name="asset">Internal name of the villager (3char, 2digit)</param>
    public static Bitmap? GetVillagerSprite(string asset) => ItemSprite.GetResource($"sprites/Villagers/{asset}.png");
}
