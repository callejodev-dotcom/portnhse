using System.Globalization;
using System.Text;
using NHSE.Core;

/// <summary>
/// vecino_tablas.hpp: lo que la ficha de cada vecino necesita con nombre, como la página de vecinos de NHSE para Mac:
/// la muletilla original de cada vecino, los papeles de pared y los suelos (las listas de «Paredes y suelo», sin
/// alfombras) y los tejados, fachadas y puertas de las casas de los vecinos (como VillagerHouseWindow y HouseStyleNames).
/// </summary>
public sealed class VecinoTablas : ITabla
{
    private static readonly string Recursos = Path.Combine(Comun.Raiz, "src/NHSE.Mac/Resources");

    public string Generar()
    {
        var str = GameInfo.Strings;
        var sb = new StringBuilder();

        // Muletillas originales, por nombre interno (ordenadas para buscar por mitades).
        var frases = str.VillagerMap.Keys.Select(k => (k, str.GetVillagerDefaultPhrase(k)))
            .Where(z => z.Item2 != z.k).OrderBy(z => z.k, StringComparer.Ordinal).ToList();
        sb.AppendLine("// La muletilla original de cada vecino, por su nombre interno (en orden, para buscar por mitades).");
        sb.AppendLine("struct FraseVecino { const char* interno; const char* frase; };");
        sb.AppendLine("constexpr FraseVecino FRASES_VECINOS[] = {");
        foreach (var (k, f) in frases)
            sb.AppendLine($"    {{{Comun.C(k)}, {Comun.C(f)}}},");
        sb.AppendLine("};");

        // Papeles de pared y suelos (RoomWindow.GetEntries).
        static string Mayuscula(string s) => s.Length == 0 ? s : char.ToUpper(s[0], CultureInfo.GetCultureInfo("es-ES")) + s[1..];
        var display = str.itemlistdisplay;
        var partes = new List<(ushort Id, string Nombre, string Clave, ItemKind Tipo)>();
        for (int i = 1; i < display.Length; i++)
        {
            var id = (ushort)i;
            var tipo = ItemInfo.GetItemKind(id);
            if (tipo is not (ItemKind.Kind_RoomWall or ItemKind.Kind_RoomFloor) || !display[i].Any(char.IsLetter) || display[i].StartsWith("(Item #", StringComparison.Ordinal))
                continue;
            partes.Add((id, Mayuscula(display[i]), Comun.Normalizar(display[i]), tipo));
        }
        partes = [.. partes.OrderBy(z => Comun.Orden(z.Nombre), StringComparer.Ordinal)];
        sb.AppendLine("// Los papeles de pared y los suelos, por orden alfabético (clave: en minúsculas y sin tildes, para buscar).");
        sb.AppendLine("struct ParteHabitacion { uint16_t id; const char* nombre; const char* clave; };");
        foreach (var (nombre, tipo) in new[] { ("PAREDES", ItemKind.Kind_RoomWall), ("SUELOS", ItemKind.Kind_RoomFloor) })
        {
            sb.AppendLine($"constexpr ParteHabitacion {nombre}[] = {{");
            foreach (var p in partes.Where(z => z.Tipo == tipo))
                sb.AppendLine($"    {{{p.Id}, {Comun.C(p.Nombre)}, {Comun.C(p.Clave)}}},");
            sb.AppendLine("};");
        }

        // Tejados, fachadas y puertas de las casas de los vecinos.
        var textos = Textos("MapStrings_es.txt");
        var exterior = Exterior();
        sb.AppendLine("// Lo de fuera de las casas de los vecinos: tejados, fachadas y puertas (en arco y rectas).");
        sb.AppendLine("struct AspectoCasa { uint16_t id; const char* nombre; };");
        var tejados = Enum.GetValues<RoofType>().Where(v => v.ToString().StartsWith("HouseRoofN", StringComparison.Ordinal)).ToList();
        Lista(sb, "TEJADOS", tejados.Select(v => (ushort)v).ToList(), Nombres(tejados.Select(v => v.ToString()).ToList(), textos, false));
        var fachadas = Enum.GetValues<WallType>().Where(v => v.ToString().StartsWith("HouseWallN", StringComparison.Ordinal) && exterior.ContainsKey(v.ToString()))
            .OrderBy(v => v.ToString(), StringComparer.Ordinal).ToList();
        Lista(sb, "FACHADAS", fachadas.Select(v => (ushort)v).ToList(), Nombres(fachadas.Select(v => v.ToString()).ToList(), textos, false));
        var puertas = Enum.GetValues<DoorKind>().Where(v => v.ToString() is var n && n.StartsWith("HouseDoor", StringComparison.Ordinal) && n.EndsWith('R') && exterior.ContainsKey(n))
            .Select(v => (Arco: v, Nombre: exterior[v.ToString()] is { Length: > 0 } es ? Mayuscula(es) : Nombres([v.ToString()], textos, true)[0]))
            .OrderBy(z => z.Nombre, Comun.Es).ToList();
        sb.AppendLine("struct PuertaCasa { uint16_t arco; uint16_t recta; const char* nombre; }; // recta = arco si no la hay");
        sb.AppendLine("constexpr PuertaCasa PUERTAS[] = {");
        foreach (var (arco, nombre) in puertas)
        {
            var recta = Enum.TryParse<DoorKind>(arco.ToString()[..^1] + "S", out var r) ? r : arco;
            sb.AppendLine($"    {{{(ushort)arco}, {(ushort)recta}, {Comun.C(nombre)}}},");
        }
        sb.AppendLine("};");

        Comun.Hpp("vecino_tablas.hpp", "Muletillas originales, paredes y suelos, y tejados, fachadas y puertas de las casas de los vecinos.", sb.ToString());
        return $"{frases.Count} muletillas, {partes.Count} paredes y suelos, {tejados.Count} tejados, {fachadas.Count} fachadas, {puertas.Count} puertas";
    }

