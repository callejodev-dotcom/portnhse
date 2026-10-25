using System.ComponentModel;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class AchievementEditor : NhseWindow
{
    private readonly Personal Personal;
    private readonly AchievementList List;
    private readonly AchievementRow[] Rows;
    private readonly Entry[] Entries;

    private int Index = -1;

    public AchievementEditor(Personal personal)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);

        Personal = personal;
        var list = List = personal.Achievements;
        var str = GameInfo.Strings.InternalNameTranslation;
        Entries = new Entry[list.Counts.Length];
        for (int i = 0; i < list.Counts.Length; i++)
            Entries[i] = new Entry(LifeSupportAchievement.GetName(i, list.Counts[i], str));
        LB_Counts.DisplayMemberBinding = new Binding(nameof(Entry.Text));
        LB_Counts.ItemsSource = Entries;

        Rows = [AR_1, AR_2, AR_3, AR_4, AR_5, AR_6];

        LB_Counts.SelectedIndex = 0;
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        SaveIndex(Index);
        Personal.Achievements = List;
        Close(true);
    }

    private void NUD_Count_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        if (Index < 0)
            return;

        List.Counts[Index] = (uint)(NUD_Count.Value ?? 0);

        for (int i = 0; i < Rows.Length; i++)
            Rows[i].ChangeCount(Index, i, List.Counts[Index]);

        SetEntryDescription(Index);
    }

    private void SetEntryDescription(int index)
    {
        Entries[index].Text = LifeSupportAchievement.GetName(index, List.Counts[index], GameInfo.Strings.InternalNameTranslation);
    }

    private void LB_Counts_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (LB_Counts.SelectedIndex < 0)
            return;

        SaveIndex(Index);
        LoadIndex(Index = LB_Counts.SelectedIndex);
    }

    private void LoadIndex(int index)
    {
        if (index < 0)
            return;

        for (int i = 0; i < Rows.Length; i++)
            Rows[i].LoadRow(List, Index, i);

        NUD_Unk.Value = List.Flags[index];

        var val = List.Counts[index];
        NUD_Count.Value = val;
    }

    private void SaveIndex(in int index)
    {
        if (index < 0)
            return;

        for (int i = 0; i < Rows.Length; i++)
            Rows[i].SaveRow(List, index, i);

        List.Flags[index] = (byte)(NUD_Unk.Value ?? 0);

        // count updated on value changed
    }

    private void B_Clear_Click(object? sender, RoutedEventArgs e)
    {
        List.ClearAll(Index);
        LoadIndex(Index);
        Sound.Asterisk();
    }

    private void B_Max_Click(object? sender, RoutedEventArgs e)
    {
        List.GiveAll(Index, DateTime.Now);
        LoadIndex(Index);
        Sound.Asterisk();
    }

    private void B_GiveAll_Click(object? sender, RoutedEventArgs e)
    {
        SaveIndex(Index);
        var index = Index;
        Index = -1;

        List.GiveAll(DateTime.Now);
        for (int i = 0; i < List.Counts.Length; i++)
            SetEntryDescription(i);

        LoadIndex(Index = index);
        Sound.Asterisk();
    }

    private void B_ClearAll_Click(object? sender, RoutedEventArgs e)
    {
        SaveIndex(Index);
        var index = Index;
        Index = -1;

        List.ClearAll();
        for (int i = 0; i < List.Counts.Length; i++)
            SetEntryDescription(i);

        LoadIndex(Index = index);
        Sound.Asterisk();
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
                field = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Text)));
            }
        } = text;
    }
}
