using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Input;
using Avalonia.Interactivity;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// Player editors (records, received items, misc, reactions, recipes, post box): screenshots, and a change saved through the same code path as the buttons.
/// </summary>
public class GalleryPlayer
{
    private static HorizonSave Save()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        return TestSave.LoadCopy(out _);
    }

    private static T Find<T>(Window w, string name) where T : Control => w.FindControl<T>(name) ?? throw new InvalidOperationException(name);

    private static void Click(Window w, string name) => Find<Button>(w, name).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));

    [AvaloniaFact]
    public void Achievements()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var w = new AchievementEditor(pers);
        Gallery.Capture(w, "10-achievements", false);

        Find<ListBox>(w, "LB_Counts").SelectedIndex = 1;
        Find<NumericUpDown>(w, "NUD_Count").Value = 1234;
        Click(w, "B_Save");
        Assert.Equal(1234u, pers.Achievements.Counts[1]);
    }

    [AvaloniaFact]
    public void ItemsReceived()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var w = new ItemReceivedEditor(sav.Players[0]);
        Gallery.Capture(w, "11-items-received", false);

        // "Give All Fish" from the Give All menu, then save. Alt (held in an earlier test's click) would take them away instead.
        UiUtil.ModifierKeys = KeyModifiers.None;
        var fish = Find<Button>(w, "B_GiveAll").Flyout is MenuFlyout f ? f.Items.OfType<MenuItem>().First(z => z.Name == "B_AllFish") : null;
        Assert.NotNull(fish);
        fish!.RaiseEvent(new RoutedEventArgs(MenuItem.ClickEvent));
        Click(w, "B_Save");
        foreach (var id in GameLists.Fish)
            Assert.True(FlagUtil.GetFlag(pers.Data, pers.Offsets.ItemCollectBit, id));
    }

    [AvaloniaFact]
    public void MiscPlayer()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var w = new MiscPlayerEditor(sav.Players[0], sav.Main);
        var tabs = Find<TabControl>(w, "tabControl1");
        Gallery.Capture(w, "12-misc-player-0", false);
        tabs.SelectedIndex = 1;
        Gallery.Capture(w, "12-misc-player-1", false);

        Find<NumericUpDown>(w, "NUD_BirthMonth").Value = 2;
        Find<NumericUpDown>(w, "NUD_BirthDay").Value = 29;
        Click(w, "B_Save");
        Assert.Equal(2, pers.Birthday.Month);
        Assert.Equal(29, pers.Birthday.Day);
        Assert.Equal(29, pers.ProfileBirthday.Day);
    }

    [AvaloniaFact]
    public void Reactions()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var w = new ReactionEditor(pers);
        Gallery.Capture(w, "13-reactions", false);

        Click(w, "B_GiveAll");
        Click(w, "B_Save");
        var known = pers.Reactions.ManpuBit;
        Assert.Contains(Reaction.Happiness, known);
        Assert.Contains(Reaction.Surprise, known);
    }

    [AvaloniaFact]
    public void Recipes()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var w = new RecipeListEditor(sav.Players[0]);
        Gallery.Capture(w, "14-recipes", false);

        Click(w, "B_All");
        Click(w, "B_Save");
        var book = pers.GetRecipeBook();
        foreach (var index in RecipeList.Recipes.Keys)
        {
            if (index is not (RecipeList.BridgeConstructionKit or RecipeList.CampsiteConstructionKit)) // never given
                Assert.True(book.GetIsKnown(index));
        }
    }

    [AvaloniaFact]
    public void PostBox()
    {
        var sav = Save();
        var player = sav.Players[0];
        var pb = player.PostBox;
        var w = new PostBoxEditor(player, pb);
        var tabs = Find<TabControl>(w, "TC_Post_SR");
        Gallery.Capture(w, "15-postbox-0", false);
        tabs.SelectedIndex = 1;
        Gallery.Capture(w, "15-postbox-1", false);

        // Edit the From line of the first used slot and save that slot.
        var slots = pb.GetUsedPostBoxSlots();
        Assert.SkipWhen(slots.Count == 0, "The test save has no mail");
        var slot = slots[0];
        Find<NumericUpDown>(w, "NUD_Post_Slots").Value = slot;
        Find<TextBox>(w, "TB_Post_FromLine").Text = "NHSE";
        Click(w, "B_Save");
        Assert.Equal("NHSE", pb.GetLetterFromLineText(slot));
        w.Close();
    }
}
