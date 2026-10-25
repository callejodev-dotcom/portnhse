using System.Collections;
using System.ComponentModel;
using System.Reflection;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;

namespace NHSE.Mac;

public enum PropertySort
{
    NoSort,
    Alphabetical,
    Categorized,
    CategorizedAlphabetical,
}

/// <summary>
/// Reflection-based property editor standing in for the WinForms PropertyGrid: public properties of the
/// selected object, grouped by <see cref="CategoryAttribute"/> if asked, with nested structs, classes and arrays
/// expandable (built only when expanded, since some save structures hold thousands of entries).
/// </summary>
public sealed class PropertyGrid : UserControl
{
    private const int ArrayPageSize = 100;
    private const double Indent = 14;

    private readonly StackPanel Rows = new() { Spacing = 1 };
    private readonly TextBlock Help = new() { TextWrapping = TextWrapping.Wrap, Margin = new Thickness(6, 4), MinHeight = 34 };
    private readonly Border HelpPanel;
    private object? _selected;

    /// <summary>
    /// Raised after a value was written to the selected object.
    /// </summary>
    public event EventHandler? PropertyValueChanged;

    public PropertySort PropertySort { get; set; } = PropertySort.CategorizedAlphabetical;

    public bool HelpVisible
    {
        get => HelpPanel.IsVisible;
        set => HelpPanel.IsVisible = value;
    }

    /// <summary>
    /// Object being edited. A boxed struct is edited in place: read it back from here after editing.
    /// </summary>
    public object? SelectedObject
    {
        get => _selected;
        set
        {
            _selected = value;
            Rebuild();
        }
    }

    public PropertyGrid()
    {
        HelpPanel = new Border
        {
            Child = Help,
            BorderThickness = new Thickness(0, 1, 0, 0),
            BorderBrush = Brushes.Gray,
            IsVisible = false,
        };
        var dock = new DockPanel();
        DockPanel.SetDock(HelpPanel, Dock.Bottom);
        dock.Children.Add(HelpPanel);
        dock.Children.Add(new ScrollViewer
        {
            Content = Rows,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled,
            Padding = new Thickness(0, 0, 12, 0),
        });
        Content = new Border
        {
            Child = dock,
            BorderThickness = new Thickness(1),
            BorderBrush = new SolidColorBrush(Color.FromArgb(0x60, 0x80, 0x80, 0x80)),
        };
        Grid.SetIsSharedSizeScope(Rows, true);
    }

    /// <summary>
    /// Rebuilds the rows from the current values (after the object was changed elsewhere).
    /// </summary>
    public void Refresh() => Rebuild();

    private void Rebuild()
    {
        Rows.Children.Clear();
        if (_selected is null)
            return;

        var root = new Node(_selected.GetType(), () => _selected, v => _selected = v, true);
        var props = GetProperties(_selected.GetType());
        if (PropertySort is PropertySort.Categorized or PropertySort.CategorizedAlphabetical)
        {
            foreach (var group in props.GroupBy(GetCategory).OrderBy(z => z.Key, StringComparer.Ordinal))
            {
                Rows.Children.Add(new TextBlock { Text = group.Key, FontWeight = FontWeight.SemiBold, Margin = new Thickness(2, 6, 0, 2) });
                foreach (var p in group.OrderBy(z => z.Name, StringComparer.Ordinal))
                    AddRows(Rows.Children, root.Child(p), 0);
            }
        }
        else
        {
            if (PropertySort == PropertySort.Alphabetical)
                props = [.. props.OrderBy(z => z.Name, StringComparer.Ordinal)];
            foreach (var p in props)
                AddRows(Rows.Children, root.Child(p), 0);
        }
    }

    private static string GetCategory(PropertyInfo p) => p.GetCustomAttribute<CategoryAttribute>()?.Category ?? "Misc";

    private static PropertyInfo[] GetProperties(Type t) => t.GetProperties(BindingFlags.Public | BindingFlags.Instance)
        .Where(p => p.GetIndexParameters().Length == 0 && !p.PropertyType.IsByRefLike && p.GetCustomAttribute<BrowsableAttribute>()?.Browsable != false)
        .OrderBy(p => p.MetadataToken)
        .ToArray();

    /// <summary>
    /// A value somewhere inside the selected object, with how to read it and write it back up the chain
    /// (so that fields of structs nested in structs end up in the selected object).
    /// </summary>
    private sealed class Node(Type type, Func<object?> get, Action<object?> set, bool canWrite, string name = "", string? description = null)
    {
        public Type Type { get; } = type;
        public string Name { get; } = name;
        public string? Description { get; } = description;
        public bool CanWrite { get; } = canWrite;
        public object? Get() => get();
        public void Set(object? value) => set(value);

