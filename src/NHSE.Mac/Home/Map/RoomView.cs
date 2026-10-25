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
/// Openings of a room, on the sides seen from above: front (the entrance, or the way to the main room) at the bottom.
/// </summary>
[Flags]
public enum RoomDoors
{
    None = 0,
    Front = 1,
    Back = 2,
    Left = 4,
    Right = 8,
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
    private RoomDoors Doors;

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
    private (Rect Area, string Text, IImage? Picture)? Hover;

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
    public void SetRoom(LayerRoomItem[] layers, int squares, IImage? floor, IImage? wall, RoomDoors doors = RoomDoors.None)
    {
        Layers = layers;
        Squares = Math.Clamp(squares, 1, GridSize / 2);
        FloorPicture = floor;
        WallPicture = wall;
        Doors = doors;
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

    /// <summary> Thickness of the walls in the plan, in tiles. </summary>
    private const double WallThickness = 1.1;

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
        // A plan: the floor with its real shape, and the walls as a frame around it.
        var u = Math.Min(b.Width, b.Height) / (Span + (2 * WallThickness));
        var size = (Span + (2 * WallThickness)) * u;
        var room = new Rect(b.X + ((b.Width - size) / 2), b.Y + ((b.Height - size) / 2), size, size);
        var floor = room.Deflate(WallThickness * u);
        var backWall = new Rect(floor.X, room.Y, floor.Width, WallThickness * u);
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
                Hover = (new Rect(rx, ry, w, h), GridItems.GetName(root), GridItems.GetPicture(root));
            }
            else
            {
                Hover = (new Rect(t.X & ~1, t.Y & ~1, 2, 2), string.Empty, null);
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
            DrawBubble(context, tip.Text, tip.Picture, at);
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

        // Walls: a frame in the colour of the wallpaper.
        var wall = AverageColor(WallPicture, WallPatternCrop, Color.FromRgb(0xC9, 0xB8, 0x9C));
        context.DrawRectangle(new SolidColorBrush(Shade(wall, 0.82)), new Pen(new SolidColorBrush(Shade(wall, 0.5)), 1.5), room, 6, 6,
            new BoxShadows(new BoxShadow { Blur = 16, OffsetY = 4, Color = Color.FromArgb(0x48, 0, 0, 0) }));

        // Floor
        using (context.PushClip(floor))
        {
            context.FillRectangle(new SolidColorBrush(Color.FromRgb(0xE2, 0xCF, 0xAE)), floor);
            TileTexture(context, FloorPicture, floor, u * 3.8, FloorCrop);
            // One faint line per square, to see where things go.
            var grid = new Pen(new SolidColorBrush(Color.FromArgb(0x30, 0x30, 0x20, 0x10)), 1);
            for (int i = 1; i < Squares; i++)
            {
                var x = Math.Round(floor.X + (i * 2 * u)) + 0.5;
                var y = Math.Round(floor.Y + (i * 2 * u)) + 0.5;
                context.DrawLine(grid, new Point(x, floor.Y), new Point(x, floor.Bottom));
                context.DrawLine(grid, new Point(floor.X, y), new Point(floor.Right, y));
            }
            // The walls cast a little shadow on the floor.
            context.DrawRectangle(null, new Pen(new SolidColorBrush(Color.FromArgb(0x30, 0, 0, 0)), u * 0.5), floor.Deflate(u * 0.25));
        }
        context.DrawRectangle(null, new Pen(new SolidColorBrush(Shade(wall, 0.5)), 1.5), floor);
        DrawDoors(context, floor, u, wall);

        // What hangs on the back wall, small, at its place along the wall.
        DrawWallStrip(context, Layers[(int)RoomLayerSurface.WallNorth], backWall, u);

        // What is being edited is drawn clearly; the rest fades a little.
        var (rugs, floorItems, tables, ceiling) = Mode switch
        {
            RoomEditMode.Tables => (0.6, 0.55, 1.0, 0.0),
            RoomEditMode.Rugs => (1.0, 0.35, 0.3, 0.0),
            RoomEditMode.Ceiling => (0.5, 0.4, 0.35, 1.0),
            _ => (1.0, 1.0, 1.0, 0.0),
        };
        using (context.PushOpacity(rugs))
            DrawLayer(context, Layers[(int)RoomLayerSurface.Rugs], ItemLook.Flat);
        using (context.PushOpacity(floorItems))
            DrawTokens(context, Layers[(int)RoomLayerSurface.Floor], TokenKind.Furniture);
        using (context.PushOpacity(tables))
            DrawTokens(context, Layers[(int)RoomLayerSurface.FloorSupported], TokenKind.OnTable);
        if (ceiling > 0)
        {
            using (context.PushOpacity(ceiling))
                DrawTokens(context, Layers[(int)RoomLayerSurface.Ceiling], TokenKind.Ceiling);
        }
    }

