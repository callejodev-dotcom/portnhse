using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Data;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;
using static NHSE.Core.Mail;

namespace NHSE.Mac;

/// <summary>
/// The mailbox as a list of letters (who sent it, how it starts, the date and the gift), and the chosen letter
/// to read or change: to, message, signature, the attached gift, read/favourite. Letters can be written and deleted.
/// The letters are edited in place; «Cancel» puts the whole mailbox back as it was.
/// </summary>
public sealed class MailWindow : PlayerDialog
{
    private const string PostFilter = "New Horizons Post (*.nhpb)|*.nhpb|All files (*.*)|*.*";

    private readonly HorizonSave SAV;
    private readonly Player Player;
    private readonly PostBox Box;
    private readonly byte[] Backup;
    private readonly int BackupUniqueId;
    private readonly ListBox List;
    private readonly ContentControl Details = new();
    private readonly TextBlock Count = new() { Classes = { "subtle" }, VerticalAlignment = VerticalAlignment.Center };
    private readonly TextBlock Empty = PlayerUi.Subtle(PlayerStrings.MailEmpty, 14);
    private List<LetterRow> Rows = [];

    public MailWindow(HorizonSave sav, Player player) : base(HomeStrings.Mail, 1080, 760)
    {
        SAV = sav;
        Player = player;
        Box = player.PostBox;
        Backup = Box.MailListWholeRegion.ToArray();
        BackupUniqueId = Box.GetLatestUniqueId();

        List = new ListBox
        {
            ItemTemplate = new FuncDataTemplate<LetterRow>((r, _) => RowView(r)),
            Background = Brushes.Transparent,
        };
        List.SelectionChanged += (_, _) => BuildDetails();

        var write = PlayerUi.Button(PlayerStrings.NewLetter, () => WriteLetter(), accent: true);
        write.HorizontalAlignment = HorizontalAlignment.Left;
        write.Margin = new Thickness(0, 0, 0, 10);

        Empty.HorizontalAlignment = HorizontalAlignment.Center;
        Empty.VerticalAlignment = VerticalAlignment.Center;
        var listArea = new Grid();
        listArea.Children.Add(List);
        listArea.Children.Add(Empty);
        var listCard = new Border { Child = listArea, Classes = { "card" }, Padding = new Thickness(6) };
        var left = new DockPanel();
        DockPanel.SetDock(write, Dock.Top);
        left.Children.Add(write);
        left.Children.Add(listCard);

        var detailsCard = new Border { Child = Details, Classes = { "card" }, Width = 450, Margin = new Thickness(16, 0, 0, 0) };
        var body = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto") };
        body.Children.Add(left);
        Grid.SetColumn(detailsCard, 1);
        body.Children.Add(detailsCard);

        var more = PlayerUi.MenuButton(HomeStrings.More,
            (PlayerStrings.ExportLetter, () => _ = ExportLetter()),
            (PlayerStrings.ExportAllLetters, () => _ = ExportAll()),
            (PlayerStrings.ImportLetter, () => _ = ImportLetter()));
        SetLayout(HomeStrings.Mail, PlayerStrings.MailHint, body, more, Count);

        Reload();
        if (Rows.Count != 0)
            List.SelectedIndex = 0;
        else
            BuildDetails();
    }

    #region List of letters
    /// <summary>
    /// Reads the used slots again, newest first.
    /// </summary>
    private void Reload()
    {
        Rows = [.. Box.GetUsedPostBoxSlots()
            .Select(z => new LetterRow(this, Box, z))
            .OrderByDescending(z => z.SortDate ?? DateTime.MinValue)
            .ThenByDescending(z => Box.GetPostBoxSortIndex(z.Slot))];
        List.ItemsSource = Rows;
        Empty.IsVisible = Rows.Count == 0;
        Count.Text = string.Format(HomeStrings.MailHint, Rows.Count);
    }

    private void SelectSlot(int slot)
    {
        if (slot < 0)
            return;
        Reload();
        List.SelectedItem = Rows.FirstOrDefault(z => z.Slot == slot);
        if (List.SelectedItem is not null)
            List.ScrollIntoView(List.SelectedItem);
    }

    /// <summary>
    /// «Write a letter»: a new letter, chosen in the list.
    /// </summary>
    internal int WriteLetter()
    {
        var slot = NewLetter();
        SelectSlot(slot);
        return slot;
    }

