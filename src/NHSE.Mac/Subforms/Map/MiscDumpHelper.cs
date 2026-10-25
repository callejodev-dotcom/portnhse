using Avalonia;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Import/export of single save structures to files. Async (file dialogs are) and with the <c>owner</c> window of the dialogs first.
/// </summary>
public static class MiscDumpHelper
{
    public static async Task DumpVillagerMemoryPlayer(Visual owner, IVillager v, GSaveMemory memory)
    {
        var path = await Pickers.SaveFile(owner, "New Horizons Villager Player Memory (*.nhvpm)|*.nhvpm|All files (*.*)|*.*", $"{v.InternalName}.nhvpm");
        if (path is null)
            return;

        var data = memory.Data;
        await File.WriteAllBytesAsync(path, data.ToArray());
    }

    public static async Task<bool> LoadVillagerMemoryPlayer(Visual owner, IVillager v, GSaveMemory[] memories, int index)
    {
        var file = await Pickers.OpenFile(owner, "New Horizons Villager Player Memory (*.nhvpm)|*.nhvpm|All files (*.*)|*.*", $"{v.InternalName}.nhvpm");
        if (file is null)
            return false;

        var fi = new FileInfo(file);
        const int expectLength = GSaveMemory.SIZE;
        if (fi.Length != expectLength)
        {
            await Dialogs.Error(MessageStrings.MsgCanceling, string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expectLength));
            return false;
        }

        var data = await File.ReadAllBytesAsync(file);
        var memory = new GSaveMemory(data);
        var old = memories[index];

        if (!memory.IsOriginatedFrom(old))
        {
            string msg = string.Format(MessageStrings.MsgDataDidNotOriginateFromHost_0, old.PlayerName);
            var result = await Dialogs.Prompt(MessageBoxButtons.YesNoCancel, msg, MessageStrings.MsgAskUpdateValues);
            if (result == DialogResult.Cancel)
                return false;
            if (result == DialogResult.Yes)
                memory.ChangeOrigins(old, memory.Data);
        }

