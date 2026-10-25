using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' PostBoxEditor: replaced by PostBoxEditor.axaml + PostBoxEditor.axaml.cs.
public sealed class PostBoxEditor : PendingWindow
{
    public PostBoxEditor(Player p, PostBox pb) => _ = (p, pb);
}
