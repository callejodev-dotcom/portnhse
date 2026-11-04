// Genera las tablas de NHSE para Switch desde NHSE (kwsch/NHSE), con los nombres en español:
//   consola/source/core/tablas.hpp   flores y malas hierbas
//   consola/source/core/vecinos.hpp  los vecinos por nombre; sus datos van en consola/romfs/vecinos.bin (comprimidos)
//   consola/source/core/objetos.hpp  los objetos que se pueden llevar en los bolsillos (y las recetas como objeto)
//   consola/source/core/listas.hpp   recetas, catálogo y reacciones («aprender todo»)
// Uso: dotnet run --project consola/herramientas/Generar
using System.Globalization;
using System.IO.Compression;
using System.Text;
using System.Text.RegularExpressions;
using NHSE.Core;
using NHSE.Villagers;

GameInfo.SetLanguage2Char(3);
var root = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "../../../../../.."));
var core = Path.Combine(root, "consola/source/core");
var romfs = Path.Combine(root, "consola/romfs");
var es = StringComparer.Create(new CultureInfo("es-ES"), true);
string C(string s) => s.Replace("\\", "\\\\").Replace("\"", "\\\"");

static string Normalize(string s)
{
    var d = s.ToLowerInvariant().Normalize(NormalizationForm.FormD);
    var sb = new StringBuilder(d.Length);
    foreach (var c in d)
    {
        if (CharUnicodeInfo.GetUnicodeCategory(c) != UnicodeCategory.NonSpacingMark)
            sb.Append(c);
    }
    return sb.ToString();
}
char[] notLetter = [.. "¡!¿?()[]\"'«»-—–.,:;#*+ "];
string SortKey(string name) => Normalize(name).TrimStart(notLetter);
static bool IsRealName(string? text) => !string.IsNullOrWhiteSpace(text) && !text.StartsWith("(Item #", StringComparison.Ordinal) && text.Any(char.IsLetterOrDigit);

const string Head = "// Generado desde NHSE (kwsch/NHSE) con consola/herramientas/Generar: no editar a mano.";

// ------------------------------------------------------------------------------------------------ flores y hierbas
{
    var names = GameInfo.Strings.itemlist;
    string Name(FieldItemDefinition d) => d.HeldItemId < names.Length && !string.IsNullOrWhiteSpace(names[d.HeldItemId]) ? names[d.HeldItemId] : d.Name;
    var flowers = FieldItemList.Items.Where(z => z.Value.Kind.IsFlower).OrderBy(z => z.Key).ToList();
    var weeds = FieldItemList.Items.Where(z => z.Value.Kind.IsWeed).OrderBy(z => z.Key).ToList();
    // Florecidas: la fase 3 («PltFlwRose3Red») o las que no tienen fases.
    bool Bloom(FieldItemDefinition d) => !Regex.IsMatch(d.Name, @"[12][A-Z][a-z]+$") && !new[] { "brote", "capullo", "yema", "semilla" }.Any(p => Name(d).StartsWith(p));
    var sb = new StringBuilder();
    sb.AppendLine(Head);
    sb.AppendLine("// Flores y malas hierbas del suelo de la isla.");
    sb.AppendLine("#pragma once");
    sb.AppendLine("#include <cstdint>");
    sb.AppendLine("namespace acnh {");
    sb.AppendLine($"constexpr uint16_t FLORES[] = {{{string.Join(", ", flowers.Select(z => $"0x{z.Key:X4}"))}}};");
    sb.AppendLine($"constexpr uint16_t MALAS_HIERBAS[] = {{{string.Join(", ", weeds.Select(z => $"0x{z.Key:X4}"))}}};");
    sb.AppendLine("struct FlorConNombre { uint16_t id; const char* nombre; };");
    sb.AppendLine("// Las florecidas, para elegir con qué cambiar, por orden alfabético.");
    sb.AppendLine("constexpr FlorConNombre FLORES_ABIERTAS[] = {");
    foreach (var f in flowers.Where(z => Bloom(z.Value)).OrderBy(z => Name(z.Value), es))
        sb.AppendLine($"    {{0x{f.Key:X4}, \"{C(Name(f.Value))}\"}},");
    sb.AppendLine("};");
    sb.AppendLine("} // namespace acnh");
    File.WriteAllText(Path.Combine(core, "tablas.hpp"), sb.ToString());
    Console.WriteLine($"{flowers.Count} flores, {weeds.Count} malas hierbas, {flowers.Count(z => Bloom(z.Value))} abiertas");
}

