using System.Text;
using NHSE.Core;

/// <summary>
/// mapa_casas.hpp: los tejados, fachadas y puertas que se pueden elegir para la casa de un jugador, como HouseWindow
/// del Mac (los de la casa terminada, «04», con su nombre del juego), y el color del tejado de cada casa en el mapa.
/// </summary>
public sealed class MapaCasas : ITabla
{
    private static readonly Dictionary<string, string> Ext = MapaTerreno.Exterior;
    private static string T(string k) => MapaObjetos.Textos[k];

    /// <summary> Como ExteriorPictures.Resolve del Mac. </summary>
    private static string? Resolve(string name)
    {
        if (Ext.ContainsKey(name))
            return name;
        if (name.StartsWith("HouseDoor", StringComparison.Ordinal) && name.EndsWith('S') && Ext.ContainsKey(name[..^1] + "R"))
            return name[..^1] + "R";
        if (name.StartsWith("HouseRoofPA0", StringComparison.Ordinal) && name.Length > 12)
        {
            var finished = "HouseRoofPA04" + name[13..];
            if (Ext.ContainsKey(finished))
                return finished;
        }
        return null;
    }

    private static bool Has(string name) => Resolve(name) is not null;

    private static string? GetName(string name) => Resolve(name) is { } k && Ext[k].Length != 0 ? Ext[k] : null;

    /// <summary> Como HousePartPicker.NameOf del Mac. </summary>
    private static string NameOf<TE>(TE v) where TE : struct, Enum => GetName(v.ToString()) ?? StyleName(v);

    private static readonly string[] Prefixes = ["HouseRoof", "HouseWall", "HouseDoor"];
    private static readonly string[] Regions = ["PA00", "PA01", "PA02", "PA03", "PA04", "PB04", "NA", "NB", "NC", "ND"];

    /// <summary> Como HouseStyleNames.GetName del Mac. </summary>
    private static string StyleName<TE>(TE value) where TE : struct, Enum
    {
        var name = value.ToString();
        string material;
        int color = 0;
        bool round = false;
        if (name.StartsWith('_') || char.IsDigit(name[0]))
            material = $"{T("StyleOther")} {name.TrimStart('_')}";
        else if (name.StartsWith("HouseTent", StringComparison.Ordinal))
            material = T("StyleTent");
        else if (name.Contains("ForSale", StringComparison.Ordinal))
            material = T("StyleForSale");
        else if (name.Contains("SoldOut", StringComparison.Ordinal))
            material = T("StyleSoldOut");
        else if (name.Contains("HouseMoving", StringComparison.Ordinal))
            material = T("StyleMoving");
        else
        {
            foreach (var p in Prefixes)
                if (name.StartsWith(p, StringComparison.Ordinal))
                    name = name[p.Length..];
            foreach (var r in Regions)
            {
                if (name.StartsWith(r, StringComparison.Ordinal))
                {
                    name = name[r.Length..];
                    break;
                }
            }
            if (typeof(TE) == typeof(DoorKind) && name.Length > 2 && name[^1] is 'S' or 'R' && char.IsUpper(name[^2]))
            {
                round = name[^1] == 'R';
                name = name[..^1];
            }
            if (name.Length > 1 && name[^1] is >= 'A' and <= 'H' && char.IsLower(name[^2]))
            {
                color = name[^1] - 'A' + 1;
                name = name[..^1];
            }
            material = name switch
            {
                "Standard" => T("StyleStandard"), "Thatched" => T("StyleThatched"), "Stone" => T("StyleStone"),
                "Westerntile" => T("StyleWesternTile"), "Slate" => T("StyleSlate"), "Metal" => T("StyleMetal"), "Tile" => T("StyleTile"),
                "Woodtile" => T("StyleWoodTile"), "Woodpanel" => T("StyleWoodPanel"), "Japanese" => T("StyleJapanese"),
                "Stoneweight" => T("StyleStoneWeight"), "Wood" => T("StyleWood"), "Woodframe" => T("StyleWoodFrame"),
                "Woodsiding" or "Woodsidng" => T("StyleWoodSiding"), "Brick" => T("StyleBrick"), "Stucco" => T("StyleStucco"),
                "Log" => T("StyleLog"), "Oriental" => T("StyleOriental"), "Soil" => T("StyleSoil"), "Iron" => T("StyleIron"),
                "Ironparts" => T("StyleIronParts"), "Relief" => T("StyleRelief"), "Window" => T("StyleWindow"),
                "IronGrill" => T("StyleIronGrill"), "Chinese" => T("StyleChinese"), "Simple" => T("StyleSimple"),
                "Carving" => T("StyleCarving"), "CercleWindow" => T("StyleRoundWindow"), "VerticalWindow" => T("StyleTallWindow"),
                "Lattice" => T("StyleLattice"), "Simplicity" => T("StylePlain"),
                _ => name,
            };
        }
        var text = material;
        if (color > 0)
            text += " · " + T("ColorNumber").Replace("{0}", color.ToString());
        return round ? $"{text} ({T("DoorRound")})" : text;
    }

