using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' AchievementEditor: replaced by AchievementEditor.axaml + AchievementEditor.axaml.cs.
public sealed class AchievementEditor : PendingWindow
{
    public AchievementEditor(Personal personal) => _ = (personal, 0);
}
