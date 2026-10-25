using Avalonia.Controls;
using Avalonia.Input;

namespace NHSE.Mac;

/// <summary>
/// Base class for the ported forms: centered on their owner, closed with ⌘W.
/// Subclasses keep the WinForms class name, which is also the translation key of the form.
/// </summary>
public class NhseWindow : Window
{
    public NhseWindow()
    {
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.Key == Key.W && e.KeyModifiers == UiUtil.CommandModifier)
        {
            e.Handled = true;
            Close();
            return;
        }
        base.OnKeyDown(e);
    }
}