    /// <summary>
    /// Openings in the walls: a gap one square wide in the middle of the wall, with a door drawn as in a plan
    /// (the leaf and the arc it sweeps).
    /// </summary>
    private void DrawDoors(DrawingContext context, Rect floor, double u, Color wall)
    {
        var width = 2 * u;
        var depth = WallThickness * u;
        var mid = new Point(floor.Center.X, floor.Center.Y);
        var leaf = new Pen(new SolidColorBrush(Color.FromArgb(0x90, 0x30, 0x20, 0x10)), 1.5);
        var arc = new Pen(new SolidColorBrush(Color.FromArgb(0x90, 0x40, 0x30, 0x20)), 1) { DashStyle = new DashStyle([3, 3], 0) };
        var gap = new SolidColorBrush(Color.FromRgb(0xE8, 0xDC, 0xC4));

        void Door(Rect opening, Point hinge, Vector closed, Vector open)
        {
            context.FillRectangle(gap, opening);
            // Leaf open into the room, and the arc of its swing.
            context.DrawLine(leaf, hinge, hinge + (open * width));
            var g = new StreamGeometry();
            using (var c = g.Open())
            {
                c.BeginFigure(hinge + (closed * width), false);
                c.ArcTo(hinge + (open * width), new Size(width, width), 0, false, SweepDirection.Clockwise);
                c.EndFigure(false);
            }
            context.DrawGeometry(null, arc, g);
        }

        if (Doors.HasFlag(RoomDoors.Front))
            Door(new Rect(mid.X - (width / 2), floor.Bottom, width, depth), new Point(mid.X - (width / 2), floor.Bottom), new Vector(1, 0), new Vector(0, -1));
        if (Doors.HasFlag(RoomDoors.Back))
            Door(new Rect(mid.X - (width / 2), floor.Y - depth, width, depth), new Point(mid.X + (width / 2), floor.Y), new Vector(-1, 0), new Vector(0, 1));
        if (Doors.HasFlag(RoomDoors.Left))
            Door(new Rect(floor.X - depth, mid.Y - (width / 2), depth, width), new Point(floor.X, mid.Y - (width / 2)), new Vector(0, 1), new Vector(1, 0));
        if (Doors.HasFlag(RoomDoors.Right))
            Door(new Rect(floor.Right, mid.Y - (width / 2), depth, width), new Point(floor.Right, mid.Y + (width / 2)), new Vector(0, -1), new Vector(-1, 0));
    }

