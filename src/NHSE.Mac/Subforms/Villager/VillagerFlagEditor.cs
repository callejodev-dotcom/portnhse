using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' VillagerFlagEditor: replaced by VillagerFlagEditor.axaml + VillagerFlagEditor.axaml.cs.
public sealed class VillagerFlagEditor : PendingWindow
{
    public VillagerFlagEditor(ushort[] counts) => _ = (counts, 0);
}
