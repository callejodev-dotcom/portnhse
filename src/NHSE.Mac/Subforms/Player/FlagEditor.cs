using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' FlagEditor: replaced by FlagEditor.axaml + FlagEditor.axaml.cs.
public sealed class FlagEditor : PendingWindow
{
    public FlagEditor(short[] counts) => _ = (counts, 0);
}
