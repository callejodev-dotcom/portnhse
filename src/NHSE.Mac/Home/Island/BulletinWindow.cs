using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Friendly bulletin board: the notes pinned on the plaza board, newest first, each with who wrote it and when,
/// and its text ready to change. Handwritten notes can only be seen in the game, so they are just listed.
/// </summary>
public sealed class BulletinWindow : PlayerDialog
{
    private const int BodyLength = 239; // 240 characters with the terminator
    private const int FooterLength = 47;

    private readonly MainSave Main;
    private GSaveBulletinBoard Board;
    private readonly GSaveBBS[] Notes;

    /// <summary>
    /// Slots of the board with a note, newest first.
    /// </summary>
    internal IReadOnlyList<int> Shown { get; }

    public BulletinWindow(MainSave main) : base(HomeStrings.Bulletin, 720, 680)
    {
        Main = main;
        Board = main.Bulletin;
        Notes = Board.Stock.Buffer;
        Shown = [.. Enumerable.Range(0, Notes.Length)
            .Where(i => Notes[i].UniqueId != 0 || !string.IsNullOrEmpty(Notes[i].Body))
            .OrderByDescending(i => PlayerUi.ToDate(Notes[i].Date) ?? DateTime.MinValue)
            .ThenByDescending(i => Notes[i].UniqueId)];

        var list = new StackPanel { Spacing = 10 };
        if (Shown.Count == 0)
            list.Children.Add(Card(CardHeader("Megaphone", IslandStrings.BulletinEmpty, null)));
        foreach (var i in Shown)
            list.Children.Add(Note(i));

        SetLayout(HomeStrings.Bulletin, IslandStrings.BulletinHint, new ScrollViewer { Content = list });
    }

    protected override void Save()
    {
        var stock = Board.Stock;
        stock.Buffer = Notes;
        Board.Stock = stock;
        Main.Bulletin = Board;
    }

    /// <summary>
    /// Changes the text of a note (as typing in its box does).
    /// </summary>
    public void SetText(int slot, string body)
    {
        Notes[slot].Body = body.Length > BodyLength ? body[..BodyLength] : body;
    }

    internal static bool IsHandwritten(GSaveBBS note) => note.HandWrite.VerticesNum != 0 && string.IsNullOrEmpty(note.Body);

    private static string NoteIcon(GSaveBBS note) => note.DesignId switch
    {
        _ when IsHandwritten(note) => "LostQuestMemo",
        1 => "BdayCupcake",
        2 => "Fishingrod",
        3 => "Net",
        _ => "Megaphone",
    };

    private Control Note(int slot)
    {
        var note = Notes[slot];
        var player = note.PlayerId.BaseId.Name;
        var author = !string.IsNullOrEmpty(note.Footer) ? note.Footer
            : !string.IsNullOrEmpty(player) ? player
            : IslandStrings.BulletinResidentServices;
        var date = PlayerUi.ToDate(note.Date);
        var when = date is { } d ? PlayerUi.FormatDate(d) : string.Empty;

        var head = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        head.Children.Add(new TextBlock { Text = author, FontWeight = FontWeight.SemiBold, FontSize = 14, VerticalAlignment = VerticalAlignment.Center });
        head.Children.Add(new TextBlock { Text = when, Classes = { "subtle" }, VerticalAlignment = VerticalAlignment.Center });

        var content = new StackPanel { Spacing = 6 };
        content.Children.Add(head);
        if (IsHandwritten(note))
        {
            content.Children.Add(PlayerUi.Subtle(string.Format(IslandStrings.BulletinHandwritten, author), 13));
        }
        else
        {
            var body = new TextBox
            {
                Text = note.Body,
                AcceptsReturn = true,
                TextWrapping = TextWrapping.Wrap,
                MaxLength = BodyLength,
                MinHeight = 60,
            };
            body.TextChanged += (_, _) => SetText(slot, body.Text ?? string.Empty);
            content.Children.Add(body);

            if (!string.IsNullOrEmpty(note.Footer))
            {
                var footer = new TextBox { Text = note.Footer, MaxLength = FooterLength, Width = 220, HorizontalAlignment = HorizontalAlignment.Right };
                footer.TextChanged += (_, _) => Notes[slot].Footer = footer.Text ?? string.Empty;
                content.Children.Add(PlayerUi.Labeled(IslandStrings.BulletinSignature, 70, footer));
            }
        }

        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 12 };
        var icon = HomeUi.Icon(NoteIcon(note), 32);
        icon.VerticalAlignment = VerticalAlignment.Top;
        g.Children.Add(icon);
        Grid.SetColumn(content, 1);
        g.Children.Add(content);
        return Card(g);
    }
}
