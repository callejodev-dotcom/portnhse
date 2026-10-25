using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' MuseumEditor: replaced by MuseumEditor.axaml + MuseumEditor.axaml.cs.
public sealed class MuseumEditor : PendingWindow
{
    public MuseumEditor(Museum museum) => _ = (museum, 0);
}
