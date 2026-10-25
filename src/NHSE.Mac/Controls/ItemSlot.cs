using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// One item cell: kind color, sprite and the item values on top (the PictureBox + GetItemMarkup of WinForms).
/// </summary>
public sealed class ItemSlot : Control
{
    private static readonly Typeface Font = new(FontFamily.Default);
    private static readonly IPen Border = new Pen(new SolidColorBrush(Color.FromArgb(0x60, 0x80, 0x80, 0x80)));
    private static readonly IPen Highlight = new Pen(Brushes.DodgerBlue, 2);
    private static readonly IPen Selection = new Pen(new SolidColorBrush(Color.FromRgb(0x2F, 0x9E, 0x62)), 3);

    private IBrush? Back;
    private Bitmap? Sprite;
    private (string Count, string Uses, string System, string Additional) Markup = (string.Empty, string.Empty, string.Empty, string.Empty);

    /// <summary>
    /// Index of the slot in its grid.
    /// </summary>
    public int Index { get; init; }

    /// <summary>
    /// Draws the values over the sprite (off for the single item preview of the item editor).
    /// </summary>
    public bool ShowMarkup { get; set; } = true;

    /// <summary>
    /// Show only the quantity of stackable items, bottom right, as the game does (the raw values are for the classic editors).
    /// </summary>
    public bool GameStyleMarkup { get; set; }

    private bool _isSelected;
    public bool IsSelected
    {
        get => _isSelected;
        set { _isSelected = value; InvalidateVisual(); }
    }

    private bool _isHovered;
    public bool IsHighlighted
    {
        get => _isHovered;
        set { _isHovered = value; InvalidateVisual(); }
    }

    public ItemSlot()
    {
        ClipToBounds = true;
        Cursor = new Avalonia.Input.Cursor(Avalonia.Input.StandardCursorType.Hand);
    }

    public void SetItem(Item item)
    {
        Back = ToBrush(ItemColor.GetItemColor(item));
        Sprite = ItemSprite.GetItemSprite(item);
        Markup = GameStyleMarkup ? GetQuantityMarkup(item) : ItemSprite.GetItemMarkup(item);
        InvalidateVisual();
    }

    public void SetItem(ushort id, ushort count)
    {
        Back = ToBrush(ItemColor.GetItemColor(id));
        Sprite = ItemSprite.GetItemSprite(id, count);
        Markup = (string.Empty, string.Empty, string.Empty, string.Empty);
        InvalidateVisual();
    }

    private static (string, string, string, string) GetQuantityMarkup(Item item)
    {
        if (item.IsNone || !ItemInfo.TryGetMaxStackCount(item, out var max) || max <= 1)
            return (string.Empty, string.Empty, string.Empty, string.Empty);
        return (string.Empty, string.Empty, string.Empty, (item.Count + 1).ToString());
    }

    private static IBrush? ToBrush(System.Drawing.Color c) => c.A == 0 ? null : new SolidColorBrush(Color.FromArgb(c.A, c.R, c.G, c.B));

    public override void Render(DrawingContext context)
    {
        var r = new Rect(Bounds.Size);
        if (Back is not null)
            context.FillRectangle(Back, r);
        if (Sprite is not null)
        {
            var size = Sprite.Size;
            var scale = Math.Min(r.Width / size.Width, r.Height / size.Height);
            var dest = new Rect((r.Width - (size.Width * scale)) / 2, (r.Height - (size.Height * scale)) / 2, size.Width * scale, size.Height * scale);
            context.DrawImage(Sprite, dest);
        }

        if (ShowMarkup)
        {
            var fs = Math.Max(9, r.Height / 6);
            DrawText(context, Markup.Count, fs, 1, 0, false, false);
            DrawText(context, Markup.Uses, fs, r.Width / 2, r.Height / 2, true, true);
            DrawText(context, Markup.System, fs, r.Width - 1, 0, false, false, rightAlign: true);
            if (GameStyleMarkup)
                DrawText(context, Markup.Additional, fs + 2, r.Width - 3, r.Height - 1, false, false, rightAlign: true, bottomAlign: true);
            else
                DrawText(context, Markup.Additional, fs, 1, r.Height, false, false, bottomAlign: true);
        }

        if (IsSelected)
            context.DrawRectangle(null, Selection, r.Deflate(1.5));
        else
            context.DrawRectangle(null, IsHighlighted ? Highlight : Border, r.Deflate(0.5));
    }

    private static void DrawText(DrawingContext context, string text, double size, double x, double y, bool centerX, bool centerY, bool rightAlign = false, bool bottomAlign = false)
    {
        if (text.Length == 0)
            return;
        var ft = new FormattedText(text, CultureInfo.InvariantCulture, FlowDirection.LeftToRight, Font, size, Brushes.Black);
        if (centerX)
            x -= ft.Width / 2;
        if (rightAlign)
            x -= ft.Width;
        if (centerY)
            y -= ft.Height / 2;
        if (bottomAlign)
            y -= ft.Height;

        // White halo so the numbers stay readable over any sprite.
        var halo = new FormattedText(text, CultureInfo.InvariantCulture, FlowDirection.LeftToRight, Font, size, Brushes.White);
        var geometry = halo.BuildGeometry(new Point(x, y));
        if (geometry is not null)
            context.DrawGeometry(null, new Pen(new SolidColorBrush(Color.FromArgb(0xC0, 0xFF, 0xFF, 0xFF)), 2.5), geometry);
        context.DrawText(ft, new Point(x, y));
    }
}
