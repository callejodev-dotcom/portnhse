using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Paints the island's ground as an illustrated map, like the one in the game's phone: sea getting deeper away from the shore,
/// sand with a wet edge, grass in a lighter green on each cliff level with the cliff face and its shadow, rivers and ponds,
/// waterfalls, rocks and paths in the colour of their kind. One bitmap for the whole map, 16 pixels per square.
/// </summary>
public static class IslandPainter
{
    /// <summary> Pixels per square (terrain tile) of the painted bitmap. </summary>
    public const int PixelsPerSquare = 16;

    /// <summary> Squares across and down the whole map, sea included. </summary>
    public const int SquaresWide = 144;
    public const int SquaresHigh = 128;

    private const int W = SquaresWide * PixelsPerSquare;
    private const int H = SquaresHigh * PixelsPerSquare;

    /// <summary>
    /// What the ground is at each pixel.
    /// </summary>
    private enum K : byte
    {
        Sea, Sand, Grass, Water, Fall, FallSide, Face, Rock, Stone, Wood, Building, Tide, Ice, Sandstone,
    }

    /// <summary>
    /// Kinds of path, as the game's custom designs: 0 is none.
    /// </summary>
    private enum PathKind : byte
    {
        None, Brick, DarkSoil, Soil, Stone, Arched, TerraCotta, Sand, Wood,
    }

    // Palette (RGB)
    private static readonly Rgb SeaShallow = new(132, 216, 214);
    private static readonly Rgb SeaMid = new(96, 197, 213);
    private static readonly Rgb SeaDeep = new(70, 170, 204);
    private static readonly Rgb Foam = new(236, 250, 244);
    private static readonly Rgb SandColor = new(244, 230, 176);
    private static readonly Rgb WetSand = new(228, 208, 148);
    private static readonly Rgb[] GrassLevels = [new(112, 178, 82), new(132, 194, 96), new(153, 207, 113), new(172, 218, 130), new(188, 226, 146)];
    private static readonly Rgb FaceDark = new(86, 112, 62);
    private static readonly Rgb WaterBank = new(132, 210, 238);
    private static readonly Rgb WaterDeep = new(80, 172, 226);
    private static readonly Rgb FallColor = new(196, 238, 250);
    private static readonly Rgb RockColor = new(164, 168, 174);
    private static readonly Rgb StoneColor = new(200, 196, 186);
    private static readonly Rgb WoodColor = new(198, 158, 108);
    private static readonly Rgb BuildingColor = new(226, 214, 190);
    private static readonly Rgb TideColor = new(118, 204, 222);
    private static readonly Rgb IceColor = new(222, 238, 248);
    private static readonly Rgb SandstoneColor = new(206, 182, 128);

    private static readonly Rgb[] PathColors =
    [
        default,
        new(196, 106, 72), // brick
        new(142, 108, 76), // dark soil
        new(208, 176, 126), // soil
        new(186, 183, 172), // stone
        new(216, 207, 188), // arched tile
        new(214, 140, 94), // terra-cotta
        new(238, 220, 170), // sand
        new(186, 138, 94), // wood
    ];

    private static readonly int RiverArgb = System.Drawing.Color.FromArgb(128, 215, 195).ToArgb();

    /// <summary>
    /// Map colour of a path kind, for legends.
    /// </summary>
    public static Avalonia.Media.Color GetPathColor(TerrainUnitModel road)
    {
        var c = PathColors[(int)GetPathKind(road)];
        return Avalonia.Media.Color.FromRgb(c.R, c.G, c.B);
    }

    /// <summary> Colour of the open sea, to fill around the map. </summary>
    public static Avalonia.Media.Color SeaColor => Avalonia.Media.Color.FromRgb(SeaDeep.R, SeaDeep.G, SeaDeep.B);

    /// <summary>
    /// Paints the ground of the whole map.
    /// </summary>
    public static WriteableBitmap Paint(MapTileManager map)
    {
        var pixels = PaintPixels(map);
        return ImageUtil.GetBitmap(pixels, W, H);
    }

