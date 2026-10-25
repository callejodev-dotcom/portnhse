using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class SimpleHexEditor : NhseWindow
{
    public byte[] Bytes { get; private set; }

    public SimpleHexEditor() : this([]) { }

    public SimpleHexEditor(byte[] originalBytes)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);
        RTB_RAM.Text = string.Join(" ", originalBytes.Select(z => $"{z:X2}"));
        Bytes = originalBytes;
    }

    private async void Update_Click(object? sender, RoutedEventArgs e)
    {
        // The text box wraps instead of using tabs and line breaks, but pasted text may have any of them.
        var bytestring = string.Concat((RTB_RAM.Text ?? string.Empty).Where(c => !char.IsWhiteSpace(c)));
        try
        {
            Bytes = Convert.FromHexString(bytestring);
        }
        catch (FormatException ex)
        {
            await Dialogs.Error(ex.Message);
            return;
        }
        Close(true);
    }
}
