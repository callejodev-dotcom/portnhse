using System.Diagnostics;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using NHSE.Core;
using NHSE.Injection;

namespace NHSE.Mac;

public partial class PlayerItemEditor : NhseWindow
{
    private readonly Action LoadItems;
    private readonly ItemGridEditor ItemGrid;
    private readonly ItemArrayEditor<Item> ItemArray;

    public PlayerItemEditor() : this([], 1, 1) { }

    public PlayerItemEditor(IReadOnlyList<Item> array, int width, int height, bool sysbot = false)
    {
        InitializeComponent();
        ItemArray = new ItemArrayEditor<Item>(array);

        var Editor = ItemGrid = new ItemGridEditor(ItemEditor, array);
        Editor.InitializeGrid(width, height, 64, 64);
        PAN_Items.Content = Editor;

        ItemEditor.Initialize(GameInfo.Strings.ItemDataSource);
        Editor.LoadItems();
        LoadItems = () => Editor.LoadItems();
        B_Inject.IsVisible = sysbot;

        EnableDragDrop(PAN_Items);
        EnableDragDrop(ItemEditor);
        this.TranslateInterface(GameInfo.CurrentLanguage);
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e) => Close(true);

    private async void B_Dump_Click(object? sender, RoutedEventArgs e)
    {
        var path = await Pickers.SaveFile(this, "New Horizons Inventory (*.nhi)|*.nhi|All files (*.*)|*.*", "items.nhi");
        if (path is null)
            return;
        var bytes = ItemArray.Write();
        await File.WriteAllBytesAsync(path, bytes);
    }

    private async void B_Load_Click(object? sender, RoutedEventArgs e)
    {
        var modifiers = UiUtil.ModifierKeys;
        bool skipOccupiedSlots = (modifiers & KeyModifiers.Alt) != 0;
        bool importCheatClipboard = UiUtil.IsCommand(modifiers);
        if (importCheatClipboard && await UiUtil.GetClipboardText(this) is { } text)
        {
            var bytes = ItemCheatCode.ReadCode(text);
            if (bytes.Length % ItemArray.ItemSize == 0)
            {
                ImportItemData(bytes, skipOccupiedSlots);
                return;
            }
        }

        var path = await Pickers.OpenFile(this, "New Horizons Inventory (*.nhi)|*.nhi|All files (*.*)|*.*", "items.nhi");
        if (path is null)
            return;

        var data = await File.ReadAllBytesAsync(path);
        ImportItemData(data, skipOccupiedSlots);
    }

    private void ImportItemData(byte[] data, bool skipOccupiedSlots, int start = 0)
    {
        ItemArray.ImportItemDataX(data, skipOccupiedSlots, start);

        LoadItems();
        Sound.Asterisk();
    }

    private void B_Inject_Click(object? sender, RoutedEventArgs e)
    {
        var exist = UiUtil.FirstFormOfType<SysBotUI>();
        if (exist != null)
        {
            exist.Activate();
            return;
        }

        void AfterRead(InjectionResult r)
        {
            if (r == InjectionResult.Success)
                Avalonia.Threading.Dispatcher.UIThread.Post(LoadItems);
        }

        static void AfterWrite(InjectionResult r)
        {
            Debug.WriteLine($"Write result: {r}");
            Sound.Asterisk();
        }

        var sb = new SysBotController(InjectionType.Pouch);
        var pockInject = new PocketInjector(ItemArray.Items, sb.Bot);
        var ai = new AutoInjector(pockInject, AfterRead, AfterWrite);
        var ub = new USBBotController();
        var pockInjectUSB = new PocketInjector(ItemArray.Items, ub.Bot);
        var aiUSB = new AutoInjector(pockInjectUSB, AfterRead, AfterWrite);

        ItemGrid.ItemChanged = () => ai.Write();
        var sysbot = new SysBotUI(ai, sb, aiUSB, ub);
        sysbot.Show(this);
    }

    private void EnableDragDrop(Control target)
    {
        DragDrop.SetAllowDrop(target, true);
        target.AddHandler(DragDrop.DragOverEvent, ItemEditor_DragEnter);
        target.AddHandler(DragDrop.DropEvent, (s, e) => PlayerItemEditor_DragDrop(target, e));
    }

    private static void ItemEditor_DragEnter(object? sender, DragEventArgs e)
    {
        e.DragEffects = e.DataTransfer.Contains(DataFormat.File) ? DragDropEffects.Copy : DragDropEffects.None;
    }

    private void PlayerItemEditor_DragDrop(Control target, DragEventArgs e)
    {
        var files = e.DataTransfer.TryGetFiles();
        if (files is not { Length: 1 } || files[0].TryGetLocalPath() is not { } path || Directory.Exists(path))
            return;

        var fi = new FileInfo(path);
        if (fi.Length > ItemArray.TotalSize || fi.Length % Item.SIZE != 0)
            return;

        Sound.Asterisk();
        var data = File.ReadAllBytes(path);
        if (target == ItemEditor)
        {
            var item = new Item(System.Buffers.Binary.BinaryPrimitives.ReadUInt64LittleEndian(data));
            ItemEditor.LoadItem(item);
        }
        else
        {
            bool skipOccupiedSlots = (e.KeyModifiers & KeyModifiers.Alt) != 0;
            ImportItemData(data, skipOccupiedSlots);
        }
        e.Handled = true;
    }
}
