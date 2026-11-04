using System.IO.Compression;
using System.Text;
using NHSE.Core;

/// <summary>
/// mapa_terreno.hpp: lo que hace falta para pintar el suelo de la isla como IslandPainter del Mac: qué es cada pieza de
/// terreno (río, cascada, acantilado, camino), dónde hay agua dentro de cada pieza de río o acantilado (16×16 puntos por
/// giro), las plantillas fijas de los bordes de la isla (outside.bin de NHSE, comprimido) y los puentes y cuestas.
/// </summary>
public sealed class MapaTerreno : ITabla
{
    /// <summary> Los nombres del juego de exterior.tsv del Mac (en español), por nombre interno. </summary>
    public static readonly Dictionary<string, string> Exterior = LeerExterior();

    private static Dictionary<string, string> LeerExterior()
    {
        var r = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var l in File.ReadAllLines(Path.Combine(Comun.Raiz, "src/NHSE.Mac/Resources/exterior.tsv")))
        {
            if (l.Length == 0 || l[0] == '#')
                continue;
            var p = l.Split('\t');
            if (p.Length >= 4)
                r[p[0]] = p[3];
        }
        return r;
    }

    private static int Camino(TerrainUnitModel m)
    {
        if (!m.IsRoad) return 0;
        if (m.IsRoadBrick) return 1;
        if (m.IsRoadDarkSoil) return 2;
        if (m.IsRoadSoil) return 3;
        if (m.IsRoadStone) return 4;
        if (m.IsRoadPattern) return 5;
        if (m.IsRoadTile) return 6;
        if (m.IsRoadSand) return 7;
        return 8;
    }

    // Como IslandPainter.CliffAsRiver del Mac.
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

    private static readonly int RiverArgb = System.Drawing.Color.FromArgb(128, 215, 195).ToArgb();

    private static bool Agua(TerrainUnitModel model, ushort angle, int px, int py)
    {
        var t = new TerrainTile { UnitModel = model, LandMakingAngle = angle };
        return TerrainTileColor.GetTileColor(t, px, py).ToArgb() == RiverArgb;
    }

    /// <summary> Como IslandPainter.BridgeShape del Mac. </summary>
    private static (string Material, double Length, bool Diagonal) BridgeShape(string name)
    {
        var digits = name.Length;
        while (digits > 0 && char.IsDigit(name[digits - 1]))
            digits--;
        var number = int.TryParse(name.AsSpan(digits), out var n) ? n : 4;
        var diagonal = name.Contains("Diagonal", StringComparison.Ordinal);
        var material = name[..digits].Replace("Diagonal", string.Empty, StringComparison.Ordinal);
        var squares = diagonal ? number / 10.0 * Math.Sqrt(2) : number;
        return (material, (squares + 1) * 2, diagonal);
    }

    /// <summary> Como IslandMapWindow.CrossingColor del Mac. </summary>
    private static uint ColorCruce(string material) => material switch
    {
        "BridgeStone" or "SlopeStoneStair" => 0xA4A29C,
        "BridgeSuspension" => 0xD2B07E,
        "BridgeJapanese" => 0x9C6B45,
        "BridgeLog" or "SlopeWoodStair" => 0x8E5E37,
        "BridgeRed" => 0xC24538,
        "BridgeIron" or "SlopeIronStair" => 0x7E8A96,
        "SlopeIronStairBlue" or "SlopeWoodBlue" => 0x5B8FC9,
        "BridgeWood" or "SlopeWood" => 0xC99D6A,
        "BridgeBricks" or "SlopeBrickStair" => 0xB35A42,
        "SlopeNatural" => 0xB8A67E,
        _ => 0xB98A5A,
    };

    public string Generar()
    {
        var sb = new StringBuilder();
        var modelos = Enum.GetValues<TerrainUnitModel>();
        int max = modelos.Max(z => (int)z) + 1;
        // Clase de cada pieza: bits 0-1 (0 otra, 1 río, 2 cascada, 3 acantilado), 2-5 el camino (1 ladrillo, 2 tierra oscura,
        // 3 tierra, 4 piedra, 5 baldosa en arco, 6 terracota, 7 arena, 8 madera), y su máscara de agua (MASCARAS, o 255).
        var clase = new int[max];
        var mascara = Enumerable.Repeat(255, max).ToArray();
        var mascaras = new List<byte[]>();
        foreach (var m in modelos)
        {
            int c = m.IsRiver ? 1 : m.IsFall ? 2 : m.IsCliff ? 3 : 0;
            clase[(int)m] = c | (Camino(m) << 2);
            if (c is 1 or 3)
            {
                var forma = c == 1 ? m : CliffRiver.GetValueOrDefault(m, TerrainUnitModel.River5B);
                // Giros 0-3 y uno que no existe (4: lo que hacen los demás números).
                var bits = new byte[5 * 32];
                for (int a = 0; a < 5; a++)
                    for (int y = 0; y < 16; y++)
                        for (int x = 0; x < 16; x++)
                            if (Agua(forma, (ushort)a, x, y))
                                bits[(a * 32) + (y * 2) + (x >> 3)] |= (byte)(1 << (x & 7));
                mascara[(int)m] = mascaras.Count;
                mascaras.Add(bits);
            }
        }
        sb.AppendLine($"constexpr int TERRENO_MODELOS = {max};");
        sb.AppendLine("// Por pieza de terreno: bits 0-1 qué es (0 otra, 1 río, 2 cascada, 3 acantilado), 2-5 el camino (0 ninguno,");
        sb.AppendLine("// 1 ladrillo, 2 tierra oscura, 3 tierra, 4 piedra, 5 baldosa en arco, 6 terracota, 7 arena, 8 madera).");
        sb.AppendLine($"constexpr uint8_t TERRENO_CLASE[] = {{{Comun.Numeros(clase)}}};");
        sb.AppendLine("// Su forma (en MASCARAS), o 255: los puntos de agua (río) o de lo alto (acantilado), 16×16 por giro (0-3, y 4");
        sb.AppendLine("// para los demás números), un bit por punto.");
        sb.AppendLine($"constexpr uint8_t TERRENO_MASCARA[] = {{{Comun.Numeros(mascara)}}};");
        sb.AppendLine($"constexpr uint8_t MASCARAS[][160] = {{");
        foreach (var b in mascaras)
            sb.AppendLine($"    {{{Comun.Numeros(b)}}},");
        sb.AppendLine("};");

        // Plantillas de los bordes (outside.bin), comprimidas con zlib.
        var outside = AcreTileColor.AcreTiles;
        using var ms = new MemoryStream();
        using (var z = new ZLibStream(ms, CompressionLevel.SmallestSize, true))
            z.Write(outside);
        var comprimido = ms.ToArray();
        sb.AppendLine("// Las plantillas de los bordes de la isla (outside.bin de NHSE: 64×64 puntos por zona), comprimidas con zlib.");
        sb.AppendLine($"constexpr uint32_t BORDES_TAM = {outside.Length};");
        sb.AppendLine($"constexpr uint8_t BORDES_ZLIB[] = {{{Comun.Numeros(comprimido)}}};");

        // Puentes y cuestas.
        sb.AppendLine("// Los puentes por su modelo: largo en casillas de objeto, si va en diagonal, color y nombre.");
        sb.AppendLine("struct Cruce { uint16_t tipo; float largo; bool diagonal; uint32_t color; const char* nombre; };");
        sb.AppendLine("constexpr Cruce PUENTES[] = {");
        foreach (var t in Enum.GetValues<BridgeType>())
        {
            var (mat, largo, diag) = BridgeShape(t.ToString());
            var nombre = Exterior.GetValueOrDefault(mat, MapaObjetos.Textos["Bridge"]);
            sb.AppendLine($"    {{{(int)t}, {largo.ToString("0.0000", System.Globalization.CultureInfo.InvariantCulture)}f, {(diag ? "true" : "false")}, 0x{ColorCruce(mat):X6}, {Comun.C(nombre)}}},");
        }
        sb.AppendLine("};");
        sb.AppendLine("constexpr Cruce CUESTAS[] = {");
        foreach (var t in Enum.GetValues<SlopeType>())
        {
            var nombre = Exterior.GetValueOrDefault(t.ToString(), MapaObjetos.Textos["Incline"]);
            sb.AppendLine($"    {{{(int)t}, 8, false, 0x{ColorCruce(t.ToString()):X6}, {Comun.C(nombre)}}},");
        }
        sb.AppendLine("};");
        Comun.Hpp("mapa_terreno.hpp", "Para pintar el suelo de la isla como el Mac: piezas de terreno, bordes, puentes y cuestas.", sb.ToString());
        return $"{max} piezas ({mascaras.Count} con forma), bordes {comprimido.Length} bytes";
    }
}
