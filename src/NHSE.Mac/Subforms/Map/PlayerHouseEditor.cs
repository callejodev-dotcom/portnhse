using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' PlayerHouseEditor: replaced by PlayerHouseEditor.axaml + PlayerHouseEditor.axaml.cs.
public sealed class PlayerHouseEditor : PendingWindow
{
    public PlayerHouseEditor(IPlayerHouse[] houses, IReadOnlyList<Player> players, MainSave sav, int index) => _ = (houses, players, sav, index);
}
