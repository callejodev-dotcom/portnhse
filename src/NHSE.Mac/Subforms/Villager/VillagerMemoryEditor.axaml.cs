using Avalonia.Controls;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class VillagerMemoryEditor : NhseWindow
{
    /// <summary>
    /// First date of the WinForms DateTimePicker: earlier dates (an unset date) were shown as it and saved as an empty date.
    /// Here the picker is left empty instead.
    /// </summary>
    private static readonly DateTime MinDate = new(1753, 1, 1);

    private readonly IVillager Villager;
    private readonly GSaveMemory[] Memories;
    private readonly FlagListItem[] PlayerRows;

    private int PlayerIndex = -1;
    private int FlagIndex = -1;
    private byte[] Flags = [];
    private FlagListItem[] FlagRows = [];

    private readonly TextBox[] Greetings;

    public VillagerMemoryEditor(IVillager villager)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);

        Greetings =
        [
            TB_Greeting,
            TB_Greeting1, TB_Greeting2, TB_Greeting3, TB_Greeting4, TB_Greeting5,
            TB_Greeting6, TB_Greeting7, TB_Greeting8, TB_Greeting9, TB_Greeting10,
        ];

        Villager = villager;
        Memories = villager.GetMemories();

        PlayerRows = FlagListItem.Create(Memories.Length, GetPlayerSummary);
        LB_Players.ItemsSource = PlayerRows;

        LB_Players.SelectedIndex = 0;

        LB_Counts.SelectedIndex = 0;
    }

    private string GetPlayerSummary(int i) => $"{i} - {Memories[i].PlayerName} ({Memories[i].TownName})";

    private void UpdatePlayerIslandStrings() => FlagListItem.Refresh(PlayerRows, GetPlayerSummary);

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        SavePlayer(PlayerIndex);
        Villager.SetMemories(Memories);
        Close(true);
    }

    private static string GetFlagDescription(int index, ReadOnlySpan<byte> flags, IReadOnlyDictionary<string, string> str)
    {
        return EventFlagVillagerMemoryPlayer.GetName((ushort)index, flags[index], str);
    }

    private void NUD_Count_ValueChanged(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        var flagIndex = FlagIndex;
        if (flagIndex < 0)
            return;

        Flags[flagIndex] = (byte)(NUD_Count.Value ?? 0);
        FlagRows[flagIndex].Text = GetFlagDescription(flagIndex, Flags, GameInfo.Strings.InternalNameTranslation);
    }

    private void LB_Counts_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (LB_Counts.SelectedIndex < 0)
            return;

        NUD_Count.Value = Flags[FlagIndex = LB_Counts.SelectedIndex];
    }

    private void LB_Players_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        SavePlayer(PlayerIndex);
        PlayerIndex = LB_Players.SelectedIndex;
        if (PlayerIndex >= 0)
            LoadPlayer(PlayerIndex);
    }

    private void LoadPlayer(int playerIndex)
    {
        FlagIndex = -1;
        var index = LB_Counts.SelectedIndex;
        var memory = Memories[playerIndex];
        var flags = memory.GetEventFlags();

        var str = GameInfo.Strings.InternalNameTranslation;
        FlagRows = FlagListItem.Create(flags.Length, i => GetFlagDescription(i, flags, str));
        LB_Counts.ItemsSource = FlagRows;

        Flags = flags;
        LB_Counts.SelectedIndex = FlagIndex = index;

        TB_NickName.Text = memory.NickName;
        for (int i = 0; i < Greetings.Length; i++)
            Greetings[i].Text = memory.GetGreeting(i);

        DateTime date = memory.GreetingSetDate;
        CAL_GreetDate.SelectedDate = date < MinDate ? null : date;

        TB_NamePlayer.Text = memory.PlayerName;
        TB_Island.Text = memory.TownName;
    }

    private void SavePlayer(int playerIndex)
    {
        if (playerIndex < 0)
            return;

        var memory = Memories[playerIndex];
        memory.SetEventFlags(Flags);

        memory.NickName = TB_NickName.Text ?? string.Empty;
        for (int i = 0; i < Greetings.Length; i++)
            memory.SetGreeting(Greetings[i].Text ?? string.Empty, i);
        memory.GreetingSetDate = CAL_GreetDate.SelectedDate is { } date ? (GSaveDate)date : new GSaveDate();
    }

    private async void B_Dump_Click(object? sender, RoutedEventArgs e)
    {
        SavePlayer(PlayerIndex);
        await MiscDumpHelper.DumpVillagerMemoryPlayer(this, Villager, Memories[PlayerIndex]);
    }

    private async void B_Load_Click(object? sender, RoutedEventArgs e)
    {
        if (!await MiscDumpHelper.LoadVillagerMemoryPlayer(this, Villager, Memories, PlayerIndex))
            return;
        LoadPlayer(PlayerIndex);
        Sound.Asterisk();
    }

    private void TB_NamePlayer_TextChanged(object? sender, TextChangedEventArgs e)
    {
        if (LB_Players.SelectedIndex < 0)
            return;
        Memories[LB_Players.SelectedIndex].PlayerName = TB_NamePlayer.Text ?? string.Empty; // this should be enough for player name changes within NHSE as identities are never changed by the end-user
        UpdatePlayerIslandStrings();
    }

    private void TB_Island_TextChanged(object? sender, TextChangedEventArgs e)
    {
        if (LB_Players.SelectedIndex < 0)
            return;
        Memories[LB_Players.SelectedIndex].TownName = TB_Island.Text ?? string.Empty; // as above
        UpdatePlayerIslandStrings();
    }
}
