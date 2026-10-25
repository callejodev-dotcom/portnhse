using Avalonia.Controls;
using NHSE.Core;

namespace NHSE.Mac;

public partial class BuildingHelp : NhseWindow
{
    private static readonly IReadOnlyDictionary<string, string[]> HelpDictionary = StructureUtil.GetStructureHelpList();

    public BuildingHelp()
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);

        CB_StructureType.SetComboItems(HelpDictionary.Keys);
        CB_StructureType.SelectedIndex = 0;
    }

    private void CB_StructureType_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (CB_StructureType.SelectedItem is not string name)
            return;
        var values = HelpDictionary[name];
        CB_StructureValues.SetComboItems(values);
        CB_StructureValues.SelectedIndex = 0;
    }
}
