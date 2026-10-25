using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Paged grid of item slots linked to an <see cref="ItemEditor"/> (ItemGrid + ItemGridEditor of WinForms).
/// </summary>
/// <remarks>
/// Mouse actions follow WinForms with ⌘ in place of Ctrl: ⌘ (or a plain click) views, ⇧ sets, ⌥ deletes, ⌘⌥ clones to every slot.
/// Right click opens the same actions as a menu.
/// </remarks>
public partial class ItemGridEditor : UserControl
{
    private const int Border = 1; // between slots

    private readonly ItemEditor Editor;
    private readonly IReadOnlyList<Item> Items;

    private readonly List<ItemSlot> SlotPictureBoxes = [];
    private int Count => Items.Count;
    private int Page;
    private int ItemsPerPage;

    public Action? ItemChanged { private get; set; }

    private void ItemUpdated() => ItemChanged?.Invoke();

    public ItemGridEditor() : this(new ItemEditor(), []) { }

    public ItemGridEditor(ItemEditor editor, IReadOnlyList<Item> items)
    {
        Editor = editor;
        Items = items;
        InitializeComponent();

        L_ItemName.Text = string.Empty;
        L_Hint.Text = MacStrings.HintItemGrid;
    }

    public void InitializeGrid(int width, int height, int itemWidth, int itemHeight)
    {
        ItemsPerPage = width * height;
        ItemGrid.Children.Clear();
        SlotPictureBoxes.Clear();

        var menu = CreateSlotMenu();
        for (int row = 0; row < height; row++)
        {
            for (int column = 0; column < width; column++)
            {
                var pb = new ItemSlot
                {
                    Index = SlotPictureBoxes.Count,
                    Width = itemWidth + 2,
                    Height = itemHeight + 2,
                    Margin = new Thickness(column * (itemWidth + 2 + Border), row * (itemHeight + 2 + Border), 0, 0),
                    HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Left,
                    VerticalAlignment = Avalonia.Layout.VerticalAlignment.Top,
                    ContextMenu = menu,
                };
                pb.PointerEntered += Slot_MouseEnter;
                pb.PointerExited += Slot_MouseLeave;
                pb.PointerReleased += Slot_MouseClick;
                pb.PointerWheelChanged += Slot_MouseWheel;
                pb.ContextRequested += (_, _) => LastContextSlot = pb;
                ItemGrid.Children.Add(pb);
                SlotPictureBoxes.Add(pb);
            }
        }
        ItemGrid.Width = (width * (itemWidth + 2 + Border)) - Border;
        ItemGrid.Height = (height * (itemHeight + 2 + Border)) - Border;
        ChangePage();
    }

    /// <summary>
    /// Slot that opened the context menu.
    /// </summary>
    private ItemSlot? LastContextSlot;

    private ContextMenu CreateSlotMenu()
    {
        MenuItem Item(string name, string text, Action<ItemSlot> action)
        {
            var m = new MenuItem { Name = name, Header = text };
            m.Click += (_, _) =>
            {
                if (LastContextSlot is { } pb)
                {
                    action(pb);
                    Slot_Refresh(pb);
                }
            };
            return m;
        }
        var cm = new ContextMenu { Name = "CM_Hand" };
        cm.Items.Add(Item("Menu_View", "View", ClickView));
        cm.Items.Add(Item("Menu_Set", "Set", ClickSet));
        cm.Items.Add(Item("Menu_Delete", "Delete", ClickDelete));
        return cm;
    }

    private void Slot_MouseWheel(object? sender, PointerWheelEventArgs e)
    {
        var delta = e.Delta.Y < 0 ? 1 : -1; // scrolling down increases page #
        var newpage = Math.Min(PageCount - 1, Math.Max(0, Page + delta));
        if (newpage == Page || newpage < 0)
            return;
        Page = newpage;
        ChangePage();
        e.Handled = true;
    }

    private void Slot_MouseEnter(object? sender, PointerEventArgs e)
    {
        if (sender is not ItemSlot pb)
            return;
        Slot_Refresh(pb);
        pb.IsHighlighted = true;
    }

    private void Slot_Refresh(ItemSlot pb)
    {
        var item = GetItem(pb.Index);
        var text = GetItemText(item);
        ToolTip.SetTip(pb, text);
        L_ItemName.Text = text;
    }

    private void Slot_MouseLeave(object? sender, PointerEventArgs e)
    {
        if (sender is not ItemSlot pb)
            return;
        pb.IsHighlighted = false;
        L_ItemName.Text = string.Empty;
    }

    public static string GetItemText(Item item) => GameInfo.Strings.GetItemName(item);

    private void Slot_MouseClick(object? sender, PointerReleasedEventArgs e)
    {
        if (sender is not ItemSlot pb || e.InitialPressMouseButton != MouseButton.Left)
            return;

        var m = e.KeyModifiers & ~KeyModifiers.Shift;
        var cmd = UiUtil.CommandModifier;
        if (m == (cmd | KeyModifiers.Alt))
            ClickClone(pb);
        else if ((e.KeyModifiers & KeyModifiers.Shift) != 0)
            ClickSet(pb);
        else if (m == KeyModifiers.Alt)
            ClickDelete(pb);
        else if (m == cmd || m == KeyModifiers.None)
            ClickView(pb);
        else
            return;

        // restart hovering since the mouse event isn't fired
        Slot_Refresh(pb);
        e.Handled = true;
    }

    public Item LoadItem(int index) => Editor.LoadItem(GetItem(index));
    public Item SetItem(int index) => Editor.SetItem(GetItem(index));

