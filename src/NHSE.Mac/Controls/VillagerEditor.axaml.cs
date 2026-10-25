using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using NHSE.Core;
using NHSE.Villagers;

namespace NHSE.Mac;

public partial class VillagerEditor : UserControl
{
    public IVillager[] Villagers;
    public IVillagerOrigin Origin;
    private readonly HorizonSave SAV;
    private int VillagerIndex = -1;
    private int PlayerIndex = -1;
    private bool Loading;

    private Window Owner => TopLevel.GetTopLevel(this) as Window ?? throw new InvalidOperationException("Villager editor without a window.");

    public VillagerEditor() : this([], null!, null!, false, 0) { }

    public VillagerEditor(IVillager[] villagers, IVillagerOrigin origin, HorizonSave sav, bool hasHouses, int playerIndex)
    {
        InitializeComponent();
        Villagers = villagers;
        Origin = origin;
        SAV = sav;
        PlayerIndex = playerIndex;
        if (villagers.Length != 0)
            LoadVillagers();

        B_EditHouses.IsVisible = hasHouses;
    }

    public void Save() => SaveVillager(VillagerIndex);

    public void Reload() => LoadVillagers();

    private void LoadVillagers()
    {
        var personalities = Enum.GetNames<VillagerPersonality>();
        CB_Personality.SetComboItems(personalities);

        VillagerIndex = -1;
        NUD_Villager.Maximum = Villagers.Length - 1;
        NUD_Villager.Value = 0;
        LoadVillager(0);
    }

    private void LoadVillager(object? sender, NumericUpDownValueChangedEventArgs e)
    {
        if (Villagers.Length != 0 && (int)(NUD_Villager.Value ?? 0) != VillagerIndex)
            LoadVillager((int)(NUD_Villager.Value ?? 0));
    }

    private void LoadVillager(int index)
    {
        if (VillagerIndex >= 0)
            SaveVillager(VillagerIndex);

        if (index < 0)
            return;

        LoadVillager(Villagers[index]);
        VillagerIndex = index;
    }

    private void LoadVillager(IVillager v)
    {
        Loading = true;
        NUD_Species.Value = v.Species;
        NUD_Variant.Value = v.Variant;
        CB_Personality.SelectedIndex = (int)v.Personality;
        TB_Catchphrase.Text = v.CatchPhrase;
        CHK_VillagerMovingOut.IsChecked = v.MovingOut;
        ChangeVillager();
        Loading = false;
    }

    private void SaveVillager(int index)
    {
        if (index < 0)
            return;
        var v = Villagers[index];

        v.Species = (byte)(NUD_Species.Value ?? 0);
        v.Variant = (byte)(NUD_Variant.Value ?? 0);
        v.Personality = (VillagerPersonality)CB_Personality.SelectedIndex;
        v.CatchPhrase = TB_Catchphrase.Text ?? string.Empty;
        v.MovingOut = CHK_VillagerMovingOut.IsChecked == true;
    }

    private async void CHK_VillagerMovingOut_CheckedChanged(object? sender, RoutedEventArgs e)
    {
        if (Loading)
            return;

        if (CHK_VillagerMovingOut.IsChecked != true)
            return;

        await Dialogs.Alert(MessageStrings.MsgMoveOut, MessageStrings.MsgMoveOutSuggest);
    }

    private async void B_DumpVillager_Click(object? sender, RoutedEventArgs e)
    {
        if (UiUtil.ModifierKeys == KeyModifiers.Shift)
        {
            var folder = await Pickers.OpenFolder(Owner);
            if (folder is null || !Directory.Exists(folder))
                return;
            Villagers.Dump(folder);
            return;
        }

        var name = L_ExternalName.Text;
        const string filter = "New Horizons Villager (*.nhv)|*.nhv|" +
                              "New Horizons Villager (*.nhv2)|*.nhv2|" +
                              "All files (*.*)|*.*";
        var path = await Pickers.SaveFile(Owner, filter, $"{name}.{Villagers[VillagerIndex].Extension}");
        if (path is null)
            return;

        SaveVillager(VillagerIndex);
        var v = Villagers[VillagerIndex];
        await File.WriteAllBytesAsync(path, v.Write());
    }

