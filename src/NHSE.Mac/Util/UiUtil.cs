using System.Diagnostics;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Controls.Primitives;
using Avalonia.Data;
using Avalonia.Input;
using Avalonia.Input.Platform;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Helpers that stand in for the WinFormsUtil ones (combo boxes, modifier keys, clipboard, windows).
/// </summary>
public static class UiUtil
{
    /// <summary>
    /// The "Control" modifier of the WinForms version: Command on macOS, Control elsewhere.
    /// </summary>
    public static KeyModifiers CommandModifier { get; } = OperatingSystem.IsMacOS() ? KeyModifiers.Meta : KeyModifiers.Control;

    /// <summary>
    /// Modifier keys currently held, like WinForms' Control.ModifierKeys. Kept up to date by the class handlers registered in <see cref="App"/>.
    /// </summary>
    public static KeyModifiers ModifierKeys { get; internal set; }

    public static bool IsCommand(KeyModifiers m) => (m & CommandModifier) != 0;
    public static bool IsAlt(KeyModifiers m) => (m & KeyModifiers.Alt) != 0;
    public static bool IsShift(KeyModifiers m) => (m & KeyModifiers.Shift) != 0;

    /// <summary>
    /// Populates a <see cref="ComboBox"/> with <see cref="ComboItem"/>s, showing the text and using the value as SelectedValue.
    /// </summary>
    public static void SetComboItems(this ComboBox cb, IReadOnlyList<ComboItem> items)
    {
        cb.DisplayMemberBinding = new Binding(nameof(ComboItem.Text));
        cb.SelectedValueBinding = new Binding(nameof(ComboItem.Value));
        TextSearch.SetTextBinding(cb, new Binding(nameof(ComboItem.Text)));
        cb.ItemsSource = items;
    }

    /// <summary>
    /// Populates a <see cref="ComboBox"/> with plain strings (Items.AddRange in WinForms).
    /// </summary>
    public static void SetComboItems(this ComboBox cb, IEnumerable<string> items) => cb.ItemsSource = items.ToArray();

    /// <summary>
    /// Gets the selected value of the input <see cref="cb"/>. If no value is selected, will return 0.
    /// </summary>
    public static int GetIndex(ComboBox cb) => cb.SelectedValue is int i ? i : 0;

    /// <summary>
    /// Selects the <see cref="ComboItem"/> with the given value (SelectedValue in WinForms).
    /// </summary>
    public static void SetValue(this ComboBox cb, int value) => cb.SelectedValue = value;

    public static Window? GetActiveWindow()
    {
        if (Application.Current?.ApplicationLifetime is not IClassicDesktopStyleApplicationLifetime desktop)
            return null;
        var windows = desktop.Windows;
        return windows.LastOrDefault(z => z.IsActive && z.IsVisible) ?? windows.LastOrDefault(z => z.IsVisible);
    }

    public static IReadOnlyList<Window> GetWindows()
    {
        if (Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            return desktop.Windows;
        return [];
    }

    public static T? FirstFormOfType<T>() where T : Window => GetWindows().OfType<T>().FirstOrDefault();

    private static IClipboard? GetClipboard(Visual? v) => (v is null ? null : TopLevel.GetTopLevel(v))?.Clipboard ?? GetActiveWindow()?.Clipboard;

    public static async Task SetClipboardText(Visual? v, string text)
    {
        var cb = GetClipboard(v);
        if (cb is not null)
            await cb.SetTextAsync(text);
    }

    public static async Task<string?> GetClipboardText(Visual? v)
    {
        var cb = GetClipboard(v);
        return cb is null ? null : await cb.TryGetTextAsync();
    }

    /// <summary>
    /// Shows a context menu (or flyout) below the <see cref="n"/> control, like the WinForms ShowContextMenuBelow.
    /// </summary>
    public static void ShowContextMenuBelow(ContextMenu c, Control n)
    {
        c.Placement = PlacementMode.BottomEdgeAlignedLeft;
        c.PlacementTarget = n;
        c.Open(n);
    }

    public static void OpenInFinder(string path)
    {
        try
        {
            Process.Start(new ProcessStartInfo("open", [path]) { UseShellExecute = false });
        }
        catch (Exception ex)
        {
            Debug.WriteLine(ex);
        }
    }
}

/// <summary>
/// System sounds used for feedback (System.Media.SystemSounds in WinForms).
/// </summary>
public static class Sound
{
    [DllImport("/System/Library/Frameworks/AppKit.framework/AppKit")]
    private static extern void NSBeep();

    /// <summary> Error sound. </summary>
    public static void Hand()
    {
        if (!OperatingSystem.IsMacOS())
            return;
        try { NSBeep(); }
        catch (Exception ex) { Debug.WriteLine(ex.Message); }
    }

    /// <summary> Soft "done" sound. </summary>
    public static void Asterisk()
    {
        const string tink = "/System/Library/Sounds/Tink.aiff";
        if (!OperatingSystem.IsMacOS() || !File.Exists(tink))
            return;
        try { Process.Start(new ProcessStartInfo("/usr/bin/afplay", [tink]) { UseShellExecute = false }); }
        catch (Exception ex) { Debug.WriteLine(ex.Message); }
    }
}