        public Node Child(PropertyInfo p)
        {
            var desc = p.GetCustomAttribute<DescriptionAttribute>()?.Description;
            var display = p.GetCustomAttribute<DisplayNameAttribute>()?.DisplayName ?? p.Name;
            return new Node(p.PropertyType, () => Get() is { } o ? p.GetValue(o) : null, v =>
            {
                var o = Get();
                if (o is null)
                    return;
                p.SetValue(o, v);
                Set(o); // structs: write the modified copy back to the parent
            }, CanWrite && p.CanWrite && p.SetMethod?.IsPublic == true, display, desc);
        }

        public Node Element(Type elementType, int index) => new(elementType, () => (Get() as Array)?.GetValue(index), v =>
        {
            if (Get() is not Array a)
                return;
            a.SetValue(v, index);
            Set(a);
        }, CanWrite, $"[{index}]");
    }

    private static bool IsExpandable(Type t)
    {
        if (t.IsArray)
            return true;
        if (t.IsPrimitive || t.IsEnum || t == typeof(string) || t == typeof(decimal))
            return false;
        return GetProperties(t).Length != 0;
    }

    private void AddRows(Controls target, Node node, int depth)
    {
        if (IsExpandable(node.Type))
        {
            AddExpandable(target, node, depth);
            return;
        }

        var row = MakeRow(node.Name, depth, node.Description);
        var editor = CreateEditor(node);
        Grid.SetColumn(editor, 1);
        row.Children.Add(editor);
        target.Add(row);
    }

