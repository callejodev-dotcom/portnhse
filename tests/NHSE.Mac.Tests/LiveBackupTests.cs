using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The copies kept of the game brought over Wi-Fi: small, listed newest first, and read back exactly.
/// </summary>
public class LiveBackupTests
{
    [Fact]
    public void CopyRoundTrip()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var sav = TestSave.LoadCopy(out _);
        var main = sav.Main.Data.ToArray();
        var player = sav.Players[0].Personal.Data.ToArray();
        var path = LiveBackups.Save(main, player, sav.Players[0].Personal, "prueba");
        try
        {
            Assert.True(new FileInfo(path).Length < 2_000_000, "a copy is small");
            Assert.Contains(LiveBackups.List(), c => c.Path == path);
            var copy = LiveBackups.Read(path, main.Length, player.Length);
            Assert.NotNull(copy);
            Assert.Equal(main, copy.Value.Main);
            Assert.Equal(player, copy.Value.Player);
            Assert.Null(LiveBackups.Read(path, main.Length + 1, player.Length));
        }
        finally
        {
            File.Delete(path);
        }
    }
}
