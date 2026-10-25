using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Friendly editor of a list of items (pockets, storage, recycle box, a villager's furniture or clothes).
/// Click a space to choose it, pick the item from a searchable list with pictures, and only the options that
/// make sense for that item are shown (how many, color and pattern, recipe, gift wrap). Items can be dragged.
/// The list is edited in place; the caller keeps it only if the window returns true.
/// </summary>
public sealed class ItemsWindow : NhseWindow
{
    private const double SlotSize = 58;

    private readonly IReadOnlyList<Item> Items;
    private readonly int PerPage;
    private readonly List<ItemSlot> Slots = [];
    private readonly ContentControl Details = new();
    private readonly TextBlock PageLabel = new() { VerticalAlignment = VerticalAlignment.Center, Classes = { "subtle" } };
    private readonly Button PrevPage = new() { Content = "‹", Width = 34 };
    private readonly Button NextPage = new() { Content = "›", Width = 34 };
    private readonly UniformGrid SlotGrid;

    private int Page;
    private int Selected; // absolute index in Items
    private Item? Copied;

    // Drag and drop between spaces
    private int DragFrom = -1;
    private Point DragStart;
    private bool Dragging;

    private int PageCount => (Items.Count + PerPage - 1) / PerPage;

    public ItemsWindow(IReadOnlyList<Item> items, int columns, int rows, string title, string? hint = null)
    {
        Items = items;
        PerPage = columns * rows;
        Title = title;
        SizeToContent = SizeToContent.WidthAndHeight;
        CanResize = false;

        SlotGrid = new UniformGrid { Columns = columns, ColumnSpacing = 4, RowSpacing = 4 };
        for (int i = 0; i < PerPage; i++)
        {
            var slot = new ItemSlot { Index = i, Width = SlotSize, Height = SlotSize, GameStyleMarkup = true, ContextMenu = SlotMenu() };
            slot.PointerPressed += Slot_PointerPressed;
            slot.PointerMoved += Slot_PointerMoved;
            slot.PointerReleased += Slot_PointerReleased;
            SetItemTip(slot, () => GetItem(slot.Index));
            Slots.Add(slot);
            SlotGrid.Children.Add(slot);
        }

        PrevPage.Click += (_, _) => ChangePage(Page - 1);
        NextPage.Click += (_, _) => ChangePage(Page + 1);
        var pager = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, HorizontalAlignment = HorizontalAlignment.Center, Margin = new Thickness(0, 10, 0, 0) };
        pager.Children.Add(PrevPage);
        pager.Children.Add(PageLabel);
        pager.Children.Add(NextPage);
        pager.IsVisible = PageCount > 1;

        var left = new StackPanel();
        left.Children.Add(SlotGrid);
        left.Children.Add(pager);
        left.Children.Add(Toolbar());

        var detailsCard = new Border { Child = Details, Classes = { "card" }, Width = 370, Margin = new Thickness(18, 0, 0, 0), VerticalAlignment = VerticalAlignment.Stretch };

        var header = new StackPanel { Margin = new Thickness(0, 0, 0, 14) };
        header.Children.Add(Text(title, "title"));
        header.Children.Add(Text(hint ?? HomeStrings.ItemsHint, "subtle"));

        var save = new Button { Content = HomeStrings.Done, Classes = { "accent" }, IsDefault = true, MinWidth = 100 };
        save.Click += (_, _) => Close(true);
        var cancel = new Button { Content = HomeStrings.Cancel, IsCancel = true, MinWidth = 100 };
        cancel.Click += (_, _) => Close(false);
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, HorizontalAlignment = HorizontalAlignment.Right, Margin = new Thickness(0, 16, 0, 0) };
        buttons.Children.Add(cancel);
        buttons.Children.Add(save);

        var body = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,Auto") };
        body.Children.Add(left);
        Grid.SetColumn(detailsCard, 1);
        body.Children.Add(detailsCard);

        var root = new DockPanel { Margin = new Thickness(22, 18, 22, 18) };
        DockPanel.SetDock(header, Dock.Top);
        DockPanel.SetDock(buttons, Dock.Bottom);
        root.Children.Add(header);
        root.Children.Add(buttons);
        root.Children.Add(body);
        Content = root;
        this.Bind(BackgroundProperty, this.GetResourceObservable("NhWindowBrush"));

        ChangePage(0);
        Select(0);
    }

    #region Spaces
    private Item GetItem(int slotIndex) => Items[(Page * PerPage) + slotIndex];

    private static string GetName(Item item) => item.IsNone ? HomeStrings.EmptySlot : GameInfo.Strings.GetItemName(item);

    private void ChangePage(int page)
    {
        Page = Math.Clamp(page, 0, Math.Max(0, PageCount - 1));
        PageLabel.Text = string.Format(HomeStrings.PageOf, Page + 1, PageCount);
        PrevPage.IsEnabled = Page > 0;
        NextPage.IsEnabled = Page < PageCount - 1;
        for (int i = 0; i < Slots.Count; i++)
        {
            var absolute = (Page * PerPage) + i;
            Slots[i].IsVisible = absolute < Items.Count;
            if (absolute < Items.Count)
                Slots[i].SetItem(Items[absolute]);
        }
        Select((Page * PerPage) + Math.Max(0, Selected % PerPage));
    }

    private void Select(int absolute)
    {
        if (Items.Count == 0)
            return;
        Selected = Math.Clamp(absolute, 0, Items.Count - 1);
        var slot = Selected - (Page * PerPage);
        for (int i = 0; i < Slots.Count; i++)
            Slots[i].IsSelected = i == slot;
        BuildDetails();
    }

    private void Refresh(int absolute)
    {
        var slot = absolute - (Page * PerPage);
        if ((uint)slot < Slots.Count)
            Slots[slot].SetItem(Items[absolute]);
    }

    private void RefreshAll() => ChangePage(Page);

    private ItemSlot? SlotAt(PointerEventArgs e)
    {
        var p = e.GetPosition(SlotGrid);
        return Slots.FirstOrDefault(z => z.IsVisible && z.Bounds.Contains(p));
    }

    private void Slot_PointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (sender is not ItemSlot slot || !e.GetCurrentPoint(slot).Properties.IsLeftButtonPressed)
            return;
        Select((Page * PerPage) + slot.Index);
        DragFrom = Selected;
        DragStart = e.GetPosition(SlotGrid);
        Dragging = false;
        if (e.ClickCount == 2)
            FocusSearch?.Invoke();
        e.Handled = true;
    }

    private void Slot_PointerMoved(object? sender, PointerEventArgs e)
    {
        if (DragFrom < 0)
            return;
        var p = e.GetPosition(SlotGrid);
        if (!Dragging && (Math.Abs(p.X - DragStart.X) > 6 || Math.Abs(p.Y - DragStart.Y) > 6))
        {
            Dragging = true;
            Cursor = new Cursor(StandardCursorType.DragMove);
        }
    }

    private void Slot_PointerReleased(object? sender, PointerReleasedEventArgs e)
    {
        var from = DragFrom;
        var wasDragging = Dragging;
        DragFrom = -1;
        Dragging = false;
        Cursor = Cursor.Default;
        if (!wasDragging || from < 0 || SlotAt(e) is not { } target)
            return;

        var to = (Page * PerPage) + target.Index;
        if (to == from || to >= Items.Count)
            return;
        // Swap the two spaces.
        var tmp = new Item();
        tmp.CopyFrom(Items[to]);
        Items[to].CopyFrom(Items[from]);
        Items[from].CopyFrom(tmp);
        Refresh(from);
        Refresh(to);
        Select(to);
    }

    private ContextMenu SlotMenu()
    {
        var clear = new MenuItem { Header = HomeStrings.ClearSlot };
        clear.Click += (_, _) => ClearSelected();
        var fill = new MenuItem { Header = HomeStrings.FillAll };
        fill.Click += async (_, _) => await FillPage();
        var menu = new ContextMenu();
        menu.Items.Add(clear);
        menu.Items.Add(fill);
        menu.Opening += (s, _) =>
        {
            // Right click also chooses the space under the pointer.
            if (s is ContextMenu { PlacementTarget: ItemSlot slot })
                Select((Page * PerPage) + slot.Index);
        };
        return menu;
    }

    private void ClearSelected()
    {
        Items[Selected].Delete();
        Refresh(Selected);
        BuildDetails();
    }

    private async Task FillPage()
    {
        var item = Items[Selected];
        if (item.IsNone)
            return;
        if (await Dialogs.Prompt(MessageBoxButtons.YesNo, string.Format(HomeStrings.FillAllAsk, GetName(item))) != DialogResult.Yes)
            return;
        var start = Page * PerPage;
        for (int i = start; i < Math.Min(Items.Count, start + PerPage); i++)
            Items[i].CopyFrom(item);
        RefreshAll();
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.Source is TextBox or ComboBox or NumericUpDown)
        {
            base.OnKeyDown(e);
            return;
        }
        var cols = SlotGrid.Columns;
        switch (e.Key)
        {
            case Key.Delete or Key.Back:
                ClearSelected();
                break;
            case Key.Left: MoveSelection(-1); break;
            case Key.Right: MoveSelection(1); break;
            case Key.Up: MoveSelection(-cols); break;
            case Key.Down: MoveSelection(cols); break;
            case Key.C when e.KeyModifiers == UiUtil.CommandModifier:
                Copied = new Item();
                Copied.CopyFrom(Items[Selected]);
                break;
            case Key.V when e.KeyModifiers == UiUtil.CommandModifier && Copied is not null:
                Items[Selected].CopyFrom(Copied);
                Refresh(Selected);
                BuildDetails();
                break;
            default:
                base.OnKeyDown(e);
                return;
        }
        e.Handled = true;
    }

    private void MoveSelection(int delta)
    {
        var target = Math.Clamp(Selected + delta, 0, Items.Count - 1);
        var page = target / PerPage;
        if (page != Page)
        {
            Selected = target;
            ChangePage(page);
        }
        else
        {
            Select(target);
        }
    }
    #endregion

    #region Details of the chosen space
    private Action? FocusSearch;

    private void BuildDetails()
    {
        var item = Items[Selected];
        var panel = new StackPanel { Spacing = 10 };

        // Picture and name
        var big = new ItemSlot { Width = 76, Height = 76, ShowMarkup = false, Cursor = Cursor.Default };
        big.SetItem(item);
        var name = Text(GetName(item), "cardtitle");
        name.FontSize = 17;
        var top = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 12 };
        top.Children.Add(big);
        var nameBox = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Spacing = 4 };
        nameBox.Children.Add(name);
        if (!item.IsNone)
        {
            var clear = new Button { Content = HomeStrings.ClearSlot, Padding = new Thickness(10, 3) };
            clear.Click += (_, _) => ClearSelected();
            nameBox.Children.Add(clear);
        }
        Grid.SetColumn(nameBox, 1);
        top.Children.Add(nameBox);
        panel.Children.Add(top);

        // Options that make sense for this item
        if (!item.IsNone)
            AddOptions(panel, item);

        // Choose another item
        panel.Children.Add(new TextBlock { Text = HomeStrings.ChangeItem, FontWeight = FontWeight.SemiBold, Margin = new Thickness(0, 6, 0, 0) });
        var search = new ItemSearchBox(item.IsNone ? 280 : 180);
        search.ItemChosen += (id, count) =>
        {
            var target = Items[Selected];
            target.Delete();
            target.ItemId = id;
            target.Count = count;
            Refresh(Selected);
            BuildDetails();
        };
        FocusSearch = search.FocusSearch;
        panel.Children.Add(search);

        // Everything else, for those who know (built when opened: the classic editor loads every item name)
        var advanced = new StackPanel();
        var expander = new Expander { Header = HomeStrings.AdvancedOptions, Content = advanced, HorizontalAlignment = HorizontalAlignment.Stretch };
        expander.PropertyChanged += (_, e) =>
        {
            if (e.Property != Expander.IsExpandedProperty || !expander.IsExpanded || advanced.Children.Count != 0)
                return;
            var classic = new ItemEditor();
            classic.Initialize(GameInfo.Strings.ItemDataSource);
            classic.LoadItem(Items[Selected]);
            var apply = new Button { Content = HomeStrings.Apply, Margin = new Thickness(0, 6, 0, 0) };
            apply.Click += (_, _) =>
            {
                classic.SetItem(Items[Selected]);
                Refresh(Selected);
                BuildDetails();
            };
            advanced.Children.Add(classic);
            advanced.Children.Add(apply);
        };
        panel.Children.Add(expander);

        Details.Content = new ScrollViewer { Content = panel, MaxHeight = 560, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled };
    }

    private void AddOptions(StackPanel panel, Item item)
    {
        var id = item.ItemId;
        var kind = ItemInfo.GetItemKind(item);

        // How many (the game stores the amount minus one)
        if (ItemInfo.TryGetMaxStackCount(item, out var max) && max > 1 && !kind.IsFlowerGene(id))
        {
            var qty = new NumericUpDown { Minimum = 1, Maximum = max, Value = Math.Min(max, item.Count + 1), Width = 110 };
            qty.ValueChanged += (_, _) =>
            {
                Items[Selected].Count = (ushort)((qty.Value ?? 1) - 1);
                Refresh(Selected);
            };
            var maxButton = new Button { Content = HomeStrings.MaxButton };
            maxButton.Click += (_, _) => qty.Value = max;
            panel.Children.Add(Labeled(HomeStrings.Quantity, qty, maxButton));
        }

        // Color and pattern of furniture and clothes
        var remake = ItemRemakeUtil.GetRemakeIndex(id);
        if (remake >= 0)
        {
            var info = ItemRemakeInfoData.List[remake];
            var str = GameInfo.Strings;
            var bodies = Enumerable.Range(0, ItemRemakeInfo.BodyColorCountMax).Where(info.HasBodyColor).ToArray();
            if (bodies.Length > 1)
            {
                var names = bodies.Select(i => str.BodyColor.GetValueOrDefault($"{info.ItemUniqueID:00000}_{i}", info.GetBodyDescription(i))).ToArray();
                var body = new ComboBox { ItemsSource = names, SelectedIndex = Array.IndexOf(bodies, item.BodyType), MinWidth = 170 };
                body.SelectionChanged += (_, _) =>
                {
                    if (body.SelectedIndex < 0)
                        return;
                    Items[Selected].BodyType = bodies[body.SelectedIndex];
                    Refresh(Selected);
                };
                panel.Children.Add(Labeled(HomeStrings.Variant, body));
            }

            var fabrics = Enumerable.Range(0, ItemRemakeInfo.BodyColorCountMax).Where(info.HasFabricColor).ToArray();
            if (fabrics.Length > 1)
            {
                var names = fabrics.Select(i => str.FabricColor.GetValueOrDefault($"{info.ItemUniqueID:00000}_{i}", info.GetFabricDescription(i))).ToArray();
                var fabric = new ComboBox { ItemsSource = names, SelectedIndex = Array.IndexOf(fabrics, item.PatternChoice), MinWidth = 170 };
                fabric.SelectionChanged += (_, _) =>
                {
                    if (fabric.SelectedIndex < 0)
                        return;
                    Items[Selected].PatternChoice = fabrics[fabric.SelectedIndex];
                    Refresh(Selected);
                };
                panel.Children.Add(Labeled(HomeStrings.Fabric, fabric));
            }
        }

        // DIY recipe cards and unknown fossils keep the real item in the count
        if (kind is ItemKind.Kind_DIYRecipe or ItemKind.Kind_MessageBottle or ItemKind.Kind_FossilUnknown)
        {
            var source = kind == ItemKind.Kind_FossilUnknown
                ? GameInfo.Strings.CreateItemDataSource(GameLists.Fossils, false)
                : GameInfo.Strings.CreateItemDataSource(RecipeList.Recipes, false);
            var cb = new ComboBox { IsEditable = true, MaxDropDownHeight = 360, Width = 200 };
            cb.SetComboItems(source);
            cb.SelectedValue = (int)item.Count;
            cb.SelectionChanged += (_, _) =>
            {
                if (cb.SelectedItem is null)
                    return;
                Items[Selected].Count = (ushort)UiUtil.GetIndex(cb);
                Refresh(Selected);
            };
            panel.Children.Add(Labeled(kind == ItemKind.Kind_FossilUnknown ? HomeStrings.FossilOf : HomeStrings.RecipeOf, cb));
        }

        // Gift wrap
        if (kind != ItemKind.Kind_MessageBottle && id < 60_000)
        {
            string[] papers = // ItemWrappingPaper order
            [
                HomeStrings.ColorYellow, HomeStrings.ColorPink, HomeStrings.ColorOrange, HomeStrings.ColorLightGreen,
                HomeStrings.ColorGreen, HomeStrings.ColorMint, HomeStrings.ColorLightBlue, HomeStrings.ColorPurple,
                HomeStrings.ColorNavy, HomeStrings.ColorBlue, HomeStrings.ColorWhite, HomeStrings.ColorRed,
                HomeStrings.ColorGold, HomeStrings.ColorBrown, HomeStrings.ColorGray, HomeStrings.ColorBlack,
            ];
            var paper = new ComboBox { ItemsSource = papers, SelectedIndex = (int)item.WrappingPaper, MinWidth = 140, IsVisible = item.WrappingType != 0 };
            var gift = new CheckBox { Content = HomeStrings.Gift, IsChecked = item.WrappingType != 0 };
            void ApplyGift()
            {
                var target = Items[Selected];
                if (gift.IsChecked == true)
                    target.SetWrapping(ItemWrapping.WrappingPaper, (ItemWrappingPaper)Math.Max(0, paper.SelectedIndex), true);
                else
                    target.SetWrapping(0, 0);
                paper.IsVisible = gift.IsChecked == true;
                Refresh(Selected);
            }
            gift.IsCheckedChanged += (_, _) => ApplyGift();
            paper.SelectionChanged += (_, _) =>
            {
                if (gift.IsChecked == true)
                    ApplyGift();
            };
            var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
            row.Children.Add(gift);
            row.Children.Add(paper);
            panel.Children.Add(row);
        }
    }

    private static Grid Labeled(string label, params Control[] controls)
    {
        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("90,*"), ColumnSpacing = 8 };
        g.Children.Add(new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center });
        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6 };
        foreach (var c in controls)
            row.Children.Add(c);
        Grid.SetColumn(row, 1);
        g.Children.Add(row);
        return g;
    }
    #endregion

    #region Sort, empty, import and export
    private Control Toolbar()
    {
        MenuItem Item(string text, Action action)
        {
            var m = new MenuItem { Header = text };
            m.Click += (_, _) => action();
            return m;
        }

        Button Menu(string text, params MenuItem[] items)
        {
            var flyout = new MenuFlyout { Placement = PlacementMode.TopEdgeAlignedLeft };
            foreach (var i in items)
                flyout.Items.Add(i);
            return new Button { Content = text + " ▾", Flyout = flyout };
        }

        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Margin = new Thickness(0, 14, 0, 0) };
        row.Children.Add(Menu(HomeStrings.Sort,
            Item(HomeStrings.SortAZ, () => SortBy(z => GetName(z).ToLowerInvariant())),
            Item(HomeStrings.SortType, () => SortBy(z => $"{(int)ItemInfo.GetItemKind(z):000} {GetName(z).ToLowerInvariant()}"))));
        row.Children.Add(Menu(HomeStrings.Clear,
            Item(HomeStrings.ClearAll, () => ClearIf(_ => true)),
            Item(HomeStrings.ClearClothes, () => ClearIf(z => ItemInfo.GetItemKind(z).IsClothing)),
            Item(HomeStrings.ClearMaterials, () => ClearIf(z => ItemInfo.GetItemKind(z).IsCrafting)),
            Item(HomeStrings.ClearFurniture, () => ClearIf(z => ItemInfo.GetItemKind(z).IsFurniture)),
            Item(HomeStrings.ClearBugs, () => ClearIf(z => GameLists.Bugs.Contains(z.ItemId))),
            Item(HomeStrings.ClearFish, () => ClearIf(z => GameLists.Fish.Contains(z.ItemId))),
            Item(HomeStrings.ClearDive, () => ClearIf(z => GameLists.Dive.Contains(z.ItemId)))));
        row.Children.Add(Menu(HomeStrings.More,
            Item(HomeStrings.ExportItems, () => _ = Export()),
            Item(HomeStrings.ImportItems, () => _ = Import())));
        return row;
    }

    private void SortBy(Func<Item, string> key)
    {
        var sorted = Items.Where(z => !z.IsNone).OrderBy(key).Select(z =>
        {
            var copy = new Item();
            copy.CopyFrom(z);
            return copy;
        }).ToList();
        for (int i = 0; i < Items.Count; i++)
            Items[i].CopyFrom(i < sorted.Count ? sorted[i] : Item.NO_ITEM);
        RefreshAll();
    }

    private void ClearIf(Func<Item, bool> criteria)
    {
        foreach (var item in Items.Where(z => !z.IsNone && criteria(z)))
            item.Delete();
        RefreshAll();
    }

    private async Task Export()
    {
        var path = await Pickers.SaveFile(this, "New Horizons Inventory (*.nhi)|*.nhi|All files (*.*)|*.*", "items.nhi");
        if (path is null)
            return;
        await File.WriteAllBytesAsync(path, new ItemArrayEditor<Item>(Items).Write());
    }

    private async Task Import()
    {
        var path = await Pickers.OpenFile(this, "New Horizons Inventory (*.nhi)|*.nhi|All files (*.*)|*.*", "items.nhi");
        if (path is null)
            return;
        var data = await File.ReadAllBytesAsync(path);
        new ItemArrayEditor<Item>(Items).ImportItemDataX(data, false, 0);
        RefreshAll();
    }
    #endregion
}
