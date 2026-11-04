using System.Text;
using NHSE.Core;

/// <summary>
/// objetos.hpp: los objetos que se pueden llevar (bolsillos, almacén...), con su nombre en español y el mismo nombre en
/// minúsculas y sin tildes para buscar, cuántos caben en un hueco y sus variantes (colores y telas), como el editor de
/// objetos de NHSE para Mac.
/// </summary>
public sealed class Objetos : ITabla
{
    public string Generar()
    {
        var str = GameInfo.Strings;
        var display = str.itemlistdisplay;
        var rows = new List<(ushort Id, ushort Count, ushort Max, int Tipo, string Name)>();
        for (int i = 1; i < display.Length && i < Item.FieldItemMin; i++)
        {
            var id = (ushort)i;
            if (id is Item.MessageBottle or Item.DIYRecipe or Item.MessageBottleEgg || !Comun.EsNombre(display[i]))
                continue;
            var kind = ItemInfo.GetItemKind(id);
            if (kind == ItemKind.Kind_DIYRecipe || kind == ItemKind.Unknown || kind.ToString().StartsWith("Kind_Dummy", StringComparison.Ordinal))
                continue;
            ushort max = ItemInfo.TryGetMaxStackCount(id, out var m) && m > 1 && !kind.IsFlowerGene(id) ? m : (ushort)1;
            rows.Add((id, 0, max, kind == ItemKind.Kind_FossilUnknown ? 2 : 0, display[i]));
        }
        // Las recetas de bricolaje como objeto: «receta» con el número de la receta en la cantidad.
        var recipes = 0;
        foreach (var (index, made) in RecipeList.Recipes)
        {
            if (made >= display.Length || !Comun.EsNombre(display[made]))
                continue;
            rows.Add((Item.DIYRecipe, index, 1, 1, $"Receta: {display[made]}"));
            recipes++;
        }
        rows = [.. rows.OrderBy(z => Comun.Orden(z.Name), StringComparer.Ordinal).ThenBy(z => z.Id)];

        // Variantes: por cada «remake» usado, sus colores (cuerpo) y telas, si hay más de uno.
        var variantes = new List<(int Numero, string Nombre)>();
        var remakes = new List<(int Cuerpos, int NumCuerpos, int Telas, int NumTelas)>();
        var remakeDe = new Dictionary<int, int>();
        int RemakeDe(ushort id)
        {
            var remake = ItemRemakeUtil.GetRemakeIndex(id);
            if (remake < 0 || !ItemRemakeInfoData.List.TryGetValue(remake, out var info))
                return -1;
            if (remakeDe.TryGetValue(remake, out var ya))
                return ya;
            var bodies = Enumerable.Range(0, ItemRemakeInfo.BodyColorCountMax).Where(info.HasBodyColor).ToArray();
            var fabrics = Enumerable.Range(0, ItemRemakeInfo.BodyColorCountMax).Where(info.HasFabricColor).ToArray();
            if (bodies.Length <= 1 && fabrics.Length <= 1)
                return remakeDe[remake] = -1;
            int c = variantes.Count;
            if (bodies.Length > 1)
                variantes.AddRange(bodies.Select(i => (i, str.BodyColor.GetValueOrDefault($"{info.ItemUniqueID:00000}_{i}", info.GetBodyDescription(i)))));
            int t = variantes.Count;
            if (fabrics.Length > 1)
                variantes.AddRange(fabrics.Select(i => (i, str.FabricColor.GetValueOrDefault($"{info.ItemUniqueID:00000}_{i}", info.GetFabricDescription(i)))));
            remakes.Add((c, bodies.Length > 1 ? bodies.Length : 0, t, fabrics.Length > 1 ? fabrics.Length : 0));
            return remakeDe[remake] = remakes.Count - 1;
        }

        var fossils = rows.Where(z => ItemInfo.GetItemKind(z.Id) == ItemKind.Kind_Fossil).Select(z => z.Id).ToList();

        var sb = new StringBuilder();
        sb.AppendLine("// tipo: 0 normal, 1 receta (el número de la receta va en la cantidad), 2 fósil sin identificar (el fósil va en");
        sb.AppendLine("// la cantidad). remake: sus variantes en REMAKES, o -1.");
        sb.AppendLine("struct ObjetoConNombre { uint16_t id; uint16_t cuenta; uint16_t pila; uint8_t tipo; int16_t remake; const char* nombre; const char* clave; };");
        sb.AppendLine("constexpr ObjetoConNombre OBJETOS[] = {");
        foreach (var r in rows)
            sb.AppendLine($"    {{{r.Id}, {r.Count}, {r.Max}, {r.Tipo}, {(r.Tipo == 1 ? -1 : RemakeDe(r.Id))}, {Comun.C(r.Name)}, {Comun.C(Comun.Normalizar(r.Name))}}},");
        sb.AppendLine("};");
        sb.AppendLine("// Los colores (cuerpo, en los bits 0-2 de la cantidad) y telas (bits 5 y siguientes) de los objetos que los tienen.");
        sb.AppendLine("struct Variante { uint8_t numero; const char* nombre; };");
        sb.AppendLine("constexpr Variante VARIANTES[] = {");
        foreach (var (n, name) in variantes)
            sb.AppendLine($"    {{{n}, {Comun.C(name)}}},");
        sb.AppendLine("};");
        sb.AppendLine("struct Remake { uint16_t cuerpos, numCuerpos, telas, numTelas; }; // índices en VARIANTES");
        sb.AppendLine("constexpr Remake REMAKES[] = {");
        foreach (var r in remakes)
            sb.AppendLine($"    {{{r.Cuerpos}, {r.NumCuerpos}, {r.Telas}, {r.NumTelas}}},");
        sb.AppendLine("};");
        sb.AppendLine("// Los fósiles ya identificados (para los «fósil sin identificar»).");
        sb.AppendLine($"constexpr uint16_t FOSILES[] = {{{Comun.Numeros(fossils)}}};");
        Comun.Hpp("objetos.hpp", "Los objetos que se pueden llevar, por orden alfabético, con su nombre en español.", sb.ToString());
        return $"{rows.Count} objetos ({recipes} recetas), {remakes.Count} con variantes, {fossils.Count} fósiles";
    }
}