// -------------------------------------------------------------------------------------------------------- vecinos
{
    var resDir = Path.Combine(root, "NHSE/NHSE.Villagers/Resources");
    var names = Directory.GetFiles(Path.Combine(resDir, "Villagers"), "*V.bin").Select(f => Path.GetFileNameWithoutExtension(f)[..^1]).OrderBy(z => z, StringComparer.Ordinal).ToList();
    names = [.. names.Where(VillagerResources.IsVillagerDataKnown)];
    using var bin = new MemoryStream();
    var index = new List<(string Name, long Off, int Len)>();
    foreach (var n in names)
    {
        var d = VillagerResources.GetVillager(n);
        using var ms = new MemoryStream();
        using (var z = new ZLibStream(ms, CompressionLevel.SmallestSize, true))
        {
            z.Write(d.Villager.Span);
            z.Write(d.House.Span);
        }
        index.Add((n, bin.Position, (int)ms.Length));
        ms.Position = 0;
        ms.CopyTo(bin);
    }
    // Formato: "VECI", número, y por vecino 8 bytes de nombre, posición y tamaño; luego los datos.
    using (var f = File.Create(Path.Combine(romfs, "vecinos.bin")))
    using (var w = new BinaryWriter(f))
    {
        w.Write("VECI"u8);
        w.Write(index.Count);
        var header = 8 + (index.Count * 16);
        foreach (var (n, off, len) in index)
        {
            var nb = new byte[8];
            Encoding.ASCII.GetBytes(n).CopyTo(nb, 0);
            w.Write(nb);
            w.Write((uint)(header + off));
            w.Write(len);
        }
        bin.Position = 0;
        bin.CopyTo(f);
    }
    var sb = new StringBuilder();
    sb.AppendLine(Head);
    sb.AppendLine("// Los vecinos con su nombre en español, por orden alfabético.");
    sb.AppendLine("#pragma once");
    sb.AppendLine("namespace acnh {");
    var species = Enum.GetValues<VillagerSpecies>().OrderBy(z => (int)z).ToList();
    sb.AppendLine("// Las especies por su número (el primer byte de cada vecino), para su nombre interno («kal09»).");
    sb.AppendLine($"constexpr const char* ESPECIES[] = {{{string.Join(", ", Enumerable.Range(0, (int)species.Max() + 1).Select(i => $"\"{(Enum.IsDefined((VillagerSpecies)i) ? ((VillagerSpecies)i).ToString() : "")}\""))}}};");
    sb.AppendLine("struct VecinoConNombre { const char* interno; const char* nombre; };");
    sb.AppendLine("constexpr VecinoConNombre LISTA_VECINOS[] = {");
    foreach (var n in names.Select(n => (n, GameInfo.Strings.GetVillager(n))).OrderBy(z => z.Item2, es))
        sb.AppendLine($"    {{\"{n.n}\", \"{C(n.Item2)}\"}},");
    sb.AppendLine("};");
    sb.AppendLine("} // namespace acnh");
    File.WriteAllText(Path.Combine(core, "vecinos.hpp"), sb.ToString());
    Console.WriteLine($"{names.Count} vecinos, {new FileInfo(Path.Combine(romfs, "vecinos.bin")).Length / 1024} KB");
}

// -------------------------------------------------------------------------------------------------------- objetos
{
    var display = GameInfo.Strings.itemlistdisplay;
    var rows = new List<(ushort Id, ushort Count, ushort Max, string Name)>();
    for (int i = 1; i < display.Length && i < Item.FieldItemMin; i++)
    {
        var id = (ushort)i;
        if (id is Item.MessageBottle or Item.DIYRecipe or Item.MessageBottleEgg || !IsRealName(display[i]))
            continue;
        var kind = ItemInfo.GetItemKind(id);
        if (kind == ItemKind.Kind_DIYRecipe || kind == ItemKind.Unknown || kind.ToString().StartsWith("Kind_Dummy", StringComparison.Ordinal))
            continue;
        ushort max = ItemInfo.TryGetMaxStackCount(id, out var m) && m > 1 && !kind.IsFlowerGene(id) ? m : (ushort)1;
        rows.Add((id, 0, max, display[i]));
    }
    // Las recetas de bricolaje como objeto: «receta» con el número de la receta en la cantidad.
    var recipes = 0;
    foreach (var (index, made) in RecipeList.Recipes)
    {
        if (made >= display.Length || !IsRealName(display[made]))
            continue;
        rows.Add((Item.DIYRecipe, index, 1, $"Receta: {display[made]}"));
        recipes++;
    }
    rows = [.. rows.OrderBy(z => SortKey(z.Name), StringComparer.Ordinal).ThenBy(z => z.Id)];

    var sb = new StringBuilder();
    sb.AppendLine(Head);
    sb.AppendLine("// Los objetos que se pueden poner en los bolsillos, por orden alfabético, con su nombre en español y el mismo");
    sb.AppendLine("// nombre en minúsculas y sin tildes para buscar. «cuenta» es lo que va en la cantidad (el número de la receta en");
    sb.AppendLine("// las recetas); «pila», cuántos caben en un hueco.");
    sb.AppendLine("#pragma once");
    sb.AppendLine("#include <cstdint>");
    sb.AppendLine("namespace acnh {");
    sb.AppendLine("struct ObjetoConNombre { uint16_t id; uint16_t cuenta; uint16_t pila; const char* nombre; const char* clave; };");
    sb.AppendLine("constexpr ObjetoConNombre OBJETOS[] = {");
    foreach (var r in rows)
        sb.AppendLine($"    {{{r.Id}, {r.Count}, {r.Max}, \"{C(r.Name)}\", \"{C(Normalize(r.Name))}\"}},");
    sb.AppendLine("};");
    sb.AppendLine("} // namespace acnh");
    File.WriteAllText(Path.Combine(core, "objetos.hpp"), sb.ToString());
    Console.WriteLine($"{rows.Count} objetos ({recipes} recetas)");
}

