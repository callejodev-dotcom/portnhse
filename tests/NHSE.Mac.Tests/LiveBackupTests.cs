using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The copies kept of the game brought over Wi-Fi: small, listed newest first, and read back exactly.
/// </summary>
[Collection("Copias en directo")] // they share the folder of the copies with LiveOpenTests
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

    /// <summary> Reopening takes what was last brought or sent over Wi-Fi when it is newer than the files, and only for the same island. </summary>
    [Fact]
    public void ReopenUsesTheNewestState()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var sav = TestSave.LoadCopy(out _);
        var pers = sav.Players[0].Personal;
        var wallet = pers.Wallet;
        var inGame = TestSave.LoadCopy(out _);
        var p = inGame.Players[0].Personal;
        var w = p.Wallet;
        w.Value = wallet.Value == 4242 ? 4243u : 4242u;
        p.Wallet = w;
        LiveBackups.SaveLatest(inGame.Main.Data, p.Data, p);

        // The files are older: the save gets the game's state.
        Assert.NotNull(LiveBackups.UseNewest(sav, DateTime.Now.AddDays(-1)));
        Assert.Equal(w.Value, pers.Wallet.Value);

        // The files are newer (a new copy from JKSV): nothing changes.
        var again = TestSave.LoadCopy(out _);
        Assert.Null(LiveBackups.UseNewest(again, DateTime.Now.AddMinutes(1)));
        Assert.Equal(wallet.Value, again.Players[0].Personal.Wallet.Value);

        // Another island's state is never used.
        var other = TestSave.LoadCopy(out _);
        var id = other.Players[0].Personal.Offsets.PersonalId;
        other.Players[0].Personal.Data[id] ^= 0xFF; // another island ID
        Assert.Null(LiveBackups.UseNewest(other, DateTime.Now.AddDays(-1)));
    }
}
