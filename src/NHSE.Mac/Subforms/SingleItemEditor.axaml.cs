using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class SingleItemEditor : NhseWindow
{
    public Item Item { get; }

    public SingleItemEditor(Item item)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);

        // disassociate from original reference
        Item = item.ToBytesClass().ToClass<Item>();

        ItemEditor.Initialize(GameInfo.Strings.ItemDataSource);
        ItemEditor.LoadItem(Item);
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        ItemEditor.SetItem(Item);
        Close(true);
    }
}
