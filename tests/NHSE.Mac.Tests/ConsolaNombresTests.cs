using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary> Nombres (jugador e isla en todas sus copias) y Poki, como la página del jugador del Mac. </summary>
public partial class ConsolaTests
{
    private static void Rename(HorizonSave sav, string value, bool isTown)
    {
        // As PlayerPage.Rename of the Mac.
        var pers = sav.Players[0].Personal;
        var orig = (isTown ? pers.GetTownIdentity() : pers.GetPlayerIdentity()).ToArray();
        if (isTown)
            pers.TownName = value;
        else
            pers.PlayerName = value;
        sav.ChangeIdentity(orig, isTown ? pers.GetTownIdentity() : pers.GetPlayerIdentity());
    }

    private static void SameOtherFiles(HorizonSave expected, HorizonSave actual)
    {
        for (int i = 0; i < expected.Players.Count; i++)
        {
            SameBytes(expected.Players[i].PostBox.Data, actual.Players[i].PostBox.Data);
            SameBytes(expected.Players[i].Photo.Data, actual.Players[i].Photo.Data);
            SameBytes(expected.Players[i].Profile.Data, actual.Players[i].Profile.Data);
            if (expected.Players[i].WhereAreN is { } w)
                SameBytes(w.Data, actual.Players[i].WhereAreN!.Data);
        }
    }

    [Theory]
    [InlineData(false, "Carlota")]
    [InlineData(true, "Barrito")]
    [InlineData(false, "Ñandú áé")]
    public void RenamedEverywhereLikeNhse(bool isTown, string name)
    {
        Skip();
        var (nhse, sw) = Both(sav => Rename(sav, name, isTown), "nombre", isTown ? "isla" : "jugador", name);
        foreach (var f in nhse.Players.SelectMany(z => z))
            f.Hash();
        SameOtherFiles(nhse, sw);
        Assert.Equal(name, isTown ? sw.Players[0].Personal.TownName : sw.Players[0].Personal.PlayerName);
        Assert.Empty(sw.GetInvalidHashes());
    }

    [Fact]
    public void PokiLikeNhse()
    {
        Skip();
        var sav = TestSave.LoadCopy(out var folder);
        Assert.SkipWhen(sav.Players[0].WhereAreN is null, "no wherearen.dat");
        Run(folder, "poki", "12345");
        var after = HorizonSave.FromFolder(folder);
        Assert.Empty(after.GetInvalidHashes());
        var w = sav.Players[0].WhereAreN!;
        w.Poki = w.Poki with { Value = 12345 };
        w.Hash();
        SameBytes(w.Data, after.Players[0].WhereAreN!.Data);
        EncryptedInt32.ReadVerify(after.Players[0].WhereAreN!.Data, after.Players[0].WhereAreN!.Offsets.Poki);
    }
}
