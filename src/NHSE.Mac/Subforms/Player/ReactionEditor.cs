using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' ReactionEditor: replaced by ReactionEditor.axaml + ReactionEditor.axaml.cs.
public sealed class ReactionEditor : PendingWindow
{
    public ReactionEditor(Personal p) => _ = (p, 0);
}
