using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// A building drawn on the island map. Coordinates are in item tiles (two per square) from the top left of the whole map.
/// </summary>
public sealed record MapBuilding(BuildingType Type, double CenterX, double CenterY, double Width, double Height, string Name,
    Color Roof, IImage? Icon, IImage? Portrait, double Angle, int Index)
{
    public Rect Bounds => new(CenterX - (Width / 2), CenterY - (Height / 2), Width, Height);

    public bool IsHouse => Type is >= BuildingType.PlayerHouse1 and <= BuildingType.Villager10;
    public bool IsCrossing => Type is BuildingType.Bridge or BuildingType.Incline;
}

/// <summary>
/// The island drawn like the game's map: the painted ground, the buildings and every item with its picture.
/// Drag to move, ⌘ + scroll / pinch / <see cref="ZoomBy"/> to zoom; a click reports the tile under the pointer.
/// Positions are item tiles from the top left corner of the whole map (two per square).
/// </summary>
public sealed class IslandMapView : Control
{
    /// <summary> Item tiles across and down the whole map. </summary>
    public const double MapWidth = IslandPainter.SquaresWide * 2;
    public const double MapHeight = IslandPainter.SquaresHigh * 2;

    /// <summary> The part of the map with land (beaches included), shown by <see cref="FitIsland"/>. </summary>
    public static readonly Rect IslandArea = new(26, 26, MapWidth - 52, MapHeight - 52);

    private const double MaxZoom = 30;

    private WriteableBitmap? Ground;
    private readonly IReadOnlyList<MapBuilding> Buildings;
    private readonly Rect Plaza;
    private readonly LayerFieldItem[] Layers;
    private readonly (int X, int Y) LayerOrigin;
    private List<MapItem>[] Drawn = [[], []];

    /// <summary> Screen pixels per item tile. </summary>
    public double Zoom { get; private set; } = 3;

    /// <summary> Screen position of the map's top left corner. </summary>
    public Point Offset { get; private set; }

    public bool ShowGrid { get => _showGrid; set { _showGrid = value; InvalidateVisual(); } }
    private bool _showGrid;

    /// <summary> Item layer being edited (0 = on the ground, 1 = on top of things); the other one is drawn faded. </summary>
    public int ActiveLayer { get => _activeLayer; set { _activeLayer = value; InvalidateVisual(); } }
    private int _activeLayer;

    /// <summary> Selected area (item tiles), drawn with a thick outline. </summary>
    public Rect? Selection { get => _selection; set { _selection = value; InvalidateVisual(); } }
    /// <summary> Where the chosen furniture faces (0 down, 1 right, 2 up, 3 left), shown with an arrow. </summary>
    public int? SelectionFacing { get => _facing; set { _facing = value; InvalidateVisual(); } }
    private int? _facing;

    private Rect? _selection;

    /// <summary> Raised when a tile is clicked (item tile coordinates of the whole map). </summary>
    public event Action<int, int>? TileClicked;

    /// <summary> Raised when the view moves or zooms. </summary>
    public event Action? ViewChanged;

    /// <summary> Raised when an item of the active layer is dragged elsewhere: its corner, then the new one (map item tiles). </summary>
    public event Action<int, int, int, int>? ItemDragged;

    /// <summary> Raised when an area is chosen by dragging with ⇧ held (map item tiles, whole squares). </summary>
    public event Action<Rect>? AreaChosen;

    /// <summary> Chosen area (map item tiles), drawn dashed until it is cleared. </summary>
    public Rect? Area { get => _area; set { _area = value; InvalidateVisual(); } }
    private Rect? _area;

    /// <summary> Whether an item would fit with its corner at a map tile, to colour the item being dragged. </summary>
    public Func<Item, int, int, bool>? CanPut { get; set; }

    private enum Gesture { None, Pan, Item, Area }
    private Gesture Doing;
    private (int X, int Y, int HoldX, int HoldY, Item Item)? Held;
    private (Item Item, int X, int Y)? Ghost;
    private Point AreaStart;

    // Pointer state
    private Point PressPoint;
    private Point PressOffset;
    private bool Pressed;
    private bool Panning;
    private Point? HoverPoint;
    private (Rect Area, string Text, IImage? Picture)? Hover;
    private bool FirstLayout = true;

    private static readonly Typeface LabelFont = new(FontFamily.Default, FontStyle.Normal, FontWeight.SemiBold);

    /// <param name="ground">Painted ground (<see cref="IslandPainter"/>), or null to give it later with <see cref="SetGround"/>.</param>
    public IslandMapView(WriteableBitmap? ground, IReadOnlyList<MapBuilding> buildings, Rect plaza, LayerFieldItem[] layers, (int X, int Y) layerOrigin)
    {
        Ground = ground;
        Buildings = buildings;
        Plaza = plaza;
        Layers = layers;
        LayerOrigin = layerOrigin;
        ClipToBounds = true;
        Focusable = true;
        Cursor = new Cursor(StandardCursorType.Hand);
        RefreshItems();
    }