    /// <summary>
    /// Paints the ground of the whole map as 32bpp ARGB pixels (<see cref="SquaresWide"/> × 16 wide).
    /// </summary>
    public static int[] PaintPixels(MapTileManager map)
    {
        var kind = new byte[W * H];
        var elev = new byte[W * H];
        var path = new byte[W * H];
        Classify(map.LayerTerrain, kind, elev, path);

        var dSea = Distance(kind, k => k != (byte)K.Sea && k != (byte)K.Tide); // in the sea: how far is the shore
        var dShore = Distance(kind, k => k == (byte)K.Sea); // on land: how far is the sea
        var dWater = Distance(kind, k => k != (byte)K.Water); // in rivers: how far is the bank
        var shade = CliffShadows(kind, elev);

        var result = new int[W * H];
        for (int y = 0; y < H; y++)
        {
            for (int x = 0; x < W; x++)
            {
                var i = (y * W) + x;
                var k = (K)kind[i];
                var e = elev[i];
                Rgb c;
                var p = (PathKind)path[i];
                if (p != PathKind.None)
                {
                    c = PathPixel(p, x, y);
                    if (Differs(path, i, x, y))
                        c = c.Mul(0.84);
                }
                else
                {
                    c = k switch
                    {
                        K.Sea => SeaPixel(dSea[i], x, y),
                        K.Sand => SandPixel(dShore[i], x, y),
                        K.Grass => Grass(e).Mul(1 + (Noise(x, y) * 0.035)),
                        K.Water => WaterPixel(dWater[i], x, y),
                        K.Fall => FallPixel(x, y, true),
                        K.FallSide => FallPixel(x, y, false),
                        K.Face => Face(e).Mul(1 + (Noise(x, y) * 0.04)),
                        K.Rock => RockColor.Mul(1 + (Noise(x, y) * 0.05)),
                        K.Stone => StoneColor.Mul(1 + (Noise(x, y) * 0.04)),
                        K.Wood => WoodColor.Mul((y & 3) == 0 ? 0.88 : 1),
                        K.Building => BuildingColor,
                        K.Tide => TideColor,
                        K.Ice => IceColor,
                        _ => SandstoneColor.Mul(1 + (Noise(x, y) * 0.05)),
                    };
                    c = Outline(c, k, e, kind, elev, i, x, y);
                }
                if (shade[i] != 0)
                    c = c.Mul(1 - (shade[i] / 255d * 0.22));
                result[i] = c.ToArgb();
            }
        }
        return result;
    }

    #region Classification
    private const int CellsWide = SquaresWide * 4;
    private const int CellsHigh = SquaresHigh * 4;
    private const byte DynamicCell = 255;

    private static void Classify(LayerTerrain t, byte[] kind, byte[] elev, byte[] path)
    {
        const int P = PixelsPerSquare;
        var cells = GetFixedCells(t);
        var scratch = new TerrainTile();

        for (int sy = 0; sy < SquaresHigh; sy++)
        {
            for (int sx = 0; sx < SquaresWide; sx++)
            {
                // The terrain grid starts one area (16 squares) in from the edge of the map.
                int relX = sx - 16, relY = sy - 16;
                var tile = t.Contains(relX, relY) ? t.GetTile(relX, relY) : null;
                bool vertical = tile is not null && tile.UnitModel.IsFall && IsFallVertical(t, relX, relY);

                for (int py = 0; py < P; py++)
                {
                    var y = (sy * P) + py;
                    var row = y * W;
                    for (int px = 0; px < P; px++)
                    {
                        var x = (sx * P) + px;
                        var i = row + x;
                        var cell = cells[((y >> 2) * CellsWide) + (x >> 2)];
                        if (cell == DynamicCell && tile is not null)
                        {
                            var (k, e) = ClassifyTile(tile, px, py, scratch, vertical);
                            kind[i] = (byte)k;
                            elev[i] = e;
                        }
                        else
                        {
                            kind[i] = SmoothCell(cells, x, y);
                        }
                    }
                }

                if (tile is not null && tile.UnitModelRoad.IsRoad)
                    PaintPath(t, relX, relY, sx, sy, path);
            }
        }
    }