// ----------------------------------------------------------------------------------- recetas, catálogo, reacciones
{
    // Recetas: las de RecipeBook.GiveAll (sin los dos kits de construcción).
    var recipes = RecipeList.Recipes.Keys.Where(z => z is not RecipeList.BridgeConstructionKit and not RecipeList.CampsiteConstructionKit).Order().ToList();

    // Catálogo: lo que marca «Marcar todo» del catálogo (CatalogWindow), con todos los colores de cada objeto.
    var ofs = new PersonalOffsets30();
    var max = (ofs.ItemRemakeCollectBit - ofs.ItemCollectBit) * 8;
    var variantMax = ofs.MaxRemakeBitFlag;
    var skip = GameLists.NoCheckReceived.ToArray();
    var catalog = GameInfo.Strings.ItemDataSource
        .Where(z => z.Value is > 0 and < Item.NONE && z.Value < max && IsRealName(z.Text) && Array.IndexOf(skip, (ushort)z.Value) < 0)
        .Select(z => (ushort)z.Value)
        .Where(id => ItemInfo.GetItemKind(id) != ItemKind.Kind_DIYRecipe)
        .Distinct().Order().ToList();
    var variants = new SortedSet<int>();
    foreach (var id in catalog)
    {
        var remake = ItemRemakeUtil.GetRemakeIndex(id);
        if (!ItemRemakeInfoData.List.TryGetValue(remake, out var info))
            continue;
        for (int i = 0; i < ItemRemakeInfo.BodyColorCountMax; i++)
        {
            var bit = (remake * ItemRemakeInfo.BodyColorCountMax) + i;
            if (info.HasBodyColor(i) && bit < ((variantMax + 7) / 8) * 8)
                variants.Add(bit);
        }
    }

    // Reacciones: las de AddMissingReactions, en su orden (sin las que no se usan).
    var reactions = Enum.GetValues<Reaction>().Where(z => z != Reaction.None && !z.ToString().StartsWith("UNUSED", StringComparison.Ordinal)).Select(z => (byte)z).ToList();

    var sb = new StringBuilder();
    sb.AppendLine(Head);
    sb.AppendLine("// Lo que se marca al «aprender todo»: recetas, catálogo (objetos y sus colores) y reacciones.");
    sb.AppendLine("#pragma once");
    sb.AppendLine("#include <cstdint>");
    sb.AppendLine("namespace acnh {");
    sb.AppendLine($"constexpr uint16_t RECETAS[] = {{{string.Join(", ", recipes)}}};");
    sb.AppendLine($"constexpr uint16_t CATALOGO[] = {{{string.Join(", ", catalog)}}};");
    sb.AppendLine($"constexpr uint16_t CATALOGO_COLORES[] = {{{string.Join(", ", variants)}}};");
    sb.AppendLine($"constexpr uint8_t REACCIONES[] = {{{string.Join(", ", reactions)}}};");
    sb.AppendLine("} // namespace acnh");
    File.WriteAllText(Path.Combine(core, "listas.hpp"), sb.ToString());
    Console.WriteLine($"{recipes.Count} recetas, {catalog.Count} objetos del catálogo ({variants.Count} colores), {reactions.Count} reacciones");
}
