using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The friendly villager page and its windows (friendship, walls and floor): captures (galeria/6x-*.png) and what
/// each one writes to a copy of the test save.
/// </summary>
public class GalleryHomeVillagers
{
    private static HorizonSave Save()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        return TestSave.LoadCopy(out _);
    }

    private static Task<bool> ShowDialog(Window w)
    {
        var owner = new Window { Width = 200, Height = 200 };
        owner.Show();
        var result = w.ShowDialog<bool>(owner);
        Dispatcher.UIThread.RunJobs();
        return result;
    }

    private static bool Saved(Task<bool> dialog)
    {
        Dispatcher.UIThread.RunJobs();
        Assert.True(dialog.IsCompletedSuccessfully);
        return dialog.Result;
    }

    private static void Click(Button b)
    {
        b.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        Dispatcher.UIThread.RunJobs();
    }

    private static Button SaveButton(Window w) => w.GetVisualDescendants().OfType<Button>().Single(b => b.IsDefault);
    private static Button CancelButton(Window w) => w.GetVisualDescendants().OfType<Button>().Single(b => b.IsCancel);

    /// <summary>
    /// The page in a window of its own, with the context the home window gives it.
    /// </summary>
    private static (VillagersPage Page, HorizonSave Sav) Page()
    {
        var sav = Save();
        var w = new Window { Width = 1000, Height = 900 };
        var page = new VillagersPage(new HomeContext(sav, w));
        w.Content = page;
        w.Show();
        Dispatcher.UIThread.RunJobs();
        return (page, sav);
    }

    #region Captures
    [AvaloniaFact]
    public void VillagersPageCapture()
    {
        var w = new HomeWindow(Save()) { Height = 900 };
        w.Show();
        var nav = w.FindControl<ListBox>("LB_Nav")!;
        for (int i = 0; i < nav.ItemCount && !w.GetVisualDescendants().OfType<VillagersPage>().Any(); i++)
        {
            nav.SelectedIndex = i;
            Dispatcher.UIThread.RunJobs();
        }
        var page = w.GetVisualDescendants().OfType<VillagersPage>().Single();

        // The villager who is crafting, so the page shows it
        var crafting = page.Shown.ToList().FindIndex(VillagerTools.IsCrafting);
        page.SelectedIndex = Math.Max(0, crafting);
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "60-villagers-page", false);

        page.SelectedIndex = page.Shown.ToList().FindIndex(VillagerTools.IsEmpty) is var empty and >= 0 ? empty : 0;
        Dispatcher.UIThread.RunJobs();
        Gallery.Capture(w, "60-villagers-page-empty", false);
        w.Close();
    }

    [AvaloniaFact]
    public void FriendshipCapture()
    {
        var sav = Save();
        var w = new FriendshipWindow(sav.Main.GetVillagers()[0], sav.Players[0].Personal);
        Gallery.Capture(w, "61-friendship", false);
        w.GetVisualDescendants().OfType<Expander>().Single().IsExpanded = true;
        Gallery.Capture(w, "61-friendship-advanced");
    }

    [AvaloniaFact]
    public void RoomCapture()
    {
        var v = Save().Main.GetVillagers()[0];
        var w = new RoomWindow(v.Room, GameInfo.Strings.GetVillager(v.InternalName));
        Gallery.Capture(w, "62-room", false);

        // The floor, and a search: only floorings are listed
        Click(w.GetVisualDescendants().OfType<Button>().Where(b => b.Classes.Contains("card")).ElementAt(1));
        w.GetVisualDescendants().OfType<TextBox>().Single().Text = "madera";
        Gallery.Capture(w, "62-room-floor", false);

        w.GetVisualDescendants().OfType<Expander>().Single().IsExpanded = true;
        Dispatcher.UIThread.RunJobs();
        w.GetVisualDescendants().OfType<CheckBox>().Single().IsChecked = true;
        Gallery.Capture(w, "62-room-advanced");
    }
    #endregion

    #region Friendship
    [AvaloniaFact]
    public void FriendshipLevels()
    {
        Assert.Equal(0, FriendshipWindow.GetLevel(0));
        Assert.Equal(0, FriendshipWindow.GetLevel(24));
        Assert.Equal(1, FriendshipWindow.GetLevel(25));
        Assert.Equal(2, FriendshipWindow.GetLevel(69));
        Assert.Equal(3, FriendshipWindow.GetLevel(70));
        Assert.Equal(6, FriendshipWindow.GetLevel(199));
        Assert.Equal(7, FriendshipWindow.GetLevel(200));
        Assert.Equal(7, FriendshipWindow.GetLevel(255));
    }

    [AvaloniaFact]
    public void FriendshipWindowWritesTheMemory()
    {
        var sav = Save();
        var v = sav.Main.GetVillagers()[0];
        var before = v.GetMemory(0).Friendship;

        // Cancel keeps everything as it was
        var w = new FriendshipWindow(v, sav.Players[0].Personal);
        var dialog = ShowDialog(w);
        w.GetVisualDescendants().OfType<Slider>().First().Value = before == 10 ? 11 : 10;
        Click(CancelButton(w));
        Assert.False(Saved(dialog));
        Assert.Equal(before, v.GetMemory(0).Friendship);

        // The bar, the nickname and a click on the fourth heart
        w = new FriendshipWindow(v, sav.Players[0].Personal);
        dialog = ShowDialog(w);
        w.GetVisualDescendants().OfType<Slider>().First().Value = 77;
        w.GetVisualDescendants().OfType<TextBox>().First().Text = "Pal";
        Click(SaveButton(w));
        Assert.True(Saved(dialog));
        Assert.Equal(77, v.GetMemory(0).Friendship);
        Assert.Equal("Pal", v.GetMemory(0).NickName);

        w = new FriendshipWindow(v, sav.Players[0].Personal);
        dialog = ShowDialog(w);
        var hearts = w.GetVisualDescendants().OfType<Button>().Where(b => b.Content is Image).ToArray();
        Assert.Equal(FriendshipWindow.MaxLevel, hearts.Length);
        Click(hearts[3]);
        Click(SaveButton(w));
        Assert.True(Saved(dialog));
        Assert.Equal(100, v.GetMemory(0).Friendship);
        Assert.Equal(4, FriendshipWindow.GetLevel(v.GetMemory(0).Friendship));

        // Best friends with everyone
        w = new FriendshipWindow(v, sav.Players[0].Personal);
        dialog = ShowDialog(w);
        w.SetAll(byte.MaxValue);
        Click(SaveButton(w));
        Assert.True(Saved(dialog));
        Assert.All(v.GetMemories().Where(m => m.PlayerName.Length != 0), m => Assert.Equal(byte.MaxValue, m.Friendship));
        Assert.All(v.GetMemories().Where(m => m.PlayerName.Length == 0), m => Assert.Equal(0, m.Friendship));
    }

    [AvaloniaFact]
    public void BestFriendsWithEveryVillager()
    {
        var (page, sav) = Page();
        page.BestFriendsEveryVillager();
        foreach (var v in sav.Main.GetVillagers().Where(z => !VillagerTools.IsEmpty(z)))
            Assert.All(v.GetMemories().Where(m => m.PlayerName.Length != 0), m => Assert.Equal(byte.MaxValue, m.Friendship));
    }
    #endregion

    #region Walls and floor
    private static ushort AnyOf(ItemKind kind, ushort except = Item.NONE) => GameInfo.Strings.ItemDataSource
        .Select(z => (ushort)z.Value)
        .First(id => id != except && ItemInfo.GetItemKind(id) == kind);

    [AvaloniaFact]
    public void RoomWindowPutsWallAndFloor()
    {
        var sav = Save();
        var villagers = sav.Main.GetVillagers();
        var v = villagers[0];
        var original = v.Room;
        var wall = AnyOf(ItemKind.Kind_RoomWall, original.WallItem.UniqueID);
        var floor = AnyOf(ItemKind.Kind_RoomFloor, original.FloorItem.UniqueID);

        var w = new RoomWindow(v.Room, "test");
        var dialog = ShowDialog(w);
        Assert.True(w.Put(wall));
        Assert.True(w.Put(floor));
        Assert.False(w.Put(AnyOf(ItemKind.Kind_Rug))); // not a floor: ignored

        // Clicking one in the list puts it: the floor is chosen now, so the list has only floorings
        var list = w.GetVisualDescendants().OfType<ListBox>().Single();
        Assert.NotEmpty(list.Items);
        Assert.All(list.Items, e => Assert.Contains("Kind = Kind_RoomFloor", e!.ToString()));
        Click(w.GetVisualDescendants().OfType<Button>().Where(b => b.Classes.Contains("card")).First());
        Assert.All(list.Items, e => Assert.Contains("Kind = Kind_RoomWall", e!.ToString()));
        Click(SaveButton(w));
        Assert.True(Saved(dialog));

        v.Room = w.Room;
        sav.Main.SetVillagers(villagers);
        var room = sav.Main.GetVillagers()[0].Room;
        Assert.Equal(wall, room.WallItem.UniqueID);
        Assert.Equal(floor, room.FloorItem.UniqueID);
        Assert.Equal(original.AccentWallItem.UniqueID, room.AccentWallItem.UniqueID);
        Assert.Equal(original.FloorDirection, room.FloorDirection);
        Assert.Equal(original.WallMyDesignID, room.WallMyDesignID);
    }
    #endregion

    #region Moving out, crafting and villager files
    [AvaloniaFact]
    public void EveryoneMovesOut()
    {
        var (page, sav) = Page();
        var count = page.EveryoneMovesOut();
        Gallery.Capture((Window)TopLevel.GetTopLevel(page)!, "63-villagers-moving-out");
        var living = sav.Main.GetVillagers().Where(z => !VillagerTools.IsEmpty(z)).ToArray();
        Assert.Equal(living.Length, count);
        Assert.All(living, v =>
        {
            Assert.True(v.MovingOut);
            Assert.Equal(1, v.GetEventFlagsSave()[VillagerTools.ForceMoveOutFlag]);
        });

        // And back
        var first = living[0];
        VillagerTools.SetMovingOut(first, false);
        Assert.False(first.MovingOut);
        Assert.Equal(0, first.GetEventFlagsSave()[VillagerTools.ForceMoveOutFlag]);
    }

    [AvaloniaFact]
    public void CraftingText()
    {
        var v = Save().Main.GetVillagers()[0];
        v.DIYEndHour = v.DIYEndMinute = byte.MaxValue;
        Assert.Null(VillagerTools.GetCraftingText(v));

        var recipe = RecipeList.Recipes.First();
        v.DIYEndHour = 13;
        v.DIYEndMinute = 5;
        v.DIYRecipeIndex = recipe.Key;
        var text = VillagerTools.GetCraftingText(v);
        Assert.NotNull(text);
        Assert.Contains("13:05", text);
        Assert.Contains(GameInfo.Strings.GetItemName(recipe.Value), text);
    }

    [AvaloniaFact]
    public async Task ExportThenImport()
    {
        var (page, sav) = Page();
        var source = sav.Main.GetVillagers()[0];
        Directory.CreateDirectory(TestSave.SavesFolder);
        var path = Path.Combine(TestSave.SavesFolder, $"{Guid.NewGuid():N}.{source.Extension}");

        page.SelectedIndex = 0;
        await page.ExportVillager(path);
        Assert.Equal(source.Write(), await File.ReadAllBytesAsync(path));

        // Into the second house: same island, so no question is asked
        page.SelectedIndex = 1;
        Assert.True(await page.ImportVillager(path));
        var imported = sav.Main.GetVillagers()[1];
        Assert.Equal(source.InternalName, imported.InternalName);
        Assert.Equal(source.Write(), imported.Write());

        Assert.Null(VillagerTools.Read(sav.Main, new byte[100]));
    }

    [AvaloniaFact]
    public void AdaptToIsland()
    {
        var sav = Save();
        var host = sav.Players[0].Personal;
        var v = sav.Main.GetVillagers()[0];

        // Pretend the villager comes from another island
        var memory = v.GetMemory(0);
        memory.TownID ^= 0x5A5A5A5A;
        memory.TownName = "Otra";
        v.SetMemory(memory, 0);
        Assert.False(v.IsOriginatedFrom(host));

        var adapted = VillagerTools.AdaptToIsland(v, host, sav.Main);
        Assert.True(adapted.IsOriginatedFrom(host));
        Assert.Equal(host.TownName, adapted.TownName);
        Assert.Equal(v.InternalName, adapted.InternalName);
    }
    #endregion
}