    /// <summary>
    /// The fixed edges of the island (sea, beaches, rocks) come from the game's area templates, in 4×4 cells per square;
    /// <see cref="DynamicCell"/> where the terrain tiles decide.
    /// </summary>
    private static byte[] GetFixedCells(LayerTerrain t)
    {
        var acres = AcreTileColor.AcreTiles;
        var cells = new byte[CellsWide * CellsHigh];
        for (int sy = 0; sy < SquaresHigh; sy++)
        {
            for (int sx = 0; sx < SquaresWide; sx++)
            {
                int relX = sx - 16, relY = sy - 16;
                bool inside = t.Contains(relX, relY);
                var acre = t.GetAcreTemplate(relX, relY);
                bool fixedLook = acre != 0 && ((acre + 1) * 4096) <= acres.Length;
                int ax = (sx % 16) * 4, ay = (sy % 16) * 4;
                for (int cy = 0; cy < 4; cy++)
                {
                    for (int cx = 0; cx < 4; cx++)
                    {
                        byte col = fixedLook ? acres[(acre * 4096) + ((ay + cy) * 64) + ax + cx] : (byte)(inside ? 7 : 5);
                        var value = col == 7 && inside ? DynamicCell : (byte)FromCollision(col);
                        cells[(((sy * 4) + cy) * CellsWide) + (sx * 4) + cx] = value;
                    }
                }
            }
        }
        return cells;
    }

    /// <summary>
    /// Kind of a pixel in the fixed cells, blended from the four nearest cells so the coast is round instead of stepped.
    /// </summary>
    private static byte SmoothCell(byte[] cells, int x, int y)
    {
        var own = cells[((y >> 2) * CellsWide) + (x >> 2)];
        double fx = ((x + 0.5) / 4) - 0.5, fy = ((y + 0.5) / 4) - 0.5;
        int x0 = (int)Math.Floor(fx), y0 = (int)Math.Floor(fy);
        double tx = fx - x0, ty = fy - y0;
        x0 = Math.Clamp(x0, 0, CellsWide - 2);
        y0 = Math.Clamp(y0, 0, CellsHigh - 2);
        var a = cells[(y0 * CellsWide) + x0];
        var b = cells[(y0 * CellsWide) + x0 + 1];
        var c = cells[((y0 + 1) * CellsWide) + x0];
        var d = cells[((y0 + 1) * CellsWide) + x0 + 1];
        if (a == b && b == c && c == d)
            return a == DynamicCell ? own : a;
        if (a == DynamicCell || b == DynamicCell || c == DynamicCell || d == DynamicCell)
            return own;

        // Weight of each kind among the four cells; the heaviest wins.
        double wa = (1 - tx) * (1 - ty), wb = tx * (1 - ty), wc = (1 - tx) * ty, wd = tx * ty;
        byte best = own;
        double bestWeight = -1;
        Span<byte> kinds = [a, b, c, d];
        foreach (var k in kinds)
        {
            var weight = (a == k ? wa : 0) + (b == k ? wb : 0) + (c == k ? wc : 0) + (d == k ? wd : 0);
            if (weight > bestWeight + 1e-9)
                (best, bestWeight) = (k, weight);
        }
        return best;
    }

    private static bool IsFallVertical(LayerTerrain t, int x, int y)
    {
        bool Wet(int dx, int dy) => t.Contains(x + dx, y + dy) && t.GetTile(x + dx, y + dy).UnitModel is var m && (m.IsRiver || m.IsFall);
        return Wet(0, -1) || Wet(0, 1) || !(Wet(-1, 0) || Wet(1, 0));
    }

