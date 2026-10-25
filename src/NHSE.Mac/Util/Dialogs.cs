using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;

namespace NHSE.Mac;

public enum DialogResult
{
    None,
    OK,
    Cancel,
    Yes,
    No,
}

public enum MessageBoxButtons
{
    OK,
    OKCancel,
    YesNo,
    YesNoCancel,
}

/// <summary>
/// Message boxes, equivalent to the WinFormsUtil message displays. All of them are modal to the active window.
/// </summary>
public static class Dialogs
{
    /// <summary>
    /// Displays a dialog showing the details of an error.
    /// </summary>
    /// <param name="lines">User-friendly message about the error.</param>
    public static Task<DialogResult> Error(params string[] lines)
    {
        Sound.Hand();
        return Show(MacStrings.MsgTitleError, MessageBoxButtons.OK, lines);
    }

    public static Task<DialogResult> Alert(params string[] lines) => Alert(true, lines);

    public static Task<DialogResult> Alert(bool sound, params string[] lines)
    {
        if (sound)
            Sound.Asterisk();
        return Show(MacStrings.MsgTitleAlert, MessageBoxButtons.OK, lines);
    }

    public static Task<DialogResult> Prompt(MessageBoxButtons btn, params string[] lines)
    {
        Sound.Asterisk();
        return Show(MacStrings.MsgTitlePrompt, btn, lines);
    }

    private static async Task<DialogResult> Show(string title, MessageBoxButtons buttons, string[] lines)
    {
        var box = new MessageBoxWindow(title, buttons, lines);
        var owner = UiUtil.GetActiveWindow();
        DialogResult result;
        if (owner is not null)
        {
            result = await box.ShowDialog<DialogResult>(owner);
        }
        else
        {
            var tcs = new TaskCompletionSource<DialogResult>();
            box.Closed += (_, _) => tcs.TrySetResult(box.Result);
            box.Show();
            result = await tcs.Task;
        }

        if (result != DialogResult.None)
            return result;
        // Closed with the window button: treat it like the cancel button.
        return buttons switch
        {
            MessageBoxButtons.YesNo => DialogResult.No,
            MessageBoxButtons.OKCancel or MessageBoxButtons.YesNoCancel => DialogResult.Cancel,
            _ => DialogResult.OK,
        };
    }
}

/// <summary>
/// Small alert-style window (app icon, bold first line, buttons on the right) like an NSAlert.
/// </summary>
public sealed class MessageBoxWindow : Window
{
    public DialogResult Result { get; private set; } = DialogResult.None;

    public MessageBoxWindow(string title, MessageBoxButtons buttons, string[] lines)
    {
        Title = title;
        SizeToContent = SizeToContent.WidthAndHeight;
        CanResize = false;
        ShowInTaskbar = false;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        MinWidth = 320;
        MaxWidth = 560;

        var text = new StackPanel { Spacing = 8, VerticalAlignment = VerticalAlignment.Center };
        for (int i = 0; i < lines.Length; i++)
        {
            text.Children.Add(new SelectableTextBlock
            {
                Text = lines[i],
                TextWrapping = TextWrapping.Wrap,
                FontWeight = i == 0 ? FontWeight.SemiBold : FontWeight.Normal,
            });
        }

        var icon = new Image
        {
            Source = new Bitmap(AssetLoader.Open(new Uri("avares://NHSE/Assets/icon.png"))),
            Width = 56,
            Height = 56,
            VerticalAlignment = VerticalAlignment.Top,
            Margin = new Thickness(0, 0, 16, 0),
        };

        var row = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Margin = new Thickness(0, 16, 0, 0),
        };
        foreach (var (result, isDefault, isCancel) in GetButtons(buttons))
        {
            var b = new Button
            {
                Content = GetText(result),
                IsDefault = isDefault,
                IsCancel = isCancel,
                MinWidth = 80,
                HorizontalContentAlignment = HorizontalAlignment.Center,
            };
            if (isDefault)
                b.Classes.Add("accent");
            b.Click += (_, _) => Finish(result);
            row.Children.Add(b);
        }

        var grid = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*"),
            RowDefinitions = new RowDefinitions("*,Auto"),
            Margin = new Thickness(20),
        };
        grid.Children.Add(icon);
        Grid.SetColumn(text, 1);
        grid.Children.Add(text);
        Grid.SetRow(row, 1);
        Grid.SetColumnSpan(row, 2);
        grid.Children.Add(row);
        Content = grid;

        KeyDown += (_, e) =>
        {
            if (e.Key == Key.C && e.KeyModifiers == UiUtil.CommandModifier)
                _ = UiUtil.SetClipboardText(this, string.Join(Environment.NewLine + Environment.NewLine, lines));
        };
    }

    private void Finish(DialogResult result)
    {
        Result = result;
        Close(result);
    }

    private static IEnumerable<(DialogResult Result, bool IsDefault, bool IsCancel)> GetButtons(MessageBoxButtons buttons) => buttons switch
    {
        MessageBoxButtons.OKCancel => [(DialogResult.Cancel, false, true), (DialogResult.OK, true, false)],
        MessageBoxButtons.YesNo => [(DialogResult.No, false, true), (DialogResult.Yes, true, false)],
        MessageBoxButtons.YesNoCancel => [(DialogResult.Cancel, false, true), (DialogResult.No, false, false), (DialogResult.Yes, true, false)],
        _ => [(DialogResult.OK, true, true)],
    };

    private static string GetText(DialogResult result) => result switch
    {
        DialogResult.Yes => MessageStrings.MsgDialogButtonYes,
        DialogResult.No => MessageStrings.MsgDialogButtonNo,
        DialogResult.Cancel => MessageStrings.MsgDialogButtonCancel,
        _ => "OK",
    };
}
