using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Media;
using NHSE.Core;
using static NHSE.Core.Mail;

namespace NHSE.Mac;

public partial class PostBoxEditor : NhseWindow
{
    private const string PostFilter = "New Horizons Post (*.nhpb)|*.nhpb|All files (*.*)|*.*";

    private readonly Player Player;
    private readonly PostBox PostBox;

    private readonly string[] Wrap;
    private readonly string[] Box;
    private Item Attachment;
    private List<int> PostSlots;

    private int SlotIndex => (int)(NUD_Post_Slots.Value ?? 0);

    private string GetUpdatedVillagerInternalName() => VillagerUtil.GetInternalVillagerName((VillagerSpecies)(int)(NUD_Species.Value ?? 0), (int)(NUD_Variant.Value ?? 0));

    public PostBoxEditor(Player p, PostBox pb)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);

        Player = p;
        PostBox = pb;
        Attachment = new Item();

        PostSlots = PostBox.GetUsedPostBoxSlots();

        NUD_Post_Slots.Maximum = MaxSlots;

        L_Post_UsedSlots_Value.Text = PostSlots.Count + " / 300";

        CB_Post_SenderType.SetComboItems(NPCTypeList);
        var senderNPC = SenderNPCData.Values.Select(npc => GameInfo.Strings.GetVillager(npc.NameConversion));
        CB_Post_NPC.SetComboItems(senderNPC);
        CB_Post_Language.SetComboItems(BCP47LanguageString.Keys);

        CB_Post_Stationery.SetComboItems(LetterStationerySkin.Values.Select(skin => GameInfo.Strings.GetMail(skin.name)));

        TB_Post_ToLine.MaxLength = LetterToTextLength;
        TB_Post_Message.MaxLength = LetterMessageTextLength;
        TB_Post_FromLine.MaxLength = LetterFromTextLength;

        ItemEditor.Initialize(GameInfo.Strings.ItemDataSource, true, true);

        CB_Post_MailType.SetComboItems(MailTypeList);

        Wrap = ItemWrappingExtended.ItemWrappingSkin.Values.Select(skin => GameInfo.Strings.GetMail(skin.name)).ToArray();
        Box = ItemWrappingExtended.ItemBoxSkin.Values.Select(skin => GameInfo.Strings.GetMail(skin.name)).ToArray();

        LoadMailItem();
    }

    private void ChangeVillager(object? sender, NumericUpDownValueChangedEventArgs e) => ChangeVillager();
    private void ChangeVillager()
    {
        var index = SlotIndex;

        if (!PostBox.GetMailCheck(SlotIndex))
        {
            return;
        }

        if (CB_Post_SenderType.SelectedIndex != 0)
        {
            var spVal = (int)(NUD_Species.Value ?? 0);
            var vaVal = (int)(NUD_Variant.Value ?? 0);

            if (spVal >= 35 || (spVal == 25 && vaVal == 0) || (spVal == 22 && vaVal == 0))
            {
                CB_Post_SenderType.SelectedIndex = 2;
            }
            else
            {
                CB_Post_SenderType.SelectedIndex = 1;
            }
        }

        var name = GetUpdatedVillagerInternalName();
        L_InternalName.Text = name;
        L_ExternalName.Text = GameInfo.Strings.GetVillager(name);

        var sprite = VillagerSprite.GetVillagerSprite(name);

        if (CB_Post_SenderType.SelectedIndex == 1) // Villager
        {
            GB_Post_NPC.IsEnabled = false;
            GB_Post_Player.IsEnabled = false;

            PB_Villager.Source = sprite;

            CB_Post_SenderType.SelectedIndex = 1;
            CB_Post_NPC.SelectedIndex = -1;
        }
        else if (CB_Post_SenderType.SelectedIndex == 2) // NPC
        {
            GB_Post_NPC.IsEnabled = true;
            GB_Post_Player.IsEnabled = false;

            var speciesValue = (int)(NUD_Species.Value ?? 0);
            if (!SenderNPCData.TryGetValue(speciesValue, out var npcData))
            {
                PB_Villager.Source = sprite;
                CB_Post_SenderType.SelectedIndex = 2;
                CB_Post_NPC.SelectedIndex = -1;
                return;
            }
            name = npcData.Image;

            sprite = VillagerSprite.GetVillagerSprite(name);
            var nameNPC = SenderNPCData[(int)(NUD_Species.Value ?? 0)].NameConversion;
            L_InternalName.Text = nameNPC;
            L_ExternalName.Text = GameInfo.Strings.GetVillager(nameNPC);

            PB_Villager.Source = sprite;

            CB_Post_SenderType.SelectedIndex = 2;
            CB_Post_NPC.SelectedIndex = npcData.MappingIndex;
        }
        else if (CB_Post_SenderType.SelectedIndex == 0) // Player
        {
            GB_Post_NPC.IsEnabled = false;
            GB_Post_Player.IsEnabled = true;

            sprite = VillagerSprite.GetVillagerSprite("Leaf");
            L_InternalName.Text = PostBox.GetSenderName(index);
            L_ExternalName.Text = PostBox.GetSenderName(index);

            PB_Villager.Source = sprite;

            CB_Post_SenderType.SelectedIndex = 0;
            CB_Post_NPC.SelectedIndex = -1;
        }
        else
        {
            var senderType = PostBox.GetSenderType(index);
            throw new NotSupportedException($"Unknown sender type: 0x{senderType:X2}");
        }
    }

    private void UpdateSenderNPC(object? sender, SelectionChangedEventArgs e)
    {
        var npcSelected = SenderNPCData.FirstOrDefault(x => x.Value.MappingIndex == CB_Post_NPC.SelectedIndex).Key;
        NUD_Species.Value = npcSelected;
    }

    private void UpdateMailType(object? sender, SelectionChangedEventArgs e)
    {
        if (CB_Post_MailType.SelectedIndex == 1) // Letter
        {
            CB_Post_Stationery.IsEnabled = true;
            CB_Post_AttachmentSkin.SetComboItems(Wrap);
            CB_Post_AttachmentSkin.SelectedIndex = 0;
        }
        else if (CB_Post_MailType.SelectedIndex == 0) // Package
        {
            CB_Post_Stationery.IsEnabled = false;
            CB_Post_AttachmentSkin.SetComboItems(Box);
            CB_Post_AttachmentSkin.SelectedIndex = 0;
            CB_Post_Stationery.SelectedIndex = -1;
        }
    }

    private void UpdateSenderType(object? sender, SelectionChangedEventArgs e)
    {
        if (CB_Post_SenderType.SelectedIndex == 0) // Player
        {
            GB_Post_NPC.IsEnabled = false;
            GB_Post_Player.IsEnabled = true;
            L_InternalName.Text = "player";
            L_ExternalName.Text = "Player";
            PB_Villager.Source = null;
        }
        else if (CB_Post_SenderType.SelectedIndex == 1) // Villager
        {
            GB_Post_NPC.IsEnabled = false;
            GB_Post_Player.IsEnabled = false;
        }
        else
        {
            GB_Post_NPC.IsEnabled = true;
            GB_Post_Player.IsEnabled = false;
        }
    }

    private void UpdateMailCounters(object? sender, TextChangedEventArgs e) => UpdateMailCounters();
    private void UpdateMailCounters()
    {
        static void UpdateCounter(TextBlock label, TextBox textBox, int maxLength)
        {
            var currentLength = (textBox.Text ?? string.Empty).Replace("\r\n", "\n").Length;
            label.Text = currentLength + " / " + maxLength;
            if (currentLength > maxLength)
                label.Foreground = Brushes.Red;
            else
                label.ClearValue(TextBlock.ForegroundProperty);
        }

        // Raised while the window is being built, before the boxes exist.
        if (TB_Post_ToLine is null || TB_Post_Message is null || TB_Post_FromLine is null)
            return;

        UpdateCounter(L_Post_CountTo, TB_Post_ToLine, LetterToTextLength);
        UpdateCounter(L_Post_CountMsg, TB_Post_Message, LetterMessageTextLength);
        UpdateCounter(L_Post_CountFrom, TB_Post_FromLine, LetterFromTextLength);
    }

    private void ResetInterface()
    {
        L_InternalName.Text = "";
        L_ExternalName.Text = "";
        PB_Villager.Source = null;
        TB_Post_ToLine.Text = "";
        TB_Post_Message.Text = "";
        TB_Post_FromLine.Text = "";
        CK_Post_Read.IsChecked = false;
        CK_Post_Faved.IsChecked = false;
        CAL_PostTimestamp.SelectedDate = DateTime.Now;
        CB_Post_SenderType.SelectedIndex = -1;
        CB_Post_NPC.SelectedIndex = -1;
        TB_Post_PlayerName.Text = "";
        TB_Post_ReceiverPlayer.Text = "";
        TB_Post_ReceiverIsland.Text = "";
        CB_Post_Language.SelectedIndex = -1;
        CB_Post_Stationery.SelectedIndex = -1;
        CB_Post_MailType.SelectedIndex = -1;
        CB_Post_AttachmentSkin.SetComboItems(Array.Empty<string>());
    }

    private void LoadMailItem(object? sender, NumericUpDownValueChangedEventArgs e) => LoadMailItem();
    private void LoadMailItem()
    {
        ResetInterface();

        if (PostBox.GetUsedPostBoxSlots().Count == 0)
        {
            ShowNoMail();
            return;
        }

        if (!PostBox.GetMailCheck(SlotIndex))
        {
            return;
        }

        LoadVillagerFromMail();
        LoadMailText();
        LoadMailProperties();
        LoadMailAttachment();
    }

    /// <summary>
    /// WinForms showed the message box right away, even from the constructor; here it waits for the window to be open.
    /// </summary>
    private void ShowNoMail()
    {
        const string msg = "No mail items found in the post box.";
        if (IsVisible)
        {
            _ = Dialogs.Alert(false, msg);
            return;
        }

        void OnOpened(object? sender, EventArgs e)
        {
            Opened -= OnOpened;
            _ = Dialogs.Alert(false, msg);
        }
        Opened += OnOpened;
    }

    private void LoadVillagerFromMail()
    {
        var index = SlotIndex;
        NUD_Species.Value = PostBox.GetVillagerSenderSpeciesId(index);
        NUD_Variant.Value = PostBox.GetVillagerSenderVariantId(index);

        CB_Post_SenderType.SelectedIndex = PostBox.GetSenderType(index);

        ChangeVillager();
    }

    private void LoadMailText()
    {
        var index = SlotIndex;

        var toText = PostBox.GetLetterToLineText(index);
        TB_Post_ToLine.Text = toText;
        // The game stores \n, which is also the line break of a macOS text box (WinForms needed \r\n).
        var msgText = PostBox.GetLetterMessageText(index);
        TB_Post_Message.Text = msgText;
        var fromText = PostBox.GetLetterFromLineText(index);
        TB_Post_FromLine.Text = fromText;

        UpdateMailCounters();
    }

    private void LoadMailProperties()
    {
        var index = SlotIndex;

        CK_Post_Read.IsChecked = PostBox.GetMailReadStatus(index);
        CK_Post_Faved.IsChecked = PostBox.GetMailFavStatus(index);
        DateTime timestamp = PostBox.GetTimestamp(index);
        CAL_PostTimestamp.SelectedDate = timestamp == DateTime.MinValue ? null : timestamp;
        CB_Post_SenderType.SelectedIndex = PostBox.GetSenderType(index);

        SenderNPCData.TryGetValue((int)(NUD_Species.Value ?? 0), out var npcData);

        if (CB_Post_SenderType.SelectedIndex == 2 && npcData != null) // NPC
            CB_Post_NPC.SelectedIndex = npcData.MappingIndex;

        if (CB_Post_SenderType.SelectedIndex == 0) // Player
            TB_Post_PlayerName.Text = PostBox.GetSenderName(index);

        TB_Post_ReceiverPlayer.Text = PostBox.GetReceiverPlayerName(index);
        TB_Post_ReceiverIsland.Text = PostBox.GetReceiverIslandName(index);

        var lang = PostBox.GetMailLanguageString(index);
        if (lang != null && BCP47LanguageString.TryGetValue(lang, out var langIndex))
        {
            CB_Post_Language.SelectedIndex = langIndex;
        }
        else
        {
            CB_Post_Language.SelectedIndex = -1;
        }

        var stationerySkin = PostBox.GetMailStationerySkin(index);
        if (LetterStationerySkin.TryGetValue(stationerySkin, out var skin))
        {
            CB_Post_Stationery.SelectedIndex = skin.index;
        }
        else
        {
            CB_Post_Stationery.SelectedIndex = -1;
        }
    }

    private void LoadMailAttachment()
    {
        var index = SlotIndex;

        var attachedItemId = PostBox.GetAttachedItem(index);
        Attachment = ItemEditor.LoadItem(attachedItemId);

        var wrap = PostBox.GetItemWrappingType(index);
        var letterType = PostBox.GetMailType(index);

        CB_Post_MailType.SelectedIndex = Convert.ToInt32(letterType);

        // WinForms indexed the dictionaries directly (an unknown skin threw); here it leaves the skin unselected.
        if (CB_Post_MailType.SelectedIndex == 1)
        {
            CB_Post_Stationery.IsEnabled = true;
            CB_Post_AttachmentSkin.SetComboItems(Wrap);
            CB_Post_AttachmentSkin.SelectedIndex = ItemWrappingExtended.ItemWrappingSkin.GetValueOrDefault(wrap)?.index ?? -1;
        }
        if (CB_Post_MailType.SelectedIndex == 0)
        {
            CB_Post_Stationery.IsEnabled = false;
            CB_Post_AttachmentSkin.SetComboItems(Box);
            CB_Post_AttachmentSkin.SelectedIndex = ItemWrappingExtended.ItemBoxSkin.GetValueOrDefault(wrap)?.index ?? -1;
            CB_Post_Stationery.SelectedIndex = -1;
        }
    }

    private void UpdateMailSlotIndex(int index)
    {
        if (!PostBox.GetMailCheck(index) || PostBox.GetPostBoxSortIndex(index) == 0x00)
        {
            var nextSlot = PostBox.GetLatestUniqueId();
            PostBox.SetPostBoxSortIndex(index, (byte)nextSlot);
            PostBox.SetLatestUniqueId(nextSlot + 1);
        }
    }

    private void UpdateMailSenderInfo(int index)
    {
        PostBox.SetSenderType(index, (byte)CB_Post_SenderType.SelectedIndex);
        if (CB_Post_SenderType.SelectedIndex == 0x00) // Player
        {
            PostBox.SetPlayerSenderIslandId(index, Player.Personal.TownID);
            PostBox.SetPlayerSenderIslandName(index, Player.Personal.TownName);
            PostBox.SetSenderId(index, Player.Personal.PlayerID);
            PostBox.SetSenderName(index, TB_Post_PlayerName.Text ?? string.Empty);
        }

        if (CB_Post_SenderType.SelectedIndex == 0x01) // Villager
        {
            PostBox.SetVillagerSenderSpeciesId(index, (int)(NUD_Species.Value ?? 0));
            PostBox.SetVillagerSenderVariantId(index, (int)(NUD_Variant.Value ?? 0));
            PostBox.SetNPCSenderIslandId(index, Player.Personal.TownID);
            PostBox.SetNPCSenderIslandName(index, Player.Personal.TownName);
            PostBox.SetSenderId(index, 0x02);
            // Don't set a sender nameNPC, this is blank for villagers.
        }

        if (CB_Post_SenderType.SelectedIndex == 0x02) // NPC
        {
            PostBox.SetVillagerSenderSpeciesId(index, (int)(NUD_Species.Value ?? 0));
            PostBox.SetVillagerSenderVariantId(index, (int)(NUD_Variant.Value ?? 0));
            // Don't set a sender island ID or nameNPC, this is blank for NPCs.
            PostBox.SetSenderId(index, 0x00);
            // Don't set a sender nameNPC, this is blank for NPCs.
        }
    }

    private void UpdateMailAttachment(int index)
    {
        PostBox.SetAttachedItem(index, ItemEditor.SetItem(Attachment));

        var selectedSkinIndex = CB_Post_AttachmentSkin.SelectedIndex;
        if (CB_Post_MailType.SelectedIndex == 1)
        {
            PostBox.SetMailType(index, true);
            var attachmentSkin = ItemWrappingExtended.ItemWrappingSkin.FirstOrDefault(x => x.Value.index == selectedSkinIndex);
            if (attachmentSkin.Value != null)
            {
                PostBox.SetItemWrappingType(index, (byte)attachmentSkin.Key);
            }

            var letterSkinIndex = CB_Post_Stationery.SelectedIndex;
            var letterSkin = (ushort)LetterStationerySkin.FirstOrDefault(skin => skin.Value.index == letterSkinIndex).Key;
            PostBox.SetMailStationerySkin(index, letterSkin);
        }
        else if (CB_Post_MailType.SelectedIndex == 0)
        {
            PostBox.SetMailType(index, false);
            var attachmentSkin = ItemWrappingExtended.ItemBoxSkin.FirstOrDefault(x => x.Value.index == selectedSkinIndex);
            if (attachmentSkin.Value != null)
            {
                PostBox.SetItemWrappingType(index, (byte)attachmentSkin.Key);
            }

            PostBox.SetMailStationerySkin(index, 0);
        }
    }

    private void UpdateMailReceiverInfo(int index)
    {
        PostBox.SetReceiverPlayerName(index, TB_Post_ReceiverPlayer.Text ?? string.Empty);
        PostBox.SetReceiverPlayerId(index, Player.Personal.PlayerID);
        PostBox.SetReceiverIslandName(index, TB_Post_ReceiverIsland.Text ?? string.Empty);
        PostBox.SetReceiverIslandId(index, Player.Personal.TownID);
    }

    private void UpdateMailText(int index)
    {
        var langString = BCP47LanguageString.FirstOrDefault(x => x.Value == CB_Post_Language.SelectedIndex).Key;
        PostBox.SetMailLanguageString(index, langString, CB_Post_SenderType.SelectedIndex == 0);
        PostBox.SetLetterToLineText(index, (TB_Post_ToLine.Text ?? string.Empty).Replace("\r\n", "\n").Replace("\n", ""));
        PostBox.SetLetterMessageText(index, (TB_Post_Message.Text ?? string.Empty).Replace("\r\n", "\n"));
        PostBox.SetLetterFromLineText(index, (TB_Post_FromLine.Text ?? string.Empty).Replace("\r\n", "\n").Replace("\n", ""));
    }

    private void UpdateMailItem()
    {
        var index = SlotIndex;

        UpdateMailSlotIndex(index);

        PostBox.WipeTxRxSection(index);

        PostBox.SetMailReadStatus(index, CK_Post_Read.IsChecked == true);
        PostBox.SetMailFavStatus(index, CK_Post_Faved.IsChecked == true);

        UpdateMailSenderInfo(index);

        UpdateMailReceiverInfo(index);

        UpdateMailAttachment(index);

        UpdateMailText(index);

        PostBox.SetTimeStamp(index, CAL_PostTimestamp.SelectedDate ?? DateTime.MinValue);
    }

    private async Task ImportPostItem(int index)
    {
        var currentIndex = PostBox.GetPostBoxSortIndex(index);
        int currentUniqueId = PostBox.GetLatestUniqueId();

        currentUniqueId = Math.Min(currentUniqueId, 299); // Cap unique ID to prevent overflow and ensure it stays within the 300 mail item limit.

        if (currentIndex != 0x00 || PostBox.GetMailCheck(index))
        {
            var path = await Pickers.OpenFile(this, PostFilter);
            if (path is null)
                return;
            var d = await File.ReadAllBytesAsync(path);
            PostBox.SetIndividualLetter(index, d);
            PostBox.SetPostBoxSortIndex(index, (byte)currentIndex);
        }
        else
        {
            var path = await Pickers.OpenFile(this, PostFilter);
            if (path is null)
                return;
            var d = await File.ReadAllBytesAsync(path);
            PostBox.SetIndividualLetter(index, d);
            PostBox.SetPostBoxSortIndex(index, (byte)currentUniqueId);
            PostBox.SetLatestUniqueId(Math.Min(currentUniqueId + 1, 299));
        }
        LoadMailItem();
    }

    private async Task DumpPostItem(int index)
    {
        var name = Player.Personal.PlayerName + "_Post_Slot" + index + "_" + DateTime.Now.ToString("yyyyMMddHHmmss");
        var path = await Pickers.SaveFile(this, PostFilter, $"{name}.nhpb");
        if (path is null)
            return;

        var d = PostBox.GetIndividualLetter(index);
        File.WriteAllBytes(path, d);
    }

    private async Task DumpAllPostItems()
    {
        var folder = await Pickers.OpenFolder(this);
        if (folder is null)
            return;
        var postSlots = PostBox.GetUsedPostBoxSlots();
        postSlots.ForEach(i =>
        {
            var name = Player.Personal.PlayerName + "_Post_Slot" + i + "_" + DateTime.Now.ToString("yyyyMMddHHmmss");
            var path = Path.Combine(folder, $"{name}.nhpb");
            var d = PostBox.GetIndividualLetter(i);
            File.WriteAllBytes(path, d);
        });
    }

    private async void B_Dump_Click(object? sender, RoutedEventArgs e)
    {
        var index = SlotIndex;

        if (sender == B_Post_DumpAll)
        {
            await DumpAllPostItems();
            Sound.Asterisk();
        }
        else
        {
            await DumpPostItem(index);
            Sound.Asterisk();
        }
    }

    private async void B_Import_Click(object? sender, RoutedEventArgs e)
    {
        var index = SlotIndex;
        await ImportPostItem(index);
        Sound.Asterisk();
    }

    private void B_Delete_Click(object? sender, RoutedEventArgs e)
    {
        var index = SlotIndex;

        var arr = new byte[Mail.SIZE];
        arr[AttachedItemID] = 0xFE;
        arr[AttachedItemID + 1] = 0xFF;
        arr[PostBoxSortIndex - 4] = 0xFE;
        arr[PostBoxSortIndex - 3] = 0xFF;
        Span<byte> bytes = arr;
        PostBox.SetIndividualLetter(index, bytes);

        Sound.Asterisk();
    }

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        UpdateMailItem();
        Sound.Asterisk();
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);
}