    private Grid MakeRow(string name, int depth, string? description)
    {
        var row = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), MinHeight = 26 };
        row.ColumnDefinitions[0].SharedSizeGroup = "Name";
        var label = new TextBlock
        {
            Text = name,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(4 + (depth * Indent), 0, 10, 0),
            MinWidth = 150 - (depth * Indent),
        };
        if (description is not null)
            ToolTip.SetTip(label, description);
        row.Children.Add(label);
        row.GotFocus += (_, _) => Help.Text = description is null ? name : $"{name}: {description}";
        return row;
    }

    private void AddExpandable(Controls target, Node node, int depth)
    {
        var children = new StackPanel { Spacing = 1, IsVisible = false };
        var summary = new TextBlock { VerticalAlignment = VerticalAlignment.Center, Foreground = Brushes.Gray, TextTrimming = TextTrimming.CharacterEllipsis };
        var toggle = new ToggleButton
        {
            Content = $"▸ {node.Name}",
            Background = Brushes.Transparent,
            BorderThickness = new Thickness(0),
            Padding = new Thickness(0),
            Margin = new Thickness(depth * Indent, 0, 10, 0),
            HorizontalContentAlignment = HorizontalAlignment.Left,
            MinWidth = 150 - (depth * Indent),
        };
        var row = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), MinHeight = 26 };
        row.ColumnDefinitions[0].SharedSizeGroup = "Name";
        row.Children.Add(toggle);
        Grid.SetColumn(summary, 1);
        row.Children.Add(summary);
        if (node.Description is not null)
            ToolTip.SetTip(row, node.Description);

        void UpdateSummary() => summary.Text = GetSummary(node.Get());
        UpdateSummary();

        bool built = false;
        toggle.IsCheckedChanged += (_, _) =>
        {
            var open = toggle.IsChecked == true;
            toggle.Content = $"{(open ? "▾" : "▸")} {node.Name}";
            if (open && !built)
            {
                built = true;
                BuildChildren(children.Children, node, depth + 1, UpdateSummary);
            }
            children.IsVisible = open;
        };

        target.Add(row);
        target.Add(children);
    }

    private void BuildChildren(Controls target, Node node, int depth, Action changed)
    {
        var value = node.Get();
        if (value is null)
            return;

        if (node.Type.IsArray && value is Array a)
        {
            var elementType = node.Type.GetElementType()!;
            if (a.Length <= ArrayPageSize)
            {
                for (int i = 0; i < a.Length; i++)
                    AddRows(target, Watch(node.Element(elementType, i), changed), depth);
                return;
            }

            // Big arrays: one page at a time.
            var pages = (a.Length + ArrayPageSize - 1) / ArrayPageSize;
            var page = new NumericUpDown { Minimum = 1, Maximum = pages, Value = 1, Width = 110, HorizontalAlignment = HorizontalAlignment.Left };
            var pageRow = MakeRow($"Page (1-{pages}, {a.Length} items)", depth, null);
            Grid.SetColumn(page, 1);
            pageRow.Children.Add(page);
            var content = new StackPanel { Spacing = 1 };
            void ShowPage()
            {
                content.Children.Clear();
                var start = ((int)(page.Value ?? 1) - 1) * ArrayPageSize;
                for (int i = start; i < Math.Min(a.Length, start + ArrayPageSize); i++)
                    AddRows(content.Children, Watch(node.Element(elementType, i), changed), depth);
            }
            page.ValueChanged += (_, _) => ShowPage();
            target.Add(pageRow);
            target.Add(content);
            ShowPage();
            return;
        }

        foreach (var p in GetProperties(node.Type))
            AddRows(target, Watch(node.Child(p), changed), depth);
    }

    /// <summary>
    /// Wraps a node so that writing it also refreshes the summary of the parent row.
    /// </summary>
    private static Node Watch(Node n, Action changed) => new(n.Type, n.Get, v => { n.Set(v); changed(); }, n.CanWrite, n.Name, n.Description);

    private static string GetSummary(object? value) => value switch
    {
        null => "(null)",
        Array a => $"{a.GetType().GetElementType()?.Name}[{a.Length}]",
        _ => value.ToString() ?? string.Empty,
    };

    private Control CreateEditor(Node node)
    {
        var t = Nullable.GetUnderlyingType(node.Type) ?? node.Type;
        var value = node.Get();
        Control editor;

        if (t == typeof(bool))
        {
            var chk = new CheckBox { IsChecked = value is true };
            chk.IsCheckedChanged += (_, _) => Write(node, chk.IsChecked == true);
            editor = chk;
        }
        else if (t.IsEnum)
        {
            var names = Enum.GetValues(t).Cast<object>().ToList();
            if (value is not null && !names.Contains(value))
                names.Add(value);
            var cb = new ComboBox { ItemsSource = names, SelectedItem = value, HorizontalAlignment = HorizontalAlignment.Stretch, MaxDropDownHeight = 400 };
            cb.SelectionChanged += (_, _) =>
            {
                if (cb.SelectedItem is { } v)
                    Write(node, v);
            };
            editor = cb;
        }
        else if (GetRange(t) is { } range)
        {
            var nud = new NumericUpDown
            {
                Minimum = range.Min,
                Maximum = range.Max,
                Value = value is null ? 0 : Convert.ToDecimal(value),
                HorizontalAlignment = HorizontalAlignment.Stretch,
                FormatString = range.Integer ? "0" : "G",
                Increment = range.Integer ? 1 : 0.1m,
            };
            nud.ValueChanged += (_, _) => Write(node, Convert.ChangeType(nud.Value ?? 0, t));
            editor = nud;
        }
        else if (t == typeof(string))
        {
            var tb = new TextBox { Text = value as string ?? string.Empty };
            tb.LostFocus += (_, _) => Write(node, tb.Text ?? string.Empty);
            tb.KeyDown += (_, e) =>
            {
                if (e.Key == Key.Enter)
                    Write(node, tb.Text ?? string.Empty);
            };
            editor = tb;
        }
        else
        {
            editor = new TextBlock { Text = GetSummary(value), VerticalAlignment = VerticalAlignment.Center, Foreground = Brushes.Gray };
        }

        editor.IsEnabled = node.CanWrite || editor is TextBlock;
        editor.Margin = new Thickness(0, 1);
        if (node.Description is not null)
            ToolTip.SetTip(editor, node.Description);
        return editor;
    }

    private void Write(Node node, object? value)
    {
        try
        {
            node.Set(value);
            PropertyValueChanged?.Invoke(this, EventArgs.Empty);
        }
        catch (Exception ex)
        {
            Help.Text = ex.InnerException?.Message ?? ex.Message;
            HelpPanel.IsVisible = true;
        }
    }

    private static (decimal Min, decimal Max, bool Integer)? GetRange(Type t)
    {
        if (t == typeof(byte)) return (byte.MinValue, byte.MaxValue, true);
        if (t == typeof(sbyte)) return (sbyte.MinValue, sbyte.MaxValue, true);
        if (t == typeof(short)) return (short.MinValue, short.MaxValue, true);
        if (t == typeof(ushort)) return (ushort.MinValue, ushort.MaxValue, true);
        if (t == typeof(int)) return (int.MinValue, int.MaxValue, true);
        if (t == typeof(uint)) return (uint.MinValue, uint.MaxValue, true);
        if (t == typeof(long)) return (long.MinValue, long.MaxValue, true);
        if (t == typeof(ulong)) return (ulong.MinValue, ulong.MaxValue, true);
        if (t == typeof(float)) return ((decimal)-1e12, (decimal)1e12, false);
        if (t == typeof(double)) return ((decimal)-1e15, (decimal)1e15, false);
        if (t == typeof(decimal)) return (decimal.MinValue, decimal.MaxValue, false);
        return null;
    }
}
