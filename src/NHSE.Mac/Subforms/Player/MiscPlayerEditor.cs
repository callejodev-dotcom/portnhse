using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' MiscPlayerEditor: replaced by MiscPlayerEditor.axaml + MiscPlayerEditor.axaml.cs.
public sealed class MiscPlayerEditor : PendingWindow
{
    public MiscPlayerEditor(Player p, MainSave s) => _ = (p, s);
}