    private void RenderWall(DrawingContext context)
    {
        var (face, u, _, _) = Layout();
        var floorStrip = new Rect(face.X, face.Bottom, face.Width, u * 2);
        context.DrawRectangle(new SolidColorBrush(Color.FromRgb(0xE2, 0xCF, 0xAE)), null, floorStrip.Inflate(new Thickness(0, 0, 0, 0)), 0, 0,
            new BoxShadows(new BoxShadow { Blur = 18, OffsetY = 4, Color = Color.FromArgb(0x50, 0, 0, 0) }));
        using (context.PushClip(floorStrip))
            TileTexture(context, FloorPicture, floorStrip, u * 3.8, FloorCrop);
        context.FillRectangle(new SolidColorBrush(Color.FromArgb(0x30, 0, 0, 0)), floorStrip);
        context.DrawRectangle(new SolidColorBrush(Color.FromRgb(0xEF, 0xE7, 0xD8)), null, face, 0, 0,
            new BoxShadows(new BoxShadow { Blur = 18, OffsetY = 4, Color = Color.FromArgb(0x50, 0, 0, 0) }));
        using (context.PushClip(face))
        {
            PaintWall(context, WallPicture, face, u * 2.6);
            var grid = new Pen(new SolidColorBrush(Color.FromArgb(0x1C, 0x30, 0x20, 0x10)), 1);
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
        DrawLayer(context, Layers[(int)Wall], ItemLook.Fit);
    }

    private enum ItemLook
    {
        /// <summary> Lying on the floor (rugs). </summary>
        Flat,
        /// <summary> Inside its spot (walls, ceiling). </summary>
        Fit,
    }

    /// <summary>
    /// Root items of a layer with their area in tiles.
    /// </summary>
    private static List<(Rect Area, Item Item)> Roots(LayerRoomItem layer)
    {
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
        return items;
    }

    /// <summary>
    /// Items of a layer on their tiles (in the active coordinates: the floor, or the wall face).
    /// </summary>
    private void DrawLayer(DrawingContext context, LayerRoomItem layer, ItemLook look)
    {
        using var _ = context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality });
        foreach (var (area, item) in Roots(layer).OrderBy(z => z.Area.Bottom))
        {
            var r = ToScreen(area);
            var picture = GridItems.GetPicture(item);
            if (look == ItemLook.Fit)
            {
                DrawPicture(context, picture, r.Deflate(Math.Min(r.Width, r.Height) * 0.04));
                continue;
            }
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
        }
    }

    private enum TokenKind { Furniture, OnTable, Ceiling }

    /// <summary>
    /// Each item as a piece on its exact spot, the size it takes in the room, with its picture inside:
    /// furniture on a soft card, what stands on it on a smaller one, ceiling things only outlined.
    /// </summary>
    private void DrawTokens(DrawingContext context, LayerRoomItem layer, TokenKind kind)
    {
        using var _ = context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality });
        foreach (var (area, item) in Roots(layer).OrderBy(z => z.Area.Bottom))
        {
            var r = ToScreen(area).Deflate(1.5);
            var picture = GridItems.GetPicture(item);
            switch (kind)
            {
                case TokenKind.Furniture:
                {
                    var radius = Math.Min(r.Width, r.Height) * 0.16;
                    context.DrawRectangle(TokenFill, TokenEdge, r, radius, radius,
                        new BoxShadows(new BoxShadow { Blur = 5, OffsetY = 1.5, Color = Color.FromArgb(0x50, 0, 0, 0) }));
                    DrawPicture(context, picture, r.Deflate(Math.Min(r.Width, r.Height) * 0.06));
                    break;
                }
                case TokenKind.OnTable:
                {
                    var t = r.Deflate(Math.Min(r.Width, r.Height) * 0.22);
                    var radius = Math.Min(t.Width, t.Height) / 2;
                    context.DrawRectangle(TableFill, TokenEdge, t, radius, radius,
                        new BoxShadows(new BoxShadow { Blur = 4, OffsetY = 1, Color = Color.FromArgb(0x60, 0, 0, 0) }));
                    DrawPicture(context, picture, t.Deflate(Math.Min(t.Width, t.Height) * 0.1));
                    break;
                }
                default:
                {
                    var radius = Math.Min(r.Width, r.Height) * 0.16;
                    context.DrawRectangle(null, CeilingEdge, r, radius, radius);
                    DrawPicture(context, picture, r.Deflate(Math.Min(r.Width, r.Height) * 0.1));
                    break;
                }
            }
        }
    }

    private static readonly IBrush TokenFill = new SolidColorBrush(Color.FromArgb(0x9C, 0xFF, 0xFC, 0xF5));
    private static readonly IBrush TableFill = new SolidColorBrush(Color.FromArgb(0xF0, 0xFF, 0xFF, 0xFF));
    private static readonly IPen TokenEdge = new Pen(new SolidColorBrush(Color.FromArgb(0x70, 0x40, 0x30, 0x20)), 1);
    private static readonly IPen CeilingEdge = new Pen(new SolidColorBrush(Color.FromArgb(0xB0, 0xFF, 0xFF, 0xFF)), 1.5) { DashStyle = new DashStyle([4, 3], 0) };

    private static Color Shade(Color c, double f) => Color.FromRgb((byte)(c.R * f), (byte)(c.G * f), (byte)(c.B * f));

    private static readonly System.Runtime.CompilerServices.ConditionalWeakTable<Bitmap, object> AverageColors = new();

    /// <summary>
    /// Average colour of part of a picture (e.g. a wallpaper's pattern), ignoring see-through pixels.
    /// </summary>
    private static Color AverageColor(IImage? picture, Rect crop, Color fallback)
    {
        if (picture is not Bitmap bmp)
            return fallback;
        if (AverageColors.TryGetValue(bmp, out var known))
            return (Color)known;
        var (w, h) = (bmp.PixelSize.Width, bmp.PixelSize.Height);
        using var copy = ImageUtil.Create(w, h);
        using (var fb = copy.Lock())
            bmp.CopyPixels(fb);
        var pixels = copy.GetPixels();
        long r = 0, g = 0, b = 0, n = 0;
        for (int y = (int)(crop.Y * h); y < (int)(crop.Bottom * h); y++)
        {
            for (int x = (int)(crop.X * w); x < (int)(crop.Right * w); x++)
            {
                var p = (uint)pixels[(y * w) + x];
                if (p >> 24 < 200)
                    continue;
                r += (p >> 16) & 0xFF;
                g += (p >> 8) & 0xFF;
                b += p & 0xFF;
                n++;
            }
        }
        var color = n == 0 ? fallback : Color.FromRgb((byte)(r / n), (byte)(g / n), (byte)(b / n));
        AverageColors.AddOrUpdate(bmp, color);
        return color;
    }

    /// <summary>
    /// What hangs on a wall, drawn small along a strip (the back wall seen from above).
    /// </summary>
    private void DrawWallStrip(DrawingContext context, LayerRoomItem layer, Rect strip, double u)
    {
        using var _ = context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality });
        foreach (var (area, item) in Roots(layer))
        {
            var r = new Rect(strip.X + ((area.X - Start) * u), strip.Y, area.Width * u, strip.Height);
            DrawPicture(context, GridItems.GetPicture(item), r.Deflate(1));
        }
    }

    /// <summary> The flooring's square in its picture (it spans 15–85 %), without the bevelled edge. </summary>
    private static readonly Rect FloorCrop = new(0.165, 0.165, 0.67, 0.67);

    /// <summary> The wallpaper's pattern in its picture (the upper part of the wall panel, seen slightly from the side). </summary>
    private static readonly Rect WallPatternCrop = new(0.26, 0.26, 0.42, 0.25);

    /// <summary> The lower part of the wall panel: wood panelling in many wallpapers, more pattern in the rest. </summary>
    private static readonly Rect WallLowerCrop = new(0.26, 0.65, 0.42, 0.14);

    /// <summary> Share of the wall height taken by its lower part. </summary>
    private const double WallLowerShare = 0.36;

    /// <summary>
    /// Paints a wall as its wallpaper looks: the pattern above, repeated in rows no taller than <paramref name="rowHeight"/>,
    /// and the lower part in one row below, with a thin rail between them.
    /// </summary>
    private static void PaintWall(DrawingContext context, IImage? picture, Rect wall, double rowHeight)
    {
        if (picture is null)
            return;
        var lowerHeight = wall.Height * WallLowerShare;
        var upper = new Rect(wall.X, wall.Y, wall.Width, wall.Height - lowerHeight);
        var lower = new Rect(wall.X, upper.Bottom, wall.Width, lowerHeight);
        var rows = Math.Max(1, Math.Ceiling(upper.Height / Math.Max(1, rowHeight)));
        TileTexture(context, picture, upper, Width(picture, WallPatternCrop, upper.Height / rows), WallPatternCrop);
        TileTexture(context, picture, lower, Width(picture, WallLowerCrop, lower.Height), WallLowerCrop);
        context.FillRectangle(new SolidColorBrush(Color.FromArgb(0x48, 0x20, 0x14, 0x08)), new Rect(wall.X, lower.Y - 1, wall.Width, 2.5));

        static double Width(IImage picture, Rect crop, double height)
            => height * (picture.Size.Width * crop.Width) / (picture.Size.Height * crop.Height);
    }

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

    /// <param name="anchorBottom">Puts the picture at the bottom of <paramref name="dest"/> (standing on it) instead of in the middle.</param>
    private static void DrawPicture(DrawingContext context, IImage? picture, Rect dest, bool anchorBottom = false)
    {
        if (picture is null)
            return;
        var size = picture.Size;
        var scale = Math.Min(dest.Width / size.Width, dest.Height / size.Height);
        var w = size.Width * scale;
        var h = size.Height * scale;
        var y = anchorBottom ? dest.Bottom - h : dest.Y + ((dest.Height - h) / 2);
        context.DrawImage(picture, new Rect(size), new Rect(dest.X + ((dest.Width - w) / 2), y, w, h));
    }

    private void DrawBubble(DrawingContext context, string text, IImage? picture, Point at)
        => DrawBubble(context, text, picture, at, Bounds.Size, ActualThemeVariant == Avalonia.Styling.ThemeVariant.Dark);

    private const double TipPicture = 128;

    /// <summary>
    /// Bubble next to the pointer with the name of what is under it and, for items, its picture in large.
    /// </summary>
    internal static void DrawBubble(DrawingContext context, string text, IImage? picture, Point at, Size area, bool dark)
    {
        IBrush fg = dark ? Brushes.White : new SolidColorBrush(Color.FromRgb(0x22, 0x22, 0x22));
        IBrush bg = dark ? new SolidColorBrush(Color.FromArgb(0xF2, 0x2C, 0x2C, 0x2F)) : new SolidColorBrush(Color.FromArgb(0xF7, 0xFF, 0xFF, 0xFF));
        var ft = new FormattedText(text, CultureInfo.CurrentCulture, FlowDirection.LeftToRight, Font, picture is null ? 13 : 14, fg)
        {
            MaxTextWidth = picture is null ? double.PositiveInfinity : 200,
            TextAlignment = picture is null ? TextAlignment.Left : TextAlignment.Center,
        };
        var pictureHeight = picture is null ? 0 : TipPicture + 6;
        var w = Math.Max(ft.Width, picture is null ? 0 : TipPicture) + 20;
        var h = ft.Height + pictureHeight + (picture is null ? 10 : 16);
        var x = Math.Max(6, Math.Min(at.X + 16, area.Width - w - 6));
        var y = at.Y + 20;
        if (y + h > area.Height - 6)
            y = Math.Max(6, at.Y - h - 12);
        var r = new Rect(x, y, w, h);
        context.DrawRectangle(bg, new Pen(new SolidColorBrush(Color.FromArgb(0x40, 0, 0, 0)), 1), r, 10, 10, new BoxShadows(new BoxShadow { Blur = 12, OffsetY = 3, Color = Color.FromArgb(0x48, 0, 0, 0) }));
        if (picture is not null)
        {
            using (context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality }))
                context.DrawImage(picture, new Rect(picture.Size), new Rect(r.X + ((w - TipPicture) / 2), r.Y + 8, TipPicture, TipPicture));
            context.DrawText(ft, new Point(r.X + ((w - ft.Width) / 2), r.Y + 8 + pictureHeight));
            return;
        }
        context.DrawText(ft, new Point(r.X + 10, r.Y + 5));
    }
    #endregion
}
