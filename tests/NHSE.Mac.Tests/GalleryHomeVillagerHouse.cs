using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The friendly villager house (64-…) and the big item tooltip (65-…).
/// </summary>
public class GalleryHomeVillagerHouse
{
    private static HorizonSave Save(out string folder)
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        return TestSave.LoadCopy(out folder);
    }

    private static VillagerHouseWindow Open(HorizonSave sav, out IVillagerHouse[] houses, out int slot)
    {
        houses = sav.Main.GetVillagerHouses();
        slot = Array.FindIndex(houses, z => z.NPC1 == 0);
        Assert.True(slot >= 0, "villager 0 has a house");
        var v = sav.Main.GetVillagers()[0];
        return new VillagerHouseWindow(sav.Main, houses, slot, GameInfo.Strings.GetVillager(v.InternalName), v.InternalName);
    }

    [AvaloniaFact]
    public void HouseLooksAndSaves()
    {
        var sav = Save(out var folder);
        var w = Open(sav, out var houses, out var slot);
        Gallery.Capture(w, "64-villager-house", false);

        // Another villager roof, as the roof list does; the order goes with it so the game keeps it.
        var roof = Enum.GetValues<RoofType>().First(z => HouseStyleNames.IsForVillagers(z) && z != houses[slot].RoofUniqueID);
        houses[slot].RoofUniqueID = roof;
        houses[slot].OrderRoofUniqueID = roof;
        w.SaveAndClose();

        sav.Save((uint)DateTime.Now.Ticks);
        var reloaded = HorizonSave.FromFolder(folder);
        Assert.Empty(reloaded.GetInvalidHashes());
        var house = reloaded.Main.GetVillagerHouses()[slot];
        Assert.Equal(roof, house.RoofUniqueID);
        Assert.Equal(roof, house.OrderRoofUniqueID);
    }

    [AvaloniaFact]
    public void VillagerStylesHaveNames()
    {
        var roofs = Enum.GetValues<RoofType>().Where(HouseStyleNames.IsForVillagers).ToList();
        var walls = Enum.GetValues<WallType>().Where(HouseStyleNames.IsForVillagers).ToList();
        Assert.NotEmpty(roofs);
        Assert.NotEmpty(walls);
        Assert.All(HouseStyleNames.GetNames(roofs), n => Assert.DoesNotContain("House", n));
        Assert.All(HouseStyleNames.GetNames(walls), n => Assert.DoesNotContain("House", n));
    }

    [AvaloniaFact]
    public void BigItemTip()
    {
        var w = new Window { Width = 260, Height = 280, Content = HomeUi.ItemTip(new Item(0x0A4D) { Count = 0 }) };
        Gallery.Capture(w, "65-item-tip");
    }
}
