using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using NHSE.Villagers;

namespace NHSE.Mac;

/// <summary>
/// Searchable list of every villager the swap can use, with portraits and names in the game language.
/// </summary>
public sealed class VillagerPicker : NhseWindow
{
    public sealed record Entry(string InternalName, string Name)
    {
        public override string ToString() => Name;
    }

    private readonly List<Entry> All;
    private readonly ListBox List;

    /// <summary>
    /// Internal name (e.g. "cat01") of the chosen villager, once the dialog returns true.
    /// </summary>
    public string? SelectedInternalName => (List.SelectedItem as Entry)?.InternalName;

    public VillagerPicker()
    {
        Title = HomeStrings.PickerTitle;
        Width = 380;
        Height = 560;
        MinWidth = 320;
        MinHeight = 360;

        All = [.. GameInfo.Strings.VillagerMap
            .Where(z => VillagerResources.IsVillagerDataKnown(z.Key))
            .Select(z => new Entry(z.Key, z.Value))
            .OrderBy(z => z.Name, StringComparer.CurrentCultureIgnoreCase)];

        var search = new TextBox { PlaceholderText = HomeStrings.PickerSearch };
        List = new ListBox
        {
            ItemsSource = All,
            ItemTemplate = new FuncDataTemplate<Entry>((e, _) =>
            {
                var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 10 };
                if (e is null)
                    return row;
                row.Children.Add(new Image { Source = VillagerSprite.GetVillagerSprite(e.InternalName), Width = 40, Height = 40 });
                row.Children.Add(new TextBlock { Text = e.Name, VerticalAlignment = VerticalAlignment.Center, FontSize = 14 });
                return row;
            }),
            Margin = new Thickness(0, 8),
        };

        search.TextChanged += (_, _) =>
        {
            var text = search.Text?.Trim() ?? string.Empty;
            List.ItemsSource = text.Length == 0 ? All : All.Where(z => z.Name.Contains(text, StringComparison.CurrentCultureIgnoreCase)).ToList();
            if (List.ItemCount > 0)
                List.SelectedIndex = 0;
        };

        var ok = new Button { Content = HomeStrings.PickerChoose, IsDefault = true, Classes = { "accent" }, MinWidth = 90, Margin = new Thickness(6, 0, 0, 0) };
        ok.Click += (_, _) =>
        {
            if (SelectedInternalName is not null)
                Close(true);
        };
        var cancel = new Button { Content = HomeStrings.Cancel, IsCancel = true, MinWidth = 90 };
        cancel.Click += (_, _) => Close(false);
        List.DoubleTapped += (_, _) =>
        {
            if (SelectedInternalName is not null)
                Close(true);
        };

        var buttons = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right };
        buttons.Children.Add(cancel);
        buttons.Children.Add(ok);

        var dock = new DockPanel { Margin = new Thickness(14) };
        DockPanel.SetDock(search, Dock.Top);
        DockPanel.SetDock(buttons, Dock.Bottom);
        dock.Children.Add(search);
        dock.Children.Add(buttons);
        dock.Children.Add(List);
        Content = dock;

        Opened += (_, _) => search.Focus();
        KeyDown += (_, e) =>
        {
            // Arrow keys move through the list while typing in the search box.
            if (e.Key is Key.Down or Key.Up && List.ItemCount > 0)
            {
                var i = Math.Clamp(List.SelectedIndex + (e.Key == Key.Down ? 1 : -1), 0, List.ItemCount - 1);
                List.SelectedIndex = i;
                List.ScrollIntoView(i);
                e.Handled = true;
            }
        };
    }

    /// <summary>
    /// Selects a villager by internal name (used by the tests).
    /// </summary>
    public void Select(string internalName) => List.SelectedItem = All.FirstOrDefault(z => z.InternalName == internalName);
}