    /// <summary>
    /// Shows the painted ground (once it is ready).
    /// </summary>
    public void SetGround(WriteableBitmap ground)
    {
        Ground = ground;
        InvalidateVisual();
    }

    #region Items
    private enum Look : byte { Normal, Tree, Bush, Flower, Stump, Rock, Hole, Buried, Weed }

    private sealed record MapItem(Rect Area, Item Item, IImage? Picture, Look Look);

    /// <summary>
    /// Reads the items again from the layers (after a change).
    /// </summary>
    public void RefreshItems()
    {
        Drawn = [Collect(Layers[0]), Collect(Layers[1])];
        InvalidateVisual();
    }

    /// <summary> Pictures already looked up (finding one checks the disk). </summary>
    private readonly Dictionary<(ushort Id, ushort Count, bool Odd), IImage?> Pictures = [];

    private IImage? GetPicture(Item item)
    {
        var key = (item.ItemId, item.Count, item.IsBuried);
        if (!Pictures.TryGetValue(key, out var picture))
            Pictures[key] = picture = GridItems.GetPicture(item);
        return picture;
    }

    private List<MapItem> Collect(LayerFieldItem layer)
    {
        var list = new List<MapItem>();
        var (w, h) = (layer.TileInfo.TotalWidth, layer.TileInfo.TotalHeight);
        for (int x = 0; x < w; x++)
        {
            for (int y = 0; y < h; y++)
            {
                var item = layer.GetTile(x, y);
                if (item.IsNone || !item.IsRoot)
                    continue;
                var (iw, ih) = GridItems.GetSize(item);
                var area = new Rect(x + LayerOrigin.X, y + LayerOrigin.Y, iw, ih);
                list.Add(new MapItem(area, item, GetPicture(item), GetLook(item)));
            }
        }
        // Lower things are drawn last, so trees in front cover the ones behind.
        list.Sort((a, b) => a.Area.Bottom.CompareTo(b.Area.Bottom));
        return list;
    }

    private static Look GetLook(Item item)
    {
        if (item.IsBuried)
            return Look.Buried;
        if (!FieldItemList.Items.TryGetValue(item.ItemId, out var def))
            return Look.Normal;
        var k = def.Kind;
        if (def.Name.Contains("Stump", StringComparison.Ordinal))
            return Look.Stump;
        if (k.IsTree)
            return Look.Tree;
        if (k.IsBush)
            return Look.Bush;
        if (k.IsStone)
            return Look.Rock;
        if (k == FieldItemKind.UnitIconHole)
            return Look.Hole;
        if (k.IsWeed)
            return Look.Weed;
        return k.IsPlant ? Look.Flower : Look.Normal;
    }

    /// <summary>
    /// The item under a point of the map (item tiles), on the given layer.
    /// </summary>
    public Item? ItemAt(int layer, double x, double y)
    {
        var list = Drawn[layer];
        for (int i = list.Count - 1; i >= 0; i--)
        {
            if (list[i].Area.Contains(new Point(x, y)))
                return list[i].Item;
        }
        return null;
    }

    public MapBuilding? BuildingAt(double x, double y)
    {
        foreach (var b in Buildings)
        {
            if (!b.IsCrossing && b.Bounds.Contains(new Point(x, y)))
                return b;
        }
        return null;
    }
    #endregion

    #region View
    /// <summary> Map position (item tiles) of a point of the control. </summary>
    public Point ToMap(Point p) => new((p.X - Offset.X) / Zoom, (p.Y - Offset.Y) / Zoom);

    /// <summary> Point of the control of a map position (item tiles). </summary>
    public Point ToScreen(Point p) => new((p.X * Zoom) + Offset.X, (p.Y * Zoom) + Offset.Y);

    private Rect ToScreen(Rect r) => new(ToScreen(r.TopLeft), ToScreen(r.BottomRight));

    private double MinZoom => Math.Max(0.5, FitZoom(new Rect(0, 0, MapWidth, MapHeight)) * 0.9);

    private double FitZoom(Rect area)
    {
        var size = Bounds.Size;
        if (size.Width <= 0 || size.Height <= 0)
            return Zoom;
        return Math.Min(size.Width / area.Width, size.Height / area.Height);
    }

    /// <summary>
    /// Shows the whole island in the control.
    /// </summary>
    public void FitIsland() => ShowArea(IslandArea);

    /// <summary>
    /// Zooms so the area fills the control, centered.
    /// </summary>
    public void ShowArea(Rect area, double maxZoom = MaxZoom)
    {
        var zoom = Math.Clamp(FitZoom(area), MinZoom, maxZoom);
        SetView(zoom, new Point(area.Center.X, area.Center.Y), new Point(Bounds.Width / 2, Bounds.Height / 2));
    }

