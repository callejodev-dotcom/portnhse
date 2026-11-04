using System.Reflection;
using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The player section of NHSE for Switch (consola/source/core/jugador.cpp): each change leaves the save byte by byte as
/// the player windows of the Mac leave it.
/// </summary>
public class ConsolaJugadorTests
{
    private static string Today => DateTime.Today.ToString("yyyy-MM-dd");

    private static T Private<T>(object o, string name) => (T)o.GetType().GetField(name, BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(o)!;

    private static void Call(object o, string name, params object[] args) => o.GetType().GetMethod(name, BindingFlags.NonPublic | BindingFlags.Instance)!.Invoke(o, args);

    [Fact]
    public void StorageItemLikeNhse()
    {
        // Slot 398 of the storage gets 30 iron nuggets (as the items window of «Storage» leaves it).
        var (nhse, _) = ConsolaTests.Both(sav =>
        {
            var pers = sav.Players[0].Personal;
            var items = pers.ItemChest;
            var unlocked = items.Take((int)Math.Clamp(pers.ItemChestCount, 50, (uint)items.Count)).ToArray();
            unlocked[398].CopyFrom(new Item(2511) { Count = 29 });
            pers.ItemChest = items;
        }, "almacen", "398", "2511", "29");
        Assert.Equal(2511, nhse.Players[0].Personal.ItemChest[398].ItemId);
    }

    [Fact]
    public void StorageUnlockedLikeTheMac()
    {
        ConsolaTests.Skip();
        var sav = TestSave.LoadCopy(out var folder);
        var pers = sav.Players[0].Personal;
        var unlocked = (int)Math.Clamp(pers.ItemChestCount, 50, (uint)pers.ItemChest.Count);
        var used = pers.ItemChest.Take(unlocked).Count(z => !z.IsNone);
        Assert.Contains($"abiertos {unlocked}, usados {used}", ConsolaTests.Run(folder, "almacen-ver"));
    }

    [Fact]
    public void RecycleBinLikeNhse()
    {
        ConsolaTests.Both(sav =>
        {
            var items = sav.Main.RecycleBin;
            items[7].CopyFrom(new Item(Item.DIYRecipe) { Count = 107 });
            sav.Main.RecycleBin = items;
        }, "papelera", "7", "0x16A2", "107");
    }

    [Fact]
    public void HotelTicketsLikeNhse()
    {
        ConsolaTests.Skip();
        var data30 = TestSave.LoadCopy(out _).Players[0].Personal.Data30;
        Assert.SkipUnless(data30 is { IsInitialized30: true }, "the save has no hotel");
        ConsolaTests.Both(sav =>
        {
            var a = sav.Players[0].Personal.Data30!;
            a.HotelTickets = a.HotelTickets with { Value = 1234 };
        }, "hotel", "1234");
    }

    /// <summary> A recipe the player knows and one that it doesn't, by the window of the Mac. </summary>
    private static (ushort Known, ushort Missing) Recipes()
    {
        var book = TestSave.LoadCopy(out _).Players[0].Personal.GetRecipeBook();
        var known = RecipeList.Recipes.Keys.First(z => book.GetIsKnown(z) && !book.GetIsMade(z) && !book.GetIsFavorite(z));
        var missing = RecipeList.Recipes.Keys.First(z => !book.GetIsKnown(z) && z is not RecipeList.BridgeConstructionKit and not RecipeList.CampsiteConstructionKit);
        return (known, missing);
    }

    [AvaloniaFact]
    public void RecipeLearnedLikeNhse()
    {
        ConsolaTests.Skip();
        var (_, missing) = Recipes();
        ConsolaTests.Both(sav =>
        {
            var w = new RecipesWindow(sav.Players[0].Personal);
            w.Shown.First(z => z.Index == missing).Known = true;
            w.SaveAndClose();
        }, "receta", missing.ToString(), "1");
    }

    [AvaloniaFact]
    public void RecipeForgottenLikeNhse()
    {
        ConsolaTests.Skip();
        var (known, _) = Recipes();
        ConsolaTests.Both(sav =>
        {
            var w = new RecipesWindow(sav.Players[0].Personal);
            var row = w.Shown.First(z => z.Index == known);
            row.Made = true; // forgetting it also clears «made»
            row.Known = false;
            w.SaveAndClose();
        }, "receta", known.ToString(), "0");
    }

    [AvaloniaFact]
    public void RecipeMadeAndFavoriteLikeNhse()
    {
        ConsolaTests.Skip();
        var (known, _) = Recipes();
        ConsolaTests.Both(sav =>
        {
            var w = new RecipesWindow(sav.Players[0].Personal);
            w.Shown.First(z => z.Index == known).Favorite = true;
            w.SaveAndClose();
        }, "receta", known.ToString(), "1", "3");
    }

    [AvaloniaFact]
    public void KnownRecipesMadeLikeNhse()
    {
        ConsolaTests.Both(sav =>
        {
            var w = new RecipesWindow(sav.Players[0].Personal);
            Call(w, "MarkAllMade");
            w.SaveAndClose();
        }, "fabricar-sabidas");
    }

    [Fact]
    public void RecipesForgottenLikeNhse()
    {
        ConsolaTests.Both(sav =>
        {
            var b = sav.Players[0].Personal.GetRecipeBook();
            b.ClearAll(); // «Forget all» of the recipes window
            sav.Players[0].Personal.SetRecipeBook(b);
        }, "olvidar-recetas");
    }

    [AvaloniaFact]
    public void CatalogItemsLikeNhse()
    {
        ConsolaTests.Skip();
        var w0 = new CatalogWindow(TestSave.LoadCopy(out _).Players[0]);
        w0.ShowCategory(ItemCategory.All);
        // One with colours that is missing, and one the player has.
        var missing = w0.Shown.First(z => !z.Received && ItemRemakeUtil.GetRemakeIndex(z.Id) >= 0).Id;
        var received = w0.Shown.First(z => z.Received && ItemRemakeUtil.GetRemakeIndex(z.Id) >= 0).Id;
        ConsolaTests.Both(sav =>
        {
            var w = new CatalogWindow(sav.Players[0]);
            w.ShowCategory(ItemCategory.All);
            w.Shown.First(z => z.Id == missing).Received = true;
            w.Shown.First(z => z.Id == received).Received = false;
            w.SaveAndClose();
        }, "recibir", missing.ToString(), "1", received.ToString(), "0");
    }

    [AvaloniaFact]
    public void CatalogCategoryMarkedLikeNhse()
    {
        // «Mark all» of what is shown, with the «Fossils» category (on the Switch, one by one, as ui::marcas does it).
        ConsolaTests.Skip();
        var w0 = new CatalogWindow(TestSave.LoadCopy(out _).Players[0]);
        w0.ShowCategory(ItemCategory.Fossils);
        string[] args = ["recibir", .. w0.Shown.Where(z => !z.Received).SelectMany(z => new[] { z.Id.ToString(), "1" })];
        ConsolaTests.Both(sav =>
        {
            var w = new CatalogWindow(sav.Players[0]);
            w.ShowCategory(ItemCategory.Fossils);
            w.MarkShown(true);
            w.SaveAndClose();
        }, args);
    }

    [AvaloniaFact]
    public void ReactionsOneByOneLikeNhse()
    {
        ConsolaTests.Skip();
        var store = TestSave.LoadCopy(out _).Players[0].Personal.Reactions;
        var known = ReactionsWindow.Valid.Where(z => Array.IndexOf(store.ManpuBit, z) >= 0).ToArray();
        var missing = ReactionsWindow.Valid.Where(z => Array.IndexOf(store.ManpuBit, z) < 0).Take(2).ToArray();
        Assert.True(known.Length > 2);
        // Forget the first one the player knows (the rest move up) and one on the wheel, and learn two new ones.
        var onWheel = store.UIList.First(z => z != Reaction.None);
        var forget = new[] { known[0], onWheel }.Distinct().ToArray();
        var args = new List<string> { "reaccion" };
        foreach (var r in forget)
            args.AddRange([((byte)r).ToString(), "0"]);
        foreach (var r in missing)
            args.AddRange([((byte)r).ToString(), "1"]);
        ConsolaTests.Both(sav =>
        {
            var w = new ReactionsWindow(sav.Players[0].Personal);
            foreach (var r in forget)
                Call(w, "Forget", r);
            foreach (var r in missing)
                Call(w, "Learn", r);
            w.SaveAndClose();
        }, [.. args]);
    }

    [AvaloniaFact]
    public void ReactionWheelLikeNhse()
    {
        ConsolaTests.Skip();
        var store = TestSave.LoadCopy(out _).Players[0].Personal.Reactions;
        var r = ReactionsWindow.Valid.Where(z => Array.IndexOf(store.ManpuBit, z) >= 0).Last();
        ConsolaTests.Both(sav =>
        {
            var w = new ReactionsWindow(sav.Players[0].Personal);
            var wheel = Private<ComboBox[]>(w, "Wheel");
            wheel[5].SelectedItem = wheel[5].Items.Cast<object>().First(z => z.ToString() == ReactionsWindow.GetName(r));
            w.SaveAndClose();
        }, "rueda", "5", ((byte)r).ToString());
    }

    [AvaloniaFact]
    public void AllRecordsCompletedLikeNhse()
    {
        var today = Today;
        ConsolaTests.Both(sav =>
        {
            var w = new AchievementsWindow(sav.Players[0].Personal);
            w.CompleteAllNow();
            w.SaveAndClose();
        }, "records", today);
    }

    [AvaloniaFact]
    public void AllRecordsResetLikeNhse()
    {
        ConsolaTests.Both(sav =>
        {
            var pers = sav.Players[0].Personal;
            var r = pers.Achievements;
            r.ClearAll();
            pers.Achievements = r;
        }, "records", "borrar");
    }

    /// <summary> A record that is not complete: by count (CatchFish) or by steps. </summary>
    private static LifeSupportAchievement Record(bool steps)
    {
        var records = TestSave.LoadCopy(out _).Players[0].Personal.Achievements;
        return LifeSupportAchievement.List.Values.OrderBy(z => z.Index)
            .First(z => (z.GetThresholdValue(0) == 0) == steps && !new RecordRow(records, z).IsComplete && z.AchievementCount > 1);
    }

    [AvaloniaFact]
    public void OneRecordMaxLikeNhse()
    {
        ConsolaTests.Skip();
        foreach (var steps in new[] { false, true })
        {
            var info = Record(steps);
            ConsolaTests.Both(sav =>
            {
                var w = new AchievementsWindow(sav.Players[0].Personal);
                w.Select(info.Index);
                var row = Private<ListBox>(w, "List").SelectedItem!;
                Call(w, "Max", row);
                w.SaveAndClose();
            }, "record", info.Index.ToString(), "max", Today);
        }
    }

    [AvaloniaFact]
    public void OneRecordChangedLikeNhse()
    {
        ConsolaTests.Skip();
        var count = Record(false);
        ConsolaTests.Both(sav =>
        {
            var w = new AchievementsWindow(sav.Players[0].Personal);
            w.Select(count.Index);
            ((RecordRow)Private<ListBox>(w, "List").SelectedItem!).Count = 1234;
            w.SaveAndClose();
        }, "record", count.Index.ToString(), "cuenta", "1234");

        var steps = Record(true);
        ConsolaTests.Both(sav =>
        {
            var w = new AchievementsWindow(sav.Players[0].Personal);
            w.Select(steps.Index);
            var row = (RecordRow)Private<ListBox>(w, "List").SelectedItem!;
            row.SetStep(1, !row.IsReached(1));
            w.SaveAndClose();
        }, "record", steps.Index.ToString(), "paso", "1", new RecordRow(TestSave.LoadCopy(out _).Players[0].Personal.Achievements, steps).IsReached(1) ? "0" : "1");

        ConsolaTests.Both(sav =>
        {
            var pers = sav.Players[0].Personal;
            var r = pers.Achievements;
            r.ClearAll(0); // «Start this record from zero»
            pers.Achievements = r;
        }, "record", "0", "borrar");

        ConsolaTests.Both(sav =>
        {
            var pers = sav.Players[0].Personal;
            var r = pers.Achievements;
            r.Date[0][1] = new DateTime(2021, 2, 28); // the date of a stamp
            pers.Achievements = r;
        }, "record", "0", "fecha", "1", "2021-02-28");
    }

    [AvaloniaFact]
    public void ProfileLikeNhse()
    {
        ConsolaTests.Both(sav =>
        {
            var w = new ProfileWindow(sav.Players[0]);
            w.SetBirthday(2, 29);
            w.SetFruit(2287);
            Private<CalendarDatePicker>(w, "Registered").SelectedDate = new DateTime(2020, 3, 20);
            Private<CheckBox>(w, "Founder").IsChecked = false;
            w.SaveAndClose();
        }, "perfil", "2", "29", "2287", "2020-03-20", "0");
    }

    /// <summary> The mailbox changed on the Switch and with the mail window of the Mac: the same postbox.dat. </summary>
    private static void MailBoth(Action<MailWindow, PostBox> mac, params string[] args)
    {
        ConsolaTests.Skip();
        var previous = GameInfo.CurrentLanguage;
        try
        {
            App.SetLanguage(GameLanguage.GetLanguageIndex("es"));
            var sav = TestSave.LoadCopy(out var folder);
            ConsolaTests.Run(folder, ["buzon", .. args]);
            var after = HorizonSave.FromFolder(folder);
            Assert.Empty(after.GetInvalidHashes());
            var w = new MailWindow(sav, sav.Players[0]);
            mac(w, sav.Players[0].PostBox);
            w.SaveAndClose();
            sav.Players[0].PostBox.Hash();
            ConsolaTests.SameBytes(sav.Players[0].PostBox.Data, after.Players[0].PostBox.Data);
            ConsolaTests.SameBytes(sav.Players[0].Personal.Data, after.Players[0].Personal.Data);
        }
        finally
        {
            App.SetLanguage(GameLanguage.GetLanguageIndex(previous));
        }
    }

    [AvaloniaFact]
    public void MailListedLikeTheMac()
    {
        ConsolaTests.Skip();
        var previous = GameInfo.CurrentLanguage;
        try
        {
            App.SetLanguage(GameLanguage.GetLanguageIndex("es"));
            var sav = TestSave.LoadCopy(out var folder);
            var w = new MailWindow(sav, sav.Players[0]);
            var expected = w.Letters.Select(z => $"carta {z.Slot}: {z.Sender},").ToArray();
            var output = ConsolaTests.Run(folder, "buzon", "lista").Split('\n').Where(z => z.StartsWith("carta ", StringComparison.Ordinal))
                .Select(z => z[..(z.IndexOf(',') + 1)]).ToArray();
            Assert.Equal(expected, output);
        }
        finally
        {
            App.SetLanguage(GameLanguage.GetLanguageIndex(previous));
        }
    }

    [AvaloniaFact]
    public void NewLetterLikeNhse()
    {
        // A new letter with a message, a gift and another sender; read and favourite.
        MailBoth((w, box) =>
        {
            var slot = w.WriteLetter();
            box.SetLetterMessageText(slot, "¡Hola!\nQué tal.");
            box.SetLetterToLineText(slot, "Querida Carla:");
            box.SetLetterFromLineText(slot, "Tom");
            w.SetGift(slot, 2511, 29);
            w.SetSender(slot, 2, 22);
            box.SetMailReadStatus(slot, true);
            box.SetMailFavStatus(slot, true);
        }, "nueva", Today, "texto", "n", "1", "¡Hola!\\nQué tal.", "texto", "n", "0", "Querida Carla:", "texto", "n", "2", "Tom",
            "regalo", "n", "2511", "29", "remitente", "n", "2", "22", "0", "leida", "n", "1", "favorita", "n", "1");
    }

    [AvaloniaFact]
    public void LetterChangedLikeNhse()
    {
        ConsolaTests.Skip();
        var box0 = TestSave.LoadCopy(out _).Players[0].PostBox;
        var slot = box0.GetUsedPostBoxSlots()[0];
        var s = slot.ToString();
        MailBoth((w, box) =>
        {
            w.SetSender(slot, 0); // from the player
            w.SetGift(slot, Item.NONE, 0);
            box.SetMailType(slot, false); // a parcel (as the «kind» list does)
            box.SetItemWrappingType(slot, 0);
            box.SetMailStationerySkin(slot, 0);
            box.SetItemWrappingType(slot, 0x6B);
            box.SetTimeStamp(slot, new DateTime(2022, 12, 24));
            box.SetReceiverPlayerName(slot, "Otra");
            box.SetReceiverIslandName(slot, "Isla");
        }, "remitente", s, "0", "0", "0", "regalo", s, "0xFFFE", "0", "carta", s, "0", "envoltorio", s, "0x6B", "fecha", s, "2022-12-24",
            "texto", s, "3", "Otra", "texto", s, "4", "Isla");
    }

    [AvaloniaFact]
    public void LetterPaperAndVillagerLikeNhse()
    {
        ConsolaTests.Skip();
        var slot = TestSave.LoadCopy(out _).Players[0].PostBox.GetUsedPostBoxSlots()[1];
        var s = slot.ToString();
        MailBoth((w, box) =>
        {
            box.SetMailType(slot, true);
            box.SetItemWrappingType(slot, 0);
            box.SetMailStationerySkin(slot, 372);
            box.SetItemWrappingType(slot, 0x15);
            w.SetSender(slot, 1, (int)VillagerSpecies.cat, 4); // a villager
        }, "carta", s, "1", "papel", s, "372", "envoltorio", s, "0x15", "remitente", s, "1", ((int)VillagerSpecies.cat).ToString(), "4");
    }

    [AvaloniaFact]
    public void LetterDeletedAndImportedLikeNhse()
    {
        ConsolaTests.Skip();
        var box0 = TestSave.LoadCopy(out _).Players[0].PostBox;
        var slots = box0.GetUsedPostBoxSlots();
        var file = Path.Combine(TestSave.SavesFolder, $"carta-{Guid.NewGuid():N}.nhpb");
        Directory.CreateDirectory(TestSave.SavesFolder);
        File.WriteAllBytes(file, box0.GetIndividualLetter(slots[2]).ToArray());
        MailBoth((w, box) =>
        {
            w.DeleteLetter(slots[0]);
            // As «Import a letter…»: in the first free slot, with the next position.
            var free = Enumerable.Range(0, Mail.MaxSlots + 1).First(i => !box.GetMailCheck(i));
            box.SetIndividualLetter(free, File.ReadAllBytes(file));
            var next = box.GetLatestUniqueId();
            box.SetPostBoxSortIndex(free, (byte)next);
            box.SetLatestUniqueId(next + 1);
        }, "borrar", slots[0].ToString(), "importar", file);
    }
}
