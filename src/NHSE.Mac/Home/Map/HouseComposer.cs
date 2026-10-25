using System.Collections.Concurrent;
using Avalonia;
using Avalonia.Media.Imaging;

namespace NHSE.Mac;

/// <summary>
/// The front of a house made from the game's own pictures: the picture of the walls (a render of the house), its roof
/// covered with the tiles of the chosen roof, and the chosen door in its doorway.
/// The walls of one family («PA04», «PB04»… the house models) are all drawn from the same angle, so the parts that are
/// roof (always light grey) and doorway (always lit) are found by comparing every picture of the family.
/// </summary>
public static class HouseComposer
{
    /// <summary> The house front, and where its door goes (pixels of the front). </summary>
    /// <param name="Exact">The door's place is the whole doorway (found by its light); otherwise it is what is always the
    /// same in the family's pictures, which leaves out the top of an arch.</param>
    public sealed record Front(WriteableBitmap Picture, Rect? Door, bool Exact);

    private sealed record Masks(int Width, int Height, bool[] Roof, Rect? Door, bool Exact = false);

    private static readonly ConcurrentDictionary<string, Masks?> FamilyMasks = new();
    private static readonly ConcurrentDictionary<(string Wall, string Roof), Front?> Fronts = new();

    /// <summary> Part of a roof picture covered with tiles in every roof. </summary>
    private static readonly Rect RoofTiles = new(0.24, 0.42, 0.16, 0.20);

    /// <summary> Forgets what was made (after more pictures are downloaded). </summary>
    public static void Clear()
    {
        FamilyMasks.Clear();
        Fronts.Clear();
    }

    /// <summary>
    /// The front of a house with these walls and roof, or null without the picture of the walls.
    /// </summary>
    public static Front? Compose(string wall, string roof)
    {
        if (ExteriorPictures.Get(wall) is not { } wallPicture)
            return null;
        return Fronts.GetOrAdd((wall, roof), _ =>
        {
            var (w, h) = (wallPicture.PixelSize.Width, wallPicture.PixelSize.Height);
            var pixels = Pixels(wallPicture);
            var masks = GetMasks(wall, w, h) ?? Single(pixels, w, h);
            if (masks.Roof.Length == pixels.Length && ExteriorPictures.Get(roof) is { } roofPicture)
                PaintRoof(pixels, masks.Roof, w, roofPicture);
            return new Front(ImageUtil.GetBitmap(pixels, w, h), masks.Door, masks.Exact);
        });
    }

    private static int[] Pixels(Bitmap bmp)
    {
        using var copy = ImageUtil.Create(bmp.PixelSize.Width, bmp.PixelSize.Height);
        using (var fb = copy.Lock())
            bmp.CopyPixels(fb);
        return copy.GetPixels();
    }

