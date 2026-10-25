using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' VillagerDIYTimerEditor: replaced by VillagerDIYTimerEditor.axaml + VillagerDIYTimerEditor.axaml.cs.
public sealed class VillagerDIYTimerEditor : PendingWindow
{
    public VillagerDIYTimerEditor(IVillager v) => _ = (v, 0);
}
