using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' PatternEditorPRO: replaced by PatternEditorPRO.axaml + PatternEditorPRO.axaml.cs.
public sealed class PatternEditorPRO : PendingWindow
{
    public PatternEditorPRO(DesignPatternPRO[] patterns, Player player) => _ = (patterns, player);
}
