using Avalonia.Controls;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class RestrictedItemSelect : UserControl
{
    private IList<ComboItem> DataSource = [];

    public RestrictedItemSelect() => InitializeComponent();

    public void Initialize(IList<ComboItem> items, bool canType = false)
    {
        CB_ItemID.SetComboItems([.. items]);
        DataSource = items;
        CB_ItemID.IsEditable = canType;
    }

    public ushort Value
    {
        get => CHK_CustomItem.IsChecked == true ? (ushort)(NUD_CustomItem.Value ?? 0) : (ushort)UiUtil.GetIndex(CB_ItemID);
        set
        {
            if (DataSource.Any(z => z.Value == value))
            {
                CHK_CustomItem.IsChecked = false;
                CB_ItemID.SelectedValue = (int)value;
            }
            else
            {
                CHK_CustomItem.IsChecked = true;
                NUD_CustomItem.Value = value;
            }
        }
    }

    private void CHK_Custom_CheckedChanged(object? sender, RoutedEventArgs e)
    {
        CB_ItemID.IsEnabled = CHK_CustomItem.IsChecked != true;
        NUD_CustomItem.IsEnabled = CHK_CustomItem.IsChecked == true;
    }

    private void CB_ItemID_SelectedValueChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (CB_ItemID.SelectedItem is not null)
            NUD_CustomItem.Value = UiUtil.GetIndex(CB_ItemID);
    }
}