    private static void Lista(StringBuilder sb, string nombre, List<ushort> ids, List<string> nombres)
    {
        sb.AppendLine($"constexpr AspectoCasa {nombre}[] = {{");
        for (int i = 0; i < ids.Count; i++)
            sb.AppendLine($"    {{{ids[i]}, {Comun.C(nombres[i])}}},");
        sb.AppendLine("};");
    }

    private static Dictionary<string, string> Textos(string archivo)
    {
        var r = new Dictionary<string, string>();
        foreach (var l in File.ReadAllLines(Path.Combine(Recursos, archivo)))
        {
            var i = l.IndexOf('=');
            if (i > 0)
                r[l[..i]] = l[(i + 1)..];
        }
        return r;
    }

    /// <summary> El nombre en español de cada aspecto de exterior.tsv (vacío si no tiene). </summary>
    private static Dictionary<string, string> Exterior()
    {
        var r = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var l in File.ReadAllLines(Path.Combine(Recursos, "exterior.tsv")))
        {
            if (l.Length == 0 || l[0] == '#')
                continue;
            var p = l.Split('\t');
            if (p.Length >= 2)
                r[p[0]] = p.Length > 3 ? p[3] : "";
        }
        return r;
    }

    private static readonly string[] Prefijos = ["HouseRoof", "HouseWall", "HouseDoor"];
    private static readonly string[] Regiones = ["PA00", "PA01", "PA02", "PA03", "PA04", "PB04", "NA", "NB", "NC", "ND"];

    private static readonly Dictionary<string, string> Materiales = new()
    {
        ["Standard"] = "StyleStandard", ["Thatched"] = "StyleThatched", ["Stone"] = "StyleStone", ["Westerntile"] = "StyleWesternTile",
        ["Slate"] = "StyleSlate", ["Metal"] = "StyleMetal", ["Tile"] = "StyleTile", ["Woodtile"] = "StyleWoodTile",
        ["Woodpanel"] = "StyleWoodPanel", ["Japanese"] = "StyleJapanese", ["Stoneweight"] = "StyleStoneWeight", ["Wood"] = "StyleWood",
        ["Woodframe"] = "StyleWoodFrame", ["Woodsiding"] = "StyleWoodSiding", ["Woodsidng"] = "StyleWoodSiding", ["Brick"] = "StyleBrick",
        ["Stucco"] = "StyleStucco", ["Log"] = "StyleLog", ["Oriental"] = "StyleOriental", ["Soil"] = "StyleSoil", ["Iron"] = "StyleIron",
        ["Ironparts"] = "StyleIronParts", ["Relief"] = "StyleRelief", ["Window"] = "StyleWindow", ["IronGrill"] = "StyleIronGrill",
        ["Chinese"] = "StyleChinese", ["Simple"] = "StyleSimple", ["Carving"] = "StyleCarving", ["CercleWindow"] = "StyleRoundWindow",
        ["VerticalWindow"] = "StyleTallWindow", ["Lattice"] = "StyleLattice", ["Simplicity"] = "StylePlain",
    };

    /// <summary>
    /// Los nombres hechos con el nombre interno, como HouseStyleNames.GetNames del Mac: «Pizarra 2 · color 3» cuando un
    /// material sale en varias familias de casas.
    /// </summary>
    private static List<string> Nombres(List<string> internos, Dictionary<string, string> t, bool puerta)
    {
        var partes = internos.Select(n => Partes(n, t, puerta)).ToList();
        var r = new List<string>();
        foreach (var p in partes)
        {
            var familias = partes.Where(z => z.Material == p.Material).Select(z => z.Familia).Distinct().ToList();
            var modelo = familias.Count > 1 ? familias.IndexOf(p.Familia) + 1 : 0;
            var texto = modelo > 0 ? $"{p.Material} {modelo}" : p.Material;
            if (p.Color > 0)
                texto += " · " + string.Format(t["ColorNumber"], p.Color);
            r.Add(p.Redonda ? $"{texto} ({t["DoorRound"]})" : texto);
        }
        var vistos = new Dictionary<string, int>();
        for (int i = 0; i < r.Count; i++)
        {
            vistos[r[i]] = vistos.GetValueOrDefault(r[i]) + 1;
            if (vistos[r[i]] > 1)
                r[i] = $"{r[i]} ({vistos[r[i]]})";
        }
        return r;
    }

    private static (string Material, int Color, bool Redonda, string Familia) Partes(string nombre, Dictionary<string, string> t, bool puerta)
    {
        foreach (var p in Prefijos)
        {
            if (nombre.StartsWith(p, StringComparison.Ordinal))
                nombre = nombre[p.Length..];
        }
        var familia = "";
        foreach (var r in Regiones)
        {
            if (nombre.StartsWith(r, StringComparison.Ordinal))
            {
                familia = r;
                nombre = nombre[r.Length..];
                break;
            }
        }
        bool redonda = false;
        if (puerta && nombre.Length > 2 && nombre[^1] is 'S' or 'R' && char.IsUpper(nombre[^2]))
        {
            redonda = nombre[^1] == 'R';
            nombre = nombre[..^1];
        }
        int color = 0;
        if (nombre.Length > 1 && nombre[^1] is >= 'A' and <= 'H' && char.IsLower(nombre[^2]))
        {
            color = nombre[^1] - 'A' + 1;
            nombre = nombre[..^1];
        }
        var material = Materiales.TryGetValue(nombre, out var k) && t.TryGetValue(k, out var m) ? m : nombre;
        return (material, color, redonda, familia);
    }
}