    /// <summary> Como HouseStyleNames.Offered del Mac. </summary>
    private static List<TE> Offered<TE>(string prefix) where TE : struct, Enum => [.. Enum.GetValues<TE>()
        .Where(v => v.ToString() is var n && n.StartsWith(prefix, StringComparison.Ordinal) && Has(n)
            && (prefix[^1] != 'P' || n.AsSpan(prefix.Length + 1).StartsWith("04")))
        .OrderBy(v => GetName(v.ToString()) ?? v.ToString(), StringComparer.Create(new System.Globalization.CultureInfo("es-ES"), false))];

    private static void Lista<TE>(StringBuilder sb, string nombre, IEnumerable<TE> valores) where TE : struct, Enum
    {
        sb.AppendLine($"constexpr Aspecto {nombre}[] = {{");
        foreach (var v in valores)
            sb.AppendLine($"    {{{Convert.ToInt32(v)}, {Comun.C(NameOf(v))}}},");
        sb.AppendLine("};");
    }

    public string Generar()
    {
        var sb = new StringBuilder();
        sb.AppendLine("struct Aspecto { uint16_t valor; const char* nombre; };");
        sb.AppendLine("// Los tejados y fachadas que el juego ofrece a los jugadores (los de la casa terminada), por su nombre.");
        var tejados = Offered<RoofType>("HouseRoofP");
        var fachadas = Offered<WallType>("HouseWallP");
        Lista(sb, "TEJADOS", tejados);
        Lista(sb, "FACHADAS", fachadas);
        // Puertas: los diseños (en arco, los que tienen dibujo) y su forma recta, si la hay.
        var puertas = Enum.GetValues<DoorKind>().Where(z => z.ToString() is var n && n.StartsWith("HouseDoor", StringComparison.Ordinal) && n.EndsWith('R') && Has(n))
            .OrderBy(v => GetName(v.ToString()) ?? v.ToString(), StringComparer.Create(new System.Globalization.CultureInfo("es-ES"), false)).ToList();
        sb.AppendLine("// Las puertas: el diseño en arco y el mismo recto (o -1 si no hay), por su nombre.");
        sb.AppendLine("struct Puerta { uint16_t arco; int32_t recta; const char* nombre; };");
        sb.AppendLine("constexpr Puerta PUERTAS[] = {");
        foreach (var p in puertas)
        {
            var recta = Enum.TryParse<DoorKind>(p.ToString()[..^1] + "S", out var r) ? (int)r : -1;
            sb.AppendLine($"    {{{(int)p}, {recta}, {Comun.C(NameOf(p))}}},");
        }
        sb.AppendLine("};");
        // Nombre de cualquier valor (para el que tiene ahora, aunque no esté en las listas).
        void Nombres<TE>(string n) where TE : struct, Enum
        {
            sb.AppendLine($"constexpr Aspecto {n}[] = {{");
            foreach (var v in Enum.GetValues<TE>().Distinct())
                sb.AppendLine($"    {{{Convert.ToInt32(v)}, {Comun.C(NameOf(v))}}},");
            sb.AppendLine("};");
        }
        sb.AppendLine("// El nombre de cada valor.");
        Nombres<RoofType>("NOMBRES_TEJADO");
        Nombres<WallType>("NOMBRES_FACHADA");
        Nombres<DoorKind>("NOMBRES_PUERTA");
        // Color del tejado en el mapa: la letra del final (A-H), como el Mac.
        var colores = new List<int>();
        int maxRoof = Enum.GetValues<RoofType>().Max(z => (int)z);
        for (int i = 0; i <= maxRoof; i++)
        {
            var name = ((RoofType)i).ToString();
            var letter = name.Length > 0 ? name[^1] : 'A';
            colores.Add(letter is >= 'A' and <= 'H' ? letter - 'A' : 7);
        }
        sb.AppendLine("// Color del tejado de cada valor en el mapa (0-7, como el Mac: rojo, azul, verde, amarillo, morado, naranja, gris, marrón).");
        sb.AppendLine($"constexpr uint8_t COLOR_TEJADO[] = {{{Comun.Numeros(colores)}}};");
        Comun.Hpp("mapa_casas.hpp", "Las casas de los jugadores: tejados, fachadas y puertas para elegir, con su nombre.", sb.ToString());
        return $"{tejados.Count} tejados, {fachadas.Count} fachadas, {puertas.Count} puertas";
    }
}
