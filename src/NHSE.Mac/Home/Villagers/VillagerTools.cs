using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// What the friendly villager page does to a villager, in plain terms (moving out, crafting, villager files).
/// </summary>
public static class VillagerTools
{
    /// <summary>
    /// Villager flag (024 ForceMoveOut) that makes the game really take a villager who is moving out off the island.
    /// </summary>
    public const int ForceMoveOutFlag = 24;

    public static bool IsEmpty(IVillager v) => v.Species == (byte)VillagerSpecies.non;

    /// <summary>
    /// Packs the villager's things and lets the game take them off the island (or keeps them, when false).
    /// </summary>
    public static void SetMovingOut(IVillager v, bool value)
    {
        v.MovingOut = value;
        var flags = v.GetEventFlagsSave();
        flags[ForceMoveOutFlag] = (ushort)(value ? 1 : 0);
        v.SetEventFlagsSave(flags);
    }

    /// <summary>
    /// The villager is crafting something (the game keeps 255:255 as the end time while they aren't).
    /// </summary>
    public static bool IsCrafting(IVillager v) => v.DIYEndHour < 24 && v.DIYEndMinute < 60;

    /// <summary>
    /// "Crafting X right now, done at 10:40", or null if the villager isn't crafting.
    /// </summary>
    public static string? GetCraftingText(IVillager v)
    {
        if (!IsCrafting(v))
            return null;
        var what = RecipeList.Recipes.TryGetValue(v.DIYRecipeIndex, out var item) ? GameInfo.Strings.GetItemName(item) : "?";
        return string.Format(VillagerStrings.CraftingNow, what, $"{v.DIYEndHour:00}:{v.DIYEndMinute:00}");
    }

    /// <summary>
    /// What the villager remembers of a player (null if they haven't met).
    /// </summary>
    public static GSaveMemory? GetMemoryOf(IVillager v, IVillagerOrigin player)
        => v.GetMemories().FirstOrDefault(m => !string.IsNullOrEmpty(m.PlayerName) && m.IsSamePlayer(player) && m.IsSameTown(player));

    /// <summary>
    /// Reads a villager file (.nhv/.nhv2), converted to the format of this save; null if it isn't a villager.
    /// </summary>
    public static IVillager? Read(MainSave main, byte[] data)
    {
        var expect = main.Offsets.VillagerSize;
        if (!VillagerConverter.IsCompatible(data.Length, expect))
            return null;
        data = VillagerConverter.GetCompatible(data, expect);
        return data.Length == expect ? main.Offsets.ReadVillager(data) : null;
    }

    /// <summary>
    /// Makes a villager from another island remember this island and its player instead of the ones they came from.
    /// </summary>
    public static IVillager AdaptToIsland(IVillager v, IVillagerOrigin host, MainSave main)
    {
        var data = v.Write(); // a copy: replace the old island and player in it and read it again
        v.ChangeOrigins(host, data);
        return main.Offsets.ReadVillager(data);
    }
}
