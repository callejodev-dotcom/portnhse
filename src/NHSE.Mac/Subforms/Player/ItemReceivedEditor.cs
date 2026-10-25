using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' ItemReceivedEditor: replaced by ItemReceivedEditor.axaml + ItemReceivedEditor.axaml.cs.
public sealed class ItemReceivedEditor : PendingWindow
{
    public ItemReceivedEditor(Player player) => _ = (player, 0);
}