    private static K FromCollision(byte col) => col switch
    {
        0 or 44 or 51 => K.Grass,
        1 or 12 or 15 => K.Water,
        3 or 45 => K.Stone,
        41 or 42 or 70 => K.Rock,
        4 or 29 => K.Sand,
        5 or 46 or 7 => K.Sea,
        6 or 22 or 49 => K.Wood,
        8 or 10 => K.Building,
        43 => K.Tide,
        47 => K.Sandstone,
        149 or 150 => K.Ice,
        _ => K.Grass,
    };

    private static (K Kind, byte Elevation) ClassifyTile(TerrainTile tile, int px, int py, TerrainTile scratch, bool verticalFall)
    {
        var m = tile.UnitModel;
        var e = (byte)Math.Min(tile.Elevation, (ushort)(GrassLevels.Length - 1));
        if (m.IsRiver)
            return (IsWaterPixel(m, tile.LandMakingAngle, px, py, scratch) ? K.Water : K.Grass, e);
        if (m.IsFall)
            return (verticalFall ? K.Fall : K.FallSide, e);
        if (m.IsCliff)
        {
            // Cliff pieces have the shapes of the river pieces: the "water" part is the top of the cliff, the rest is its face.
            var river = CliffAsRiver(m);
            return (IsWaterPixel(river, tile.LandMakingAngle, px, py, scratch) ? K.Grass : K.Face, e);
        }
        return (K.Grass, e);
    }

    private static bool IsWaterPixel(TerrainUnitModel model, ushort angle, int px, int py, TerrainTile scratch)
    {
        scratch.UnitModel = model;
        scratch.LandMakingAngle = angle;
        scratch.Elevation = 0;
        scratch.UnitModelRoad = 0;
        return TerrainTileColor.GetTileColor(scratch, px, py).ToArgb() == RiverArgb;
    }

    private static readonly Dictionary<TerrainUnitModel, TerrainUnitModel> CliffRiver = BuildCliffRiver();

    private static Dictionary<TerrainUnitModel, TerrainUnitModel> BuildCliffRiver()
    {
        var result = new Dictionary<TerrainUnitModel, TerrainUnitModel>();
        foreach (var m in Enum.GetValues<TerrainUnitModel>())
        {
            if (!m.IsCliff)
                continue;
            var name = m.ToString().Replace("Cliff", "River", StringComparison.Ordinal);
            var cut = name.IndexOf('_');
            if (cut > 0)
                name = name[..cut];
            if (name == "River8")
                name = "River8A";
            result[m] = Enum.TryParse<TerrainUnitModel>(name, out var r) ? r : TerrainUnitModel.River5B;
        }
        return result;
    }

    private static TerrainUnitModel CliffAsRiver(TerrainUnitModel m) => CliffRiver.GetValueOrDefault(m, TerrainUnitModel.River5B);

    private static PathKind GetPathKind(TerrainUnitModel m)
    {
        if (m.IsRoadBrick) return PathKind.Brick;
        if (m.IsRoadDarkSoil) return PathKind.DarkSoil;
        if (m.IsRoadSoil) return PathKind.Soil;
        if (m.IsRoadStone) return PathKind.Stone;
        if (m.IsRoadPattern) return PathKind.Arched;
        if (m.IsRoadTile) return PathKind.TerraCotta;
        if (m.IsRoadSand) return PathKind.Sand;
        return PathKind.Wood;
    }

