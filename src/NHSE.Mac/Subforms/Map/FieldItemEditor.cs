using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' FieldItemEditor: replaced by FieldItemEditor.axaml + FieldItemEditor.axaml.cs.
public sealed class FieldItemEditor : PendingWindow
{
    public FieldItemEditor(MainSave sav) => _ = (sav, 0);
}
