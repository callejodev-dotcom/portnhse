using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' VillagerMemoryEditor: replaced by VillagerMemoryEditor.axaml + VillagerMemoryEditor.axaml.cs.
public sealed class VillagerMemoryEditor : PendingWindow
{
    public VillagerMemoryEditor(IVillager villager) => _ = (villager, 0);
}
