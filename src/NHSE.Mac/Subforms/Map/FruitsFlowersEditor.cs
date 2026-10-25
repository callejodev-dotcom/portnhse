using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' FruitsFlowersEditor: replaced by FruitsFlowersEditor.axaml + FruitsFlowersEditor.axaml.cs.
public sealed class FruitsFlowersEditor : PendingWindow
{
    public FruitsFlowersEditor(Player p, MainSave s) => _ = (p, s);
}