    private static (int R, int G, int B, int A) Split(int argb) => ((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);

    private static bool IsLit(int argb)
    {
        var (r, g, b, a) = Split(argb);
        return a > 200 && r > 140 && g > 95 && r - b > 45 && b < 170;
    }

    /// <summary> "HouseWallPB04BrickF" → "HouseWallPB04", "HouseWallNDStuccoD" → "HouseWallND". </summary>
    private static string Family(string wall)
    {
        if (wall.Length > 13 && wall[9] == 'P')
            return wall[..13];
        return wall.Length > 11 ? wall[..11] : wall;
    }

    /// <summary>
    /// Roof and doorway of a family: the parts that are the same in every picture of it (only the walls change).
    /// The doorway is the biggest of them that reaches the bottom; the roof, the big ones in the upper part.
    /// Null when there are too few pictures of the family to tell.
    /// </summary>
    private static Masks? GetMasks(string wall, int w, int h)
    {
        var family = Family(wall);
        var masks = FamilyMasks.GetOrAdd(family, _ =>
        {
            var members = Enum.GetNames<NHSE.Core.WallType>().Where(n => n.StartsWith(family, StringComparison.Ordinal))
                .Select(ExteriorPictures.Get).OfType<Bitmap>().Where(b => b.PixelSize.Width == w && b.PixelSize.Height == h)
                .Select(Pixels).ToList();
            if (members.Count < 4)
                return null;
            var same = new bool[w * h];
            for (int i = 0; i < same.Length; i++)
            {
                int minR = 255, minG = 255, minB = 255, maxR = 0, maxG = 0, maxB = 0;
                bool opaque = true;
                foreach (var px in members)
                {
                    var (r, g, b, a) = Split(px[i]);
                    if (a < 200)
                    {
                        opaque = false;
                        break;
                    }
                    (minR, minG, minB) = (Math.Min(minR, r), Math.Min(minG, g), Math.Min(minB, b));
                    (maxR, maxG, maxB) = (Math.Max(maxR, r), Math.Max(maxG, g), Math.Max(maxB, b));
                }
                same[i] = opaque && Math.Max(maxR - minR, Math.Max(maxG - minG, maxB - minB)) <= 14;
            }

            var parts = Groups(same, w, h);
            var door = parts.Where(z => z.Bounds.Bottom >= h * 0.9 && z.Bounds.Y > h * 0.3).MaxBy(z => z.Pixels.Count);
            // A line that differs between pictures can cut the doorway in two: join the parts right above it.
            var doorway = door?.Bounds;
            for (bool grew = doorway is not null; grew;)
            {
                grew = false;
                foreach (var part in parts)
                {
                    var b = part.Bounds;
                    if (part == door || doorway is not { } d || b.Y >= d.Y || b.Bottom < d.Y - 10 || b.Y < h * 0.3)
                        continue;
                    var overlap = Math.Min(b.Right, d.Right) - Math.Max(b.X, d.X);
                    if (overlap < d.Width * 0.7)
                        continue;
                    doorway = d.Union(b);
                    grew = true;
                }
            }
            var roof = new bool[w * h];
            foreach (var part in parts)
            {
                if (part == door || part.Pixels.Count < 150 || part.CenterY > h * 0.6 || (doorway is { } d && d.Contains(part.Bounds)))
                    continue;
                foreach (var i in part.Pixels)
                    roof[i] = true;
            }
            return new Masks(w, h, roof, doorway);
        });
        return masks is { } m && m.Width == w && m.Height == h ? m : null;
    }

    private sealed record Group(List<int> Pixels, Rect Bounds, double CenterY);

    /// <summary> Groups of touching pixels of a mask. </summary>
    private static List<Group> Groups(bool[] mask, int w, int h)
    {
        var result = new List<Group>();
        var seen = new bool[mask.Length];
        var queue = new Queue<int>();
        for (int start = 0; start < mask.Length; start++)
        {
            if (!mask[start] || seen[start])
                continue;
            var pixels = new List<int>();
            int x0 = w, y0 = h, x1 = 0, y1 = 0;
            long sumY = 0;
            seen[start] = true;
            queue.Enqueue(start);
            while (queue.Count != 0)
            {
                var i = queue.Dequeue();
                int x = i % w, y = i / w;
                pixels.Add(i);
                sumY += y;
                (x0, y0, x1, y1) = (Math.Min(x0, x), Math.Min(y0, y), Math.Max(x1, x), Math.Max(y1, y));
                foreach (var n in new[] { i - 1, i + 1, i - w, i + w })
                {
                    if (n < 0 || n >= mask.Length || seen[n] || !mask[n] || Math.Abs((n % w) - x) > 1)
                        continue;
                    seen[n] = true;
                    queue.Enqueue(n);
                }
            }
            result.Add(new Group(pixels, new Rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1), (double)sumY / pixels.Count));
        }
        return result;
    }

    /// <summary> A wall picture alone (the smaller houses): only its doorway, found by its light. </summary>
    private static Masks Single(int[] pixels, int w, int h)
    {
        var lit = new bool[pixels.Length];
        for (int y = h / 3; y < h; y++)
        {
            for (int x = 0; x < w; x++)
                lit[(y * w) + x] = IsLit(pixels[(y * w) + x]);
        }
        var door = Groups(lit, w, h).Where(z => z.Pixels.Count > 40).MaxBy(z => z.Pixels.Count);
        return new Masks(w, h, [], door?.Bounds, Exact: true);
    }

    /// <summary>
    /// Covers the roof with the tiles of the roof picture, keeping the light and shade of the house picture.
    /// </summary>
    private static void PaintRoof(int[] pixels, bool[] roof, int w, Bitmap roofPicture)
    {
        var (rw, rh) = (roofPicture.PixelSize.Width, roofPicture.PixelSize.Height);
        var roofPixels = Pixels(roofPicture);
        int tx = (int)(RoofTiles.X * rw), ty = (int)(RoofTiles.Y * rh), tw = (int)(RoofTiles.Width * rw), th = (int)(RoofTiles.Height * rh);

        double sum = 0;
        int n = 0;
        for (int i = 0; i < pixels.Length; i++)
        {
            if (!roof[i])
                continue;
            var (r, g, b, _) = Split(pixels[i]);
            sum += (r + g + b) / 3.0;
            n++;
        }
        if (n == 0)
            return;
        var mean = sum / n;

        for (int i = 0; i < pixels.Length; i++)
        {
            if (!roof[i])
                continue;
            var (r, g, b, a) = Split(pixels[i]);
            var shade = ((r + g + b) / 3.0) / mean;
            int x = i % w, y = i / w;
            var (tr, tg, tb, _) = Split(roofPixels[((ty + (y % th)) * rw) + tx + (x % tw)]);
            pixels[i] = (a << 24) | (Clamp(tr * shade) << 16) | (Clamp(tg * shade) << 8) | Clamp(tb * shade);
        }
    }

    private static int Clamp(double v) => (int)Math.Clamp(v, 0, 255);
}
