using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Interactivity;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// Lo que la ficha de los vecinos de NHSE para Switch tiene además de la página de vecinos del Mac: los recuerdos
/// (VillagerMemoryEditor), las marcas sueltas (VillagerFlagEditor), su diseño (PatternEditorPRO) y los valores en bruto de
/// su habitación y su casa (la tabla de «Opciones avanzadas» de RoomWindow y VillagerHouseWindow). Cada cambio deja la
/// partida byte a byte como el Mac.
/// </summary>
public class ConsolaVecinosMasTests
{
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

    private static T Find<T>(Window w, string name) where T : Control => w.FindControl<T>(name)!;
    private static void Click(Window w, string button) => Find<Button>(w, button).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));

    /// <summary> Abre el editor de recuerdos del Mac en un vecino, cambia lo que se pida en el primer jugador y guarda. </summary>
    private static void Memory(HorizonSave sav, int slot, Action<VillagerMemoryEditor> change) => Villager(sav, slot, v =>
    {
        var w = new VillagerMemoryEditor(v);
        w.Show();
        Find<ListBox>(w, "LB_Players").SelectedIndex = 0;
        change(w);
        Dispatcher.UIThread.RunJobs(); // TextChanged llega después
        Click(w, "B_Save");
    });

    // ------------------------------------------------------------------------------------------------ recuerdos
    [AvaloniaFact]
    public void GreetingLikeNhse()
    {
        ConsolaTests.Both(sav => Memory(sav, 0, w => Find<TextBox>(w, "TB_Greeting").Text = "¡Hola, vecina guapa!"),
            "vecino-saludo", "0", "0", "0", "¡Hola, vecina guapa!");
        // Uno de los otros diez, más largo que lo que se ve (22 letras): se guarda hasta 47.
        var largo = "Buenos días tengas tú, que hoy hace un sol precioso";
        var (_, after) = ConsolaTests.Both(sav => Memory(sav, 1, w => Find<TextBox>(w, "TB_Greeting3").Text = largo),
            "vecino-saludo", "1", "0", "3", largo);
        Assert.Equal(largo[..22], after.Main.GetVillagers()[1].GetMemory(0).GetGreeting(3));
    }

    [AvaloniaFact]
    public void GreetingDateLikeNhse()
    {
        ConsolaTests.Both(sav => Memory(sav, 2, w => Find<CalendarDatePicker>(w, "CAL_GreetDate").SelectedDate = new DateTime(2024, 5, 17)),
            "vecino-fecha", "2", "0", "2024", "5", "17");
    }

    [AvaloniaFact]
    public void MemoryFlagLikeNhse()
    {
        // Las visitas a casa del jugador (marca 8) a 5, como el número de la lista de marcas.
        ConsolaTests.Both(sav => Memory(sav, 0, w =>
        {
            Find<ListBox>(w, "LB_Counts").SelectedIndex = 8;
            Find<NumericUpDown>(w, "NUD_Count").Value = 5;
        }), "vecino-marca-recuerdo", "0", "0", "8", "5");
    }

    [AvaloniaFact]
    public void MemoryNamesLikeNhse()
    {
        ConsolaTests.Both(sav => Memory(sav, 3, w =>
        {
            Find<TextBox>(w, "TB_NamePlayer").Text = "Carlita";
            Find<TextBox>(w, "TB_Island").Text = "Barrito";
        }), "vecino-recuerdo-nombres", "3", "0", "Carlita", "Barrito");
    }

    [Fact]
    public void ExportsTheSameMemoryAsNhse()
    {
        ConsolaTests.Skip();
        var sav = TestSave.LoadCopy(out var folder);
        var path = Temp("recuerdo.nhvpm");
        ConsolaTests.Run(folder, "vecino-recuerdo-exportar", "1", "0", path);
        Assert.Equal(sav.Main.GetVillagers()[1].GetMemory(0).Data.ToArray(), File.ReadAllBytes(path));
    }

    /// <summary> Lo que hace «Load» del editor de recuerdos (MiscDumpHelper.LoadVillagerMemoryPlayer) y luego «Save». </summary>
    private static void LoadMemoryLikeNhse(HorizonSave sav, int slot, int index, byte[] file, bool adapt) => Villager(sav, slot, v =>
    {
        var memories = v.GetMemories();
        var memory = new GSaveMemory(file.ToArray());
        var old = memories[index];
        if (adapt && !memory.IsOriginatedFrom(old))
            memory.ChangeOrigins(old, memory.Data);
        memories[index] = memory;
        v.SetMemories(memories);
    });

    [Fact]
    public void ImportsAMemoryLikeNhse()
    {
        ConsolaTests.Skip();
        // El recuerdo de Carli de otro vecino, pero de otra isla y otro jugador: se hace del de ahora, o se queda como es.
        var m = TestSave.LoadCopy(out _).Main.GetVillagers()[2].GetMemory(0);
        var data = m.Data.ToArray();
        var town = m.GetTownIdentity().ToArray();
        var player = m.GetPlayerIdentity().ToArray();
        var otherTown = (byte[])town.Clone();
        otherTown[1] ^= 0x21;
        StringUtil.GetBytes("Lejana", 10).CopyTo(otherTown, 4);
        var otherPlayer = (byte[])player.Clone();
        otherPlayer[0] ^= 0x44;
        data.AsSpan().ReplaceOccurrences(town, otherTown);
        data.AsSpan().ReplaceOccurrences(player, otherPlayer);
        data[0x38 + 10] = 99; // otra amistad, para ver que viene del archivo
        var path = Temp("recuerdo.nhvpm");
        File.WriteAllBytes(path, data);

        var (_, after) = ConsolaTests.Both(sav => LoadMemoryLikeNhse(sav, 4, 0, data, true), "vecino-recuerdo-importar", "4", "0", path, "1");
        Assert.Equal(99, after.Main.GetVillagers()[4].GetMemory(0).Friendship);
        Assert.Equal("Barro", after.Main.GetVillagers()[4].GetMemory(0).TownName);
        ConsolaTests.Both(sav => LoadMemoryLikeNhse(sav, 4, 0, data, false), "vecino-recuerdo-importar", "4", "0", path, "0");
    }

    // ------------------------------------------------------------------------------------------------ marcas sueltas
    [AvaloniaFact]
    public void VillagerFlagLikeNhse()
    {
        ConsolaTests.Both(sav => Villager(sav, 1, v =>
        {
            var flags = v.GetEventFlagsSave();
            var w = new VillagerFlagEditor(flags);
            w.Show();
            Find<ListBox>(w, "LB_Counts").SelectedIndex = 41;
            Find<NumericUpDown>(w, "NUD_Count").Value = 3;
            Click(w, "B_Save");
            v.SetEventFlagsSave(flags);
        }), "vecino-marca", "1", "41", "3");
    }

    [Fact]
    public void VillagerFlagsFileLikeNhse()
    {
        ConsolaTests.Skip();
        // Las marcas de un vecino guardadas (.nhfl) y cargadas en otro, como «Dump» y «Load» de VillagerFlagEditor.
        var sav0 = TestSave.LoadCopy(out var folder);
        var path = Temp("marcas.nhfl");
        ConsolaTests.Run(folder, "vecino-marcas-exportar", "0", path);
        var file = File.ReadAllBytes(path);
        var counts = sav0.Main.GetVillagers()[0].GetEventFlagsSave();
        Assert.Equal(System.Runtime.InteropServices.MemoryMarshal.AsBytes(counts.AsSpan()).ToArray(), file);
        ConsolaTests.Both(sav => Villager(sav, 2, v =>
        {
            var flags = v.GetEventFlagsSave();
            Buffer.BlockCopy(file, 0, flags, 0, file.Length);
            v.SetEventFlagsSave(flags);
        }), "vecino-marcas-cargar", "2", path);
    }

    // ------------------------------------------------------------------------------------------------ su diseño
    [AvaloniaFact]
    public async Task DesignFromFileLikeNhse()
    {
        ConsolaTests.Skip();
        // Un diseño PRO de la partida, hecho por el primer jugador, en un .nhpd: lo pone «Importar» de PatternEditorPRO.
        var src = TestSave.LoadCopy(out _);
        var design = new DesignPatternPRO(src.Main.GetDesignsPRO()[1].Data.ToArray());
        design.ChangeOrigins(src.Players[0].Personal, design.Data);
        var path = Temp("diseno.nhpd");
        File.WriteAllBytes(path, design.Data.ToArray());

        var sav = TestSave.LoadCopy(out var folder);
        ConsolaTests.Run(folder, "vecino-diseno", "1", path, "1");
        var after = HorizonSave.FromFolder(folder);
        var villagers = sav.Main.GetVillagers();
        DesignPatternPRO[] tmp = [villagers[1].Design];
        var w = new PatternEditorPRO(tmp, sav.Players[0]);
        w.Show();
        Assert.True(await w.ImportDesign(path));
        w.Close();
        villagers[1].Design = tmp[0];
        sav.Main.SetVillagers(villagers);
        sav.Main.Hash();
        ConsolaTests.SameBytes(sav.Main.Data, after.Main.Data);
    }

    [Fact]
    public void DesignFromAnotherIslandLikeNhse()
    {
        ConsolaTests.Skip();
        // Uno de otra isla: «sí» lo hace del jugador (ChangeOrigins) y deja el uso de un diseño PRO (0xEE01).
        var src = TestSave.LoadCopy(out _);
        var design = new DesignPatternPRO(src.Main.GetDesignsPRO()[2].Data.ToArray());
        var town = design.GetTownIdentity().ToArray();
        var other = (byte[])town.Clone();
        other[2] ^= 0x18;
        design.Data.ReplaceOccurrences(town, other);
        design.UsageCompatibility = 0x1234;
        var path = Temp("lejano.nhpd");
        File.WriteAllBytes(path, design.Data.ToArray());

        foreach (var adapt in new[] { true, false })
        {
            ConsolaTests.Both(sav => Villager(sav, 0, v =>
            {
                var d = new DesignPatternPRO(File.ReadAllBytes(path));
                var host = sav.Players[0].Personal;
                if (adapt && !d.IsOriginatedFrom(host))
                    d.ChangeOrigins(host, d.Data);
                if (d.UsageCompatibility is not (0xEE01 or 0xEE05)) // PatternEditorPRO.UpdateUsageCompatibility
                    d.UsageCompatibility = 0xEE01;
                v.Design = d;
            }), "vecino-diseno", "0", path, adapt ? "1" : "0");
        }
    }

    [Fact]
    public void DesignFromTheSaveLikeNhse() => ConsolaTests.Both(sav => Villager(sav, 3, v => v.Design = sav.Main.GetDesignsPRO()[4]),
        "vecino-diseno-de", "3", "4");

    [Fact]
    public void ExportsTheSameDesignAsNhse()
    {
        ConsolaTests.Skip();
        var sav = TestSave.LoadCopy(out var folder);
        var path = Temp("diseno.nhpd");
        ConsolaTests.Run(folder, "vecino-diseno-exportar", "2", path);
        Assert.Equal(sav.Main.GetVillagers()[2].Design.Data.ToArray(), File.ReadAllBytes(path));
    }

    // ------------------------------------------------------------------------------------------------ valores en bruto
    [Fact]
    public void RoomRawValuesLikeNhse()
    {
        // Como la tabla de RoomWindow (GSaveRoomFloorWall): el diseño de la pared (campo 13) y el parámetro libre del
        // suelo (campo 11), con signo.
        ConsolaTests.Both(sav => Villager(sav, 1, v =>
        {
            var room = v.Room;
            room.WallMyDesignID = 77;
            v.Room = room;
        }), "vecino-bruto", "cuarto", "1", "13", "77");
        ConsolaTests.Both(sav => Villager(sav, 1, v =>
        {
            var room = v.Room;
            var floor = room.FloorItem;
            floor.FreeParam = -5;
            room.FloorItem = floor;
            v.Room = room;
        }), "vecino-bruto", "cuarto", "1", "11", "-5");
    }

    [Fact]
    public void HouseRawValuesLikeNhse()
    {
        // Como la tabla de VillagerHouseWindow (VillagerHouse2): el nivel, el segundo vecino, la cantidad del objeto extra
        // y la marca.
        void House(HorizonSave sav, int slot, Action<VillagerHouse2> change)
        {
            var houses = sav.Main.GetVillagerHouses();
            change((VillagerHouse2)houses[Array.FindIndex(houses, z => z.NPC1 == slot)]);
            sav.Main.SetVillagerHouses(houses);
        }
        ConsolaTests.Both(sav => House(sav, 2, h => h.HouseLevel = 3), "vecino-bruto", "casa", "2", "0", "3");
        ConsolaTests.Both(sav => House(sav, 2, h => h.NPC2 = 4), "vecino-bruto", "casa", "2", "14", "4");
        ConsolaTests.Both(sav => House(sav, 0, h =>
        {
            var extra = h.ExtraItem;
            extra.Count = 9;
            h.ExtraItem = extra;
        }), "vecino-bruto", "casa", "0", "19", "9");
        ConsolaTests.Both(sav => House(sav, 0, h => h.Flag = 200), "vecino-bruto", "casa", "0", "21", "200");
    }
}
