using Avalonia.Media;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Outcome of putting an item on a grid of tiles (island or room).
/// </summary>
public enum PlaceResult
{
    Ok,
    /// <summary> The item would stick out of the grid. </summary>
    OutOfBounds,
    /// <summary> Another item is in the way. </summary>
    Collision,
}

/// <summary>
/// Placing, removing and turning items on a <see cref="LayerItem"/> (island layers and room layers).
/// Big items keep their extension tiles in step, as the classic editors do with "Handle Item Extensions Automatically",
/// and nothing is written over other items ("Prevent Writing Occupied Item Tiles").
/// </summary>
public static class GridItems
{
    /// <summary>
    /// The item covering a tile: the tile itself, or the main tile of the big item it is part of.
    /// </summary>
    /// <returns>False if the tile is empty.</returns>
    public static bool TryGetRoot(LayerItem layer, int x, int y, out Item root, out int rootX, out int rootY)
    {
        rootX = x;
        rootY = y;
        root = layer.GetTileSafe(x, y);
        if (root.IsNone)
            return false;
        if (!root.IsExtension)
            return true;

        var rx = x - root.ExtensionX;
        var ry = y - root.ExtensionY;
        if (layer.Contains(rx, ry))
        {
            var parent = layer.GetTile(rx, ry);
            if (parent.IsRoot && parent.ItemId == root.ExtensionItemId)
            {
                (root, rootX, rootY) = (parent, rx, ry);
                return true;
            }
        }
        return true; // dangling piece of an item: it can still be removed
    }

    /// <summary>
    /// Width and height in tiles that the item covers, turned as it is.
    /// </summary>
    public static (int W, int H) GetSize(Item item)
    {
        var size = ItemInfo.GetItemSize(item);
        var (w, h) = (size.Width, size.Height);
        if ((item.Rotation & 1) == 1)
            (w, h) = (h, w);
        return (w, h);
    }

    /// <summary>
    /// Removes the item covering the tile (all of its tiles).
    /// </summary>
    public static bool Remove(LayerItem layer, int x, int y)
    {
        if (!TryGetRoot(layer, x, y, out var root, out var rx, out var ry))
            return false;
        if (root.IsRoot)
            layer.DeleteExtensionTiles(root, rx, ry);
        root.Delete();
        return true;
    }

    /// <summary>
    /// Puts <paramref name="item"/> with its top left corner at (<paramref name="x"/>, <paramref name="y"/>),
    /// replacing whatever item covers that tile. Nothing changes if it doesn't fit.
    /// </summary>
    /// <param name="layer">Grid to change.</param>
    /// <param name="item">Item to put (copied).</param>
    /// <param name="x">Tile where the item goes; if an item is there, the new one takes its place.</param>
    /// <param name="y">Tile where the item goes; if an item is there, the new one takes its place.</param>
    /// <param name="blocker">The item in the way, when it doesn't fit.</param>
    /// <param name="snapFieldItems">Plants, fences and rocks go on whole squares (even tiles), as in the game.</param>
    public static PlaceResult Place(LayerItem layer, Item item, int x, int y, out Item? blocker, bool snapFieldItems = true)
        => Place(layer, item, x, y, out blocker, out _, snapFieldItems);

    /// <inheritdoc cref="Place(LayerItem,Item,int,int,out Item?,bool)"/>
    /// <param name="at">Tile where the item ended up (its top left corner).</param>
    public static PlaceResult Place(LayerItem layer, Item item, int x, int y, out Item? blocker, out (int X, int Y) at, bool snapFieldItems = true)
    {
        blocker = null;
        if (TryGetRoot(layer, x, y, out _, out var rx, out var ry))
            (x, y) = (rx, ry);
        if (snapFieldItems && item.IsFieldItem)
        {
            x &= ~1;
            y &= ~1;
        }
        at = (x, y);
        if (!layer.Contains(x, y))
            return PlaceResult.OutOfBounds;

        // Keep a copy of the area, to put it back if the new item doesn't fit.
        var (w, h) = GetSize(item);
        var backup = Snapshot(layer, x - 12, y - 12, w + 24, h + 24);

        Remove(layer, x, y);
        var permission = layer.IsOccupied(item, x, y);
        if (permission != PlacedItemPermission.NoCollision)
        {
            if (permission == PlacedItemPermission.Collision)
                blocker = FindBlocker(layer, x, y, w, h);
            Restore(layer, backup);
            return permission == PlacedItemPermission.OutOfBounds ? PlaceResult.OutOfBounds : PlaceResult.Collision;
        }

        var tile = layer.GetTile(x, y);
        tile.CopyFrom(item);
        if (tile.IsRoot)
            layer.SetExtensionTiles(tile, x, y);
        return PlaceResult.Ok;
    }

