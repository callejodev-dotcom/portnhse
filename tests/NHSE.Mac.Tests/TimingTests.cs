using System.Diagnostics;
using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// Rough timings of the heaviest windows, written to galeria/tiempos.txt (only with NHSE_TIEMPOS=1).
/// </summary>
public class TimingTests
{
    [AvaloniaFact]
    public void OpenTimes()
    {
        Assert.SkipUnless(Environment.GetEnvironmentVariable("NHSE_TIEMPOS") == "1", "NHSE_TIEMPOS is not set");
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var lines = new List<string>();
        void Time(string name, Func<Avalonia.Controls.Window> create)
        {
            var sw = Stopwatch.StartNew();
            var w = create();
            w.Show();
            Dispatcher.UIThread.RunJobs();
            lines.Add($"{name}: {sw.ElapsedMilliseconds} ms");
            w.Close();
        }

        var sw0 = Stopwatch.StartNew();
        var sav = TestSave.LoadCopy(out _);
        lines.Add($"load save: {sw0.ElapsedMilliseconds} ms");

        // What the first editor pays for
        sw0.Restart();
        var villagers = new VillagerEditor(sav.Main.GetVillagers(), sav.Players[0].Personal, sav, true, 0);
        lines.Add($"  villager editor control: {sw0.ElapsedMilliseconds} ms");
        sw0.Restart();
        _ = sav.Players[0].Personal.GetPhotoData().ToArray();
        lines.Add($"  photo data: {sw0.ElapsedMilliseconds} ms");
        sw0.Restart();
        var probe = new Avalonia.Controls.Window { Content = new Avalonia.Controls.NumericUpDown() };
        probe.Show();
        Dispatcher.UIThread.RunJobs();
        probe.Close();
        lines.Add($"  first window with a NumericUpDown: {sw0.ElapsedMilliseconds} ms");
        for (int i = 0; i < 2; i++)
        {
            Time($"editor #{i}", () => new Editor(sav));
            var pers = sav.Players[0].Personal;
            Time($"inventory #{i}", () => new PlayerItemEditor(pers.Pocket.Concat(pers.Bag).ToArray(), 10, 4));
            Time($"storage #{i}", () => new PlayerItemEditor(pers.ItemChest, 10, 5));
            Time($"field items #{i}", () => new FieldItemEditor(sav.Main));
            Time($"post box #{i}", () => new PostBoxEditor(sav.Players[0], sav.Players[0].PostBox));
        }
        File.WriteAllLines(Path.Combine(TestSave.GalleryFolder, "tiempos.txt"), lines);
    }
}
