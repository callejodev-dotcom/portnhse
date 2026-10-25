using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Property editor for a single object (generic, so built in code: XAML classes can't be generic).
/// </summary>
/// <remarks>
/// Structs are edited as a boxed copy: read the result back from <see cref="Object"/> when the dialog returns true.
/// </remarks>
public sealed class SingleObjectEditor<T> : NhseWindow where T : class
{
    private readonly PropertyGrid PG_Item;

    /// <summary>
    /// The edited object.
    /// </summary>
    public T Object => (T)PG_Item.SelectedObject!;

    public SingleObjectEditor(object obj, PropertySort sort, bool showHelp)
    {
        Title = "Property Editor";
        Width = 420;
        Height = 560;
        MinWidth = 320;
        MinHeight = 300;

        PG_Item = new PropertyGrid { Name = "PG_Item", PropertySort = sort, HelpVisible = showHelp };
        PG_Item.SelectedObject = obj;

        var save = new Button { Name = "B_Save", Content = "Save", Width = 80, IsDefault = true, Margin = new Thickness(6, 0, 0, 0) };
        save.Classes.Add("accent");
        save.Click += (_, _) => Close(true);
        var cancel = new Button { Name = "B_Cancel", Content = "Cancel", Width = 80, IsCancel = true };
        cancel.Click += (_, _) => Close(false);

        var buttons = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right, Margin = new Thickness(0, 10, 0, 0) };
        buttons.Children.Add(cancel);
        buttons.Children.Add(save);

        var dock = new DockPanel { Margin = new Thickness(10) };
        DockPanel.SetDock(buttons, Dock.Bottom);
        dock.Children.Add(buttons);
        dock.Children.Add(PG_Item);
        Content = dock;

        this.TranslateInterface(GameInfo.CurrentLanguage);
    }
}