    /// <summary>
    /// Moves the view so the area (item tiles) is visible, if it isn't already.
    /// </summary>
    public void Reveal(Rect area)
    {
        if (Bounds.Width <= 0 || new Rect(Bounds.Size).Deflate(20).Contains(ToScreen(area)))
            return;
        CenterOn(area.Center);
    }

    /// <summary>
    /// Centers the view on a map position (item tiles), keeping the zoom.
    /// </summary>
    public void CenterOn(Point map) => SetView(Zoom, map, new Point(Bounds.Width / 2, Bounds.Height / 2));

    /// <summary>
    /// Zooms in (factor above 1) or out around a point of the control (its center by default).
    /// </summary>
    public void ZoomBy(double factor, Point? around = null)
    {
        var anchor = around ?? new Point(Bounds.Width / 2, Bounds.Height / 2);
        var map = ToMap(anchor);
        SetView(Math.Clamp(Zoom * factor, MinZoom, MaxZoom), map, anchor);
    }

    /// <summary>
    /// Sets the zoom and puts the map position <paramref name="map"/> at the control point <paramref name="screen"/>.
    /// </summary>
    private void SetView(double zoom, Point map, Point screen)
    {
        Zoom = zoom;
        Offset = new Point(screen.X - (map.X * zoom), screen.Y - (map.Y * zoom));
        ClampOffset();
        InvalidateVisual();
        ViewChanged?.Invoke();
    }

    private void PanBy(Vector delta)
    {
        Offset += delta;
        ClampOffset();
        InvalidateVisual();
        ViewChanged?.Invoke();
    }

    /// <summary>
    /// Keeps some of the map in view (the island can't be dragged away).
    /// </summary>
    private void ClampOffset()
    {
        var w = MapWidth * Zoom;
        var h = MapHeight * Zoom;
        var bw = Bounds.Width;
        var bh = Bounds.Height;
        double x = Offset.X, y = Offset.Y;
        x = w <= bw ? (bw - w) / 2 : Math.Clamp(x, bw - w, 0);
        y = h <= bh ? (bh - h) / 2 : Math.Clamp(y, bh - h, 0);
        Offset = new Point(x, y);
    }

    protected override Size ArrangeOverride(Size finalSize)
    {
        var size = base.ArrangeOverride(finalSize);
        if (FirstLayout && finalSize.Width > 0 && finalSize.Height > 0)
        {
            FirstLayout = false;
            Dispatcher_Post(FitIsland);
        }
        return size;
    }

    private static void Dispatcher_Post(Action a) => Avalonia.Threading.Dispatcher.UIThread.Post(a, Avalonia.Threading.DispatcherPriority.Render);

    protected override void OnSizeChanged(SizeChangedEventArgs e)
    {
        base.OnSizeChanged(e);
        if (FirstLayout)
            return;
        // Keep the same spot in the middle when the window is resized.
        var oldCenter = new Point(e.PreviousSize.Width / 2, e.PreviousSize.Height / 2);
        var map = ToMap(oldCenter);
        SetView(Math.Max(Zoom, MinZoom), map, new Point(e.NewSize.Width / 2, e.NewSize.Height / 2));
    }
    #endregion