    /// <summary>
    /// A path square, with its outer corners rounded where it doesn't continue.
    /// </summary>
    private static void PaintPath(LayerTerrain t, int relX, int relY, int sx, int sy, byte[] path)
    {
        const int P = PixelsPerSquare;
        const double r = 5.5;
        var type = (byte)GetPathKind(t.GetTile(relX, relY).UnitModelRoad);
        bool IsRoad(int dx, int dy)
        {
            var x = relX + dx;
            var y = relY + dy;
            return t.Contains(x, y) && t.GetTile(x, y).UnitModelRoad.IsRoad;
        }
        bool n = IsRoad(0, -1), s = IsRoad(0, 1), w = IsRoad(-1, 0), e = IsRoad(1, 0);

        for (int py = 0; py < P; py++)
        {
            for (int px = 0; px < P; px++)
            {
                double fx = px + 0.5, fy = py + 0.5;
                bool cut = (!n && !w && Outside(fx, fy, r, r))
                        || (!n && !e && Outside(fx, fy, P - r, r))
                        || (!s && !w && Outside(fx, fy, r, P - r))
                        || (!s && !e && Outside(fx, fy, P - r, P - r));
                if (!cut)
                    path[(((sy * P) + py) * W) + (sx * P) + px] = type;
            }
        }

        static bool Outside(double x, double y, double cx, double cy)
        {
            // Only the corner box beyond the circle's centre is rounded.
            bool inCorner = (cx < 8 ? x < cx : x > cx) && (cy < 8 ? y < cy : y > cy);
            return inCorner && (((x - cx) * (x - cx)) + ((y - cy) * (y - cy)) > r * r);
        }
    }
    #endregion

    #region Shading
    private static Rgb Grass(byte e) => GrassLevels[Math.Min(e, (byte)(GrassLevels.Length - 1))];

    private static Rgb Face(byte e) => Rgb.Lerp(Grass(e), FaceDark, 0.62);

    private static Rgb SeaPixel(byte d, int x, int y)
    {
        if (d <= 1)
            return Foam;
        var t = Math.Clamp((d - 2) / 70d, 0, 1);
        var c = t < 0.35 ? Rgb.Lerp(SeaShallow, SeaMid, t / 0.35) : Rgb.Lerp(SeaMid, SeaDeep, (t - 0.35) / 0.65);
        if (d <= 4)
            c = Rgb.Lerp(c, Foam, (5 - d) * 0.12);
        return c.Mul(1 + (Noise(x, y) * 0.012));
    }

    private static Rgb SandPixel(byte d, int x, int y)
    {
        var c = d <= 6 ? Rgb.Lerp(WetSand, SandColor, d / 6d) : SandColor;
        return c.Mul(1 + (Noise(x, y) * 0.03));
    }

    private static Rgb WaterPixel(byte d, int x, int y)
    {
        var c = Rgb.Lerp(WaterBank, WaterDeep, Math.Clamp((d - 1) / 9d, 0, 1));
        return c.Mul(1 + (Noise(x, y) * 0.012));
    }

    private static Rgb FallPixel(int x, int y, bool vertical)
    {
        // Streaks of foam along the fall.
        var (along, across) = vertical ? (y, x) : (x, y);
        var streak = ((across + (Hash(across >> 2, along >> 3) & 1)) % 4) == 0;
        var c = Rgb.Lerp(WaterBank, FallColor, 0.7);
        return streak ? Rgb.Lerp(c, new Rgb(255, 255, 255), 0.75) : c.Mul(1 + (Noise(x, y) * 0.03));
    }

    private static Rgb PathPixel(PathKind p, int x, int y)
    {
        var c = PathColors[(int)p];
        switch (p)
        {
            case PathKind.Brick:
            {
                // Rows of bricks, every other row shifted half a brick.
                var row = y / 4;
                var col = (x + ((row & 1) * 4)) / 8;
                bool mortar = (y % 4) == 0 || ((x + ((row & 1) * 4)) % 8) == 0;
                return mortar ? c.Mul(1.12) : c.Mul(1 + ((Hash(col, row) & 7) - 3.5) * 0.012);
            }
            case PathKind.Stone:
            {
                var cell = Hash(x / 5, y / 5);
                bool gap = (x % 5) == 0 || (y % 5) == 0;
                return gap ? c.Mul(0.9) : c.Mul(1 + (((cell & 7) - 3.5) * 0.015));
            }
            case PathKind.TerraCotta:
                return (x % 8) == 0 || (y % 8) == 0 ? c.Mul(1.1) : c;
            case PathKind.Arched:
            {
                var dx = (x % 8) - 4;
                var dy = (y % 8);
                return (dx * dx) + (dy * dy) is >= 14 and <= 22 ? c.Mul(0.9) : c;
            }
            case PathKind.Wood:
                return (y % 4) == 0 ? c.Mul(0.86) : c.Mul(1 + (((Hash(x / 6, y / 4) & 7) - 3.5) * 0.012));
            default:
                return c.Mul(1 + (Noise(x, y) * 0.04));
        }
    }