    /// <summary>
    /// Turns the item covering the tile a quarter to the right (its space turns with it).
    /// </summary>
    public static PlaceResult Rotate(LayerItem layer, int x, int y, out Item? blocker)
    {
        blocker = null;
        if (!TryGetRoot(layer, x, y, out var root, out var rx, out var ry) || !root.IsRoot)
            return PlaceResult.OutOfBounds;
        var turned = new Item();
        turned.CopyFrom(root);
        turned.Rotation = (turned.Rotation + 1) & 3;
        return Place(layer, turned, rx, ry, out blocker, out _, false);
    }

    /// <summary>
    /// Items that look different when turned: furniture and the like, not plants or things lying on the ground.
    /// </summary>
    public static bool CanRotate(Item item)
    {
        if (item.IsNone || item.IsFieldItem || item.IsBuried || item.IsDropped)
            return false;
        var kind = ItemInfo.GetItemKind(item);
        return kind.IsFurniture || kind is ItemKind.Kind_Fence or ItemKind.Kind_Counter or ItemKind.Kind_Pillar or ItemKind.Kind_Rug
            or ItemKind.Kind_RugMyDesign or ItemKind.Kind_Poster or ItemKind.Kind_Gyroid or ItemKind.Kind_Picture or ItemKind.Kind_PictureFake
            or ItemKind.Kind_Sculpture or ItemKind.Kind_SculptureFake or ItemKind.Kind_Bromide or ItemKind.Kind_Lantern or ItemKind.Kind_Windmill
            or ItemKind.Kind_HousePost or ItemKind.Kind_XmasDeco or ItemKind.Kind_FtrWall;
    }

    private static Item FindBlocker(LayerItem layer, int x, int y, int w, int h)
    {
        for (int ix = 0; ix < w; ix++)
        {
            for (int iy = 0; iy < h; iy++)
            {
                if (TryGetRoot(layer, x + ix, y + iy, out var root, out _, out _))
                    return root;
            }
        }
        return Item.NO_ITEM;
    }

    private static List<(int X, int Y, Item Value)> Snapshot(LayerItem layer, int x, int y, int w, int h)
    {
        var result = new List<(int, int, Item)>();
        for (int ix = x; ix < x + w; ix++)
        {
            for (int iy = y; iy < y + h; iy++)
            {
                if (!layer.Contains(ix, iy))
                    continue;
                var copy = new Item();
                copy.CopyFrom(layer.GetTile(ix, iy));
                result.Add((ix, iy, copy));
            }
        }
        return result;
    }

    private static void Restore(LayerItem layer, List<(int X, int Y, Item Value)> backup)
    {
        foreach (var (x, y, value) in backup)
            layer.GetTile(x, y).CopyFrom(value);
    }

    #region Names and pictures
    /// <summary>
    /// Name of an item as the player knows it. Planted things (trees, flowers, fences…) take the name of the item they come from.
    /// </summary>
    public static string GetName(Item item)
    {
        if (item.IsNone)
            return MapStrings.EmptySpace;
        var id = item.DisplayItemId;
        if (FieldItemList.Items.TryGetValue(id, out var def))
            return GetFieldName(def);
        return GameInfo.Strings.GetItemName(item);
    }

    public static string GetFieldName(FieldItemDefinition def)
    {
        var held = def.HeldItemId;
        var names = GameInfo.Strings.itemlist;
        if (held != def.Index && held < names.Length && !string.IsNullOrWhiteSpace(names[held]))
            return names[held];
        var k = def.Kind;
        if (def.Name.Contains("Stump", StringComparison.Ordinal))
            return MapStrings.Stump;
        if (k.IsTree)
            return MapStrings.Tree;
        if (k.IsStone)
            return MapStrings.Rock;
        if (k == FieldItemKind.UnitIconHole)
            return MapStrings.Hole;
        if (k.IsWeed)
            return MapStrings.Weeds;
        if (k.IsFlower)
            return MapStrings.Flower;
        if (k.IsBush)
            return MapStrings.Bush;
        if (k.IsFence)
            return MapStrings.Fence;
        if (k is >= FieldItemKind.LadderKitA and <= FieldItemKind.LadderKitD)
            return MapStrings.Ladder;
        return MapStrings.Plant;
    }

    /// <summary>
    /// Picture of an item: the item image, or a game icon for planted things that have none (rocks, stumps, holes).
    /// </summary>
    public static IImage? GetPicture(Item item)
    {
        if (item.IsNone)
            return null;
        var id = item.DisplayItemId;
        if (FieldItemList.Items.TryGetValue(id, out var def))
        {
            if (def.Kind.IsStone)
                return HomeIcon("OreStone");
            if (def.Kind == FieldItemKind.UnitIconHole)
                return HomeIcon("Scoop");
            if (def.Name.Contains("Stump", StringComparison.Ordinal))
                return HomeIcon("DIYWoodNormal");
        }
        return ItemSprite.GetItemSprite(id, item.IsExtension ? (ushort)0 : item.Count);
    }

    public static IImage? HomeIcon(string name) => ItemSprite.GetResource($"sprites/MenuIcon/{name}.png");
    #endregion
}