    /// <summary>
    /// Attaches an item to the chosen letter.
    /// </summary>
    internal void AttachGift(ushort id, ushort count)
    {
        if (List.SelectedItem is not LetterRow row)
            return;
        SetGift(row.Slot, id, count);
        RefreshSelected();
    }

    /// <summary>
    /// Letters in the list (used by the tests).
    /// </summary>
    internal IReadOnlyList<LetterRow> Letters => Rows;

    internal void Select(int slot) => List.SelectedItem = Rows.FirstOrDefault(z => z.Slot == slot);

    private static Control RowView(LetterRow? r)
    {
        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto"), ColumnSpacing = 10, Margin = new Thickness(2, 3) };
        if (r is null)
            return g;
        var face = new Image { Source = r.SenderPicture, Width = 40, Height = 40, VerticalAlignment = VerticalAlignment.Center };
        g.Children.Add(face);

        var top = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 6 };
        var unread = new Border { Width = 8, Height = 8, CornerRadius = new CornerRadius(4), VerticalAlignment = VerticalAlignment.Center };
        unread.Bind(Border.BackgroundProperty, unread.GetResourceObservable("NhGreenBrush"));
        unread.Bind(IsVisibleProperty, new Binding(nameof(LetterRow.Unread)));
        ToolTip.SetTip(unread, PlayerStrings.Unread);
        top.Children.Add(unread);
        var sender = new TextBlock { FontWeight = FontWeight.SemiBold, FontSize = 14, TextTrimming = TextTrimming.CharacterEllipsis };
        sender.Bind(TextBlock.TextProperty, new Binding(nameof(LetterRow.Sender)));
        top.Children.Add(sender);
        var star = new TextBlock { Text = "★", Foreground = Brushes.Goldenrod, VerticalAlignment = VerticalAlignment.Center };
        star.Bind(IsVisibleProperty, new Binding(nameof(LetterRow.Favorite)));
        top.Children.Add(star);

