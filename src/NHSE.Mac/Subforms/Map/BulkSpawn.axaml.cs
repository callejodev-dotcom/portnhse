using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class BulkSpawn : NhseWindow
{
    private readonly IItemLayerEditor Editor;
    private string NHIFilePath = "";

    public BulkSpawn(IItemLayerEditor editor, int x = 0, int y = 0)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);

        Editor = editor;

        CB_SpawnType.SetComboItems(Enum.GetNames<SpawnType>());
        CB_SpawnArrange.SetComboItems(Enum.GetNames<SpawnArrangement>());

        CB_SpawnType.SelectedIndex = 0;
        CB_SpawnArrange.SelectedIndex = 0;
        NUD_SpawnX.Value = x;
        NUD_SpawnY.Value = y;
    }

    private async void B_Apply_Click(object? sender, RoutedEventArgs e)
    {
        var type = (SpawnType)CB_SpawnType.SelectedIndex;
        var arrange = (SpawnArrangement)CB_SpawnArrange.SelectedIndex;
        var x = (int)(NUD_SpawnX.Value ?? 0);
        var y = (int)(NUD_SpawnY.Value ?? 0);
        var count = (int)(NUD_SpawnCount.Value ?? 1);

        IReadOnlyList<Item> items;
        int sizeX;
        int sizeY;
        if (type is SpawnType.SequentialDIY or SpawnType.AlphabeticalDIY)
        {
            var min = (ushort)(NUD_DIYStart.Value ?? 0);
            var max = (ushort)(NUD_DIYStop.Value ?? 0);
            var diy = RecipeList.Recipes
                .Where(z => z.Key is not (RecipeList.BridgeConstructionKit or RecipeList.CampsiteConstructionKit) && min <= z.Key && z.Key <= max)
                .Select(z => z.Key)
                .Select(z => new Item(Item.DIYRecipe) { FreeParam = z });

            if (type == SpawnType.AlphabeticalDIY)
                diy = diy.OrderBy(z => GameInfo.Strings.GetItemName(z));

            items = Enumerable.Repeat(diy, count).SelectMany(z => z).ToArray();
            sizeX = sizeY = 2;
        }
        else if (type == SpawnType.ItemsFromNHI)
        {
            if (string.IsNullOrEmpty(NHIFilePath) || !File.Exists(NHIFilePath))
            {
                await Dialogs.Alert(MessageStrings.MsgFieldItemNoNHI);
                return;
            }

            // read non-empty slots into item array
            var data = await File.ReadAllBytesAsync(NHIFilePath);
            var array = data.GetArray<Item>(Item.SIZE).Where(item => !item.IsNone);
            items = Enumerable.Repeat(array, count).SelectMany(z => z).ToArray();

            // set flag0 = 0x20 for each item to ensure it gets dropped
            // this also forces a 2x2 item size
            foreach (var item in items)
                item.SystemParam = 0x20;

            sizeX = sizeY = 2;
        }
        else
        {
            var item = Editor.ItemProvider.SetItem(new Item());
            items = Enumerable.Repeat(item, count).ToArray();
            var size = ItemInfo.GetItemSize(item);
            sizeX = size.Width;
            sizeY = size.Height;
        }

        if (sizeX % 2 == 1)
            sizeX++;
        if (sizeY % 2 == 1)
            sizeY++;

        var ctr = SpawnItems(Editor.Spawn, items, x, y, arrange, sizeX, sizeY, true);
        if (ctr == 0)
        {
            await Dialogs.Alert(MessageStrings.MsgFieldItemModifyNone);
            return;
        }
        Editor.ReloadItems();
        await Dialogs.Alert(string.Format(MessageStrings.MsgFieldItemModifyCount, count));
    }

    private static int SpawnItems(LayerItem layer, IReadOnlyList<Item> items, int x, int y, SpawnArrangement arrange, int sizeX, int sizeY, bool noOverwrite)
    {
        // every {setting} tiles, we jump down to the next available row of tiles.
        int x0 = x;
        var newline = arrange switch
        {
            SpawnArrangement.Square => (int)Math.Sqrt(items.Count * sizeX * sizeY),
            SpawnArrangement.Vertical => 1 * sizeY,
            _ => items.Count * sizeX, // Horizontal
        };

        int ctr = 0;
        for (var i = 0; i < items.Count; i++)
        {
            var item = items[i];
            var permission = layer.IsOccupied(item, x, y);
            switch (permission)
            {
                case PlacedItemPermission.OutOfBounds when y >= layer.TileInfo.TotalHeight:
                    return ctr;
                case PlacedItemPermission.OutOfBounds:
                case PlacedItemPermission.Collision when noOverwrite:
                    i--;
                    break;

                default:
                    var exist = layer.GetTile(x, y);
                    layer.DeleteExtensionTiles(exist, x, y);

                    // Set new placed data
                    layer.SetExtensionTiles(item, x, y);
                    exist.CopyFrom(item);
                    ctr++;
                    break;
            }

            x += sizeX;
            if (x - x0 >= newline)
            {
                y += sizeY;
                x = x0;
            }
        }

        return ctr;
    }

    private void CB_SpawnType_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        var index = (SpawnType)CB_SpawnType.SelectedIndex;
        L_DIYStart.IsVisible = L_DIYStop.IsVisible = NUD_DIYStart.IsVisible = NUD_DIYStop.IsVisible = index is SpawnType.SequentialDIY or SpawnType.AlphabeticalDIY;
        L_NHI.IsVisible = L_NHIFileName.IsVisible = index == SpawnType.ItemsFromNHI;
        NUD_SpawnCount.Value = index switch
        {
            SpawnType.SequentialDIY => 1,
            SpawnType.AlphabeticalDIY => 1,
            SpawnType.ItemsFromNHI => 1,
            _ => 8,
        };

        if (index == SpawnType.ItemsFromNHI)
            SelectNHI();
    }

    private void L_NHIFileName_Click(object? sender, TappedEventArgs e) => SelectNHI();

    private async void SelectNHI()
    {
        var path = await Pickers.OpenFile(this, "New Horizons Inventory (*.nhi)|*.nhi|All files (*.*)|*.*", "items.nhi");
        if (path is null)
            return;

        L_NHIFileName.Text = Path.GetFileName(path);
        NHIFilePath = path;
    }

    private enum SpawnType
    {
        ItemFromEditor,
        SequentialDIY,
        AlphabeticalDIY,
        ItemsFromNHI,
    }

    private enum SpawnArrangement
    {
        Square,
        Horizontal,
        Vertical,
    }
}
