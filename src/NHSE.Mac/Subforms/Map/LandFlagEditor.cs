using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' LandFlagEditor: replaced by LandFlagEditor.axaml + LandFlagEditor.axaml.cs.
public sealed class LandFlagEditor : PendingWindow
{
    public LandFlagEditor(short[] counts) => _ = (counts, 0);
}
