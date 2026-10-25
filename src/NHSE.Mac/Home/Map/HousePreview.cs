using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// How a house will look from outside with the chosen roof, walls and door: a small drawing of the house in its garden.
/// The roof is covered with the tiles of its picture, the walls are drawn in their material (brick, logs, planks…) and
/// colour taken from their picture, and the door is its own picture. An approximation: the game has its own 3D models.
/// </summary>
public sealed class HousePreview : Control
{
    private string _roof = string.Empty;
    private string _walls = string.Empty;
    private string _door = string.Empty;

    public HousePreview()
    {
        Width = 320;
        Height = 230;
    }

    /// <summary> Shows the house with these looks (their internal names). </summary>
    public void Set(string roof, string walls, string door)
    {
        (_roof, _walls, _door) = (roof, walls, door);
        InvalidateVisual();
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnAttachedToVisualTree(e);
        ExteriorPictures.PicturesChanged += Refresh;
    }

    private void Refresh()
    {
        HouseComposer.Clear();
        InvalidateVisual();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnDetachedFromVisualTree(e);
        ExteriorPictures.PicturesChanged -= Refresh;
    }

    /// <summary> Part of a roof picture covered with tiles in every roof (left of the ridge, below the eaves). </summary>
    private static readonly RelativeRect RoofTiles = new(0.24, 0.42, 0.16, 0.20, RelativeUnit.Relative);

