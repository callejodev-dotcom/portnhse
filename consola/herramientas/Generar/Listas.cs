using System.Text;
using NHSE.Core;

/// <summary> listas.hpp: recetas, catálogo y reacciones («aprender todo»). </summary>
public sealed class Listas : ITabla
{
    public string Generar()
    {
        // Recetas: las de RecipeBook.GiveAll (sin los dos kits de construcción).
        var recipes = RecipeList.Recipes.Keys.Where(z => z is not RecipeList.BridgeConstructionKit and not RecipeList.CampsiteConstructionKit).Order().ToList();

        // Catálogo: lo que marca «Marcar todo» del catálogo (CatalogWindow), con todos los colores de cada objeto.
        var ofs = new PersonalOffsets30();
        var max = (ofs.ItemRemakeCollectBit - ofs.ItemCollectBit) * 8;
        var variantMax = ofs.MaxRemakeBitFlag;
        var skip = GameLists.NoCheckReceived.ToArray();
        var catalog = GameInfo.Strings.ItemDataSource
            .Where(z => z.Value is > 0 and < Item.NONE && z.Value < max && Comun.EsNombre(z.Text) && Array.IndexOf(skip, (ushort)z.Value) < 0)
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
        sb.AppendLine($"constexpr uint16_t RECETAS[] = {{{string.Join(", ", recipes)}}};");
        sb.AppendLine($"constexpr uint16_t CATALOGO[] = {{{string.Join(", ", catalog)}}};");
        sb.AppendLine($"constexpr uint16_t CATALOGO_COLORES[] = {{{string.Join(", ", variants)}}};");
        sb.AppendLine($"constexpr uint8_t REACCIONES[] = {{{string.Join(", ", reactions)}}};");
        Comun.Hpp("listas.hpp", "Lo que se marca al «aprender todo»: recetas, catálogo (objetos y sus colores) y reacciones.", sb.ToString());
        return $"{recipes.Count} recetas, {catalog.Count} objetos del catálogo ({variants.Count} colores), {reactions.Count} reacciones";
    }
}
