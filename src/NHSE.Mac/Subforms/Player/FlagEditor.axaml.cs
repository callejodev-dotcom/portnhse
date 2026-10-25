using System.Runtime.InteropServices;
using System.Text;
using Avalonia.Controls;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class FlagEditor : NhseWindow
{
    private readonly short[] Counts;
    private readonly FlagListItem[] Rows;

    public FlagEditor(short[] counts)
    {
        Counts = counts;
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);
        Rows = FlagListItem.Create(counts.Length, GetName);
        LB_Counts.ItemsSource = Rows;
        LB_Counts.SelectedIndex = 0;
    }

    private string GetName(int index) => EventFlagPlayer.GetName((ushort)index, Counts[index], GameInfo.Strings.InternalNameTranslation);

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e) => Close(true);

    private int Index = -1;

    private void NUD_Count_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        if (Index < 0)
            return;

        Counts[Index] = (short)(NUD_Count.Value ?? 0);
        Rows[Index].Text = GetName(Index);
    }

    private void LB_Counts_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (LB_Counts.SelectedIndex < 0)
            return;

        NUD_Count.Value = Counts[Index = LB_Counts.SelectedIndex];
    }

    private async void B_Dump_Click(object? sender, RoutedEventArgs e)
    {
        var data = MemoryMarshal.AsBytes(Counts.AsSpan()).ToArray();
        await MiscDumpHelper.DumpFlags(this, data, nameof(EventFlagPlayer));
    }

    private async void B_Load_Click(object? sender, RoutedEventArgs e)
    {
        var data = await MiscDumpHelper.LoadFlags(this, Counts.Length * 2, nameof(EventFlagPlayer));
        if (data.Length != 0)
            Buffer.BlockCopy(data, 0, Counts, 0, data.Length);
        FlagListItem.Refresh(Rows, GetName);
        if (Index >= 0)
            NUD_Count.Value = Counts[Index];
    }

    private async void B_Copy_Click(object? sender, RoutedEventArgs e)
    {
        var exportBuffer = new StringBuilder();

        var str = GameInfo.Strings.InternalNameTranslation;
        for (ushort i = 0; i < Counts.Length; i++)
            exportBuffer.AppendLine(EventFlagPlayer.GetName(i, Counts[i], str));

        await MiscDumpHelper.DumpFlagsToClipboard(this, exportBuffer.ToString());
    }
}