        var text = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Spacing = 1 };
        text.Children.Add(top);
        var preview = PlayerUi.Subtle(string.Empty, 12);
        preview.TextWrapping = TextWrapping.NoWrap;
        preview.TextTrimming = TextTrimming.CharacterEllipsis;
        preview.Bind(TextBlock.TextProperty, new Binding(nameof(LetterRow.Preview)));
        text.Children.Add(preview);
        Grid.SetColumn(text, 1);
        g.Children.Add(text);

        var right = new StackPanel { HorizontalAlignment = HorizontalAlignment.Right, VerticalAlignment = VerticalAlignment.Center, Spacing = 2 };
        var date = PlayerUi.Subtle(string.Empty, 12);
        date.HorizontalAlignment = HorizontalAlignment.Right;
        date.Bind(TextBlock.TextProperty, new Binding(nameof(LetterRow.Date)));
        right.Children.Add(date);
        var gift = new Image { Width = 26, Height = 26, HorizontalAlignment = HorizontalAlignment.Right };
        gift.Bind(Image.SourceProperty, new Binding(nameof(LetterRow.GiftPicture)));
        right.Children.Add(gift);
        Grid.SetColumn(right, 2);
        g.Children.Add(right);
        return g;
    }
    #endregion

    #region Senders
    /// <summary>
    /// Sender of a letter: 0 = a player, 1 = a villager, 2 = a special character or a service (Nook Inc., Dodo Airlines…).
    /// </summary>
    internal (string Name, Bitmap? Picture) GetSender(int slot)
    {
        var species = Box.GetVillagerSenderSpeciesId(slot);
        var variant = Box.GetVillagerSenderVariantId(slot);
        switch (Box.GetSenderType(slot))
        {
            case 0:
                var name = Box.GetSenderName(slot);
                return (string.IsNullOrWhiteSpace(name) ? PlayerStrings.SomePlayer : name, ItemSprite.Leaf);
            case 1:
                var internalName = VillagerUtil.GetInternalVillagerName((VillagerSpecies)species, variant);
                return (GameInfo.Strings.GetVillager(internalName), VillagerSprite.GetVillagerSprite(internalName) ?? ItemSprite.Leaf);
            default:
                if (SenderNPCData.TryGetValue(species, out var npc))
                    return (NpcName(npc), NpcPicture(npc));
                return (PlayerStrings.SomeoneElse, ItemSprite.Leaf);
        }
    }

    private static string NpcName(SenderNPCInfo npc) => npc.NameConversion == "npc_mom" ? PlayerStrings.SenderMom : GameInfo.Strings.GetVillager(npc.NameConversion);

    private static Bitmap? NpcPicture(SenderNPCInfo npc)
        => VillagerSprite.GetVillagerSprite(npc.Image) ?? ItemSprite.GetResource($"sprites/MenuIcon/{npc.Image}.png") ?? ItemSprite.Leaf;

    /// <summary>
    /// The language tag the game writes in letters sent by players.
    /// </summary>
    private static string LanguageTag => GameInfo.CurrentLanguage switch
    {
        "jp" => "ja-JP",
        "de" => "de-DE",
        "es" => "es-ES",
        "fr" => "fr-FR",
        "it" => "it-IT",
        "ko" => "ko-KR",
        "zhs" => "zh-CN",
        "zht" => "zh-TW",
        _ => "en-US",
    };

    /// <summary>
    /// Changes who sent the letter, filling the sender fields as the classic editor does.
    /// </summary>
    /// <param name="type">0 = this player, 1 = a villager, 2 = a special character.</param>
    internal void SetSender(int slot, byte type, int species = 0, int variant = 0)
    {
        var read = Box.GetMailReadStatus(slot);
        var favorite = Box.GetMailFavStatus(slot);
        Box.WipeTxRxSection(slot);
        Box.SetMailReadStatus(slot, read);
        Box.SetMailFavStatus(slot, favorite);
        Box.SetSenderType(slot, type);

        var pers = Player.Personal;
        switch (type)
        {
            case 0:
                Box.SetPlayerSenderIslandId(slot, pers.TownID);
                Box.SetPlayerSenderIslandName(slot, pers.TownName);
                Box.SetSenderId(slot, pers.PlayerID);
                Box.SetSenderName(slot, pers.PlayerName);
                break;
            case 1:
                Box.SetVillagerSenderSpeciesId(slot, species);
                Box.SetVillagerSenderVariantId(slot, variant);
                Box.SetNPCSenderIslandId(slot, pers.TownID);
                Box.SetNPCSenderIslandName(slot, pers.TownName);
                Box.SetSenderId(slot, 0x02);
                break;
            default:
                Box.SetVillagerSenderSpeciesId(slot, species);
                Box.SetVillagerSenderVariantId(slot, variant);
                Box.SetSenderId(slot, 0x00);
                break;
        }
        Box.SetMailLanguageString(slot, LanguageTag, type == 0);
    }

    /// <summary>
    /// Menu of possible senders: a villager (chosen by name), a special character, or the player.
    /// </summary>
    private Button SenderButton(int slot)
    {
        var flyout = new MenuFlyout();
        var villager = new MenuItem { Header = PlayerStrings.SenderVillager };
        villager.Click += async (_, _) =>
        {
            var picker = new VillagerPicker();
            if (!await picker.ShowDialog<bool>(this) || picker.SelectedInternalName is not { Length: > 2 } name)
                return;
            if (!Enum.TryParse<VillagerSpecies>(name[..^2], out var species) || !int.TryParse(name[^2..], out var variant))
                return;
            SetSender(slot, 1, (int)species, variant);
            RefreshSelected();
        };
        flyout.Items.Add(villager);

        var characters = new MenuItem { Header = PlayerStrings.SenderCharacter };
        var seen = new HashSet<string>();
        foreach (var (key, npc) in SenderNPCData.OrderBy(z => NpcName(z.Value), StringComparer.CurrentCultureIgnoreCase))
        {
            if (!seen.Add(npc.NameConversion))
                continue; // C.J. and Flick appear twice
            var item = new MenuItem { Header = NpcName(npc), Icon = new Image { Source = NpcPicture(npc), Width = 20, Height = 20 } };
            var species = key;
            item.Click += (_, _) =>
            {
                SetSender(slot, 2, species);
                RefreshSelected();
            };
            characters.Items.Add(item);
        }
        flyout.Items.Add(characters);

        var me = new MenuItem { Header = string.Format(PlayerStrings.SenderMe, Player.Personal.PlayerName) };
        me.Click += (_, _) =>
        {
            SetSender(slot, 0);
            RefreshSelected();
        };
        flyout.Items.Add(me);

        return new Button { Content = PlayerStrings.ChangeSender + " ▾", Flyout = flyout, Padding = new Thickness(10, 4) };
    }
    #endregion

    #region New, delete, import and export
    private int FreeSlot()
    {
        for (int i = 0; i <= MaxSlots; i++)
        {
            if (!Box.GetMailCheck(i))
                return i;
        }
        return -1;
    }

    /// <summary>
    /// The bytes of an empty slot, as the classic editor leaves a deleted letter.
    /// </summary>
    private static byte[] EmptyLetter()
    {
        var arr = new byte[Mail.SIZE];
        arr[AttachedItemID] = 0xFE;
        arr[AttachedItemID + 1] = 0xFF;
        arr[PostBoxSortIndex - 4] = 0xFE;
        arr[PostBoxSortIndex - 3] = 0xFF;
        return arr;
    }

    private void TakeNextPosition(int slot)
    {
        var next = Box.GetLatestUniqueId();
        Box.SetPostBoxSortIndex(slot, (byte)next);
        Box.SetLatestUniqueId(next + 1);
    }

    /// <summary>
    /// Writes an unread letter in the first free slot, from the first villager of the island (or Tom Nook), dated today.
    /// </summary>
    /// <returns>The slot, or -1 if the mailbox is full.</returns>
    internal int NewLetter()
    {
        var slot = FreeSlot();
        if (slot < 0)
        {
            _ = Dialogs.Alert(PlayerStrings.MailFull);
            return -1;
        }

        Box.SetIndividualLetter(slot, EmptyLetter());
        TakeNextPosition(slot);

        var villager = SAV.Main.GetVillagers().FirstOrDefault(z => z.Species != (byte)VillagerSpecies.non && VillagerSprite.GetVillagerSprite(z.InternalName) is not null);
        if (villager is not null)
            SetSender(slot, 1, villager.Species, villager.Variant);
        else
            SetSender(slot, 2, 22); // Tom Nook
        Box.SetMailReadStatus(slot, false);

        var pers = Player.Personal;
        Box.SetReceiverPlayerName(slot, pers.PlayerName);
        Box.SetReceiverPlayerId(slot, pers.PlayerID);
        Box.SetReceiverIslandName(slot, pers.TownName);
        Box.SetReceiverIslandId(slot, pers.TownID);
        Box.SetMailType(slot, true);
        Box.SetMailStationerySkin(slot, 0);
        Box.SetLetterToLineText(slot, pers.PlayerName);
        Box.SetLetterFromLineText(slot, GetSender(slot).Name);
        Box.SetTimeStamp(slot, DateTime.Today);
        return slot;
    }

    internal void DeleteLetter(int slot)
    {
        Box.SetIndividualLetter(slot, EmptyLetter());
        Reload();
        if (Rows.Count != 0)
            List.SelectedIndex = 0;
        else
            BuildDetails();
    }

    private async Task AskDelete(int slot)
    {
        if (await Dialogs.Prompt(MessageBoxButtons.YesNo, string.Format(PlayerStrings.DeleteLetterAsk, GetSender(slot).Name)) != DialogResult.Yes)
            return;
        DeleteLetter(slot);
    }

    private async Task ExportLetter()
    {
        if (List.SelectedItem is not LetterRow row)
            return;
        var name = $"{Player.Personal.PlayerName}_Post_Slot{row.Slot}_{DateTime.Now:yyyyMMddHHmmss}.nhpb";
        var path = await Pickers.SaveFile(this, PostFilter, name);
        if (path is null)
            return;
        await File.WriteAllBytesAsync(path, Box.GetIndividualLetter(row.Slot).ToArray());
    }

    private async Task ExportAll()
    {
        var folder = await Pickers.OpenFolder(this);
        if (folder is null)
            return;
        var stamp = DateTime.Now.ToString("yyyyMMddHHmmss");
        foreach (var slot in Box.GetUsedPostBoxSlots())
        {
            var path = Path.Combine(folder, $"{Player.Personal.PlayerName}_Post_Slot{slot}_{stamp}.nhpb");
            await File.WriteAllBytesAsync(path, Box.GetIndividualLetter(slot).ToArray());
        }
        Sound.Asterisk();
    }

    /// <summary>
    /// Puts a letter exported before (.nhpb) in a free slot.
    /// </summary>
    private async Task ImportLetter()
    {
        var path = await Pickers.OpenFile(this, PostFilter);
        if (path is null)
            return;
        var data = await File.ReadAllBytesAsync(path);
        if (data.Length != Mail.SIZE)
        {
            await Dialogs.Error(PlayerStrings.ImportLetterWrong);
            return;
        }
        var slot = FreeSlot();
        if (slot < 0)
        {
            await Dialogs.Alert(PlayerStrings.MailFull);
            return;
        }
        Box.SetIndividualLetter(slot, data);
        TakeNextPosition(slot);
        SelectSlot(slot);
    }
    #endregion

    #region The chosen letter
    private void RefreshSelected()
    {
        if (List.SelectedItem is LetterRow row)
            row.NotifyAll();
        BuildDetails();
    }

    private void BuildDetails()
    {
        if (List.SelectedItem is not LetterRow row)
        {
            Details.Content = new TextBlock
            {
                Text = Rows.Count == 0 ? PlayerStrings.MailEmptyHint : PlayerStrings.ChooseLetter,
                Classes = { "subtle" },
                TextWrapping = TextWrapping.Wrap,
                VerticalAlignment = VerticalAlignment.Center,
                HorizontalAlignment = HorizontalAlignment.Center,
                TextAlignment = TextAlignment.Center,
            };
            return;
        }
        var slot = row.Slot;
        var panel = new StackPanel { Spacing = 10 };

        // Who sent it, and when
        var (senderName, senderPicture) = GetSender(slot);
        var face = new Image { Source = senderPicture, Width = 56, Height = 56, VerticalAlignment = VerticalAlignment.Top };
        var who = new StackPanel { Spacing = 4, VerticalAlignment = VerticalAlignment.Center };
        var name = HomeUi.Text(string.Format(PlayerStrings.From, senderName), "cardtitle");
        name.FontSize = 17;
        who.Children.Add(name);
        who.Children.Add(SenderButton(slot));
        var head = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 12 };
        head.Children.Add(face);
        Grid.SetColumn(who, 1);
        head.Children.Add(who);
        panel.Children.Add(head);

        // The letter itself
        var to = new TextBox { Text = Box.GetLetterToLineText(slot), MaxLength = LetterToTextLength };
        to.TextChanged += (_, _) =>
        {
            Box.SetLetterToLineText(slot, OneLine(to.Text));
            row.NotifyAll();
        };
        var message = new TextBox
        {
            Text = Box.GetLetterMessageText(slot),
            MaxLength = LetterMessageTextLength,
            AcceptsReturn = true,
            TextWrapping = TextWrapping.Wrap,
            Height = 150,
            VerticalContentAlignment = VerticalAlignment.Top,
        };
        var counter = PlayerUi.Subtle(string.Empty, 11);
        counter.HorizontalAlignment = HorizontalAlignment.Right;
        void Count() => counter.Text = $"{(message.Text ?? string.Empty).Length} / {LetterMessageTextLength}";
        Count();
        message.TextChanged += (_, _) =>
        {
            Box.SetLetterMessageText(slot, (message.Text ?? string.Empty).Replace("\r\n", "\n"));
            Count();
            row.NotifyAll();
        };
        var signature = new TextBox { Text = Box.GetLetterFromLineText(slot), MaxLength = LetterFromTextLength };
        signature.TextChanged += (_, _) => Box.SetLetterFromLineText(slot, OneLine(signature.Text));

        panel.Children.Add(Field(PlayerStrings.LetterTo, to));
        panel.Children.Add(Field(PlayerStrings.LetterMessage, message));
        panel.Children.Add(counter);
        panel.Children.Add(Field(PlayerStrings.LetterSignature, signature));

        // The gift
        panel.Children.Add(Gift(row));

        // Read, favourite, date
        var read = new CheckBox { Content = PlayerStrings.Read, IsChecked = Box.GetMailReadStatus(slot) };
        read.IsCheckedChanged += (_, _) =>
        {
            Box.SetMailReadStatus(slot, read.IsChecked == true);
            row.NotifyAll();
        };
        var favorite = new CheckBox { Content = PlayerStrings.Favorite, IsChecked = Box.GetMailFavStatus(slot) };
        favorite.IsCheckedChanged += (_, _) =>
        {
            Box.SetMailFavStatus(slot, favorite.IsChecked == true);
            row.NotifyAll();
        };
        var date = PlayerUi.DatePicker(PlayerUi.ToDate(Box.GetTimestamp(slot)), 200);
        date.SelectedDateChanged += (_, _) =>
        {
            Box.SetTimeStamp(slot, date.SelectedDate ?? DateTime.MinValue);
            row.NotifyAll();
        };
        var marks = new WrapPanel { ItemSpacing = 14, LineSpacing = 6 };
        marks.Children.Add(read);
        marks.Children.Add(favorite);
        var dateRow = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        dateRow.Children.Add(new TextBlock { Text = PlayerStrings.LetterDate, VerticalAlignment = VerticalAlignment.Center });
        dateRow.Children.Add(date);
        marks.Children.Add(dateRow);
        panel.Children.Add(marks);

        panel.Children.Add(Advanced(row));

        var delete = PlayerUi.Button(PlayerStrings.DeleteLetter, () => _ = AskDelete(slot));
        delete.Foreground = Brushes.IndianRed;
        delete.HorizontalAlignment = HorizontalAlignment.Left;
        panel.Children.Add(delete);

        Details.Content = new ScrollViewer { Content = panel, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled, Padding = new Thickness(0, 0, 10, 0) };
    }

    private static string OneLine(string? text) => (text ?? string.Empty).Replace("\r\n", "\n").Replace("\n", string.Empty);

    private static StackPanel Field(string label, Control editor)
    {
        var p = new StackPanel { Spacing = 4 };
        p.Children.Add(new TextBlock { Text = label, FontWeight = FontWeight.SemiBold });
        p.Children.Add(editor);
        return p;
    }

    /// <summary>
    /// Picture and name of the attached item, «Remove», and a search to attach another one.
    /// </summary>
    private Control Gift(LetterRow row)
    {
        var slot = row.Slot;
        var item = Box.GetAttachedItem(slot);
        var hasGift = !item.IsNone;

        var picture = new ItemSlot { Width = 48, Height = 48, ShowMarkup = false };
        picture.SetItem(item);
        var name = new TextBlock
        {
            Text = hasGift ? GameInfo.Strings.GetItemName(item) : PlayerStrings.NoGift,
            VerticalAlignment = VerticalAlignment.Center,
            TextWrapping = TextWrapping.Wrap,
            FontSize = 14,
        };
        var line = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto"), ColumnSpacing = 10 };
        line.Children.Add(picture);
        Grid.SetColumn(name, 1);
        line.Children.Add(name);
        if (hasGift)
        {
            var remove = PlayerUi.Button(PlayerStrings.RemoveGift, () =>
            {
                SetGift(slot, Item.NONE, 0);
                row.NotifyAll();
                BuildDetails();
            });
            remove.VerticalAlignment = VerticalAlignment.Center;
            Grid.SetColumn(remove, 2);
            line.Children.Add(remove);
        }

        var search = new ItemSearchBox(220);
        search.ItemChosen += AttachGift;
        var change = new Expander { Header = hasGift ? PlayerStrings.ChangeGift : PlayerStrings.AddGift, Content = search, HorizontalAlignment = HorizontalAlignment.Stretch };

        var p = new StackPanel { Spacing = 6 };
        p.Children.Add(new TextBlock { Text = PlayerStrings.Gift, FontWeight = FontWeight.SemiBold });
        p.Children.Add(line);
        p.Children.Add(change);
        return p;
    }

    /// <summary>
    /// Attaches an item, keeping the wrapping of the letter (stored in the byte after the item id).
    /// </summary>
    internal void SetGift(int slot, ushort id, ushort count)
    {
        var wrap = Box.GetItemWrappingType(slot);
        var item = new Item(id);
        if (id != Item.NONE)
            item.Count = count;
        Box.SetAttachedItem(slot, item);
        Box.SetItemWrappingType(slot, id == Item.NONE ? (byte)0 : wrap);
    }

    /// <summary>
    /// Letter or parcel, writing paper, wrapping and the receiver's names.
    /// </summary>
    private Expander Advanced(LetterRow row)
    {
        var slot = row.Slot;
        var panel = new StackPanel { Spacing = 8 };
        var isLetter = Box.GetMailType(slot);

        var kind = new ComboBox { ItemsSource = new[] { PlayerStrings.KindParcel, PlayerStrings.KindLetter }, SelectedIndex = isLetter ? 1 : 0, MinWidth = 160 };
        kind.SelectionChanged += (_, _) =>
        {
            if (kind.SelectedIndex < 0 || (kind.SelectedIndex == 1) == Box.GetMailType(slot))
                return;
            Box.SetMailType(slot, kind.SelectedIndex == 1);
            Box.SetItemWrappingType(slot, 0);
            if (kind.SelectedIndex == 0)
                Box.SetMailStationerySkin(slot, 0);
            BuildDetails();
        };
        panel.Children.Add(PlayerUi.Labeled(PlayerStrings.Kind, 130, kind));

        if (isLetter)
        {
            var papers = LetterStationerySkin.OrderBy(z => z.Value.index).ToArray();
            var paper = new ComboBox
            {
                ItemsSource = papers.Select(z => GameInfo.Strings.GetMail(z.Value.name)).ToArray(),
                SelectedIndex = Array.FindIndex(papers, z => z.Key == Box.GetMailStationerySkin(slot)),
                MinWidth = 200,
                MaxDropDownHeight = 400,
            };
            paper.SelectionChanged += (_, _) =>
            {
                if (paper.SelectedIndex >= 0)
                    Box.SetMailStationerySkin(slot, (ushort)papers[paper.SelectedIndex].Key);
            };
            panel.Children.Add(PlayerUi.Labeled(PlayerStrings.Paper, 130, paper));
        }

        var skins = isLetter
            ? ItemWrappingExtended.ItemWrappingSkin.OrderBy(z => z.Value.index).Select(z => (z.Key, z.Value.name)).ToArray()
            : ItemWrappingExtended.ItemBoxSkin.OrderBy(z => z.Value.index).Select(z => (z.Key, z.Value.name)).ToArray();
        var wrap = new ComboBox
        {
            ItemsSource = skins.Select(z => GameInfo.Strings.GetMail(z.name)).ToArray(),
            SelectedIndex = Array.FindIndex(skins, z => z.Key == Box.GetItemWrappingType(slot)),
            MinWidth = 200,
            MaxDropDownHeight = 400,
        };
        wrap.SelectionChanged += (_, _) =>
        {
            if (wrap.SelectedIndex >= 0)
                Box.SetItemWrappingType(slot, (byte)skins[wrap.SelectedIndex].Key);
        };
        panel.Children.Add(PlayerUi.Labeled(isLetter ? PlayerStrings.Wrapping : PlayerStrings.BoxKind, 130, wrap));

        var receiver = new TextBox { Text = Box.GetReceiverPlayerName(slot), MaxLength = 10, MinWidth = 200 };
        receiver.TextChanged += (_, _) => Box.SetReceiverPlayerName(slot, receiver.Text ?? string.Empty);
        panel.Children.Add(PlayerUi.Labeled(PlayerStrings.ReceiverName, 130, receiver));
        var island = new TextBox { Text = Box.GetReceiverIslandName(slot), MaxLength = 10, MinWidth = 200 };
        island.TextChanged += (_, _) => Box.SetReceiverIslandName(slot, island.Text ?? string.Empty);
        panel.Children.Add(PlayerUi.Labeled(PlayerStrings.ReceiverIsland, 130, island));

        return new Expander { Header = HomeStrings.AdvancedOptions, Content = panel, HorizontalAlignment = HorizontalAlignment.Stretch };
    }
    #endregion

    protected override void Save()
    {
        // The letters were changed in place.
    }

    protected override void Discard()
    {
        Box.MailListWholeRegion = Backup;
        Box.SetLatestUniqueId(BackupUniqueId);
    }
}

/// <summary>
/// One letter of the list.
/// </summary>
public sealed class LetterRow(MailWindow owner, PostBox box, int slot) : RowModel
{
    public int Slot { get; } = slot;
    private PostBox Box { get; } = box;

    public string Sender => owner.GetSender(Slot).Name;
    public Bitmap? SenderPicture => owner.GetSender(Slot).Picture;
    public bool Unread => !Box.GetMailReadStatus(Slot);
    public bool Favorite => Box.GetMailFavStatus(Slot);
    public DateTime? SortDate => PlayerUi.ToDate(Box.GetTimestamp(Slot));
    public string Date => SortDate is { } d ? PlayerUi.FormatDate(d) : string.Empty;

    /// <summary>
    /// First line of the message (or the «to» line when there is no message).
    /// </summary>
    public string Preview
    {
        get
        {
            var text = Box.GetLetterMessageText(Slot).Replace('\n', ' ').Trim();
            return text.Length == 0 ? Box.GetLetterToLineText(Slot) : text;
        }
    }

    public Bitmap? GiftPicture
    {
        get
        {
            var item = Box.GetAttachedItem(Slot);
            return item.IsNone ? null : ItemSprite.GetItemSprite(item);
        }
    }
}
