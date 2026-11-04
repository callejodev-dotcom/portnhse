using Avalonia.Headless.XUnit;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// La ficha de los vecinos de NHSE para Switch (consola/source/core/vecino.cpp): cada cambio deja la partida byte a byte
/// como la página de vecinos del Mac (VillagersPage, FriendshipWindow, RoomWindow, VillagerHouseWindow, VillagerTools).
/// </summary>
public class ConsolaVecinosTests
{
    /// <summary> Cambia el vecino de un hueco como la página del Mac (que luego escribe todos con SetVillagers). </summary>
    private static void Villager(HorizonSave sav, int slot, Action<IVillager> change)
    {
        var villagers = sav.Main.GetVillagers();
        change(villagers[slot]);
        sav.Main.SetVillagers(villagers);
    }

    private static string Temp(string name)
    {
        var dir = Path.Combine(Path.GetTempPath(), "nhse-consola-vecinos", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        return Path.Combine(dir, name);
    }

    [Fact]
    public void PersonalityLikeNhse() => ConsolaTests.Both(sav => Villager(sav, 1, v => v.Personality = VillagerPersonality.Peppy),
        "vecino-personalidad", "1", "5");

    [Fact]
    public void CatchphraseLikeNhse() => ConsolaTests.Both(sav => Villager(sav, 2, v => v.CatchPhrase = "¡olé, olé!"),
        "vecino-frase", "2", "¡olé, olé!");

    [Fact]
    public void OriginalCatchphraseLikeNhse()
    {
        // «Original» pone la muletilla con la que viene el vecino, en español.
        var es = GameInfo.GetStrings("es");
        var (_, after) = ConsolaTests.Both(sav => Villager(sav, 0, v => v.CatchPhrase = es.GetVillagerDefaultPhrase(v.InternalName)),
            "vecino-frase", "0", "-");
        var v0 = after.Main.GetVillagers()[0];
        Assert.Equal(es.GetVillagerDefaultPhrase(v0.InternalName), v0.CatchPhrase);
        Assert.NotEmpty(v0.CatchPhrase);
    }

    [Fact]
    public void MovingOutLikeNhse() => ConsolaTests.Both(sav => Villager(sav, 3, v => VillagerTools.SetMovingOut(v, true)),
        "vecino-mudanza", "3", "1");

    [Fact]
    public void EveryoneMovesOutLikeNhse()
    {
        // Como VillagersPage.EveryoneMovesOut.
        ConsolaTests.Both(sav =>
        {
            var villagers = sav.Main.GetVillagers();
            foreach (var v in villagers.Where(z => !VillagerTools.IsEmpty(z)))
                VillagerTools.SetMovingOut(v, true);
            sav.Main.SetVillagers(villagers);
        }, "vecinos-mudanza");
    }

    [Fact]
    public void FriendshipAndNicknameLikeNhse()
    {
        // Como FriendshipWindow: los puntos (130, «muy buenos amigos») y cómo llama al jugador.
        ConsolaTests.Both(sav => Villager(sav, 0, v =>
        {
            var m = v.GetMemory(0);
            m.Friendship = FriendshipWindow.LevelStart[5];
            m.NickName = "Carlita";
            v.SetMemory(m, 0);
        }), "vecino-amistad", "0", "0", "130", "Carlita");
    }

    [Fact]
    public void ClothesAndFurnitureLikeNhse()
    {
        // Como ItemsWindow: al elegir un objeto se vacía el hueco y se pone el objeto con su cantidad (solo sus 8 bytes).
        ConsolaTests.Both(sav => Villager(sav, 1, v =>
        {
            // ItemsWindow ve la lista como objetos (Item): vaciar solo toca sus 8 bytes.
            IReadOnlyList<Item> wear = v.WearStockList;
            wear[3].Delete();
            wear[3].ItemId = 2511;
            wear[3].Count = 29;
            v.WearStockList = (IReadOnlyList<VillagerItem>)wear;
        }), "vecino-objeto", "1", "0", "3", "2511", "29");
        ConsolaTests.Both(sav => Villager(sav, 2, v =>
        {
            IReadOnlyList<Item> ftr = v.FtrStockList;
            ftr[0].Delete();
            v.FtrStockList = (IReadOnlyList<VillagerItem>)ftr;
        }), "vecino-objeto", "2", "1", "0", "0xFFFE", "0");
    }

    [AvaloniaFact]
    public void WallAndFloorLikeNhse()
    {
        // RoomWindow.Put: un papel de pared va a la pared y un suelo al suelo.
        ConsolaTests.Both(sav => Villager(sav, 1, v =>
        {
            var w = new RoomWindow(v.Room, "x");
            Assert.True(w.Put(5485));
            v.Room = w.Room;
        }), "vecino-cuarto", "1", "1", "5485");
        ConsolaTests.Both(sav => Villager(sav, 1, v =>
        {
            var w = new RoomWindow(v.Room, "x");
            Assert.True(w.Put(5031));
            v.Room = w.Room;
        }), "vecino-cuarto", "1", "2", "5031");
    }

    [Fact]
    public void AccentWallLikeNhse()
    {
        // La pared de acento, como RoomWindow.Put con la pared de acento elegida.
        ConsolaTests.Both(sav => Villager(sav, 0, v =>
        {
            var room = v.Room;
            var name = room.AccentWallItem;
            name.CopyFrom(new Item(5485));
            room.AccentWallItem = name;
            v.Room = room;
        }), "vecino-cuarto", "0", "0", "5485");
    }

    [Fact]
    public void HouseOutsideLikeNhse()
    {
        // Como VillagerHouseWindow: fachada, tejado y puerta (y lo pedido para mañana), y lo que cuelga en la puerta.
        ConsolaTests.Both(sav =>
        {
            var houses = sav.Main.GetVillagerHouses();
            var h = houses[Array.FindIndex(houses, z => z.NPC1 == 2)];
            h.WallUniqueID = h.OrderWallUniqueID = (WallType)57;
            h.RoofUniqueID = h.OrderRoofUniqueID = (RoofType)20;
            h.DoorUniqueID = h.OrderDoorUniqueID = (DoorKind)158;
            h.DoorDecoItemName = new Item(2511) { Count = 0 };
            sav.Main.SetVillagerHouses(houses);
        }, "vecino-casa", "2", "57", "20", "158", "2511", "0");
    }

    [Fact]
    public void CraftingLikeNhse()
    {
        // Como VillagerDIYTimerEditor: termina a las 15:30:20 haciendo la receta 107; y luego sin hacer nada (255:255).
        ConsolaTests.Both(sav => Villager(sav, 0, v =>
        {
            v.DIYEndHour = 15;
            v.DIYEndMinute = 30;
            v.DIYEndSecond = 20;
            v.DIYRecipeIndex = 107;
        }), "vecino-bricolaje", "0", "15", "30", "20", "107");
        ConsolaTests.Both(sav => Villager(sav, 1, v =>
        {
            v.DIYEndHour = 255;
            v.DIYEndMinute = 255;
        }), "vecino-bricolaje", "1", "255", "255", "0", "1058");
    }

    [Fact]
    public void ExportsTheSameFileAsNhse()
    {
        ConsolaTests.Skip();
        var sav = TestSave.LoadCopy(out var folder);
        var nhv = Temp("vecino.nhv2");
        var nhvh = Temp("casa.nhvh2");
        ConsolaTests.Run(folder, "vecino-exportar", "1", nhv, nhvh);
        Assert.Equal(sav.Main.GetVillagers()[1].Write(), File.ReadAllBytes(nhv));
        var houses = sav.Main.GetVillagerHouses();
        Assert.Equal(houses[Array.FindIndex(houses, z => z.NPC1 == 1)].Write(), File.ReadAllBytes(nhvh));
    }

    /// <summary> Lo que hace VillagersPage.ImportVillager con un archivo (y «sí» a hacerlo de la isla). </summary>
    private static void ImportLikeNhse(HorizonSave sav, int slot, byte[] file, bool adapt)
    {
        var villagers = sav.Main.GetVillagers();
        var v = VillagerTools.Read(sav.Main, file)!;
        var host = sav.Players[0].Personal;
        if (adapt && !v.IsOriginatedFrom(host))
            v = VillagerTools.AdaptToIsland(v, host, sav.Main);
        villagers[slot] = v;
        sav.Main.SetVillagers(villagers);
    }

    [Fact]
    public void ImportsAVillagerLikeNhse()
    {
        ConsolaTests.Skip();
        var file = TestSave.LoadCopy(out _).Main.GetVillagers()[1].Write();
        var path = Temp("vecino.nhv2");
        File.WriteAllBytes(path, file);
        ConsolaTests.Both(sav => ImportLikeNhse(sav, 3, file, true), "vecino-importar", "3", path, "1");
    }

    [Fact]
    public void ImportsAVillagerFromAnotherIslandLikeNhse()
    {
        ConsolaTests.Skip();
        // Un vecino de otra isla (otro número y nombre de isla, y otro jugador): se hace de la isla, o se queda como es.
        var v = (Villager2)TestSave.LoadCopy(out _).Main.GetVillagers()[2];
        var m = v.GetMemory(0);
        var town = m.GetTownIdentity().ToArray();
        var player = m.GetPlayerIdentity().ToArray();
        var data = v.Write();
        var otherTown = (byte[])town.Clone();
        otherTown[0] ^= 0x5A;
        StringUtil.GetBytes("Lejana", 10).CopyTo(otherTown, 4);
        var otherPlayer = (byte[])player.Clone();
        otherPlayer[1] ^= 0x33;
        data.AsSpan().ReplaceOccurrences(town, otherTown);
        data.AsSpan().ReplaceOccurrences(player, otherPlayer);
        var path = Temp("lejana.nhv2");
        File.WriteAllBytes(path, data);

        var (_, after) = ConsolaTests.Both(sav => ImportLikeNhse(sav, 3, data, true), "vecino-importar", "3", path, "1");
        Assert.True(after.Main.GetVillagers()[3].IsOriginatedFrom(after.Players[0].Personal));
        ConsolaTests.Both(sav => ImportLikeNhse(sav, 3, data, false), "vecino-importar", "3", path, "0");
        Assert.Contains("de otra isla 1 (Lejana)", ConsolaTests.Run(CopyFolder(), "vecino-importar", "3", path, "0"));
    }

    private static string CopyFolder()
    {
        TestSave.LoadCopy(out var folder);
        return folder;
    }

    [Fact]
    public void ImportsAnOldVillagerFileLikeNhse()
    {
        ConsolaTests.Skip();
        // Un .nhv de antes de la 1.5 se convierte como hace NHSE.
        var old = VillagerConverter.Convert21(TestSave.LoadCopy(out _).Main.GetVillagers()[0].Write());
        Assert.Equal(Villager1.SIZE, old.Length);
        var path = Temp("viejo.nhv");
        File.WriteAllBytes(path, old);
        ConsolaTests.Both(sav => ImportLikeNhse(sav, 4, old, true), "vecino-importar", "4", path, "1");
    }

    /// <summary> Lo que hace «Importar casa» de VillagerHouseWindow: la casa del archivo, con el mismo vecino viviendo en ella. </summary>
    private static void ImportHouseLikeNhse(HorizonSave sav, int slot, byte[] file)
    {
        var houses = sav.Main.GetVillagerHouses();
        var i = Array.FindIndex(houses, z => z.NPC1 == slot);
        Memory<byte> data = file;
        data = VillagerHouseConverter.GetCompatible(data, sav.Main.Offsets.VillagerHouseSize);
        var h = sav.Main.Offsets.ReadVillagerHouse(data);
        h.NPC1 = houses[i].NPC1;
        houses[i] = h;
        sav.Main.SetVillagerHouses(houses);
    }

    [Fact]
    public void ImportsAHouseLikeNhse()
    {
        ConsolaTests.Skip();
        var src = TestSave.LoadCopy(out _).Main.GetVillagerHouses();
        var file = src[Array.FindIndex(src, z => z.NPC1 == 1)].Write();
        var path = Temp("casa.nhvh2");
        File.WriteAllBytes(path, file);
        ConsolaTests.Both(sav => ImportHouseLikeNhse(sav, 3, file), "casa-importar", "3", path);

        // Y una casa de antes de la 1.5 (.nhvh).
        // (VillagerHouse2.Downgrade de NHSE falla: el principio de la casa es la de antes.)
        var old = src[Array.FindIndex(src, z => z.NPC1 == 0)].Write()[..VillagerHouse1.SIZE];
        var oldPath = Temp("casa.nhvh");
        File.WriteAllBytes(oldPath, old);
        ConsolaTests.Both(sav => ImportHouseLikeNhse(sav, 2, old), "casa-importar", "2", oldPath);
    }
}
