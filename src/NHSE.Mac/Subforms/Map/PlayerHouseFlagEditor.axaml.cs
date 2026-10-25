using System.Collections.ObjectModel;
using Avalonia.Controls;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class PlayerHouseFlagEditor : NhseWindow
{
    private readonly short[] Counts;
    private readonly ObservableCollection<string> Items = [];

    public PlayerHouseFlagEditor(short[] counts)
    {
        Counts = counts;
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);
        var str = GameInfo.Strings.InternalNameTranslation;
        for (ushort i = 0; i < counts.Length; i++)
            Items.Add(EventFlagHouse.GetName(i, counts[i], str));
        LB_Counts.ItemsSource = Items;
        LB_Counts.SelectedIndex = 0;
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e) => Close(true);

    private int Index;
    private bool Changing;

    private void NUD_Count_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        if (Index < 0 || Changing)
            return;

        Counts[Index] = (short)(NUD_Count.Value ?? 0);
        SetText(Index);
    }

    private void SetText(int index)
    {
        var text = EventFlagHouse.GetName((ushort)index, Counts[index], GameInfo.Strings.InternalNameTranslation);
        if (Items[index] == text)
            return;

        // Replacing the text deselects the row in Avalonia: select it again.
        Changing = true;
        var selected = LB_Counts.SelectedIndex;
        Items[index] = text;
        LB_Counts.SelectedIndex = selected;
        Changing = false;
    }

    private void LB_Counts_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (LB_Counts.SelectedIndex < 0 || Changing)
            return;

        NUD_Count.Value = Counts[Index = LB_Counts.SelectedIndex];
    }

    private async void B_Dump_Click(object? sender, RoutedEventArgs e)
    {
        byte[] data = new byte[Counts.Length * 2];
        Buffer.BlockCopy(Counts, 0, data, 0, data.Length);
        await MiscDumpHelper.DumpFlags(this, data, nameof(EventFlagHouse));
    }

    private async void B_Load_Click(object? sender, RoutedEventArgs e)
    {
        var data = await MiscDumpHelper.LoadFlags(this, Counts.Length * 2, nameof(EventFlagHouse));
        if (data.Length != 0)
            Buffer.BlockCopy(data, 0, Counts, 0, data.Length);

        // Show the loaded values (LB_Counts.SelectedIndex = LB_Counts.SelectedIndex in WinForms).
        for (int i = 0; i < Counts.Length; i++)
            SetText(i);
        Changing = true;
        NUD_Count.Value = Counts[Index];
        Changing = false;
    }
}
