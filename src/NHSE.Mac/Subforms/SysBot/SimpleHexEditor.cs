using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' SimpleHexEditor: replaced by SimpleHexEditor.axaml + SimpleHexEditor.axaml.cs.
public sealed class SimpleHexEditor(byte[] originalBytes) : PendingWindow
{
    public byte[] Bytes { get; } = originalBytes;
}
