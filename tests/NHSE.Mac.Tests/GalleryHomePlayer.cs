using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The friendly player windows opened from the home page (80-…): a capture of each one, and changes saved
/// through the same code as the «Save» button, checked in the save data.
/// </summary>
public class GalleryHomePlayer
{
    private static HorizonSave Save()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        return TestSave.LoadCopy(out _);
    }

    private static void Show(PlayerDialog w)
    {
        w.Show();
        Dispatcher.UIThread.RunJobs();
    }

    [AvaloniaFact]
    public void Recipes()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var w = new RecipesWindow(pers);
        Gallery.Capture(w, "80-recipes", false);

        // The search ignores case and accents: "arbol" finds "árbol".
        var accented = w.Shown.FirstOrDefault(z => ItemSearchBox.Normalize(z.Name) != z.Name.ToLowerInvariant()) ?? w.Shown[0];
        var typed = ItemSearchBox.Normalize(accented.Name).ToUpperInvariant();
        w.Search(typed);
        Assert.Contains(accented, w.Shown);
        Assert.All(w.Shown, r => Assert.True(PlayerUi.Matches(r.Key, PlayerUi.Words(typed))));
        w.Search(GameInfo.CurrentLanguage == "es" ? "arbol" : "tree");
        Assert.NotEmpty(w.Shown);
        Gallery.Capture(w, "80-recipes-search", false);

        w.LearnAll();
        w.SaveAndClose();
        var book = pers.GetRecipeBook();
        foreach (var index in RecipeList.Recipes.Keys.Where(z => !RecipeRow.IsStoryRecipe(z)))
            Assert.True(book.GetIsKnown(index), $"recipe {index}");
        var (known, total) = RecipesWindow.Count(book);
        Assert.Equal(total, known);
    }

    [AvaloniaFact]
    public void RecipesCancelKeepsTheSave()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var before = RecipesWindow.Count(pers.GetRecipeBook());
        var w = new RecipesWindow(pers);
        Show(w);
        w.LearnAll();
        w.Close();
        Assert.Equal(before, RecipesWindow.Count(pers.GetRecipeBook()));
    }

    [AvaloniaFact]
    public void Reactions()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var w = new ReactionsWindow(pers);
        Gallery.Capture(w, "81-reactions", false);

        w.LearnAll();
        w.SaveAndClose();
        var learned = pers.Reactions.ManpuBit;
        foreach (var r in ReactionsWindow.Valid)
            Assert.Contains(r, learned);
    }

    [AvaloniaFact]
    public void Records()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var w = new AchievementsWindow(pers);
        w.Select(0x00); // fish caught
        Gallery.Capture(w, "82-records", false);
        w.Select(0x07); // kinds of fruit planted: one stamp per fruit
        Gallery.Capture(w, "82-records-steps", false);

        w.CompleteAllNow();
        w.SaveAndClose();
        var records = pers.Achievements;
        foreach (var (index, info) in LifeSupportAchievement.List)
        {
            for (int i = 0; i < info.AchievementCount; i++)
                Assert.True(info.GetIsSatisfied(i, records.Counts[index]), $"record {index} stamp {i}");
        }
    }

    [AvaloniaFact]
    public void RecordNames()
    {
        // Every record has a friendly name (not the internal one).
        foreach (var info in LifeSupportAchievement.List.Values)
            Assert.NotNull(PlayerStrings.GetText($"Record{info.Name}"));
        foreach (var r in ReactionsWindow.Valid)
            Assert.NotNull(PlayerStrings.GetText($"Reaction{r}"));
    }

    [AvaloniaFact]
    public void Catalog()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var w = new CatalogWindow(sav.Players[0]);
        Gallery.Capture(w, "83-catalog", false);
        w.ShowCategory(ItemCategory.Fish);
        Gallery.Capture(w, "83-catalog-fish", false);

        // Mark every fish, and one more item through its tick.
        w.MarkShown(true);
        w.ShowCategory(ItemCategory.All);
        w.Search("sofa");
        var row = w.Shown.First(z => !z.Received);
        row.Received = true;
        w.SaveAndClose();

        var data = pers.Data;
        var ofs = pers.Offsets.ItemCollectBit;
        foreach (var id in GameLists.Fish.ToArray().Where(z => ItemSearchBox.GetCategory(z) == ItemCategory.Fish))
            Assert.True(FlagUtil.GetFlag(data, ofs, id), $"fish {id}");
        Assert.True(FlagUtil.GetFlag(data, ofs, row.Id));
    }

    [AvaloniaFact]
    public void CatalogCancelKeepsTheSave()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var before = pers.Data.ToArray();
        var w = new CatalogWindow(sav.Players[0]);
        Show(w);
        w.MarkShown(true);
        w.Close();
        Assert.True(before.AsSpan().SequenceEqual(pers.Data));
    }

    [AvaloniaFact]
    public void Profile()
    {
        var sav = Save();
        var pers = sav.Players[0].Personal;
        var w = new ProfileWindow(sav.Players[0]);
        Gallery.Capture(w, "84-profile", false);

        w.SetBirthday(7, 1);
        w.SetFruit(2287); // cherry
        w.SaveAndClose();
        Assert.Equal(7, pers.Birthday.Month);
        Assert.Equal(1, pers.Birthday.Day);
        Assert.Equal(7, pers.ProfileBirthday.Month);
        Assert.Equal(1, pers.ProfileBirthday.Day);
        Assert.Equal(2287, pers.ProfileFruit);
    }

    [AvaloniaFact]
    public void Mail()
    {
        var sav = Save();
        var player = sav.Players[0];
        var w = new MailWindow(sav, player);
        Gallery.Capture(w, "85-mail", false);

        var before = player.PostBox.GetUsedPostBoxSlots().Count;
        var slot = w.WriteLetter();
        Assert.True(slot >= 0);
        w.AttachGift(2213, 0); // an apple
        Gallery.Capture(w, "85-mail-new", false);
        w.SaveAndClose();

        var box = player.PostBox;
        Assert.Equal(before + 1, box.GetUsedPostBoxSlots().Count);
        Assert.Contains(slot, box.GetUsedPostBoxSlots());
        Assert.Equal(2213, box.GetAttachedItem(slot).ItemId);
        Assert.False(box.GetMailReadStatus(slot));
        Assert.Equal(player.Personal.PlayerName, box.GetReceiverPlayerName(slot));
    }

    [AvaloniaFact]
    public void MailCancelPutsLettersBack()
    {
        var sav = Save();
        var player = sav.Players[0];
        var before = player.PostBox.MailListWholeRegion.ToArray();
        var w = new MailWindow(sav, player);
        Show(w);
        var slot = w.WriteLetter();
        Assert.True(slot >= 0);
        var used = player.PostBox.GetUsedPostBoxSlots();
        if (used.Count > 1)
            w.DeleteLetter(used.First(z => z != slot));
        w.Close();
        Assert.True(before.AsSpan().SequenceEqual(player.PostBox.MailListWholeRegion));
    }
}