    #region Pointer
    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        base.OnPointerPressed(e);
        var point = e.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed && !point.Properties.IsMiddleButtonPressed)
            return;
        Focus();
        Pressed = true;
        Panning = point.Properties.IsMiddleButtonPressed;
        PressPoint = point.Position;
        PressOffset = Offset;
        Held = null;
        Doing = Panning ? Gesture.Pan : Gesture.None;
        if (!Panning)
        {
            var map = ToMap(point.Position);
            if ((e.KeyModifiers & KeyModifiers.Shift) != 0)
            {
                Doing = Gesture.Area;
                AreaStart = map;
            }
            else if (ItemAt(map) is { } held)
            {
                Held = held;
            }
        }
        e.Pointer.Capture(this);
        e.Handled = true;
    }

    /// <summary> The item of the active layer under a map point, its corner and where in it the point is. </summary>
    private (int X, int Y, int HoldX, int HoldY, Item Item)? ItemAt(Point map)
    {
        var layer = Layers[ActiveLayer];
        int x = (int)Math.Floor(map.X) - LayerOrigin.X, y = (int)Math.Floor(map.Y) - LayerOrigin.Y;
        if (!layer.Contains(x, y) || !GridItems.TryGetRoot(layer, x, y, out var root, out var rx, out var ry) || !root.IsRoot)
            return null;
        var copy = new Item();
        copy.CopyFrom(root);
        return (rx + LayerOrigin.X, ry + LayerOrigin.Y, x - rx, y - ry, copy);
    }

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        base.OnPointerMoved(e);
        var p = e.GetPosition(this);
        if (Pressed)
        {
            var delta = p - PressPoint;
            var moved = Math.Abs(delta.X) > 4 || Math.Abs(delta.Y) > 4;
            if (Doing == Gesture.None && moved)
            {
                Doing = Held is not null ? Gesture.Item : Gesture.Pan;
                Panning = Doing == Gesture.Pan;
                if (Panning)
                    Cursor = new Cursor(StandardCursorType.SizeAll);
            }
            switch (Doing)
            {
                case Gesture.Pan:
                    Offset = PressOffset;
                    PanBy(delta);
                    break;
                case Gesture.Item when Held is { } held:
                {
                    var map = ToMap(p);
                    int x = (int)Math.Floor(map.X) - held.HoldX, y = (int)Math.Floor(map.Y) - held.HoldY;
                    if (held.Item.IsFieldItem)
                        (x, y) = (x & ~1, y & ~1); // plants, fences and rocks go on whole squares
                    Ghost = (held.Item, x, y);
                    Hover = null;
                    InvalidateVisual();
                    break;
                }
                case Gesture.Area:
                    Area = Snap(AreaStart, ToMap(p));
                    break;
            }
            return;
        }
        SetHover(p);
    }

    /// <summary> The whole squares covered by the rectangle between two map points. </summary>
    private static Rect Snap(Point a, Point b)
    {
        var x0 = Math.Floor(Math.Min(a.X, b.X) / 2) * 2;
        var y0 = Math.Floor(Math.Min(a.Y, b.Y) / 2) * 2;
        var x1 = (Math.Floor(Math.Max(a.X, b.X) / 2) * 2) + 2;
        var y1 = (Math.Floor(Math.Max(a.Y, b.Y) / 2) * 2) + 2;
        return new Rect(Math.Max(0, x0), Math.Max(0, y0), Math.Min(MapWidth, x1) - Math.Max(0, x0), Math.Min(MapHeight, y1) - Math.Max(0, y0));
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        base.OnPointerReleased(e);
        if (!Pressed)
            return;
        Pressed = false;
        e.Pointer.Capture(null);
        Cursor = new Cursor(StandardCursorType.Hand);
        var doing = Doing;
        Doing = Gesture.None;
        Panning = false;
        var ghost = Ghost;
        Ghost = null;
        if (doing == Gesture.Pan)
            return;
        if (e.InitialPressMouseButton != MouseButton.Left)
            return;
        if (doing == Gesture.Item && Held is { } held && ghost is { } to)
        {
            InvalidateVisual();
            ItemDragged?.Invoke(held.X, held.Y, to.X, to.Y);
            return;
        }
        if (doing == Gesture.Area && Area is { } area)
        {
            AreaChosen?.Invoke(area);
            return;
        }
        var map = ToMap(e.GetPosition(this));
        if (map.X < 0 || map.Y < 0 || map.X >= MapWidth || map.Y >= MapHeight)
            return;
        TileClicked?.Invoke((int)map.X, (int)map.Y);
    }

    protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs e)
    {
        base.OnPointerCaptureLost(e);
        Pressed = false;
        Panning = false;
        Doing = Gesture.None;
        Ghost = null;
        InvalidateVisual();
    }

    protected override void OnPointerExited(PointerEventArgs e)
    {
        base.OnPointerExited(e);
        HoverPoint = null;
        Hover = null;
        InvalidateVisual();
    }

    protected override void OnPointerWheelChanged(PointerWheelEventArgs e)
    {
        base.OnPointerWheelChanged(e);
        var p = e.GetPosition(this);
        if ((e.KeyModifiers & (KeyModifiers.Meta | KeyModifiers.Control)) != 0)
            ZoomBy(Math.Exp(e.Delta.Y * 0.15), p);
        else
            PanBy(new Vector(e.Delta.X, e.Delta.Y) * 40);
        SetHover(p);
        e.Handled = true;
    }

    /// <summary>
    /// Trackpad pinch (macOS magnify gesture).
    /// </summary>
    private void OnMagnify(object? sender, PointerDeltaEventArgs e)
    {
        ZoomBy(1 + e.Delta.X, e.GetPosition(this));
        e.Handled = true;
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnAttachedToVisualTree(e);
        AddHandler(PointerTouchPadGestureMagnifyEvent, OnMagnify);
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnDetachedFromVisualTree(e);
        RemoveHandler(PointerTouchPadGestureMagnifyEvent, OnMagnify);
    }

    /// <summary>
    /// Shows the name of what is under the pointer (control coordinates).
    /// </summary>
    public void SetHover(Point p)
    {
        HoverPoint = p;
        var map = ToMap(p);
        Hover = null;
        var item = Drawn[ActiveLayer].LastOrDefault(z => z.Area.Contains(map));
        if (item is not null)
        {
            var text = GridItems.GetName(item.Item);
            if (item.Item.IsBuried)
                text += " · " + MapStrings.Buried;
            Hover = (item.Area, text, item.Picture);
        }
        else if (BuildingAt(map.X, map.Y) is { } b)
        {
            Hover = (b.Bounds, b.Name, null);
        }
        else if (map.X >= 0 && map.Y >= 0 && map.X < MapWidth && map.Y < MapHeight)
        {
            Hover = (new Rect(Math.Floor(map.X / 2) * 2, Math.Floor(map.Y / 2) * 2, 2, 2), string.Empty, null);
        }
        InvalidateVisual();
    }
    #endregion

    #region Drawing
    public override void Render(DrawingContext context)
    {
        var bounds = new Rect(Bounds.Size);
        context.FillRectangle(new SolidColorBrush(IslandPainter.SeaColor), bounds);
        if (Ground is null)
        {
            // Still painting (a moment when the window opens).
            var wait = new FormattedText(MapStrings.Painting, CultureInfo.CurrentCulture, FlowDirection.LeftToRight, LabelFont, 15, Brushes.White);
            context.DrawText(wait, new Point((bounds.Width - wait.Width) / 2, (bounds.Height - wait.Height) / 2));
            return;
        }

        var visible = new Rect(ToMap(bounds.TopLeft), ToMap(bounds.BottomRight)).Inflate(4);
        using (context.PushTransform(Matrix.CreateScale(Zoom, Zoom) * Matrix.CreateTranslation(Offset.X, Offset.Y)))
        {
            using (context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality }))
                context.DrawImage(Ground, new Rect(Ground.Size), new Rect(0, 0, MapWidth, MapHeight));

            DrawPlaza(context);
            foreach (var b in Buildings.Where(z => z.IsCrossing))
                DrawCrossing(context, b);

            using (context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality }))
            {
                using (context.PushOpacity(ActiveLayer == 0 ? 1 : 0.45))
                    DrawItems(context, Drawn[0], visible);
                foreach (var b in Buildings.Where(z => !z.IsCrossing))
                {
                    if (b.Bounds.Intersects(visible))
                        DrawBuilding(context, b);
                }
                DrawItems(context, Drawn[1], visible, true);
            }
        }

        if (ShowGrid)
            DrawGrid(context, bounds);
        DrawLabels(context, bounds);

        if (Hover is { } hover && !Panning)
            context.DrawRectangle(new SolidColorBrush(Color.FromArgb(0x30, 0xFF, 0xFF, 0xFF)), new Pen(new SolidColorBrush(Color.FromArgb(0xD0, 0xFF, 0xFF, 0xFF)), 1.5), ToScreen(hover.Area).Inflate(1), 4, 4);
        if (Selection is { } sel && Doing != Gesture.Item)
            RoomView.DrawSelection(context, ToScreen(sel), SelectionFacing);
        if (Area is { } area)
        {
            var r = ToScreen(area);
            context.DrawRectangle(new SolidColorBrush(Color.FromArgb(0x30, 0xFF, 0xFF, 0xFF)), new Pen(Brushes.White, 2) { DashStyle = new DashStyle([5, 4], 0) }, r, 4, 4);
            context.DrawRectangle(null, new Pen(new SolidColorBrush(Color.FromArgb(0x70, 0, 0, 0)), 1), r.Inflate(1.5), 5, 5);
        }
        if (Ghost is { } ghost)
            DrawGhost(context, ghost.Item, ghost.X, ghost.Y);
        if (Hover is { Text.Length: > 0 } tip && HoverPoint is { } at && !Panning && Doing == Gesture.None)
            RoomView.DrawBubble(context, tip.Text, tip.Picture, at, bounds.Size, ActualThemeVariant == Avalonia.Styling.ThemeVariant.Dark);
    }

    /// <summary>
    /// The item being dragged on the spot it would take: green if it fits, red if not.
    /// </summary>
    private void DrawGhost(DrawingContext context, Item item, int x, int y)
    {
        var (w, h) = GridItems.GetSize(item);
        var r = ToScreen(new Rect(x, y, w, h));
        var fits = CanPut?.Invoke(item, x, y) ?? true;
        var color = fits ? Color.FromRgb(0x2F, 0x9E, 0x62) : Color.FromRgb(0xD0, 0x40, 0x40);
        context.DrawRectangle(new SolidColorBrush(color, 0.3), new Pen(new SolidColorBrush(color), 2.5), r, 4, 4);
        using (context.PushOpacity(0.85))
        using (context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality }))
            DrawPicture(context, GetPicture(item), r.Deflate(Math.Min(r.Width, r.Height) * 0.05));
    }

    private void DrawItems(DrawingContext context, List<MapItem> items, Rect visible, bool onTop = false)
    {
        foreach (var it in items)
        {
            var a = it.Area;
            if (!a.Intersects(visible))
                continue;
            switch (it.Look)
            {
                case Look.Tree:
                {
                    // Trees stand taller than their square, with a shadow at their foot.
                    context.DrawEllipse(ShadowBrush, null, new Point(a.Center.X + 0.15, a.Bottom - 0.35), a.Width * 0.42, a.Height * 0.2);
                    var size = a.Width * 1.6;
                    DrawPicture(context, it.Picture, new Rect(a.Center.X - (size / 2), a.Bottom - size + 0.1, size, size));
                    break;
                }
                case Look.Bush:
                {
                    context.DrawEllipse(ShadowBrush, null, new Point(a.Center.X + 0.1, a.Bottom - 0.3), a.Width * 0.4, a.Height * 0.18);
                    var size = a.Width * 1.2;
                    DrawPicture(context, it.Picture, new Rect(a.Center.X - (size / 2), a.Bottom - size + 0.1, size, size));
                    break;
                }
                case Look.Stump:
                    context.DrawEllipse(new SolidColorBrush(Color.FromRgb(0xA8, 0x7A, 0x4E)), new Pen(new SolidColorBrush(Color.FromRgb(0x6E, 0x4B, 0x2C)), 0.12), a.Center, a.Width * 0.3, a.Height * 0.26);
                    context.DrawEllipse(null, new Pen(new SolidColorBrush(Color.FromRgb(0xD9, 0xB3, 0x82)), 0.08), a.Center, a.Width * 0.15, a.Height * 0.12);
                    break;
                case Look.Rock:
                {
                    var r = new Rect(a.X + (a.Width * 0.08), a.Y + (a.Height * 0.15), a.Width * 0.84, a.Height * 0.75);
                    context.DrawRectangle(ShadowBrush, null, r.Translate(new Vector(0.15, 0.2)), a.Width * 0.35, a.Height * 0.3);
                    context.DrawRectangle(new SolidColorBrush(Color.FromRgb(0x9C, 0x9E, 0xA3)), new Pen(new SolidColorBrush(Color.FromRgb(0x6C, 0x6E, 0x74)), 0.12), r, a.Width * 0.35, a.Height * 0.3);
                    context.DrawEllipse(new SolidColorBrush(Color.FromArgb(0x70, 0xFF, 0xFF, 0xFF)), null, new Point(r.X + (r.Width * 0.35), r.Y + (r.Height * 0.3)), r.Width * 0.18, r.Height * 0.12);
                    break;
                }
                case Look.Hole:
                    context.DrawEllipse(new SolidColorBrush(Color.FromRgb(0x5A, 0x40, 0x2A)), null, a.Center, a.Width * 0.32, a.Height * 0.25);
                    break;
                case Look.Buried:
                {
                    // The crack in the ground the game shows over buried things.
                    var pen = new Pen(new SolidColorBrush(Color.FromRgb(0x6B, 0x4E, 0x30)), 0.16, lineCap: PenLineCap.Round);
                    var c = a.Center;
                    var s = a.Width * 0.22;
                    context.DrawLine(pen, new Point(c.X - s, c.Y - s), new Point(c.X + s, c.Y + s));
                    context.DrawLine(pen, new Point(c.X - s, c.Y + s), new Point(c.X + s, c.Y - s));
                    context.DrawLine(pen, new Point(c.X, c.Y - (s * 1.2)), new Point(c.X, c.Y + (s * 1.2)));
                    break;
                }
                default:
                {
                    var pad = Math.Min(a.Width, a.Height) * (onTop ? 0.12 : 0.04);
                    DrawPicture(context, it.Picture, a.Deflate(pad));
                    break;
                }
            }
        }
    }

    private static readonly IBrush ShadowBrush = new SolidColorBrush(Color.FromArgb(0x38, 0x10, 0x30, 0x10));

    private static void DrawPicture(DrawingContext context, IImage? picture, Rect dest)
    {
        if (picture is null)
            return;
        context.DrawImage(picture, new Rect(picture.Size), Fit(picture, dest));
    }

    /// <summary>
    /// Where a picture goes inside an area, keeping its shape, centered.
    /// </summary>
    internal static Rect Fit(IImage? picture, Rect dest)
    {
        if (picture is null)
            return dest;
        var size = picture.Size;
        var scale = Math.Min(dest.Width / size.Width, dest.Height / size.Height);
        var w = size.Width * scale;
        var h = size.Height * scale;
        return new Rect(dest.X + ((dest.Width - w) / 2), dest.Y + ((dest.Height - h) / 2), w, h);
    }

    private void DrawPlaza(DrawingContext context)
    {
        if (Plaza.Width <= 0)
            return;
        context.DrawRectangle(new SolidColorBrush(Color.FromArgb(0x70, 0xF2, 0xE6, 0xC8)), new Pen(new SolidColorBrush(Color.FromArgb(0x60, 0xA0, 0x8C, 0x60)), 0.25), Plaza, 2.5, 2.5);
    }

    /// <summary>
    /// Bridges and inclines: planks across the river, steps up the cliff.
    /// </summary>
    private static void DrawCrossing(DrawingContext context, MapBuilding b)
    {
        var c = new Point(b.CenterX, b.CenterY);
        using var _ = context.PushTransform(Matrix.CreateTranslation(-c.X, -c.Y) * Matrix.CreateRotation(b.Angle * Math.PI / 180) * Matrix.CreateTranslation(c.X, c.Y));
        // Long side across (x axis before turning).
        var r = new Rect(c.X - (b.Width / 2), c.Y - (b.Height / 2), b.Width, b.Height);
        context.DrawRectangle(ShadowBrush, null, r.Translate(new Vector(0.3, 0.4)), 0.6, 0.6);
        if (b.Type == BuildingType.Bridge)
        {
            var wood = new SolidColorBrush(b.Roof);
            var dark = new Pen(new SolidColorBrush(Darken(b.Roof, 0.65)), 0.18);
            context.DrawRectangle(wood, dark, r, 0.5, 0.5);
            var plank = new Pen(new SolidColorBrush(Darken(b.Roof, 0.8)), 0.1);
            for (double x = r.X + 1; x < r.Right - 0.5; x += 1)
                context.DrawLine(plank, new Point(x, r.Y + 0.35), new Point(x, r.Bottom - 0.35));
            var rail = new Pen(new SolidColorBrush(Darken(b.Roof, 0.55)), 0.35);
            context.DrawLine(rail, new Point(r.X + 0.3, r.Y + 0.25), new Point(r.Right - 0.3, r.Y + 0.25));
            context.DrawLine(rail, new Point(r.X + 0.3, r.Bottom - 0.25), new Point(r.Right - 0.3, r.Bottom - 0.25));
        }
        else
        {
            var stone = new SolidColorBrush(b.Roof);
            context.DrawRectangle(stone, new Pen(new SolidColorBrush(Darken(b.Roof, 0.7)), 0.18), r, 0.4, 0.4);
            var step = new Pen(new SolidColorBrush(Darken(b.Roof, 0.78)), 0.14);
            for (double x = r.X + 0.9; x < r.Right - 0.4; x += 0.9)
                context.DrawLine(step, new Point(x, r.Y + 0.2), new Point(x, r.Bottom - 0.2));
        }
    }

    private void DrawBuilding(DrawingContext context, MapBuilding b)
    {
        var r = b.Bounds;
        if (b.Type == BuildingType.Campsite || b.Type == BuildingType.ResidentCenterTent)
        {
            DrawTent(context, r, b.Roof);
        }
        else
        {
            // House seen from above and a little from the front: walls, a big roof and the door.
            var walls = new Rect(r.X + (r.Width * 0.1), r.Y + (r.Height * 0.3), r.Width * 0.8, r.Height * 0.66);
            context.DrawRectangle(ShadowBrush, null, walls.Translate(new Vector(0.45, 0.55)), 0.5, 0.5);
            context.DrawRectangle(new SolidColorBrush(Color.FromRgb(0xF6, 0xEE, 0xDF)), new Pen(new SolidColorBrush(Color.FromRgb(0x9C, 0x8B, 0x73)), 0.15), walls, 0.35, 0.35);
            var door = new Rect(r.Center.X - (r.Width * 0.09), walls.Bottom - (r.Height * 0.3), r.Width * 0.18, r.Height * 0.3);
            context.DrawRectangle(new SolidColorBrush(Color.FromRgb(0x8A, 0x5A, 0x36)), null, door, 0.25, 0.25);

            var roof = new Rect(r.X, r.Y, r.Width, r.Height * 0.66);
            var roofGeometry = new StreamGeometry();
            using (var g = roofGeometry.Open())
            {
                var inset = r.Width * 0.12;
                g.BeginFigure(new Point(roof.X + inset, roof.Y), true);
                g.LineTo(new Point(roof.Right - inset, roof.Y));
                g.LineTo(new Point(roof.Right, roof.Bottom));
                g.LineTo(new Point(roof.X, roof.Bottom));
                g.EndFigure(true);
            }
            context.DrawGeometry(new SolidColorBrush(b.Roof), new Pen(new SolidColorBrush(Darken(b.Roof, 0.62)), 0.18, lineJoin: PenLineJoin.Round), roofGeometry);
            // Eaves and ridge
            context.DrawLine(new Pen(new SolidColorBrush(Darken(b.Roof, 0.75)), 0.3), new Point(roof.X + 0.2, roof.Bottom - 0.2), new Point(roof.Right - 0.2, roof.Bottom - 0.2));
            context.DrawLine(new Pen(new SolidColorBrush(Color.FromArgb(0x60, 0xFF, 0xFF, 0xFF)), 0.25), new Point(roof.X + (r.Width * 0.2), roof.Y + 0.35), new Point(roof.Right - (r.Width * 0.2), roof.Y + 0.35));
        }

        // Badge in the middle: the villager's face or the building's icon.
        var badgeSize = Math.Min(r.Width, r.Height) * (b.Portrait is not null ? 0.78 : 0.62);
        var center = new Point(r.Center.X, r.Y + (r.Height * 0.36));
        var badge = new Rect(center.X - (badgeSize / 2), center.Y - (badgeSize / 2), badgeSize, badgeSize);
        if (b.Portrait is not null)
        {
            context.DrawEllipse(Brushes.White, new Pen(new SolidColorBrush(Darken(b.Roof, 0.7)), 0.2), badge.Center, badgeSize / 2, badgeSize / 2);
            using (context.PushGeometryClip(new EllipseGeometry(badge.Deflate(0.15))))
                DrawPicture(context, b.Portrait, badge.Deflate(0.1));
        }
        else if (b.Icon is not null)
        {
            context.DrawEllipse(new SolidColorBrush(Color.FromArgb(0xF0, 0xFF, 0xFF, 0xFF)), new Pen(new SolidColorBrush(Darken(b.Roof, 0.7)), 0.18), badge.Center, badgeSize / 2, badgeSize / 2);
            DrawPicture(context, b.Icon, badge.Deflate(badgeSize * 0.16));
        }
    }

    private static void DrawTent(DrawingContext context, Rect r, Color color)
    {
        var tent = new StreamGeometry();
        using (var g = tent.Open())
        {
            g.BeginFigure(new Point(r.Center.X, r.Y + (r.Height * 0.05)), true);
            g.LineTo(new Point(r.Right - (r.Width * 0.05), r.Bottom - (r.Height * 0.08)));
            g.LineTo(new Point(r.X + (r.Width * 0.05), r.Bottom - (r.Height * 0.08)));
            g.EndFigure(true);
        }
        using (context.PushTransform(Matrix.CreateTranslation(0.45, 0.5)))
            context.DrawGeometry(ShadowBrush, null, tent);
        context.DrawGeometry(new SolidColorBrush(color), new Pen(new SolidColorBrush(Darken(color, 0.62)), 0.2, lineJoin: PenLineJoin.Round), tent);
        var door = new StreamGeometry();
        using (var g = door.Open())
        {
            g.BeginFigure(new Point(r.Center.X, r.Y + (r.Height * 0.45)), true);
            g.LineTo(new Point(r.Center.X + (r.Width * 0.12), r.Bottom - (r.Height * 0.08)));
            g.LineTo(new Point(r.Center.X - (r.Width * 0.12), r.Bottom - (r.Height * 0.08)));
            g.EndFigure(true);
        }
        context.DrawGeometry(new SolidColorBrush(Darken(color, 0.55)), null, door);
    }

    internal static Color Darken(Color c, double f) => Color.FromArgb(c.A, (byte)(c.R * f), (byte)(c.G * f), (byte)(c.B * f));

    private void DrawGrid(DrawingContext context, Rect bounds)
    {
        var step = Zoom * 2; // one square
        if (step < 5)
            return;
        var pen = new Pen(new SolidColorBrush(Color.FromArgb((byte)Math.Clamp(step * 4, 30, 80), 0xFF, 0xFF, 0xFF)), 1);
        var start = ToMap(bounds.TopLeft);
        var end = ToMap(bounds.BottomRight);
        for (var x = Math.Floor(start.X / 2) * 2; x <= end.X; x += 2)
        {
            var sx = Math.Round(ToScreen(new Point(x, 0)).X) + 0.5;
            context.DrawLine(pen, new Point(sx, 0), new Point(sx, bounds.Height));
        }
        for (var y = Math.Floor(start.Y / 2) * 2; y <= end.Y; y += 2)
        {
            var sy = Math.Round(ToScreen(new Point(0, y)).Y) + 0.5;
            context.DrawLine(pen, new Point(0, sy), new Point(bounds.Width, sy));
        }
    }

    /// <summary>
    /// Names under the buildings, at a size that doesn't change with the zoom. Houses only when there is room.
    /// </summary>
    private void DrawLabels(DrawingContext context, Rect bounds)
    {
        // Shops and services first: a name that would cover one already written is left out.
        var taken = new List<Rect>();
        foreach (var b in Buildings.OrderBy(z => z.IsHouse ? 1 : 0))
        {
            if (b.IsCrossing || b.Name.Length == 0)
                continue;
            var houseRoom = b.Width * Zoom;
            if (b.IsHouse && houseRoom < 46)
                continue;
            var anchor = ToScreen(new Point(b.CenterX, b.Bounds.Bottom));
            if (!bounds.Inflate(80).Contains(anchor))
                continue;
            var size = b.IsHouse ? 11.5 : 12.5;
            var text = new FormattedText(b.Name, CultureInfo.CurrentCulture, FlowDirection.LeftToRight, LabelFont, size, new SolidColorBrush(Color.FromRgb(0x3A, 0x33, 0x2A)));
            var pill = new Rect(anchor.X - (text.Width / 2) - 7, anchor.Y + 2, text.Width + 14, text.Height + 4);
            if (taken.Any(z => z.Intersects(pill)))
                continue;
            taken.Add(pill.Inflate(2));
            context.DrawRectangle(new SolidColorBrush(Color.FromArgb(0xEE, 0xFF, 0xFB, 0xF0)), new Pen(new SolidColorBrush(Color.FromArgb(0x50, 0x5A, 0x4A, 0x30)), 1), pill, pill.Height / 2, pill.Height / 2);
            context.DrawText(text, new Point(pill.X + 7, pill.Y + 2));
        }
    }

    #endregion
}
