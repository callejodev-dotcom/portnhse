using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' CampsiteEditor: replaced by CampsiteEditor.axaml + CampsiteEditor.axaml.cs.
public sealed class CampsiteEditor : PendingWindow
{
    public CampsiteEditor(HorizonSave sav) => _ = (sav, 0);
}