    private async void B_LoadVillager_Click(object? sender, RoutedEventArgs e)
    {
        var name = L_ExternalName.Text;
        const string filter = "New Horizons Villager (*.nhv)|*.nhv|" +
                              "New Horizons Villager (*.nhv2)|*.nhv2|" +
                              "All files (*.*)|*.*";
        var path = await Pickers.OpenFile(Owner, filter, $"{name}.{Villagers[VillagerIndex].Extension}");
        if (path is null)
            return;

        var expectLength = SAV.Main.Offsets.VillagerSize;
        var fi = new FileInfo(path);
        if (!VillagerConverter.IsCompatible((int)fi.Length, expectLength))
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expectLength), path);
            return;
        }

        var data = await File.ReadAllBytesAsync(path);
        data = VillagerConverter.GetCompatible(data, expectLength);
        if (data.Length != expectLength)
        {
            await Dialogs.Error(string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expectLength), path);
            return;
        }

        var v = SAV.Main.Offsets.ReadVillager(data);
        var player0 = Origin;
        if (!v.IsOriginatedFrom(player0))
        {
            string msg = string.Format(MessageStrings.MsgDataDidNotOriginateFromHost_0, player0.PlayerName);
            var result = await Dialogs.Prompt(MessageBoxButtons.YesNoCancel, msg, MessageStrings.MsgAskUpdateValues);
            if (result == DialogResult.Cancel)
                return;
            if (result == DialogResult.Yes)
                v.ChangeOrigins(player0, v.Write());
        }

        LoadVillager(Villagers[VillagerIndex] = v);
    }

    private async void B_EditWear_Click(object? sender, RoutedEventArgs e)
    {
        var v = Villagers[VillagerIndex];
        var items = v.WearStockList;
        var editor = new PlayerItemEditor(items, 8, 3);
        if (await editor.ShowDialog<bool>(Owner))
            v.WearStockList = items;
    }

    private async void B_EditFurniture_Click(object? sender, RoutedEventArgs e)
    {
        var v = Villagers[VillagerIndex];
        var items = v.FtrStockList;
        var editor = new PlayerItemEditor(items, 8, 4);
        if (await editor.ShowDialog<bool>(Owner))
            v.FtrStockList = items;
    }

    private async void B_EditVillagerFlags_Click(object? sender, RoutedEventArgs e)
    {
        var v = Villagers[VillagerIndex];
        var flags = v.GetEventFlagsSave();
        var editor = new VillagerFlagEditor(flags);
        if (await editor.ShowDialog<bool>(Owner))
            v.SetEventFlagsSave(flags);
    }

    private string GetCurrentVillagerInternalName() => VillagerUtil.GetInternalVillagerName((VillagerSpecies)(NUD_Species.Value ?? 0), (int)(NUD_Variant.Value ?? 0));
    private void ChangeVillager(object? sender, NumericUpDownValueChangedEventArgs e) => ChangeVillager();
    private void ChangeVillager()
    {
        var name = GetCurrentVillagerInternalName();
        L_InternalName.Text = name;
        L_ExternalName.Text = GameInfo.Strings.GetVillager(name);
        PB_Villager.Source = VillagerSprite.GetVillagerSprite(name);
    }

    private async void B_EditHouse_Click(object? sender, RoutedEventArgs e)
    {
        SaveVillager(VillagerIndex);
        var villagers = SAV.Main.GetVillagers();
        var houses = SAV.Main.GetVillagerHouses();
        var editor = new VillagerHouseEditor(houses, villagers, SAV.Main, VillagerIndex);
        if (await editor.ShowDialog<bool>(Owner))
            SAV.Main.SetVillagerHouses(houses);
    }

    private async void B_EditVillagerRoom_Click(object? sender, RoutedEventArgs e)
    {
        var v = Villagers[VillagerIndex];
        var editor = new SaveRoomFloorWallEditor(v.Room);
        if (await editor.ShowDialog<bool>(Owner))
            v.Room = editor.Entity;
    }

    private async void B_EditVillagerDesign_Click(object? sender, RoutedEventArgs e)
    {
        var player = SAV.Players[Math.Max(0, PlayerIndex)];
        var v = Villagers[VillagerIndex];
        DesignPatternPRO[] tmp = [v.Design];
        var editor = new PatternEditorPRO(tmp, player);
        if (await editor.ShowDialog<bool>(Owner))
            v.Design = tmp[0];
    }

    private async void B_EditVillagerPlayerMemories_Click(object? sender, RoutedEventArgs e)
    {
        if (UiUtil.ModifierKeys == KeyModifiers.Shift)
        {
            var prompt = await Dialogs.Prompt(MessageBoxButtons.YesNo, MessageStrings.MsgVillagerFriendshipMax);
            if (prompt != DialogResult.Yes)
                return;
            foreach (var villager in Villagers)
                villager.SetFriendshipAll();
            Sound.Asterisk();
            return;
        }

        var v = Villagers[VillagerIndex];
        var editor = new VillagerMemoryEditor(v);
        await editor.ShowDialog<bool>(Owner); // editor saves our changes
    }

    private async void B_EditDIYTimer_Click(object? sender, RoutedEventArgs e)
    {
        var v = Villagers[VillagerIndex];
        var editor = new VillagerDIYTimerEditor(v);
        await editor.ShowDialog<bool>(Owner); // editor saves our changes
    }

    private async void B_MoveOutAllVillagers_Click(object? sender, RoutedEventArgs e)
    {
        if (Loading)
            return;

        var prompt = await Dialogs.Prompt(MessageBoxButtons.OKCancel, MessageStrings.MsgMoveOutAll);
        if (prompt != DialogResult.OK)
            return;

        foreach (var villager in Villagers)
            villager.MovingOut = true;
        Loading = true;
        CHK_VillagerMovingOut.IsChecked = true;
        Loading = false;

        Sound.Asterisk();
    }

    private void B_SetPhraseOriginal_Click(object? sender, RoutedEventArgs e)
    {
        var internalName = GetCurrentVillagerInternalName();
        TB_Catchphrase.Text = GameInfo.Strings.GetVillagerDefaultPhrase(internalName);
    }

    private async void B_ReplaceVillager_Click(object? sender, RoutedEventArgs e)
    {
        var text = await UiUtil.GetClipboardText(this);
        if (string.IsNullOrWhiteSpace(text))
        {
            await Dialogs.Error(MessageStrings.MsgVillagerReplaceNoText);
            return;
        }

        var internalName = text.Trim();
        if (!VillagerResources.IsVillagerDataKnown(internalName))
        {
            var clip = internalName;
            internalName = GameInfo.Strings.VillagerMap.FirstOrDefault(z => string.Equals(z.Value, internalName, StringComparison.InvariantCultureIgnoreCase)).Key;
            if (internalName is null)
            {
                await Dialogs.Error(string.Format(MessageStrings.MsgVillagerReplaceUnknownName, clip));
                return;
            }
        }

        var index = VillagerIndex;
        var villager = Villagers[index];
        if (villager is not Villager2 v2)
        {
            await Dialogs.Error(MessageStrings.MsgVillagerReplaceOutdatedSaveFormat);
            return;
        }

        var houses = SAV.Main.GetVillagerHouses();
        var houseIndex = Array.FindIndex(houses, z => z.NPC1 == index);
        var exist = new VillagerInfo(v2, houses[houseIndex]);
        var replace = VillagerSwap.GetReplacementVillager(exist, internalName);

        var nh = new VillagerHouse1(replace.House);
        SAV.Main.SetVillagerHouse(nh, houseIndex);
        var nv = new Villager2(replace.Villager);
        LoadVillager(Villagers[index] = nv);
        Sound.Asterisk();
    }
}
