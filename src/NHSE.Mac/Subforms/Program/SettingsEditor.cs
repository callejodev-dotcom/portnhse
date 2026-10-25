using Avalonia;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Edits <see cref="Program.Settings"/>; they are saved when the window closes.
/// </summary>
public sealed class SettingsEditor : NhseWindow
{
    public SettingsEditor()
    {
        Title = "Settings Editor";
        Width = 420;
        Height = 380;
        var pg = new PropertyGrid { Name = "PG_Settings", PropertySort = PropertySort.NoSort, Margin = new Thickness(10) };
        pg.SelectedObject = Program.Settings;
        Content = pg;
        Closed += (_, _) =>
        {
            Program.SaveSettings();
            if (Avalonia.Application.Current is { } app)
                app.RequestedThemeVariant = Program.Settings.Theme.ToVariant();
        };
        this.TranslateInterface(GameInfo.CurrentLanguage);
    }
}
