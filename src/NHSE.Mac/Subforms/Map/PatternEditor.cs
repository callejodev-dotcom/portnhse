using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' PatternEditor: replaced by PatternEditor.axaml + PatternEditor.axaml.cs.
public sealed class PatternEditor : PendingWindow
{
    public PatternEditor(DesignPattern[] patterns, Player player) => _ = (patterns, player);
}
