using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;

namespace NHSE.Mac;

/// <summary>
/// Base class for the ported forms: centered on their owner, closed with ⌘W, and never taller or wider than the
/// screen (above the Dock), so the Save and Cancel buttons can always be reached.
/// Subclasses keep the WinForms class name, which is also the translation key of the form.
/// </summary>
public class NhseWindow : Window
{
    /// <summary>
    /// Room left between the window and the edges of the usable screen area.
    /// </summary>
    private const double ScreenMargin = 8;

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

    protected override void OnOpened(EventArgs e)
    {
        base.OnOpened(e);
        FitToScreen();
    }

    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change)
    {
        base.OnPropertyChanged(change);
        // A window that sizes to its content grows when something opens inside it (e.g. "Advanced options").
        if (change.Property == ClientSizeProperty && IsVisible)
            KeepOnScreen();
    }

    /// <summary>
    /// Limits the window to the usable area of its screen (without the menu bar and the Dock).
    /// </summary>
    private void FitToScreen()
    {
        if ((Screens?.ScreenFromWindow(this) ?? Screens?.Primary) is not { } screen)
            return;
        var area = screen.WorkingArea.ToRect(screen.Scaling);
        var frame = FrameSize ?? ClientSize;
        var titleBar = Math.Max(0, frame.Height - ClientSize.Height);
        MaxWidth = Math.Min(MaxWidth, area.Width - (ScreenMargin * 2));
        MaxHeight = Math.Min(MaxHeight, area.Height - titleBar - (ScreenMargin * 2));
        KeepOnScreen();
    }

    /// <summary>
    /// Moves the window up or to the side if part of it is outside the usable area of its screen.
    /// </summary>
    private void KeepOnScreen()
    {
        if ((Screens?.ScreenFromWindow(this) ?? Screens?.Primary) is not { } screen)
            return;
        var area = screen.WorkingArea;
        var scale = screen.Scaling;
        var frame = FrameSize ?? ClientSize;
        var margin = (int)(ScreenMargin * scale);
        var width = (int)Math.Ceiling(frame.Width * scale);
        var height = (int)Math.Ceiling(frame.Height * scale);

        var pos = Position;
        var x = Math.Max(area.X + margin, Math.Min(pos.X, area.Right - width - margin));
        var y = Math.Max(area.Y + margin, Math.Min(pos.Y, area.Bottom - height - margin));
        if (x != pos.X || y != pos.Y)
            Position = new PixelPoint(x, y);
    }
}
