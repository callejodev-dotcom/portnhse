using Avalonia;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Import/export of the map layers (items, terrain, buildings, acres, flags, placed designs) to files.
/// Async (file dialogs are) and with the <c>owner</c> window of the dialogs first.
/// </summary>
public static class MapDumpHelper
{
    private const int DesignDumpSizeAbove31 = 0x6C00;
    private const int DesignDumpSizeBelow31 = 0x5400;

    private const string FilterLayer = "New Horizons Field Item Layer (*.nhl)|*.nhl|All files (*.*)|*.*";
    private const string FilterTerrain = "New Horizons Terrain (*.nht)|*.nht|All files (*.*)|*.*";
    private const string FilterBuildings = "New Horizons Building List (*.nhb)|*.nhb|All files (*.*)|*.*";
    private const string FilterAcres = "New Horizons Acres (*.nha)|*.nha|All files (*.*)|*.*";
    private const string FilterFlags = "New Horizons Field Item Layer Flags (*.nhlf)|*.nhl|All files (*.*)|*.*";
    private const string FilterDesigns = "nhmd file (*.nhmd)|*.nhmd";
    private const string DesignFileName = "Island MyDesignMap.nhmd";

    public static async Task<bool> ImportToLayerAcreSingle(Visual owner, LayerFieldItem layerField, string acre, int layerIndex, int relX, int relY)
    {
        var name = GetBaseFileName(acre, "acre");
        var path = await Pickers.OpenFile(owner, FilterLayer, $"{name}-{layerIndex}.nhl");
        if (path is null)
            return false;

        var fi = new FileInfo(path);

        int expect = layerField.TileInfo.ViewCount * Item.SIZE;
        if (fi.Length != expect)
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expect));
            return false;
        }

        var data = await File.ReadAllBytesAsync(path);
        layerField.ImportAcre(data, relX, relY);
        return true;
    }

    private static string GetBaseFileName(string initialName, string fallback)
    {
        var result = StringUtil.CleanFileName(initialName);
        if (string.IsNullOrEmpty(result))
            return fallback;
        return result;
    }

    public static async Task<bool> ImportToLayerAcreAll(Visual owner, LayerFieldItem layerField)
    {
        var path = await Pickers.OpenFile(owner, FilterLayer, "acres.nhl");
        if (path is null)
            return false;

        var fi = new FileInfo(path);
        var expect = layerField.TileInfo.TotalCount * Item.SIZE;
        if (fi.Length != expect && !FieldItemUpgrade.IsUpdateNeeded(fi.Length, expect))
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expect));
            return false;
        }

        var data = await File.ReadAllBytesAsync(path);
        FieldItemUpgrade.DetectUpdate(ref data, expect);
        layerField.ImportAll(data);
        return true;
    }

    public static async Task DumpLayerAcreSingle(Visual owner, LayerFieldItem layerField, string acre, int layerIndex, int relX, int relY)
    {
        var name = GetBaseFileName(acre, "acre");
        var path = await Pickers.SaveFile(owner, FilterLayer, $"{name}-{layerIndex}.nhl");
        if (path is null)
            return;

        var data = layerField.DumpAcre(relX, relY);
        await File.WriteAllBytesAsync(path, data);
    }

    public static async Task DumpLayerAcreAll(Visual owner, LayerFieldItem layerField)
    {
        var path = await Pickers.SaveFile(owner, FilterLayer, "acres.nhl");
        if (path is null)
            return;

        var data = layerField.DumpAll();
        await File.WriteAllBytesAsync(path, data);
    }

    public static async Task<bool> ImportTerrainAcre(Visual owner, LayerTerrain m, string acre, int relX, int relY)
    {
        var name = GetBaseFileName(acre, "acre");
        var path = await Pickers.OpenFile(owner, FilterTerrain, $"{name}.nht");
        if (path is null)
            return false;

        var fi = new FileInfo(path);

        int expect = m.TileInfo.ViewCount * TerrainTile.SIZE;
        if (fi.Length != expect)
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expect));
            return false;
        }

        var data = await File.ReadAllBytesAsync(path);
        m.ImportAcre(data, relX, relY);
        return true;
    }

    public static async Task<bool> ImportTerrainAll(Visual owner, LayerTerrain m)
    {
        var path = await Pickers.OpenFile(owner, FilterTerrain, "terrainAcres.nht");
        if (path is null)
            return false;

        var fi = new FileInfo(path);

        int expect = m.TileInfo.TotalCount * TerrainTile.SIZE;
        if (fi.Length != expect)
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expect));
            return false;
        }

        var data = await File.ReadAllBytesAsync(path);
        m.ImportAll(data);
        return true;
    }

    public static async Task DumpTerrainAcre(Visual owner, LayerTerrain m, string acre, int relX, int relY)
    {
        var name = GetBaseFileName(acre, "terrainAcre");
        var path = await Pickers.SaveFile(owner, FilterTerrain, $"{name}.nht");
        if (path is null)
            return;

        var data = m.DumpAcre(relX, relY);
        await File.WriteAllBytesAsync(path, data);
    }

    public static async Task DumpTerrainAll(Visual owner, LayerTerrain m)
    {
        var path = await Pickers.SaveFile(owner, FilterTerrain, "terrainAcres.nht");
        if (path is null)
            return;

        var data = m.DumpAll();
        await File.WriteAllBytesAsync(path, data);
    }

    public static async Task DumpBuildings(Visual owner, IReadOnlyList<Building> buildings)
    {
        var path = await Pickers.SaveFile(owner, FilterBuildings, "buildings.nhb");
        if (path is null)
            return;

        byte[] data = Building.SetArray(buildings);
        await File.WriteAllBytesAsync(path, data);
    }

    public static async Task<bool> ImportBuildings(Visual owner, IReadOnlyList<Building> buildings)
    {
        var path = await Pickers.OpenFile(owner, FilterBuildings, "buildings.nhb");
        if (path is null)
            return false;

        var fi = new FileInfo(path);

        const int expect = Building.SIZE * MainSaveOffsets.BuildingCount; // 46
        const int oldSize = Building.SIZE * 40;
        if (fi.Length is not (expect or oldSize))
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expect));
            return false;
        }

        var data = await File.ReadAllBytesAsync(path);
        var arr = Building.GetArray(data);
        for (int i = 0; i < arr.Length; i++)
            buildings[i].CopyFrom(arr[i]);
        return true;
    }

    public static async Task<bool> DumpMapAcresAll(Visual owner, ReadOnlyMemory<byte> data)
    {
        var path = await Pickers.SaveFile(owner, FilterAcres, "acres.nha");
        if (path is null)
            return false;

        await File.WriteAllBytesAsync(path, data);
        return true;
    }

    public static async Task<bool> ImportMapAcresAll(Visual owner, Memory<byte> data)
    {
        var path = await Pickers.OpenFile(owner, FilterAcres, "acres.nha");
        if (path is null)
            return false;

        var original = data;
        var modified = await File.ReadAllBytesAsync(path);
        if (original.Length != modified.Length)
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, modified.Length, original.Length), path);
            return false;
        }

        modified.CopyTo(data);
        return true;
    }

    public static async Task<bool> DumpLayerAllFlag(Visual owner, ILayerFieldItemFlag layer, uint layerIndex)
    {
        var path = await Pickers.SaveFile(owner, FilterFlags, $"flags-{layerIndex}.nhlf");
        if (path is null)
            return false;

        var data = layer.ExistingData.ToArray();
        await File.WriteAllBytesAsync(path, data);
        return true;
    }

    public static async Task<bool> ImportToLayerAllFlag(Visual owner, ILayerFieldItemFlag layer, uint layerIndex)
    {
        var path = await Pickers.OpenFile(owner, FilterFlags, $"flags-{layerIndex}.nhlf");
        if (path is null)
            return false;

        var fi = new FileInfo(path);

        var expect = layer.ExistingData.Length;
        if (fi.Length != expect && !FieldItemUpgrade.IsUpdateNeeded(fi.Length, expect))
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expect));
            return false;
        }

        var data = await File.ReadAllBytesAsync(path);
        FieldItemUpgrade.DetectUpdate(ref data, expect);
        layer.Import(data);
        return true;
    }

    public static async Task<bool> ImportMapDesigns(Visual owner, MainSave sav, Tuple<int, int> size)
    {
        var path = await Pickers.OpenFile(owner, FilterDesigns, DesignFileName);
        if (path is null)
            return false;

        var tiles = await File.ReadAllBytesAsync(path);

        var rel = size;

        if (rel.Item1 == 9)
        {
            if (tiles.Length == DesignDumpSizeAbove31)
            {
                tiles.CopyTo(sav.MapDesignTileData(rel.Item1, rel.Item2).Span);
            }
            else if (tiles.Length == DesignDumpSizeBelow31)
            {
                byte[] padBytes = BitConverter.GetBytes(MainSave.MapDesignNone);
                int len = (DesignDumpSizeAbove31 - DesignDumpSizeBelow31) / 2;

                tiles = ArrayUtil.PadLeft(tiles, tiles.Length + len, padBytes);
                tiles = ArrayUtil.PadRight(tiles, tiles.Length + len, padBytes);

                var path2 = await Pickers.SaveFile(owner, FilterDesigns, DesignFileName);
                if (path2 is null)
                    return false;

                await File.WriteAllBytesAsync(path2, tiles);

                tiles.CopyTo(sav.MapDesignTileData(rel.Item1, rel.Item2).Span);
            }
            // account for previous dumps that included extra data after the tile data (oops)
            // this can be removed in the future once enough time has passed that everyone has
            // re-dumped with the fixed version, but for now it prevents data loss
            else if (tiles.Length > DesignDumpSizeAbove31)
            {
                tiles = tiles.AsSpan(0, DesignDumpSizeAbove31).ToArray();
                var msg1 = MessageStrings.MsgDataSizeMismatchImport;
                var msg2 = MessageStrings.MsgDataTrimmedWarning;
                await Dialogs.Alert(string.Format(msg1, tiles.Length, DesignDumpSizeAbove31) + " " + msg2);

                var path2 = await Pickers.SaveFile(owner, FilterDesigns, DesignFileName);
                if (path2 is null)
                    return false;

                await File.WriteAllBytesAsync(path2, tiles);

                tiles.CopyTo(sav.MapDesignTileData(rel.Item1, rel.Item2).Span);
            }
            else
            {
                var msg = MessageStrings.MsgDataSizeMismatchImport;
                await Dialogs.Error(string.Format(msg, tiles.Length, DesignDumpSizeAbove31));
                return false;
            }
        }
        else
        {
            if (tiles.Length == DesignDumpSizeBelow31)
            {
                tiles.CopyTo(sav.MapDesignTileData(rel.Item1, rel.Item2).Span);
            }
            else if (tiles.Length == DesignDumpSizeAbove31)
            {
                int trimAmount = (DesignDumpSizeAbove31 - DesignDumpSizeBelow31) / 2;
                int newLength = tiles.Length - (2 * trimAmount);

                tiles = tiles.AsSpan(trimAmount, newLength).ToArray();

                tiles.CopyTo(sav.MapDesignTileData(rel.Item1, rel.Item2).Span);
            }
            else
            {
                var msg = MessageStrings.MsgDataSizeMismatchImport;
                await Dialogs.Error(string.Format(msg, tiles.Length, DesignDumpSizeBelow31));
                return false;
            }
        }

        return true;
    }

    public static async Task<bool> DumpMapDesigns(Visual owner, MainSave sav, Tuple<int, int> size)
    {
        var path = await Pickers.SaveFile(owner, FilterDesigns, DesignFileName);
        if (path is null)
            return false;

        var tiles = sav.MapDesignTileData(size.Item1, size.Item2);
        await File.WriteAllBytesAsync(path, tiles);
        return true;
    }
}
