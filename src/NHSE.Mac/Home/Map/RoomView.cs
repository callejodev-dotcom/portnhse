using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// What clicks on the room change.
/// </summary>
public enum RoomEditMode
{
    /// <summary> Furniture on the floor. </summary>
    Floor,
    /// <summary> Things on top of furniture. </summary>
    Tables,
    Rugs,
    Walls,
    Ceiling,
}

/// <summary>
/// A room of a house seen from above, as in the game: the floor with its flooring, the back wall with its wallpaper,
/// rugs, furniture and what stands on it, each with its picture on its tiles. In <see cref="RoomEditMode.Walls"/> one wall is
/// shown face on, with what hangs on it. A click reports the tile of the layer being edited.
/// </summary>
public sealed class RoomView : Control
{
    /// <summary> Tiles of a room layer across and down. </summary>
    public const int GridSize = 20;

    /// <summary> Tiles of height of a wall (what hangs on walls is placed in these rows). </summary>
    public const int WallRows = 8;

    private LayerRoomItem[] Layers = [];
    private int Squares = 8;
    private IImage? FloorPicture;
    private IImage? WallPicture;

    private RoomEditMode _mode;
    public RoomEditMode Mode { get => _mode; set { _mode = value; InvalidateVisual(); } }

    private RoomLayerSurface _wall = RoomLayerSurface.WallNorth;
    /// <summary> Wall shown in <see cref="RoomEditMode.Walls"/>. </summary>
    public RoomLayerSurface Wall { get => _wall; set { _wall = value; InvalidateVisual(); } }

    private Rect? _selection;
    /// <summary> Chosen area in tiles of the layer being edited. </summary>
    public Rect? Selection { get => _selection; set { _selection = value; InvalidateVisual(); } }

    private int? _facing;
    /// <summary> Where the chosen furniture faces (0 down, 1 right, 2 up, 3 left), shown with an arrow. </summary>
    public int? SelectionFacing { get => _facing; set { _facing = value; InvalidateVisual(); } }

    /// <summary> Raised with the tile clicked (layer coordinates). </summary>
    public event Action<int, int>? TileClicked;

    private Point? HoverPoint;
    private (Rect Area, string Text)? Hover;

    private static readonly Typeface Font = new(FontFamily.Default, FontStyle.Normal, FontWeight.SemiBold);

    public RoomView()
    {
        ClipToBounds = true;
        Cursor = new Cursor(StandardCursorType.Hand);
        MinHeight = 300;
    }

    /// <summary> First tile of the room's floor (rooms are centered in the 20×20 grid). </summary>
    public int Start => (GridSize - (Squares * 2)) / 2;

    /// <summary> Tiles across the room's floor. </summary>
    public int Span => Squares * 2;

    /// <summary>
    /// Shows a room: its item layers, its size in squares and the pictures of its flooring and wallpaper.
    /// </summary>
    public void SetRoom(LayerRoomItem[] layers, int squares, IImage? floor, IImage? wall)
    {
        Layers = layers;
        Squares = Math.Clamp(squares, 1, GridSize / 2);
        FloorPicture = floor;
        WallPicture = wall;
        Hover = null;
        InvalidateVisual();
    }

    public void Refresh() => InvalidateVisual();

    /// <summary> Item layer edited in the current mode. </summary>
    public LayerRoomItem? ActiveLayer => Layers.Length == 0 ? null : Layers[(int)ActiveSurface];

    public RoomLayerSurface ActiveSurface => Mode switch
    {
        RoomEditMode.Tables => RoomLayerSurface.FloorSupported,
        RoomEditMode.Rugs => RoomLayerSurface.Rugs,
        RoomEditMode.Walls => Wall,
        RoomEditMode.Ceiling => RoomLayerSurface.Ceiling,
        _ => RoomLayerSurface.Floor,
    };

