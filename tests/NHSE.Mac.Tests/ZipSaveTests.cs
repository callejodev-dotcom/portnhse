using System.IO.Compression;
using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// JKSV backups as .zip: a save of 3.0 zipped is about 28 MB (it hardly compresses), and it opens and saves back.
/// </summary>
public class ZipSaveTests
{
    [AvaloniaFact]
    public void OpensAndSavesABigJksvZip()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        TestSave.LoadCopy(out var folder);
        var zip = Path.Combine(TestSave.SavesFolder, $"{Guid.NewGuid():N}.zip");
        ZipFile.CreateFromDirectory(folder, zip);
        Assert.True(new FileInfo(zip).Length > 20 * 1024 * 1024, "as big as a JKSV backup of a 3.0 save");

        var task = SaveFileLoader.TryGetSaveFile(zip);
        var until = DateTime.UtcNow.AddSeconds(30);
        while (!task.IsCompleted && DateTime.UtcNow < until)
        {
            Dispatcher.UIThread.RunJobs();
            Thread.Sleep(10);
        }
        var sav = task.Result;
        Assert.NotNull(sav);

        var pers = sav.Players[0].Personal;
        var wallet = pers.Wallet;
        wallet.Value = wallet.Value == 4321 ? 4322u : 4321u;
        pers.Wallet = wallet;
        sav.Save((uint)DateTime.Now.Ticks);

        var again = NHSE.Core.HorizonSave.FromZip(zip);
        Assert.Equal(wallet.Value, again.Players[0].Personal.Wallet.Value);
        Assert.Empty(again.GetInvalidHashes());
    }
}
