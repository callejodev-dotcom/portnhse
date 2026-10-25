using NHSE.Core;
using NHSE.Injection;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' SysBotUI: replaced by SysBotUI.axaml + SysBotUI.axaml.cs.
public sealed class SysBotUI : PendingWindow
{
    public SysBotUI(AutoInjector injector, SysBotController c, AutoInjector injectorUSB, USBBotController b) => _ = (injector, c, injectorUSB, b);
}