        memories[index] = memory;
        return true;
    }

    public static async Task DumpMuseum(Visual owner, Museum museum)
    {
        var path = await Pickers.SaveFile(owner, "New Horizons Museum (*.nhm)|*.nhm|All files (*.*)|*.*", "museum.nhm");
        if (path is null)
            return;

        var data = museum.Data;
        await File.WriteAllBytesAsync(path, data.ToArray());
    }

    public static async Task<bool> LoadMuseum(Visual owner, Museum museum)
    {
        var file = await Pickers.OpenFile(owner, "New Horizons Museum (*.nhm)|*.nhm|All files (*.*)|*.*", "museum.nhm");
        if (file is null)
            return false;

        var fi = new FileInfo(file);
        const int expectLength = Museum.SIZE;
        if (fi.Length != expectLength)
        {
            await Dialogs.Error(MessageStrings.MsgCanceling, string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expectLength));
            return false;
        }

        var data = await File.ReadAllBytesAsync(file);
        data.CopyTo(museum.Data);
        return true;
    }

    /// <summary>
    /// Name of a player house, also used by PlayerHouseEditor's list.
    /// </summary>
    public static string GetHouseSummary(IReadOnlyList<Player> players, IPlayerHouse house, int index)
    {
        var houseName = index >= players.Count ? $"House {index}" : $"{players[index].Personal.PlayerName}'s House";
        return $"{houseName} (lv {house.HouseLevel})";
    }

    public static Task DumpHouse(Visual owner, IReadOnlyList<Player> players, IReadOnlyList<IPlayerHouse> houses, int index, bool dumpAll)
    {
        if (dumpAll)
            return DumpAllPlayerHouses(owner, houses, players);
        return DumpPlayerHouse(owner, players, houses, index);
    }

    private static async Task DumpPlayerHouse(Visual owner, IReadOnlyList<Player> players, IReadOnlyList<IPlayerHouse> houses, int index)
    {
        var house = houses[index];
        var name = GetHouseSummary(players, house, index);
        const string filter = "New Horizons Player House (*.nhph)|*.nhph|" +
                              "New Horizons Player House (*.nhph2)|*.nhph2|" +
                              "All files (*.*)|*.*";
        var path = await Pickers.SaveFile(owner, filter, $"{name}.{house.Extension}");
        if (path is null)
            return;

        var data = house.Write();
        await File.WriteAllBytesAsync(path, data);
    }

    private static async Task DumpAllPlayerHouses(Visual owner, IReadOnlyList<IPlayerHouse> houses, IReadOnlyList<Player> players)
    {
        var folder = await Pickers.OpenFolder(owner);
        if (folder is null || !Directory.Exists(folder))
            return;
        houses.DumpPlayerHouses(players, folder);
    }

    public static async Task<bool> LoadHouse(Visual owner, MainSaveOffsets offsets, IReadOnlyList<Player> players, IPlayerHouse[] houses, int index)
    {
        var h = houses[index];
        var name = GetHouseSummary(players, houses[index], index);
        const string filter = "New Horizons Player House (*.nhph)|*.nhph|" +
                              "New Horizons Player House (*.nhph2)|*.nhph2|" +
                              "All files (*.*)|*.*";
        var path = await Pickers.OpenFile(owner, filter, $"{name}.{h.Extension}");
        if (path is null)
            return false;

        var expectLength = offsets.PlayerHouseSize;
        var fi = new FileInfo(path);
        if (!PlayerHouseConverter.IsCompatible((int)fi.Length, expectLength))
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expectLength), path);
            return false;
        }

        var data = await File.ReadAllBytesAsync(path);
        data = PlayerHouseConverter.GetCompatible(data, expectLength);
        if (data.Length != expectLength)
        {
            await Dialogs.Error(MessageStrings.MsgCanceling, string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expectLength), path);
            return false;
        }

        h = offsets.ReadPlayerHouse(data);
        var current = houses[index];
        h.NPC1 = current.NPC1;
        houses[index] = h;
        return true;
    }

    public static async Task DumpRoom(Visual owner, IPlayerRoom room, int index)
    {
        const string filter = "New Horizons Player House Room (*.nhpr)|*.nhpr|" +
                              "New Horizons Player House Room (*.nhpr2)|*.nhpr2|" +
                              "All files (*.*)|*.*";
        var path = await Pickers.SaveFile(owner, filter, $"Room {index + 1}.{room.Extension}");
        if (path is null)
            return;

        var data = room.Write();
        await File.WriteAllBytesAsync(path, data);
    }

    /// <summary>
    /// Loads a room from a file.
    /// </summary>
    /// <returns>The loaded room, or null if canceled or invalid (WinForms used a ref parameter, which async methods can't have).</returns>
    public static async Task<IPlayerRoom?> LoadRoom(Visual owner, MainSaveOffsets offsets, IPlayerRoom room, int index)
    {
        const string filter = "New Horizons Player House Room (*.nhpr)|*.nhpr|" +
                              "New Horizons Player House Room (*.nhpr2)|*.nhpr2|" +
                              "All files (*.*)|*.*";
        var path = await Pickers.OpenFile(owner, filter, $"Room {index + 1}.{room.Extension}");
        if (path is null)
            return null;

        var expectLength = offsets.PlayerRoomSize;
        var fi = new FileInfo(path);
        if (!PlayerRoomConverter.IsCompatible((int)fi.Length, expectLength))
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expectLength), path);
            return null;
        }

        var data = await File.ReadAllBytesAsync(path);
        data = PlayerRoomConverter.GetCompatible(data, offsets.PlayerRoomSize);
        if (data.Length != expectLength)
        {
            await Dialogs.Error(MessageStrings.MsgCanceling, string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expectLength), path);
            return null;
        }

        return offsets.ReadPlayerRoom(data);
    }

    public static async Task DumpFlags(Visual owner, byte[] data, string name)
    {
        var path = await Pickers.SaveFile(owner, "New Horizons Flag List (*.nhfl)|*.nhfl|All files (*.*)|*.*", $"{name}.nhfl");
        if (path is not null)
            await File.WriteAllBytesAsync(path, data);
    }

    public static Task DumpFlagsToClipboard(Visual owner, string data) => UiUtil.SetClipboardText(owner, data);

    public static async Task<byte[]> LoadFlags(Visual owner, int size, string name)
    {
        var file = await Pickers.OpenFile(owner, "New Horizons Flag List (*.nhfl)|*.nhfl|All files (*.*)|*.*", $"{name}.nhfl");
        if (file is null)
            return [];

        var fi = new FileInfo(file);
        int expectLength = size;
        if (fi.Length != expectLength)
        {
            await Dialogs.Error(MessageStrings.MsgCanceling, string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expectLength));
            return [];
        }

        return await File.ReadAllBytesAsync(file);
    }
}
