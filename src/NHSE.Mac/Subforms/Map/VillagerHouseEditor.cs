using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' VillagerHouseEditor: replaced by VillagerHouseEditor.axaml + VillagerHouseEditor.axaml.cs.
public sealed class VillagerHouseEditor : PendingWindow
{
    public VillagerHouseEditor(IVillagerHouse[] houses, IReadOnlyList<IVillager> villagers, MainSave sav, int index) => _ = (houses, villagers, sav, index);
}
