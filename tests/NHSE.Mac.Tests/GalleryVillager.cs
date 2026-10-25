using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Interactivity;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// Flag editors and villager subforms: screenshots (galeria/2x-*.png) and the save paths of each window.
/// </summary>
public class GalleryVillager
{
    private static HorizonSave Save()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        return TestSave.LoadCopy(out _);
    }

    private static T Find<T>(Control parent, string name) where T : Control
        => parent.FindControl<T>(name) ?? throw new InvalidOperationException($"{name} not found");

    private static void Click(Control parent, string name)
    {
        Find<Button>(parent, name).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        Dispatcher.UIThread.RunJobs();
    }

    /// <summary>
    /// Opens the window as a dialog, like the editors do, so the Save button's result can be checked.
    /// </summary>
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

    [AvaloniaFact]
    public void PlayerFlags()
    {
        var pers = Save().Players[0].Personal;
        var flags = pers.GetEventFlagsPlayer();
        var w = new FlagEditor(flags);
        var dialog = ShowDialog(w);

        var lb = Find<ListBox>(w, "LB_Counts");
        Assert.Equal(flags.Length, lb.ItemCount);
        lb.SelectedIndex = 42;
        Find<NumericUpDown>(w, "NUD_Count").Value = 1234;
        Assert.Equal(1234, flags[42]);
        Assert.EndsWith("= 1234", lb.SelectedItem!.ToString());
        Assert.Equal(42, lb.SelectedIndex); // the row text changed, the selection stays

        Gallery.Capture(w, "20-flags", false);
        // Thousands of flags, but only the visible rows have a container.
        Assert.True(lb.GetRealizedContainers().Count() < 100);

        Click(w, "B_Save");
        Assert.True(Saved(dialog));
        pers.SetEventFlagsPlayer(flags);
        Assert.Equal(1234, pers.GetEventFlagsPlayer()[42]);
    }

    [AvaloniaFact]
    public void LandFlags()
    {
        var main = Save().Main;
        var edit = main.EventFlagLand.ToArray();
        var w = new LandFlagEditor(edit);
        var dialog = ShowDialog(w);

        Find<ListBox>(w, "LB_Counts").SelectedIndex = 7;
        Find<NumericUpDown>(w, "NUD_Count").Value = -5;
        Gallery.Capture(w, "21-landflags", false);

        Click(w, "B_Save");
        Assert.True(Saved(dialog));
        edit.CopyTo(main.EventFlagLand);
        Assert.Equal(-5, main.EventFlagLand[7]);
    }

    [AvaloniaFact]
    public void VillagerFlags()
    {
        var v = Save().Main.GetVillagers()[0];
        var flags = v.GetEventFlagsSave();
        var w = new VillagerFlagEditor(flags);
        var dialog = ShowDialog(w);

        Find<ListBox>(w, "LB_Counts").SelectedIndex = 3;
        Find<NumericUpDown>(w, "NUD_Count").Value = 40000; // ushort: above the short range of the other flag lists
        Gallery.Capture(w, "22-villagerflags", false);

        Click(w, "B_Cancel");
        Assert.False(Saved(dialog));
        Assert.Equal(40000, flags[3]);
    }

    [AvaloniaFact]
    public void DIYTimer()
    {
        var villagers = Save().Main.GetVillagers();
        var v = villagers[0];
        v.DIYEndHour = v.DIYEndMinute = byte.MaxValue; // idle

        // Saving an idle villager keeps it idle.
        var idle = new VillagerDIYTimerEditor(v);
        var dialog = ShowDialog(idle);
        Assert.False(Find<CheckBox>(idle, "CHK_Crafting").IsChecked);
        Click(idle, "B_Save");
        Assert.True(Saved(dialog));
        Assert.Equal(byte.MaxValue, v.DIYEndHour);
        Assert.Equal(byte.MaxValue, v.DIYEndMinute);

        var w = new VillagerDIYTimerEditor(v);
        dialog = ShowDialog(w);
        Find<CheckBox>(w, "CHK_Crafting").IsChecked = true;
        Find<TimePicker>(w, "DT_Time").SelectedTime = new TimeSpan(13, 45, 10);
        var recipes = Find<ComboBox>(w, "CB_Recipe");
        recipes.SelectedIndex = 5;
        var recipe = ((ComboItem)recipes.SelectedItem!).Value;
        Gallery.Capture(w, "23-diytimer", false);

        Click(w, "B_Save");
        Assert.True(Saved(dialog));
        Assert.Equal(13, v.DIYEndHour);
        Assert.Equal(45, v.DIYEndMinute);
        Assert.Equal(10, v.DIYEndSecond);
        Assert.Equal(recipe, v.DIYRecipeIndex);
    }

    [AvaloniaFact]
    public void VillagerMemories()
    {
        var v = Save().Main.GetVillagers()[0];
        var w = new VillagerMemoryEditor(v);
        var dialog = ShowDialog(w);

        var players = Find<ListBox>(w, "LB_Players");
        Assert.Equal(0, players.SelectedIndex);
        Find<TextBox>(w, "TB_NickName").Text = "Pal";
        Find<ListBox>(w, "LB_Counts").SelectedIndex = 10;
        Find<NumericUpDown>(w, "NUD_Count").Value = 77;

        // Another player and back: the edits of the first one are kept.
        players.SelectedIndex = 1;
        Assert.NotEqual(77, Find<NumericUpDown>(w, "NUD_Count").Value);
        players.SelectedIndex = 0;
        Assert.Equal("Pal", Find<TextBox>(w, "TB_NickName").Text);
        Assert.Equal(77, Find<NumericUpDown>(w, "NUD_Count").Value);

        var tabs = Find<TabControl>(w, "TC_Memory");
        for (int i = 0; i < tabs.ItemCount; i++)
        {
            tabs.SelectedIndex = i;
            Gallery.Capture(w, $"24-memory-{i}", false);
        }

        Click(w, "B_Save");
        Assert.True(Saved(dialog));
        var memory = v.GetMemories()[0];
        Assert.Equal("Pal", memory.NickName);
        Assert.Equal(77, memory.GetEventFlags()[10]);
    }

    [AvaloniaFact]
    public void Room()
    {
        var v = Save().Main.GetVillagers()[0];
        var w = new SaveRoomFloorWallEditor(v.Room);
        var dialog = ShowDialog(w);
        Gallery.Capture(w, "25-room", false);

        // The wall button opens the item editor on the wall item.
        Click(w, "B_Wall");
        var item = w.OwnedWindows.OfType<SingleItemEditor>().Single();
        Assert.Equal(v.Room.WallItem.ToItem().ItemId, item.Item.ItemId);
        Find<NumericUpDown>(Find<ItemEditor>(item, "ItemEditor"), "NUD_Count").Value = 3;
        Click(item, "B_Save");
        Assert.Equal(3, w.Entity.WallItem.ToItem().Count);

        Find<NumericUpDown>(w, "NUD_DesignWall").Value = 123;
        Find<NumericUpDown>(w, "NUD_DirFloor").Value = 2;
        Click(w, "B_Save");
        Assert.True(Saved(dialog));
        v.Room = w.Entity;
        Assert.Equal(123, v.Room.WallMyDesignID);
        Assert.Equal(2, v.Room.FloorDirection);
        Assert.Equal(3, v.Room.WallItem.ToItem().Count);
    }

    [AvaloniaFact]
    public void SingleItem()
    {
        var v = Save().Main.GetVillagers()[0];
        var original = v.Room.FloorItem.ToItem();
        var uses = original.UseCount;
        var newUses = (ushort)(uses == 9 ? 8 : 9);
        var w = new SingleItemEditor(original);
        var dialog = ShowDialog(w);
        Gallery.Capture(w, "26-singleitem", false);

        Find<NumericUpDown>(Find<ItemEditor>(w, "ItemEditor"), "NUD_Uses").Value = newUses;
        Click(w, "B_Save");
        Assert.True(Saved(dialog));
        Assert.Equal(newUses, w.Item.UseCount);
        Assert.Equal(uses, original.UseCount); // the editor works on a copy

        // Flowers show the genes: the window grows to fit them.
        var flower = (ushort)Enumerable.Range(0, ushort.MaxValue).First(i => ItemInfo.GetItemKind((ushort)i).IsFlowerGene((ushort)i));
        Gallery.Capture(new SingleItemEditor(new Item(flower)), "26-singleitem-flower");
    }
}
