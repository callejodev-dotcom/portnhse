using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' RecipeListEditor: replaced by RecipeListEditor.axaml + RecipeListEditor.axaml.cs.
public sealed class RecipeListEditor : PendingWindow
{
    public RecipeListEditor(Player player) => _ = (player, 0);
}
