using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Media;
using NHSE.Core;

namespace NHSE.Mac;

public partial class BatchEditor : NhseWindow
{
    private readonly ItemMutator Mutator;
    private readonly ItemProcessor Processor;
    private readonly IReadOnlyList<Item> Items;
    private readonly Item item;
    private int currentFormat = -1;

    public BatchEditor(IReadOnlyList<Item> items, Item item)
    {
        InitializeComponent();
        Items = items;
        this.item = item;
        Mutator = new ItemMutator();
        Processor = new ItemProcessor(Mutator);

        var formats = new List<string> { "Any" };
        foreach (Type t in Mutator.Reflect.Types)
            formats.Add(t.Name.ToLowerInvariant());
        formats.Add("All");
        CB_Format.SetComboItems(formats);
        CB_Require.SetComboItems(["Set Equal To", "Require Equals", "Require Not Equals"]);

        CB_Format.SelectedIndex = CB_Require.SelectedIndex = 0;
        ToolTip.SetTip(CB_Property, "Name");
        ToolTip.SetTip(L_PropType, "Type");
        ToolTip.SetTip(L_PropValue, "Value");
    }

    private void B_Go_Click(object? sender, RoutedEventArgs e) => RunBackgroundWorker();

    /// <summary>
    /// Lines of the instructions box (RichTextBox.Lines in WinForms: none when empty).
    /// </summary>
    private string[] InstructionLines
    {
        get
        {
            var text = RTB_Instructions.Text;
            if (string.IsNullOrEmpty(text))
                return [];
            return text.Split('\n').Select(z => z.TrimEnd('\r')).ToArray();
        }
    }

    private async void B_Add_Click(object? sender, RoutedEventArgs e)
    {
        if (CB_Property.SelectedIndex < 0 || CB_Property.SelectedItem is not string property)
        { await Dialogs.Alert("Invalid Property"); return; }

        var prefix = StringInstruction.Prefixes;
        string s = prefix[CB_Require.SelectedIndex] + property + StringInstruction.SplitInstruction;
        var lines = InstructionLines;
        if (lines.Length != 0 && lines[^1].Length > 0)
            s = Environment.NewLine + s;

        RTB_Instructions.Text += s;
        RTB_Instructions.CaretIndex = RTB_Instructions.Text?.Length ?? 0;
    }

    private void CB_Format_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (currentFormat == CB_Format.SelectedIndex || CB_Format.SelectedIndex < 0)
            return;

        int format = CB_Format.SelectedIndex;
        CB_Property.SetComboItems(Mutator.Reflect.Properties[format]);
        CB_Property.SelectedIndex = 0;
        currentFormat = format;
    }

    private void CB_Property_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (CB_Property.SelectedItem is not string name)
            return;
        L_PropType.Text = Mutator.GetPropertyType(name, CB_Format.SelectedIndex);
        if (Mutator.TryGetHasProperty(item, name, out var pi))
        {
            L_PropValue.Text = pi.GetValue(item)?.ToString();
            L_PropType.ClearValue(TextBlock.ForegroundProperty); // reset color
        }
        else // no property, flag
        {
            L_PropValue.Text = string.Empty;
            L_PropType.Foreground = Brushes.Red;
        }
    }

    private async void RunBackgroundWorker()
    {
        var lines = InstructionLines;
        if (lines.Any(line => line.Length == 0))
        { await Dialogs.Error("Invalid instruction detected."); return; }

        var sets = StringInstructionSet.GetBatchSets(lines).ToArray();
        if (sets.Any(s => s.Filters.Any(z => string.IsNullOrWhiteSpace(z.PropertyValue))))
        { await Dialogs.Error("Filter empty."); return; }

        if (sets.Any(z => z.Instructions.Count == 0))
        { await Dialogs.Error("No instructions."); return; }

        var emptyVal = sets.SelectMany(s => s.Instructions.Where(z => string.IsNullOrWhiteSpace(z.PropertyValue))).ToArray();
        if (emptyVal.Length > 0)
        {
            string props = string.Join(", ", emptyVal.Select(z => z.PropertyName));
            string invalid = "Property empty." + Environment.NewLine + props;
            if (DialogResult.Yes != await Dialogs.Prompt(MessageBoxButtons.YesNo, invalid, "Continue?"))
                return;
        }

        RTB_Instructions.IsEnabled = B_Go.IsEnabled = false;
        await RunBatchEdit(sets);
    }

    private async Task RunBatchEdit(StringInstructionSet[] sets)
    {
        // don't bother reporting progress...
        await Task.Run(() => Processor.Process(sets, Items));

        string result = Processor.GetEditorResults(sets);
        await Dialogs.Alert(result);
        RTB_Instructions.IsEnabled = B_Go.IsEnabled = true;
        SetupProgressBar(0);
    }

    // Progress Bar
    private void SetupProgressBar(int count)
    {
        PB_Show.Minimum = 0;
        PB_Show.Value = 0;
        PB_Show.Maximum = count;
    }
}