    #region Geometry
    /// <summary> Area of the control taken by the floor (or the wall face), and pixels per tile. </summary>
    private (Rect Floor, double Unit, Rect BackWall, Rect Room) Layout()
    {
        var b = new Rect(Bounds.Size).Deflate(12);
        if (Mode == RoomEditMode.Walls)
        {
            // The wall face on, with a strip of floor below.
            var unit = Math.Min(b.Width / Span, b.Height / (WallRows + 2.5));
            var w = Span * unit;
            var h = WallRows * unit;
            var face = new Rect(b.X + ((b.Width - w) / 2), b.Y + ((b.Height - h - (2 * unit)) / 2), w, h);
            return (face, unit, face, face);
        }
        const double back = 3.2; // tiles of the back wall seen from above
        const double side = 0.8;
        var u = Math.Min(b.Width / (Span + (2 * side)), b.Height / (Span + back + side));
        var roomW = (Span + (2 * side)) * u;
        var roomH = (Span + back + side) * u;
        var room = new Rect(b.X + ((b.Width - roomW) / 2), b.Y + ((b.Height - roomH) / 2), roomW, roomH);
        var floor = new Rect(room.X + (side * u), room.Y + (back * u), Span * u, Span * u);
        var backWall = new Rect(floor.X, room.Y, floor.Width, back * u);
        return (floor, u, backWall, room);
    }

    /// <summary> Screen area of an area in tiles of the active layer. </summary>
    private Rect ToScreen(Rect tiles)
    {
        var (floor, u, _, _) = Layout();
        var yStart = Mode == RoomEditMode.Walls ? 0 : Start;
        return new Rect(floor.X + ((tiles.X - Start) * u), floor.Y + ((tiles.Y - yStart) * u), tiles.Width * u, tiles.Height * u);
    }

    /// <summary> Tile of the active layer under a point, if any. </summary>
    private (int X, int Y)? ToTile(Point p)
    {
        var (floor, u, _, _) = Layout();
        if (!floor.Contains(p) || u <= 0)
            return null;
        var x = Start + (int)((p.X - floor.X) / u);
        var y = (Mode == RoomEditMode.Walls ? 0 : Start) + (int)((p.Y - floor.Y) / u);
        return (Math.Clamp(x, 0, GridSize - 1), Math.Clamp(y, 0, GridSize - 1));
    }
    #endregion