    /// <summary>
    /// Thin lines where the ground changes: river banks, rocks, the top lip and the foot of cliffs, the edge of the grass.
    /// </summary>
    private static Rgb Outline(Rgb c, K k, byte e, byte[] kind, byte[] elev, int i, int x, int y)
    {
        if (k is K.Sea or K.Building or K.Tide or K.Ice)
            return c;
        Span<int> around = stackalloc int[4];
        int n = 0;
        if (x + 1 < W) around[n++] = i + 1;
        if (x > 0) around[n++] = i - 1;
        if (y + 1 < H) around[n++] = i + W;
        if (y > 0) around[n++] = i - W;

        bool bank = false, rockEdge = false, stoneEdge = false, lip = false, grassEdge = false, foot = false, sandEdge = false;
        for (int a = 0; a < n; a++)
        {
            var nk = (K)kind[around[a]];
            var ne = elev[around[a]];
            bank |= nk is not K.Water and not K.Fall and not K.FallSide;
            rockEdge |= nk != K.Rock;
            stoneEdge |= nk is not K.Stone and not K.Rock;
            lip |= nk == K.Face && ne == e;
            grassEdge |= nk is K.Sand or K.Water;
            foot |= nk != K.Face && ne < e;
            sandEdge |= nk is K.Grass or K.Rock or K.Stone;
        }

        return k switch
        {
            K.Water when bank => c.Mul(0.8),
            K.Rock when rockEdge => c.Mul(0.72),
            K.Stone when stoneEdge => c.Mul(0.86),
            K.Grass when lip => c.Mul(1.1), // top lip of a cliff
            K.Grass when grassEdge => c.Mul(0.9),
            K.Face when foot => c.Mul(0.78), // foot of the cliff
            K.Sand when sandEdge => c.Mul(0.94),
            _ => c,
        };
    }

    /// <summary>
    /// Soft shadow that cliffs cast to the south and east on the lower ground.
    /// </summary>
    private static byte[] CliffShadows(byte[] kind, byte[] elev)
    {
        var shade = new byte[W * H];
        const int reachDown = 7;
        const int reachRight = 3;
        for (int y = 0; y < H; y++)
        {
            for (int x = 0; x < W; x++)
            {
                var i = (y * W) + x;
                if (kind[i] != (byte)K.Face)
                    continue;
                var e = elev[i];
                for (int k = 1; k <= reachDown && y + k < H; k++)
                {
                    var j = i + (k * W);
                    if (kind[j] == (byte)K.Face)
                        break;
                    if (elev[j] >= e)
                        break;
                    var v = (byte)(255 * (reachDown + 1 - k) / (reachDown + 1));
                    if (shade[j] < v)
                        shade[j] = v;
                }
                for (int k = 1; k <= reachRight && x + k < W; k++)
                {
                    var j = i + k;
                    if (kind[j] == (byte)K.Face || elev[j] >= e)
                        break;
                    var v = (byte)(160 * (reachRight + 1 - k) / (reachRight + 1));
                    if (shade[j] < v)
                        shade[j] = v;
                }
            }
        }
        return shade;
    }

    private static bool Differs(byte[] path, int i, int x, int y)
    {
        var p = path[i];
        return (x + 1 < W && path[i + 1] != p) || (x > 0 && path[i - 1] != p) || (y + 1 < H && path[i + W] != p) || (y > 0 && path[i - W] != p);
    }