    public override void Render(DrawingContext context)
    {
        var b = new Rect(Bounds.Size);
        using var _ = context.PushClip(new RoundedRect(b, 12));

        // Sky and garden
        context.FillRectangle(new LinearGradientBrush
        {
            StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
            EndPoint = new RelativePoint(0, 1, RelativeUnit.Relative),
            GradientStops = { new GradientStop(Color.FromRgb(0x9F, 0xD6, 0xF2), 0), new GradientStop(Color.FromRgb(0xD8, 0xF0, 0xFA), 1) },
        }, b);
        var ground = b.Height * 0.8;
        context.FillRectangle(new SolidColorBrush(Color.FromRgb(0x8C, 0xCB, 0x6E)), new Rect(0, ground, b.Width, b.Height - ground));
        context.DrawEllipse(new SolidColorBrush(Color.FromRgb(0x7A, 0xBD, 0x5D)), null, new Point(b.Width * 0.12, ground + 4), b.Width * 0.12, 10);
        context.DrawEllipse(new SolidColorBrush(Color.FromRgb(0x7A, 0xBD, 0x5D)), null, new Point(b.Width * 0.9, ground + 6), b.Width * 0.14, 11);

        // The house made from the game's pictures, when they are there; otherwise a drawing.
        if (HouseComposer.Compose(_walls, _roof) is { } front)
        {
            DrawFront(context, front, b, ground);
            return;
        }

        // House
        var bodyW = b.Width * 0.56;
        var bodyH = b.Height * 0.38;
        var body = new Rect((b.Width - bodyW) / 2, ground - bodyH, bodyW, bodyH);
        context.DrawEllipse(new SolidColorBrush(Color.FromArgb(0x30, 0, 0, 0)), null, new Point(body.Center.X, ground + 2), bodyW * 0.6, 7);
        // Path to the door
        var path = new StreamGeometry();
        using (var g = path.Open())
        {
            g.BeginFigure(new Point(body.Center.X - (bodyW * 0.09), ground), true);
            g.LineTo(new Point(body.Center.X + (bodyW * 0.09), ground));
            g.LineTo(new Point(body.Center.X + (bodyW * 0.16), b.Height));
            g.LineTo(new Point(body.Center.X - (bodyW * 0.16), b.Height));
            g.EndFigure(true);
        }
        context.DrawGeometry(new SolidColorBrush(Color.FromRgb(0xE6, 0xD3, 0xA6)), null, path);

        var colors = ExteriorPictures.SidingColors(_walls) ?? (Color.FromRgb(0xE8, 0xE0, 0xD0), Color.FromRgb(0x9A, 0x6B, 0x4F));
        var material = Material(_walls, "HouseWall");
        var wall = colors.Main;
        Color? beams = null;
        if (material is "Woodframe" or "Standard" && colors.Second != colors.Main)
        {
            // Timber frame: the lighter colour is the plaster, the darker one the beams.
            var mainIsLighter = Light(colors.Main) >= Light(colors.Second);
            wall = mainIsLighter ? colors.Main : colors.Second;
            beams = mainIsLighter ? colors.Second : colors.Main;
        }
        var line = new Pen(new SolidColorBrush(Shade(wall, 0.45)), 2);
        using (context.PushClip(body))
            DrawSiding(context, body, wall, material, beams);
        context.DrawRectangle(null, line, body);

        // Chimney, then the roof over it
        var roofPicture = ExteriorPictures.Get(_roof);
        var roofColor = roofPicture is null ? Color.FromRgb(0x9A, 0x6B, 0x4F) : Color.FromRgb(0x80, 0x80, 0x80);
        var peak = new Point(body.Center.X, b.Height * 0.12);
        var eaveL = new Point(body.X - (bodyW * 0.1), body.Y + 6);
        var eaveR = new Point(body.Right + (bodyW * 0.1), body.Y + 6);
        var chimney = new Rect(body.Center.X + (bodyW * 0.22), peak.Y + ((body.Y - peak.Y) * 0.25), bodyW * 0.09, (body.Y - peak.Y) * 0.55);
        context.DrawRectangle(new SolidColorBrush(Shade(wall, 0.8)), line, chimney);

        var roof = new StreamGeometry();
        using (var g = roof.Open())
        {
            g.BeginFigure(eaveL, true);
            g.LineTo(peak);
            g.LineTo(eaveR);
            g.EndFigure(true);
        }
        IBrush roofFill = roofPicture is null
            ? new SolidColorBrush(roofColor)
            : new ImageBrush(roofPicture)
            {
                SourceRect = RoofTiles,
                DestinationRect = new RelativeRect(0, 0, 34, 34, RelativeUnit.Absolute),
                TileMode = TileMode.Tile,
                Stretch = Stretch.Fill,
            };
        context.DrawGeometry(roofFill, new Pen(new SolidColorBrush(Color.FromArgb(0xC0, 0x30, 0x20, 0x18)), 2.5), roof);
        // Shade under the eaves
        context.FillRectangle(new SolidColorBrush(Color.FromArgb(0x38, 0, 0, 0)), new Rect(body.X, body.Y + 6, body.Width, 6));

        // Windows
        var winSize = bodyH * 0.32;
        foreach (var cx in new[] { body.X + (bodyW * 0.2), body.Right - (bodyW * 0.2) })
        {
            var win = new Rect(cx - (winSize / 2), body.Y + (bodyH * 0.24), winSize, winSize);
            context.DrawRectangle(new SolidColorBrush(Color.FromRgb(0xF7, 0xF3, 0xE8)), new Pen(new SolidColorBrush(Shade(wall, 0.45)), 1.5), win.Inflate(3), 3, 3);
            context.FillRectangle(new LinearGradientBrush
            {
                StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
                EndPoint = new RelativePoint(1, 1, RelativeUnit.Relative),
                GradientStops = { new GradientStop(Color.FromRgb(0xB9, 0xE3, 0xF7), 0), new GradientStop(Color.FromRgb(0x7F, 0xBF, 0xE0), 1) },
            }, win);
            var frame = new Pen(new SolidColorBrush(Color.FromRgb(0xF7, 0xF3, 0xE8)), 2);
            context.DrawLine(frame, new Point(win.Center.X, win.Y), new Point(win.Center.X, win.Bottom));
            context.DrawLine(frame, new Point(win.X, win.Center.Y), new Point(win.Right, win.Center.Y));
        }

        // Door: its own picture, standing on the ground
        if (ExteriorPictures.Get(_door) is { } door)
        {
            var size = bodyH * 0.78;
            using (context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality }))
                context.DrawImage(door, new Rect(door.Size), new Rect(body.Center.X - (size / 2), ground - size + (size * 0.02), size, size));
        }
        else
        {
            var d = new Rect(body.Center.X - (bodyW * 0.09), ground - (bodyH * 0.55), bodyW * 0.18, bodyH * 0.55);
            context.DrawRectangle(new SolidColorBrush(Color.FromRgb(0x9A, 0x6B, 0x4F)), line, d, 4, 4);
        }
    }

    private static readonly Dictionary<Bitmap, Rect> DoorBounds = [];

    /// <summary> The part of a door picture that is the door (its pictures have empty space around). </summary>
    private static Rect DoorPart(Bitmap door)
    {
        if (DoorBounds.TryGetValue(door, out var known))
            return known;
        var (w, h) = (door.PixelSize.Width, door.PixelSize.Height);
        using var copy = ImageUtil.Create(w, h);
        using (var fb = copy.Lock())
            door.CopyPixels(fb);
        var px = copy.GetPixels();
        int x0 = w, y0 = h, x1 = -1, y1 = -1;
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                if ((uint)px[(y * w) + x] >> 24 < 40)
                    continue;
                (x0, y0, x1, y1) = (Math.Min(x0, x), Math.Min(y0, y), Math.Max(x1, x), Math.Max(y1, y));
            }
        }
        var part = x1 < 0 ? new Rect(0, 0, w, h) : new Rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
        DoorBounds[door] = part;
        return part;
    }

    /// <summary>
    /// The house front made by <see cref="HouseComposer"/>, standing on the grass, with the chosen door in its doorway.
    /// </summary>
    private void DrawFront(DrawingContext context, HouseComposer.Front front, Rect b, double ground)
    {
        var pic = front.Picture;
        var (w, h) = (pic.PixelSize.Width, pic.PixelSize.Height);
        var scale = Math.Min(b.Width * 0.8 / w, (ground - (b.Height * 0.06)) / h);
        var dest = new Rect(b.Center.X - (w * scale / 2), ground + 3 - (h * scale), w * scale, h * scale);
        context.DrawEllipse(new SolidColorBrush(Color.FromArgb(0x30, 0, 0, 0)), null, new Point(dest.Center.X, ground + 3), dest.Width * 0.55, 7);
        using (context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality }))
        {
            context.DrawImage(pic, new Rect(0, 0, w, h), dest);
            if (front.Door is { } doorway && ExteriorPictures.Get(_door) is { } door)
            {
                // The door fills the doorway from a little above what is always lit down to the ground.
                var part = DoorPart(door);
                var top = dest.Y + ((doorway.Y - (front.Exact ? 0 : doorway.Height * 0.25)) * scale);
                var opening = new Rect(dest.X + (doorway.X * scale), top, doorway.Width * scale, dest.Bottom - top);
                var dh = opening.Height;
                var dw = part.Width * dh / part.Height;
                if (dw > opening.Width * 0.9)
                    (dw, dh) = (opening.Width * 0.9, part.Height * opening.Width * 0.9 / part.Width);
                context.DrawImage(door, part, new Rect(opening.Center.X - (dw / 2), dest.Bottom - dh, dw, dh));
            }
        }
    }

    /// <summary> Material of a look from its internal name: "HouseWallPB04BrickF" → "Brick". </summary>
    internal static string Material(string name, string prefix)
    {
        if (!name.StartsWith(prefix, StringComparison.Ordinal))
            return string.Empty;
        var rest = name[prefix.Length..];
        // Family: P + two letters and two digits (players), or N + one letter (villagers).
        if (rest.Length > 4 && rest[0] == 'P' && char.IsDigit(rest[2]))
            rest = rest[4..];
        else if (rest.Length > 2 && rest[0] == 'N')
            rest = rest[2..];
        // Colour letter at the end.
        if (rest.Length > 1 && char.IsUpper(rest[^1]) && char.IsLower(rest[^2]))
            rest = rest[..^1];
        return rest;
    }

    private static int Light(Color c) => c.R + c.G + c.B;

    private static Color Shade(Color c, double f) => Color.FromRgb((byte)Math.Clamp(c.R * f, 0, 255), (byte)Math.Clamp(c.G * f, 0, 255), (byte)Math.Clamp(c.B * f, 0, 255));

    /// <summary>
    /// The walls in their material: brick courses, logs, planks, stones, timber frame or plain plaster.
    /// </summary>
    private static void DrawSiding(DrawingContext context, Rect r, Color c, string material, Color? beams)
    {
        context.FillRectangle(new SolidColorBrush(c), r);
        var dark = new Pen(new SolidColorBrush(Shade(c, 0.72)), 1.2);
        var light = new SolidColorBrush(Shade(c, 1.12));
        switch (material)
        {
            case "Brick":
            {
                const double h = 9, w = 20;
                var mortar = new Pen(new SolidColorBrush(Shade(c, 1.35)), 1.6);
                int row = 0;
                for (var y = r.Y; y < r.Bottom; y += h, row++)
                {
                    context.DrawLine(mortar, new Point(r.X, y), new Point(r.Right, y));
                    for (var x = r.X + ((row & 1) * w / 2); x < r.Right; x += w)
                        context.DrawLine(mortar, new Point(x, y), new Point(x, y + h));
                }
                break;
            }
            case "Log":
            {
                const double h = 13;
                for (var y = r.Y; y < r.Bottom; y += h)
                {
                    context.FillRectangle(light, new Rect(r.X, y + 2, r.Width, h * 0.35));
                    context.DrawLine(dark, new Point(r.X, y + h), new Point(r.Right, y + h));
                    context.DrawEllipse(new SolidColorBrush(Shade(c, 1.2)), dark, new Point(r.X + 4, y + (h / 2)), 5, h / 2);
                    context.DrawEllipse(new SolidColorBrush(Shade(c, 1.2)), dark, new Point(r.Right - 4, y + (h / 2)), 5, h / 2);
                }
                break;
            }
            case "Woodsidng" or "Woodsiding" or "Wood" or "Woodpanel":
            {
                for (var y = r.Y + 8; y < r.Bottom; y += 8)
                    context.DrawLine(dark, new Point(r.X, y), new Point(r.Right, y));
                break;
            }
            case "Stone":
            {
                const double h = 12, w = 18;
                int row = 0;
                for (var y = r.Y; y < r.Bottom; y += h, row++)
                {
                    for (var x = r.X - ((row % 3) * 6); x < r.Right; x += w)
                        context.DrawRectangle(new SolidColorBrush(Shade(c, 1.0 + (((int)(x + y) % 3) * 0.05))), dark, new Rect(x + 1, y + 1, w - 2, h - 2), 4, 4);
                }
                break;
            }
            case "Japanese" or "Oriental":
            {
                // Plaster above, dark wooden boards below, posts at the corners.
                var wood = new SolidColorBrush(Shade(c, 0.55));
                context.FillRectangle(wood, new Rect(r.X, r.Y + (r.Height * 0.55), r.Width, r.Height * 0.45));
                for (var x = r.X + 7; x < r.Right; x += 7)
                    context.DrawLine(new Pen(new SolidColorBrush(Shade(c, 0.4)), 1), new Point(x, r.Y + (r.Height * 0.55)), new Point(x, r.Bottom));
                context.FillRectangle(wood, new Rect(r.X, r.Y, 6, r.Height));
                context.FillRectangle(wood, new Rect(r.Right - 6, r.Y, 6, r.Height));
                break;
            }
            case "Woodframe" or "Standard":
            {
                // Plaster with timber beams.
                var beam = new Pen(new SolidColorBrush(beams ?? Shade(c, 0.5)), 4);
                context.DrawLine(beam, new Point(r.X, r.Y + (r.Height * 0.5)), new Point(r.Right, r.Y + (r.Height * 0.5)));
                foreach (var f in new[] { 0.02, 0.36, 0.64, 0.98 })
                    context.DrawLine(beam, new Point(r.X + (r.Width * f), r.Y), new Point(r.X + (r.Width * f), r.Bottom));
                context.DrawLine(beam, new Point(r.X + (r.Width * 0.02), r.Y + (r.Height * 0.5)), new Point(r.X + (r.Width * 0.36), r.Y));
                context.DrawLine(beam, new Point(r.X + (r.Width * 0.98), r.Y + (r.Height * 0.5)), new Point(r.X + (r.Width * 0.64), r.Y));
                break;
            }
            default:
            {
                // Plaster (stucco, adobe…): a few soft marks.
                var mark = new SolidColorBrush(Shade(c, 0.94));
                for (int i = 0; i < 18; i++)
                {
                    var x = r.X + ((i * 37 % 100) / 100.0 * r.Width);
                    var y = r.Y + ((i * 53 % 100) / 100.0 * r.Height);
                    context.DrawEllipse(mark, null, new Point(x, y), 5, 3);
                }
                break;
            }
        }
    }
}
