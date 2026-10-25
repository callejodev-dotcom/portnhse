using NHSE.Core;
using NHSE.Injection;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' SysBotRAMEdit: replaced by SysBotRAMEdit.axaml + SysBotRAMEdit.axaml.cs.
public sealed class SysBotRAMEdit : PendingWindow
{
    public SysBotRAMEdit(InjectionType type) => _ = (type, 0);
}