    /// <summary>
    /// Distance in pixels (up to 255) from each pixel to the nearest pixel whose kind passes <paramref name="isSource"/>.
    /// Two-pass chamfer (3-4) distance transform.
    /// </summary>
    private static byte[] Distance(byte[] kind, Func<byte, bool> isSource)
    {
        const int inf = 1 << 20;
        var d = new int[W * H];
        var lookup = new bool[256];
        for (int b = 0; b < 256; b++)
            lookup[b] = isSource((byte)b);
        for (int i = 0; i < d.Length; i++)
            d[i] = lookup[kind[i]] ? 0 : inf;

        for (int y = 0; y < H; y++)
        {
            for (int x = 0; x < W; x++)
            {
                var i = (y * W) + x;
                var v = d[i];
                if (v == 0)
                    continue;
                if (x > 0) v = Math.Min(v, d[i - 1] + 3);
                if (y > 0)
                {
                    v = Math.Min(v, d[i - W] + 3);
                    if (x > 0) v = Math.Min(v, d[i - W - 1] + 4);
                    if (x + 1 < W) v = Math.Min(v, d[i - W + 1] + 4);
                }
                d[i] = v;
            }
        }
        for (int y = H - 1; y >= 0; y--)
        {
            for (int x = W - 1; x >= 0; x--)
            {
                var i = (y * W) + x;
                var v = d[i];
                if (v == 0)
                    continue;
                if (x + 1 < W) v = Math.Min(v, d[i + 1] + 3);
                if (y + 1 < H)
                {
                    v = Math.Min(v, d[i + W] + 3);
                    if (x + 1 < W) v = Math.Min(v, d[i + W + 1] + 4);
                    if (x > 0) v = Math.Min(v, d[i + W - 1] + 4);
                }
                d[i] = v;
            }
        }

        var result = new byte[W * H];
        for (int i = 0; i < d.Length; i++)
            result[i] = (byte)Math.Min(255, d[i] / 3);
        return result;
    }

    private static int Hash(int x, int y)
    {
        unchecked
        {
            var h = (x * 374761393) + (y * 668265263);
            h = (h ^ (h >> 13)) * 1274126177;
            return (h ^ (h >> 16)) & 0x7FFFFFFF;
        }
    }

    /// <summary>
    /// Soft value noise in -1..1 (12 px cells, smoothly blended, plus a little grain).
    /// </summary>
    private static double Noise(int x, int y)
    {
        const int cell = 12;
        int gx = x / cell, gy = y / cell;
        double fx = (x % cell) / (double)cell, fy = (y % cell) / (double)cell;
        fx = fx * fx * (3 - (2 * fx));
        fy = fy * fy * (3 - (2 * fy));
        double V(int a, int b) => ((Hash(a, b) & 1023) / 511.5) - 1;
        var top = V(gx, gy) + ((V(gx + 1, gy) - V(gx, gy)) * fx);
        var bottom = V(gx, gy + 1) + ((V(gx + 1, gy + 1) - V(gx, gy + 1)) * fx);
        var smooth = top + ((bottom - top) * fy);
        var grain = ((Hash(x >> 1, y >> 1) & 255) / 127.5) - 1;
        return (smooth * 0.8) + (grain * 0.2);
    }
    #endregion

    private readonly record struct Rgb(byte R, byte G, byte B)
    {
        public Rgb(double r, double g, double b) : this(Clamp(r), Clamp(g), Clamp(b)) { }

        private static byte Clamp(double v) => (byte)Math.Clamp(Math.Round(v), 0, 255);

        public Rgb Mul(double f) => new(R * f, G * f, B * f);

        public static Rgb Lerp(Rgb a, Rgb b, double t) => new(a.R + ((b.R - a.R) * t), a.G + ((b.G - a.G) * t), a.B + ((b.B - a.B) * t));

        public int ToArgb() => unchecked((int)0xFF000000) | (R << 16) | (G << 8) | B;
    }
}