    #region Pointer
    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        base.OnPointerReleased(e);
        if (e.InitialPressMouseButton != MouseButton.Left)
            return;
        if (ToTile(e.GetPosition(this)) is { } t)
            TileClicked?.Invoke(t.X, t.Y);
    }

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        base.OnPointerMoved(e);
        SetHover(e.GetPosition(this));
    }

    protected override void OnPointerExited(PointerEventArgs e)
    {
        base.OnPointerExited(e);
        Hover = null;
        HoverPoint = null;
        InvalidateVisual();
    }

    /// <summary>
    /// Shows the name of what is under a point of the control.
    /// </summary>
    public void SetHover(Point p)
    {
        HoverPoint = p;
        Hover = null;
        if (ToTile(p) is { } t && ActiveLayer is { } layer)
        {
            if (GridItems.TryGetRoot(layer, t.X, t.Y, out var root, out var rx, out var ry))
            {
                var (w, h) = root.IsRoot ? GridItems.GetSize(root) : (1, 1);
                Hover = (new Rect(rx, ry, w, h), GridItems.GetName(root));
            }
            else
            {
                Hover = (new Rect(t.X & ~1, t.Y & ~1, 2, 2), string.Empty);
            }
        }
        InvalidateVisual();
    }
    #endregion

    #region Drawing
    public override void Render(DrawingContext context)
    {
        if (Layers.Length == 0)
            return;
        if (Mode == RoomEditMode.Walls)
            RenderWall(context);
        else
            RenderFloor(context);

        if (Hover is { } hover)
            context.DrawRectangle(new SolidColorBrush(Color.FromArgb(0x30, 0xFF, 0xFF, 0xFF)), new Pen(new SolidColorBrush(Color.FromArgb(0xD0, 0xFF, 0xFF, 0xFF)), 1.5), ToScreen(hover.Area).Inflate(1), 4, 4);
        if (Selection is { } sel)
            DrawSelection(context, ToScreen(sel), SelectionFacing);
        if (Hover is { Text.Length: > 0 } tip && HoverPoint is { } at)
            DrawBubble(context, tip.Text, at);
    }

    /// <summary>
    /// Outline of the chosen spot, and an arrow on the side the furniture faces.
    /// </summary>
    internal static void DrawSelection(DrawingContext context, Rect area, int? facing)
    {
        var r = area.Inflate(2);
        context.DrawRectangle(null, new Pen(new SolidColorBrush(Color.FromArgb(0x90, 0, 0, 0)), 4.5), r, 5, 5);
        context.DrawRectangle(null, new Pen(Brushes.White, 2.5), r, 5, 5);
        if (facing is not { } f || r.Width < 18)
            return;
        var (dx, dy) = (f & 3) switch { 0 => (0, 1), 1 => (1, 0), 2 => (0, -1), _ => (-1, 0) };
        var c = new Point(r.Center.X + (dx * ((r.Width / 2) + 1)), r.Center.Y + (dy * ((r.Height / 2) + 1)));
        const double radius = 9;
        context.DrawEllipse(Brushes.White, new Pen(new SolidColorBrush(Color.FromArgb(0x90, 0, 0, 0)), 1.5), c, radius, radius);
        var tip = new Point(c.X + (dx * 5), c.Y + (dy * 5));
        var back = new Point(c.X - (dx * 3.5), c.Y - (dy * 3.5));
        var side = new Vector(-dy * 4.5, dx * 4.5);
        var arrow = new StreamGeometry();
        using (var g = arrow.Open())
        {
            g.BeginFigure(tip, true);
            g.LineTo(back + side);
            g.LineTo(back - side);
            g.EndFigure(true);
        }
        context.DrawGeometry(new SolidColorBrush(Color.FromRgb(0x2F, 0x9E, 0x62)), null, arrow);
    }

    private void RenderFloor(DrawingContext context)
    {
        var (floor, u, backWall, room) = Layout();

        // Walls: the back one seen in full, the sides and front as a thin edge.
        var wallBase = Color.FromRgb(0xEF, 0xE7, 0xD8);
        context.DrawRectangle(new SolidColorBrush(Color.FromRgb(0xB8, 0xA8, 0x90)), null, room.Inflate(3), 10, 10,
            new BoxShadows(new BoxShadow { Blur = 18, OffsetY = 4, Color = Color.FromArgb(0x50, 0, 0, 0) }));
        context.DrawRectangle(new SolidColorBrush(wallBase), null, room, 8, 8);
        using (context.PushClip(new RoundedRect(backWall, new CornerRadius(8, 8, 0, 0))))
        {
            TileTexture(context, WallPicture, backWall, u * 4, WallCrop);
            // Skirting board
            context.FillRectangle(new SolidColorBrush(Color.FromArgb(0x70, 0x5A, 0x40, 0x28)), new Rect(backWall.X, backWall.Bottom - (u * 0.35), backWall.Width, u * 0.35));
        }

        // Floor
        using (context.PushClip(floor))
        {
            context.FillRectangle(new SolidColorBrush(Color.FromRgb(0xE2, 0xCF, 0xAE)), floor);
            TileTexture(context, FloorPicture, floor, u * 4, FloorCrop);
            // Light from the back: the floor is a little darker toward the front.
            context.FillRectangle(new LinearGradientBrush
            {
                StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
                EndPoint = new RelativePoint(0, 1, RelativeUnit.Relative),
                GradientStops = { new GradientStop(Color.FromArgb(0x30, 0, 0, 0), 0), new GradientStop(Color.FromArgb(0, 0, 0, 0), 0.08), new GradientStop(Color.FromArgb(0x18, 0, 0, 0), 1) },
            }, floor);
            // A faint line on each square, to see where things go.
            var grid = new Pen(new SolidColorBrush(Color.FromArgb(0x22, 0x40, 0x30, 0x20)), 1);
            for (int i = 1; i < Squares; i++)
            {
                var x = Math.Round(floor.X + (i * 2 * u)) + 0.5;
                var y = Math.Round(floor.Y + (i * 2 * u)) + 0.5;
                context.DrawLine(grid, new Point(x, floor.Y), new Point(x, floor.Bottom));
                context.DrawLine(grid, new Point(floor.X, y), new Point(floor.Right, y));
            }
        }
        context.DrawRectangle(null, new Pen(new SolidColorBrush(Color.FromArgb(0x50, 0x40, 0x30, 0x20)), 1), floor);

        // What hangs on the back wall, small, at its place along the wall.
        DrawWallStrip(context, Layers[(int)RoomLayerSurface.WallNorth], backWall, u);

        // What is being edited is drawn clearly; the rest fades a little.
        var (rugs, floorItems, tables, ceiling) = Mode switch
        {
            RoomEditMode.Tables => (0.7, 0.5, 1.0, 0.0),
            RoomEditMode.Rugs => (1.0, 0.35, 0.3, 0.0),
            RoomEditMode.Ceiling => (0.5, 0.4, 0.35, 1.0),
            _ => (1.0, 1.0, 1.0, 0.16),
        };
        using (context.PushOpacity(rugs))
            DrawLayer(context, Layers[(int)RoomLayerSurface.Rugs], 0, true);
        using (context.PushOpacity(floorItems))
            DrawLayer(context, Layers[(int)RoomLayerSurface.Floor], -0.1);
        using (context.PushOpacity(tables))
            DrawLayer(context, Layers[(int)RoomLayerSurface.FloorSupported], 0.06);
        if (ceiling > 0)
        {
            using (context.PushOpacity(ceiling))
                DrawLayer(context, Layers[(int)RoomLayerSurface.Ceiling], -0.02);
        }
    }

    private void RenderWall(DrawingContext context)
    {
        var (face, u, _, _) = Layout();
        var floorStrip = new Rect(face.X, face.Bottom, face.Width, u * 2);
        context.DrawRectangle(new SolidColorBrush(Color.FromRgb(0xE2, 0xCF, 0xAE)), null, floorStrip.Inflate(new Thickness(0, 0, 0, 0)), 0, 0,
            new BoxShadows(new BoxShadow { Blur = 18, OffsetY = 4, Color = Color.FromArgb(0x50, 0, 0, 0) }));
        using (context.PushClip(floorStrip))
            TileTexture(context, FloorPicture, floorStrip, u * 4, FloorCrop);
        context.FillRectangle(new SolidColorBrush(Color.FromArgb(0x30, 0, 0, 0)), floorStrip);
        context.DrawRectangle(new SolidColorBrush(Color.FromRgb(0xEF, 0xE7, 0xD8)), null, face, 0, 0,
            new BoxShadows(new BoxShadow { Blur = 18, OffsetY = 4, Color = Color.FromArgb(0x50, 0, 0, 0) }));
        using (context.PushClip(face))
        {
            TileTexture(context, WallPicture, face, u * 4, WallCrop);
            var grid = new Pen(new SolidColorBrush(Color.FromArgb(0x26, 0x30, 0x20, 0x10)), 1);
            for (int i = 1; i < Span; i++)
            {
                var x = Math.Round(face.X + (i * u)) + 0.5;
                context.DrawLine(grid, new Point(x, face.Y), new Point(x, face.Bottom));
            }
            for (int i = 1; i < WallRows; i++)
            {
                var y = Math.Round(face.Y + (i * u)) + 0.5;
                context.DrawLine(grid, new Point(face.X, y), new Point(face.Right, y));
            }
        }
        context.FillRectangle(new SolidColorBrush(Color.FromArgb(0x80, 0x5A, 0x40, 0x28)), new Rect(face.X, face.Bottom - (u * 0.3), face.Width, u * 0.3));
        DrawLayer(context, Layers[(int)Wall], 0.04);
    }

    /// <summary>
    /// Items of a layer on their tiles (in the active coordinates: the floor, or the wall face).
    /// </summary>
    private void DrawLayer(DrawingContext context, LayerRoomItem layer, double pad, bool rug = false)
    {
        using var _ = context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality });
        var items = new List<(Rect Area, Item Item)>();
        for (int x = 0; x < GridSize; x++)
        {
            for (int y = 0; y < GridSize; y++)
            {
                var it = layer.GetTile(x, y);
                if (it.IsNone || !it.IsRoot)
                    continue;
                var (w, h) = GridItems.GetSize(it);
                items.Add((new Rect(x, y, w, h), it));
            }
        }
        foreach (var (area, item) in items.OrderBy(z => z.Area.Bottom))
        {
            var r = ToScreen(area);
            var picture = GridItems.GetPicture(item);
            if (rug)
            {
                // Rugs lie flat: turned rugs are drawn turned.
                if ((item.Rotation & 1) == 1)
                {
                    var c = r.Center;
                    using (context.PushTransform(Matrix.CreateTranslation(-c.X, -c.Y) * Matrix.CreateRotation(Math.PI / 2) * Matrix.CreateTranslation(c.X, c.Y)))
                        DrawPicture(context, picture, new Rect(c.X - (r.Height / 2), c.Y - (r.Width / 2), r.Height, r.Width).Inflate(2));
                }
                else
                {
                    DrawPicture(context, picture, r.Inflate(2));
                }
                continue;
            }
            DrawPicture(context, picture, r.Deflate(Math.Min(r.Width, r.Height) * pad));
        }
    }

    /// <summary>
    /// What hangs on a wall, drawn small along a strip (the back wall seen from above).
    /// </summary>
    private void DrawWallStrip(DrawingContext context, LayerRoomItem layer, Rect strip, double u)
    {
        using var _ = context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality });
        var scaleY = (strip.Height - (u * 0.5)) / WallRows;
        for (int x = 0; x < GridSize; x++)
        {
            for (int y = 0; y < GridSize; y++)
            {
                var it = layer.GetTile(x, y);
                if (it.IsNone || !it.IsRoot)
                    continue;
                var (w, h) = GridItems.GetSize(it);
                var r = new Rect(strip.X + ((x - Start) * u), strip.Y + (y * scaleY), w * u, h * scaleY);
                DrawPicture(context, GridItems.GetPicture(it), r.Deflate(1));
            }
        }
    }

    private static readonly Rect FloorCrop = new(0.28, 0.28, 0.44, 0.44);
    private static readonly Rect WallCrop = new(0.32, 0.2, 0.4, 0.32);

    /// <summary>
    /// Fills an area with the middle of an item picture (a flooring or a wallpaper), repeated.
    /// </summary>
    private static void TileTexture(DrawingContext context, IImage? picture, Rect area, double tile, Rect crop)
    {
        if (picture is null || tile <= 2)
            return;
        var size = picture.Size;
        var src = new Rect(size.Width * crop.X, size.Height * crop.Y, size.Width * crop.Width, size.Height * crop.Height);
        var tileH = tile * src.Height / src.Width;
        using var _ = context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality });
        // Every other tile is mirrored, so the edges of neighbouring tiles match and no seams show.
        int row = 0;
        for (var y = area.Y; y < area.Bottom; y += tileH, row++)
        {
            int col = 0;
            for (var x = area.X; x < area.Right; x += tile, col++)
            {
                var dest = new Rect(x, y, tile + 0.5, tileH + 0.5);
                var flipX = (col & 1) == 1;
                var flipY = (row & 1) == 1;
                if (!flipX && !flipY)
                {
                    context.DrawImage(picture, src, dest);
                    continue;
                }
                var c = dest.Center;
                var m = Matrix.CreateTranslation(-c.X, -c.Y) * Matrix.CreateScale(flipX ? -1 : 1, flipY ? -1 : 1) * Matrix.CreateTranslation(c.X, c.Y);
                using (context.PushTransform(m))
                    context.DrawImage(picture, src, dest);
            }
        }
    }

    private static void DrawPicture(DrawingContext context, IImage? picture, Rect dest, bool stretch = false)
    {
        if (picture is null)
            return;
        var size = picture.Size;
        if (stretch)
        {
            context.DrawImage(picture, new Rect(size), dest);
            return;
        }
        var scale = Math.Min(dest.Width / size.Width, dest.Height / size.Height);
        var w = size.Width * scale;
        var h = size.Height * scale;
        context.DrawImage(picture, new Rect(size), new Rect(dest.X + ((dest.Width - w) / 2), dest.Y + ((dest.Height - h) / 2), w, h));
    }

    private void DrawBubble(DrawingContext context, string text, Point at)
    {
        var dark = ActualThemeVariant == Avalonia.Styling.ThemeVariant.Dark;
        IBrush fg = dark ? Brushes.White : new SolidColorBrush(Color.FromRgb(0x22, 0x22, 0x22));
        IBrush bg = dark ? new SolidColorBrush(Color.FromArgb(0xF2, 0x2C, 0x2C, 0x2F)) : new SolidColorBrush(Color.FromArgb(0xF7, 0xFF, 0xFF, 0xFF));
        var ft = new FormattedText(text, CultureInfo.CurrentCulture, FlowDirection.LeftToRight, Font, 13, fg);
        var w = ft.Width + 20;
        var h = ft.Height + 10;
        var x = Math.Min(at.X + 16, Bounds.Width - w - 6);
        var y = at.Y + 20;
        if (y + h > Bounds.Height - 6)
            y = at.Y - h - 12;
        var r = new Rect(x, y, w, h);
        context.DrawRectangle(bg, new Pen(new SolidColorBrush(Color.FromArgb(0x40, 0, 0, 0)), 1), r, 8, 8, new BoxShadows(new BoxShadow { Blur = 10, OffsetY = 2, Color = Color.FromArgb(0x40, 0, 0, 0) }));
        context.DrawText(ft, new Point(r.X + 10, r.Y + 5));
    }
    #endregion
}
