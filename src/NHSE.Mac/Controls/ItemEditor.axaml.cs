using System.Globalization;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class ItemEditor : UserControl
{
    private readonly List<ComboItem> Recipes = GameInfo.Strings.CreateItemDataSource(RecipeList.Recipes, false);
    private readonly List<ComboItem> Fossils = GameInfo.Strings.CreateItemDataSource(GameLists.Fossils, false);
    private readonly CheckBox[] Watered;

    private bool Loading = true;
    private bool CanExtend;
    private bool SupressedMode = false;

    public ItemEditor()
    {
        InitializeComponent();

        CB_WrapColor.SetComboItems(Enum.GetNames<ItemWrappingPaper>());
        CB_WrapType.SetComboItems(Enum.GetNames<ItemWrapping>());

        Watered =
        [
            CHK_WV0, CHK_WV1,
            CHK_WV2, CHK_WV3,
            CHK_WV4, CHK_WV5,
            CHK_WV6, CHK_WV7,
            CHK_WV8, CHK_WV9
        ];

        L_WaterDays.Cursor = new Cursor(StandardCursorType.Hand);
        CB_ItemID.PropertyChanged += (_, e) =>
        {
            if (e.Property == ComboBox.TextProperty)
                CB_ItemID_TextChanged();
        };
    }

    private IReadOnlyList<ComboItem> AllItems = [];

    public void Initialize(IReadOnlyList<ComboItem> items, bool canExtend = false, bool supressed = false)
    {
        CHK_IsExtension.IsVisible = CanExtend = canExtend;

        CB_ItemID.SetComboItems(items);
        CB_Recipe.SetComboItems(Recipes);
        CB_Fossil.SetComboItems(Fossils);

        LoadItem(Item.NO_ITEM);

        AllItems = items;

        SupressedMode = supressed;
    }

    public Item LoadItem(Item item)
    {
        Loading = true;
        var id = item.ItemId;
        if (CanExtend && id == Item.EXTENSION)
            return LoadExtensionItem(item);

        CHK_IsExtension.IsChecked = false;
        CB_ItemID.SelectedValue = (int)id;
        var kind = ItemInfo.GetItemKind(id);

        if (kind.IsFlowerGene(id))
        {
            LoadGenes(item.Genes);
            CHK_Gold.IsChecked = item.IsWateredGold;
            CHK_IsWatered.IsChecked = item.IsWatered;
            NUD_WaterDays.Value = item.DaysWatered;
            for (int i = 0; i < Watered.Length; i++)
                Watered[i].IsChecked = item.GetIsWateredByVisitor(i);
        }
        else
        {
            NUD_Count.Value = item.Count;
            NUD_Uses.Value = item.UseCount;
            NUD_Flag0.Value = item.SystemParam;
        }

        LoadItemTypeValues(kind, id);
        if (kind == ItemKind.Kind_MessageBottle || id >= 60_000)
        {
            NUD_Flag1.Value = item.AdditionalParam;
        }
        else
        {
            CHK_Wrapped.IsChecked = item.WrappingType != 0;
            CB_WrapType.SelectedIndex = (int)item.WrappingType;
            CB_WrapColor.SelectedIndex = (int)item.WrappingPaper;
            CHK_WrapShowName.IsChecked = item.WrappingShowItem;
            CHK_Wrap80.IsChecked = item.Wrapping80;
        }

        Loading = false;
        return item;
    }

    private Item LoadExtensionItem(Item item)
    {
        CB_ItemID.SelectedValue = (int)item.ExtensionItemId;
        CHK_IsExtension.IsChecked = true;
        NUD_ExtensionX.Value = item.ExtensionX;
        NUD_ExtensionY.Value = item.ExtensionY;
        return item;
    }

    public Item LoadFieldsToNewItem() => SetItem(new Item());

    public Item SetItem(Item item)
    {
        if (CHK_IsExtension.IsChecked == true)
            return SetExtensionItem(item);

        var id = (ushort)UiUtil.GetIndex(CB_ItemID);
        var kind = ItemInfo.GetItemKind(id);

        item.ItemId = id;
        if (kind.IsFlowerGene(id))
        {
            item.Genes = SaveGenes();
            item.DaysWatered = (int)(NUD_WaterDays.Value ?? 0);
            item.IsWateredGold = CHK_Gold.IsChecked == true;
            item.IsWatered = CHK_IsWatered.IsChecked == true;
            for (int i = 0; i < Watered.Length; i++)
                item.SetIsWateredByVisitor(i, Watered[i].IsChecked == true);

            item.SystemParam = 0;
            item.AdditionalParam = 0;
        }
        else
        {
            item.Count = (ushort)(NUD_Count.Value ?? 0);
            item.UseCount = (ushort)(NUD_Uses.Value ?? 0);
            item.SystemParam = (byte)(NUD_Flag0.Value ?? 0);
        }

        if (kind == ItemKind.Kind_MessageBottle || id >= 60_000)
        {
            item.AdditionalParam = (byte)(NUD_Flag1.Value ?? 0);
        }
        else
        {
            if (CHK_Wrapped.IsChecked != true)
            {
                item.SetWrapping(0, 0);
            }
            else
            {
                var type = (ItemWrapping)CB_WrapType.SelectedIndex;
                var color = (ItemWrappingPaper)Math.Max(0, CB_WrapColor.SelectedIndex);
                var show = CHK_WrapShowName.IsChecked == true;
                var flag = CHK_Wrap80.IsChecked == true;
                item.SetWrapping(type, color, show, flag);
            }
        }
        return item;
    }

    /// <summary>
    /// Hides the item wrapping UI controls from the editor for handling via custom UIs.
    /// </summary>
    /// <remarks>
    /// This method sets the visibility of both the wrapped checkbox (<see cref="CHK_Wrapped"/>)
    /// and the wrapped options panel (<see cref="FLP_Wrapped"/>) to <see langword="false"/>.
    /// Use this when wrapping information should not be displayed or edited by the standard controls.
    /// </remarks>
    public void SupressWrappedInfo()
    {
        CHK_Wrapped.IsVisible = false;
        FLP_Wrapped.IsVisible = false;
    }

    private Item SetExtensionItem(Item item)
    {
        var id = (ushort)UiUtil.GetIndex(CB_ItemID);
        item.ItemId = Item.EXTENSION;
        item.ExtensionItemId = id;
        item.ExtensionX = (byte)(NUD_ExtensionX.Value ?? 0);
        item.ExtensionY = (byte)(NUD_ExtensionY.Value ?? 0);
        return item;
    }

    private void CB_ItemID_SelectedValueChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (CB_ItemID.SelectedItem is null)
            return; // typing a name that matches nothing yet

        var itemID = (ushort)UiUtil.GetIndex(CB_ItemID);
        var itemCount = (ushort)(NUD_Count.Value ?? 0);
        ChangeItem(itemID, itemCount);
        var kind = ItemInfo.GetItemKind(itemID);

        ToggleEditorVisibility(kind, itemID);
        if (!Loading)
            LoadItemTypeValues(kind, itemID);

        var remake = ItemRemakeUtil.GetRemakeIndex(itemID);
        if (remake < 0)
        {
            var closeItems = GameInfo.Strings.GetAssociatedItems(itemID, out var bse);
            if (closeItems.Count > 1) // ignore if we are the only parenthesised item
            {
                L_RemakeBody.Text = $"{bse.Trim()}:\n" + closeItems.ToStringList(false);
                L_RemakeBody.IsVisible = true;
                L_RemakeFabric.IsVisible = false;
            }
            else
            {
                L_RemakeBody.IsVisible = false;
                L_RemakeFabric.IsVisible = false;
            }
        }
        else
        {
            var info = ItemRemakeInfoData.List[remake];
            var body = info.GetBodySummary(GameInfo.Strings);
            L_RemakeBody.Text = body;
            L_RemakeBody.IsVisible = body.Length != 0;

            var fabric = info.GetFabricSummary(GameInfo.Strings);
            L_RemakeFabric.Text = fabric;
            L_RemakeFabric.IsVisible = fabric.Length != 0;
        }
        if (SupressedMode) SupressWrappedInfo();
    }

    private void LoadItemTypeValues(ItemKind k, ushort index)
    {
        if (k == ItemKind.Kind_MessageBottle || index >= 60_000)
        {
            CHK_Wrapped.IsChecked = false;
            CHK_Wrapped.IsVisible = false;
            FLP_Flag1.IsVisible = true;
            return;
        }

        switch (k)
        {
            case ItemKind.Kind_FossilUnknown:
                CB_Fossil.SelectedValue = (int)(NUD_Count.Value ?? 0);
                break;

            case ItemKind.Kind_DIYRecipe:
                CB_Recipe.SelectedValue = (int)(NUD_Count.Value ?? 0);
                break;

            case ItemKind.Kind_MessageBottle:
                CB_Recipe.SelectedValue = (int)(NUD_Count.Value ?? 0);
                CHK_Wrapped.IsVisible = false;
                CHK_Wrapped.IsChecked = false;
                FLP_Flag1.IsVisible = true;
                return;
        }

        CHK_Wrapped.IsVisible = true;
        FLP_Flag1.IsVisible = false;

        if (SupressedMode) SupressWrappedInfo();
    }

    private void ToggleEditorVisibility(ItemKind k, ushort id)
    {
        if (k.IsFlowerGene(id))
        {
            CB_Recipe.IsVisible = false;
            FLP_Uses.IsVisible = FLP_Count.IsVisible = false;
            FLP_Flower.IsVisible = true;
            return;
        }

        switch (k)
        {
            case ItemKind.Kind_FossilUnknown:
                CB_Fossil.IsVisible = true;

                CB_Recipe.IsVisible = false;
                FLP_Uses.IsVisible = FLP_Count.IsVisible = false;
                FLP_Flower.IsVisible = false;
                break;

            case ItemKind.Kind_DIYRecipe:
                CB_Recipe.IsVisible = true;

                CB_Fossil.IsVisible = false;
                FLP_Uses.IsVisible = FLP_Count.IsVisible = false;
                FLP_Flower.IsVisible = false;
                break;

            case ItemKind.Kind_MessageBottle:
                CB_Recipe.IsVisible = true;

                CB_Fossil.IsVisible = false;
                FLP_Uses.IsVisible = true;
                FLP_Count.IsVisible = false;
                FLP_Flower.IsVisible = false;
                break;

            default:
                CB_Fossil.IsVisible = false;
                CB_Recipe.IsVisible = false;
                FLP_Uses.IsVisible = FLP_Count.IsVisible = true;
                FLP_Flower.IsVisible = false;
                break;
        }
    }

    private void L_Count_DoubleClick(object? sender, TappedEventArgs e)
    {
        Item currentItem = SetItem(new Item());
        var result = ItemInfo.TryGetMaxStackCount(currentItem, out var max);
        if (!result)
            return;
        currentItem.Count = (ushort)(max - 1);
        LoadItem(currentItem);
    }

    private void CB_CountAlias_SelectedValueChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (sender is not ComboBox { SelectedItem: not null } cb)
            return;
        var val = UiUtil.GetIndex(cb);
        NUD_Count.Value = Math.Max(0, Math.Min(NUD_Count.Maximum, val));
    }

    private void LoadGenes(FlowerGene genes)
    {
        CHK_R1.IsChecked = (genes & FlowerGene.R1) != 0;
        CHK_R2.IsChecked = (genes & FlowerGene.R2) != 0;
        CHK_Y1.IsChecked = (genes & FlowerGene.Y1) != 0;
        CHK_Y2.IsChecked = (genes & FlowerGene.Y2) != 0;
        CHK_W1.IsChecked = (genes & FlowerGene.w1) == 0; // inverted; both bits on = no gene (not white)
        CHK_W2.IsChecked = (genes & FlowerGene.w2) == 0; // inverted; both bits on = no gene (not white)
        CHK_S1.IsChecked = (genes & FlowerGene.S1) != 0;
        CHK_S2.IsChecked = (genes & FlowerGene.S2) != 0;
    }

    private FlowerGene SaveGenes()
    {
        var val = FlowerGene.None;
        if (CHK_R1.IsChecked == true) val |= FlowerGene.R1;
        if (CHK_R2.IsChecked == true) val |= FlowerGene.R2;
        if (CHK_Y1.IsChecked == true) val |= FlowerGene.Y1;
        if (CHK_Y2.IsChecked == true) val |= FlowerGene.Y2;
        if (CHK_W1.IsChecked != true) val |= FlowerGene.w1; // inverted; both bits on = no gene (not white)
        if (CHK_W2.IsChecked != true) val |= FlowerGene.w2; // inverted; both bits on = no gene (not white)
        if (CHK_S1.IsChecked == true) val |= FlowerGene.S1;
        if (CHK_S2.IsChecked == true) val |= FlowerGene.S2;
        return val;
    }

    private void L_WaterDays_Click(object? sender, PointerPressedEventArgs e)
    {
        bool value = (e.KeyModifiers & KeyModifiers.Alt) == 0;
        CHK_Gold.IsChecked = value;
        CHK_IsWatered.IsChecked = value;
        NUD_WaterDays.Value = value ? 31 : 0;
        foreach (var v in Watered)
            v.IsChecked = value;
    }

    private void CHK_IsExtension_CheckedChanged(object? sender, RoutedEventArgs e)
    {
        if (CHK_IsExtension.IsChecked == true)
        {
            FLP_Item.IsVisible = false;
            FLP_Extension.IsVisible = true;
        }
        else
        {
            FLP_Item.IsVisible = true;
            FLP_Extension.IsVisible = false;
        }
    }

    private void ChangeItem(ushort item, ushort count) => PB_Item.SetItem(item, count);

    private void CHK_Wrapped_CheckedChanged(object? sender, RoutedEventArgs e)
    {
        var wrapped = CHK_Wrapped.IsChecked == true;
        FLP_Wrapped.IsVisible = wrapped;
        if (wrapped && CB_WrapType.SelectedIndex <= 0)
            CB_WrapType.SelectedIndex = (int)ItemWrapping.WrappingPaper;
    }

    private void CB_WrapType_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e) => CB_WrapColor.IsVisible = (ItemWrapping)CB_WrapType.SelectedIndex == ItemWrapping.WrappingPaper;

    /// <summary>
    /// Typing selects the first name that starts with the text; the tooltip lists names that contain it.
    /// </summary>
    private void CB_ItemID_TextChanged()
    {
        var entered = CB_ItemID.Text;
        if (string.IsNullOrEmpty(entered))
        {
            ToolTip.SetTip(CB_ItemID, null);
            return;
        }
        var itemNames = AllItems.Where(z => z.Text.Contains(entered, StringComparison.CurrentCultureIgnoreCase)).Take(10).Select(z => z.Text);
        var caption = string.Join(Environment.NewLine, itemNames);
        ToolTip.SetTip(CB_ItemID, caption.Length == 0 ? null : caption);
    }

    private void NUD_Count_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        var itemID = (ushort)UiUtil.GetIndex(CB_ItemID);
        var itemCount = (ushort)(NUD_Count.Value ?? 0);
        ChangeItem(itemID, itemCount);
    }

    private async void PB_Item_Click(object? sender, PointerPressedEventArgs e)
    {
        // Import if requested
        if (e.KeyModifiers == KeyModifiers.Shift)
        {
            var text = await UiUtil.GetClipboardText(this);
            if (text is null || !ulong.TryParse(text.Trim(), NumberStyles.AllowHexSpecifier, CultureInfo.CurrentCulture, out var val))
                return;
            var import = new Item(val);
            LoadItem(import);
            Sound.Asterisk();
            return;
        }

        // Otherwise, export
        var item = SetItem(new Item());
        var data = item.ToBytesClass();
        var u64 = System.Buffers.Binary.BinaryPrimitives.ReadUInt64LittleEndian(data);
        await UiUtil.SetClipboardText(this, $"{u64:X16}");
        Sound.Asterisk();
    }
}
