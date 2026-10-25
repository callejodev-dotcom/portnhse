using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;

namespace NHSE.Mac;

/// <summary>
/// Placeholder for a form that has not been ported yet: tells the user and closes.
/// </summary>
public abstract class PendingWindow : NhseWindow
{
    protected PendingWindow()
    {
        Title = GetType().Name;
        SizeToContent = SizeToContent.WidthAndHeight;
        CanResize = false;
        var ok = new Button { Content = "OK", IsDefault = true, IsCancel = true, HorizontalAlignment = HorizontalAlignment.Right, Width = 80 };
        ok.Click += (_, _) => Close(false);
        var panel = new StackPanel { Margin = new Thickness(20), Spacing = 14 };
        panel.Children.Add(new TextBlock { Text = MacStrings.MsgNotPorted, MaxWidth = 360, TextWrapping = Avalonia.Media.TextWrapping.Wrap });
        panel.Children.Add(ok);
        Content = panel;
    }
}
