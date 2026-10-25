using System.ComponentModel;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class RecipeListEditor : NhseWindow
{
    private readonly Player Player;
    private readonly RecipeBook Book;
    private const string Unknown = "???";
    private readonly List<Entry> Entries = [];

    public RecipeListEditor(Player player)
    {
        Player = player;
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);

        Book = player.Personal.GetRecipeBook();
        var gotoSource = FillCheckBoxes();
        gotoSource.SortByText();
        LB_Recipes.DisplayMemberBinding = new Binding(nameof(Entry.Text));
        LB_Recipes.ItemsSource = Entries;
        CB_Goto.SetComboItems(gotoSource);
        CB_Goto.SelectedIndex = 0; // WinForms selects the first item of a bound data source
    }

    private List<ComboItem> FillCheckBoxes()
    {
        var book = Book;
        var recipes = RecipeList.Recipes;
        var strings = GameInfo.Strings.itemlist;
        const ushort max = RecipeBook.RecipeCount;
        FillListBox(max, recipes, strings, book);

        var gotoSource = new List<ComboItem>();
        return GetJumpList(max, recipes, strings, gotoSource);
    }

    private void FillListBox(ushort max, IReadOnlyDictionary<ushort, ushort> recipes, ReadOnlySpan<string> strings, RecipeBook book)
    {
        Loading = true;
        for (ushort i = 0; i <= max; i++)
        {
            var name = GetItemName(i, recipes, strings);
            var text = GetEntryText(book, i, name);
            if (i < Entries.Count)
                Entries[i].Text = text; // refilled in place (WinForms cleared and refilled the list)
            else
                Entries.Add(new Entry(text));
        }
        Loading = false;
    }

    private static List<ComboItem> GetJumpList(ushort max, IReadOnlyDictionary<ushort, ushort> recipes, ReadOnlySpan<string> strings, List<ComboItem> gotoSource)
    {
        for (ushort i = 0; i <= max; i++)
        {
            var name = GetItemName(i, recipes, strings);
            if (recipes.ContainsKey(i))
                gotoSource.Add(new ComboItem(name, i));
        }

        return gotoSource;
    }

    private static string GetItemName(ushort index, IReadOnlyDictionary<ushort, ushort> recipes, ReadOnlySpan<string> itemNames)
    {
        bool exists = recipes.TryGetValue(index, out var value);
        string item = exists ? itemNames[value] : Unknown;
        if (string.IsNullOrEmpty(item))
            item = value.ToString();
        return item;
    }

    private static string GetEntryText(RecipeBook recipeBook, ushort index, string item)
    {
        var known = recipeBook.GetIsKnown(index) ? "✅" : "❌";
        return $"0x{index:X3} - {known} {item}";
    }

    private void B_All_Click(object? sender, RoutedEventArgs e) => DoAll(() => Book.GiveAll(RecipeList.Recipes, UiUtil.ModifierKeys != UiUtil.CommandModifier));
    private void B_ClearAll_Click(object? sender, RoutedEventArgs e) => DoAll(Book.ClearAll);
    private void B_CraftAll_Click(object? sender, RoutedEventArgs e) => DoAll(Book.CraftAll);

    private void DoAll(Action action)
    {
        var index = LB_Recipes.SelectedIndex;
        var book = Book;
        var recipes = RecipeList.Recipes;
        var strings = GameInfo.Strings.itemlist;
        const ushort max = RecipeBook.RecipeCount;

        action();
        FillListBox(max, recipes, strings, book);
        LoadIndex(index);
        LB_Recipes.SelectedIndex = index;
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        Player.Personal.SetRecipeBook(Book);
        Close(true);
    }

    private void CB_Goto_SelectedValueChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (CB_Goto.SelectedItem is null)
            return; // typing a name that matches nothing yet
        LB_Recipes.SelectedIndex = UiUtil.GetIndex(CB_Goto);
    }

    private int Index = -1;
    private bool Loading;

    private void LB_Recipes_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (Loading)
            return;
        SaveIndex(Index);
        Index = LB_Recipes.SelectedIndex;
        Loading = true;
        LoadIndex(Index);
        Loading = false;
    }

    private void LoadIndex(in int index)
    {
        if (index < 0)
            return;
        var book = Book;
        CHK_Known.IsChecked = book.GetIsKnown(index);
        CHK_New.IsChecked = book.GetIsNew(index);
        CHK_Made.IsChecked = book.GetIsMade(index);
        CHK_Favorite.IsChecked = book.GetIsFavorite(index);
    }

    private void SaveIndex(in int index)
    {
        if (index < 0)
            return;
        var book = Book;
        book.SetIsKnown(index, CHK_Known.IsChecked == true);
        book.SetIsNew(index, CHK_New.IsChecked == true);
        book.SetIsMade(index, CHK_Made.IsChecked == true);
        book.SetIsFavorite(index, CHK_Favorite.IsChecked == true);
    }

    private void CHK_Known_CheckedChanged(object? sender, RoutedEventArgs e)
    {
        if (Loading || Index < 0)
            return;
        Loading = true;
        var book = Book;
        book.SetIsKnown(Index, CHK_Known.IsChecked == true);
        var recipes = RecipeList.Recipes;
        var strings = GameInfo.Strings.itemlist;
        var name = GetItemName((ushort)Index, recipes, strings);
        Entries[Index].Text = GetEntryText(book, (ushort)Index, name);
        Loading = false;
    }

    /// <summary>
    /// Text of a list entry, updated in place (WinForms replaced the ListBox item).
    /// </summary>
    public sealed class Entry(string text) : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler? PropertyChanged;

        public string Text
        {
            get;
            set
            {
                if (field == value)
                    return;
                field = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Text)));
            }
        } = text;
    }
}
