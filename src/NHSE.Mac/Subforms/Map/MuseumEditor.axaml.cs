using System.Collections.ObjectModel;
using Avalonia.Controls;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class MuseumEditor : NhseWindow
{
    private readonly Core.MuseumEditor Editor;
    private readonly ObservableCollection<string> Items = [];
    private int Index = -1;

    public MuseumEditor(Museum museum)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);

        Editor = new Core.MuseumEditor(museum);

        var str = GameInfo.Strings;
        ItemEdit.Initialize(str.ItemDataSource);
        for (int i = 0; i < Editor.Dates.Length; i++)
            Items.Add(Editor.GetDonationText(str, i));
        LB_Donations.ItemsSource = Items;

        LB_Donations.SelectedIndex = 0;
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        // WinForms only kept the selected donation's changes after selecting another one.
        SaveIndex(Index);
        Editor.Save();
        Close(true);
    }

    private void LB_Counts_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (LB_Donations.SelectedIndex < 0 || Changing)
            return;

        SaveIndex(Index);
        Index = LB_Donations.SelectedIndex;
        LoadIndex(Index);
    }

    private void LoadIndex(in int index)
    {
        if (index < 0)
            return;

        // Empty dates (DateTime.MinValue) are shown as no date.
        var date = (DateTime)Editor.Dates[index];
        CAL_Date.SelectedDate = date == DateTime.MinValue ? null : date;
        NUD_Player.Value = Editor.Players[index];
        ItemEdit.LoadItem(Editor.Items[index]);
    }

    private void SaveIndex(in int index)
    {
        if (index < 0)
            return;

        var date = CAL_Date.SelectedDate ?? DateTime.MinValue;
        Editor.Dates[index] = date;
        Editor.Players[index] = (byte)(NUD_Player.Value ?? 0);
        ItemEdit.SetItem(Editor.Items[index]);

        SetNameForIndex(index);
    }

    private bool Changing;

    private void SetNameForIndex(in int index)
    {
        var text = Editor.GetDonationText(GameInfo.Strings, index);
        if (Items[index] == text)
            return;

        // Replacing the selected row's text deselects it in Avalonia: keep the selection.
        Changing = true;
        var selected = LB_Donations.SelectedIndex;
        Items[index] = text;
        LB_Donations.SelectedIndex = selected;
        Changing = false;
    }

    private async void B_Dump_Click(object? sender, RoutedEventArgs e)
    {
        Editor.Save();
        await MiscDumpHelper.DumpMuseum(this, Editor.Museum);
    }

    private async void B_Load_Click(object? sender, RoutedEventArgs e)
    {
        if (!await MiscDumpHelper.LoadMuseum(this, Editor.Museum))
            return;

        // data imported, mark as saved and close
        Close(true);
    }

    private void B_GiveAll_Click(object? sender, RoutedEventArgs e)
    {
        SaveIndex(Index);
        Editor.GiveAll(GameInfo.GetStrings("en").itemlist, 1000);
        Editor.Save();
        Sound.Asterisk();
        Close(true);
    }

    private void B_SortDate_Click(object? sender, RoutedEventArgs e)
    {
        SaveIndex(Index);
        Editor.SortAll();
        Editor.Save();
        Sound.Asterisk();
        Close(true);
    }
}