    private Item GetItem(int index)
    {
        index += Page * ItemsPerPage;
        return Items[index];
    }

    private void ClickView(ItemSlot pb) => LoadItem(pb.Index);

    private void ClickSet(ItemSlot pb)
    {
        var item = SetItem(pb.Index);
        pb.SetItem(item);
        ItemUpdated();
    }

    private void ClickDelete(ItemSlot pb)
    {
        var item = GetItem(pb.Index);
        item.Delete();
        pb.SetItem(item);
        ItemUpdated();
    }

    private void ClickClone(ItemSlot pb)
    {
        var index = pb.Index;
        var item = GetItem(index);
        for (int i = 0; i < SlotPictureBoxes.Count; i++)
        {
            if (i == index)
                continue;
            var dest = GetItem(i);
            dest.CopyFrom(item);
            SlotPictureBoxes[i].SetItem(item);
            ItemUpdated();
        }
        Sound.Asterisk();
    }

    private static int GetPageJump()
    {
        var m = UiUtil.ModifierKeys;
        if (m == UiUtil.CommandModifier)
            return 10;
        if (m == KeyModifiers.Alt)
            return 25;
        if (m == KeyModifiers.Shift)
            return 1000;
        return 1;
    }

    private void B_Up_Click(object? sender, RoutedEventArgs e)
    {
        if (Page == 0)
            return;
        Page = Math.Max(0, Page - GetPageJump());
        ChangePage();
    }

    private void B_Down_Click(object? sender, RoutedEventArgs e)
    {
        if (ItemsPerPage * (Page + 1) == Count)
            return;
        Page = Math.Min(PageCount - 1, Page + GetPageJump());
        ChangePage();
    }

    private int PageCount => Count / ItemsPerPage;

    private void ChangePage()
    {
        bool hasPages = Count > ItemsPerPage;
        B_Up.IsVisible = hasPages && Page > 0;
        B_Down.IsVisible = hasPages && Page < PageCount - 1;
        L_Page.IsVisible = hasPages;

        L_Page.Text = $"{Page + 1}/{Count / ItemsPerPage}";
        LoadItems();
    }

    public void LoadItems()
    {
        for (int i = 0; i < SlotPictureBoxes.Count; i++)
        {
            var item = GetItem(i);
            SlotPictureBoxes[i].SetItem(item);
        }
        ItemUpdated();
    }

    private void ClearItemIf(Func<Item, bool> criteria)
    {
        bool all = UiUtil.ModifierKeys == KeyModifiers.Shift;
        int start = 0, end = Items.Count;
        if (!all)
        {
            start = ItemsPerPage * Page;
            end = start + ItemsPerPage;
        }
        for (int i = start; i < end; i++)
        {
            var item = Items[i];
            if (criteria(item))
                item.Delete();
        }
        LoadItems();
        Sound.Asterisk();
    }

    private void B_SortAlpha_Click(object? sender, RoutedEventArgs e)
    {
        Page = 0;
        ChangePage(); // reset to beginning before perform sort

        var sortedItems = Items.Where(item => item.ItemId != Item.NONE)
            .OrderBy(item => GetItemText(item).ToLower());
        var sortedItemsCopy = new List<Item>(); // to prevent object reference issues

        foreach (var item in sortedItems)
        {
            var itemCopy = new Item();
            itemCopy.CopyFrom(item);
            sortedItemsCopy.Add(itemCopy);
        }

        SetEditorItems(sortedItemsCopy);
    }

    private void B_SortType_Click(object? sender, RoutedEventArgs e)
    {
        Page = 0;
        ChangePage(); // reset to beginning before perform sort

        var sortedItems = Items.Where(item => item.ItemId != Item.NONE)
            .OrderBy(ItemInfo.GetItemKind)
            .ThenBy(item => GetItemText(item).ToLower());
        var sortedItemsCopy = new List<Item>(); // to prevent object reference issues

        foreach (var item in sortedItems)
        {
            var itemCopy = new Item();
            itemCopy.CopyFrom(item);
            sortedItemsCopy.Add(itemCopy);
        }

        SetEditorItems(sortedItemsCopy);
    }

    private void SetEditorItems(List<Item> items)
    {
        if (items.Count > Items.Count)
            return;

        for (int i = 0; i < Items.Count; i++)
        {
            var src = i < items.Count ? items[i] : Item.NO_ITEM;
            Items[i].CopyFrom(src);
            ItemUpdated();
        }

        LoadItems();
        Editor.LoadItem(Item.NO_ITEM);
        Sound.Asterisk();
    }

    private void B_ClearAll_Click(object? sender, RoutedEventArgs e) => ClearItemIf(_ => true);
    private void B_ClearClothing_Click(object? sender, RoutedEventArgs e) => ClearItemIf(z => ItemInfo.GetItemKind(z).IsClothing);
    private void B_ClearCrafting_Click(object? sender, RoutedEventArgs e) => ClearItemIf(z => ItemInfo.GetItemKind(z).IsCrafting);
    private void B_ClearFurniture_Click(object? sender, RoutedEventArgs e) => ClearItemIf(z => ItemInfo.GetItemKind(z).IsFurniture);
    private void B_ClearBugs_Click(object? sender, RoutedEventArgs e) => ClearItemIf(z => GameLists.Bugs.Contains(z.ItemId));
    private void B_ClearFish_Click(object? sender, RoutedEventArgs e) => ClearItemIf(z => GameLists.Fish.Contains(z.ItemId));
    private void B_ClearDive_Click(object? sender, RoutedEventArgs e) => ClearItemIf(z => GameLists.Dive.Contains(z.ItemId));
}
