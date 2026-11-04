using System.Text;
using NHSE.Core;
using static NHSE.Core.ItemKind;

/// <summary>
/// jugador_listas.hpp: las listas de la sección Jugador con sus nombres, como las ventanas del jugador de NHSE para Mac:
/// recetas (RecipesWindow), catálogo por categorías (CatalogWindow), reacciones (ReactionsWindow), récords de millas
/// (AchievementsWindow) y frutas del pasaporte (ProfileWindow).
/// </summary>
public sealed class JugadorListas : ITabla
{
    /// <summary> Los textos del Mac en español (PlayerStrings_es.txt, HomeStrings_es.txt). </summary>
    private static Dictionary<string, string> TextosMac(string archivo)
    {
        var r = new Dictionary<string, string>();
        foreach (var l in File.ReadAllLines(Path.Combine(Comun.Raiz, "src/NHSE.Mac/Resources", archivo)))
        {
            var i = l.IndexOf('=');
            if (i > 0)
                r[l[..i]] = l[(i + 1)..];
        }
        return r;
    }

    /// <summary> Como PlayerUi.SplitWords del Mac: «CatchFish» → «Catch fish». </summary>
    private static string Separar(string name)
    {
        var sb = new StringBuilder();
        for (int i = 0; i < name.Length; i++)
        {
            var c = name[i];
            if (c == '_')
            {
                sb.Append(' ');
                continue;
            }
            if (i > 0 && char.IsUpper(c) && name[i - 1] != '_' && !char.IsUpper(name[i - 1]))
                sb.Append(' ').Append(char.ToLowerInvariant(c));
            else
                sb.Append(i == 0 ? c : char.ToLowerInvariant(c));
        }
        return sb.ToString();
    }

    // Las categorías del catálogo, en el orden del Mac (ItemCategory sin «Recetas»).
    private static readonly string[] Categorias = ["CatAll", "CatFurniture", "CatWalls", "CatClothes", "CatTools", "CatMaterials", "CatFood", "CatPlants", "CatFish", "CatBugs", "CatSea", "CatFossils", "CatArt", "CatOther"];

    /// <summary> Como ItemSearchBox.GetCategory del Mac, con el número de Categorias (-1: receta). </summary>
    private static int Categoria(ushort id)
    {
        var k = ItemInfo.GetItemKind(id);
        if (k == Kind_Fish) return 8;
        if (k == Kind_Insect) return 9;
        if (k == Kind_DiveFish) return 10;
        if (k is Kind_Fossil or Kind_FossilUnknown) return 11;
        if (k is Kind_Picture or Kind_PictureFake or Kind_Sculpture or Kind_SculptureFake) return 12;
        if (k is Kind_RoomWall or Kind_RoomFloor or Kind_Rug or Kind_RugMyDesign) return 2;
        if (k.IsClothing || k is Kind_Umbrella or Kind_Shoes) return 3;
        if (k.IsFurniture || k is Kind_FtrWall or Kind_Counter or Kind_Pillar or Kind_Fence or Kind_Lantern or Kind_Windmill or Kind_HousePost or Kind_DoorDeco or Kind_Poster or Kind_Gyroid or Kind_XmasDeco)
            return 1;
        if (k is Kind_FishingRod or Kind_Net or Kind_Shovel or Kind_Axe or Kind_Watering or Kind_Slingshot or Kind_Ladder or Kind_ChangeStick
            or Kind_WoodenStickTool or Kind_GroundMaker or Kind_RiverMaker or Kind_CliffMaker or Kind_Timer or Kind_Megaphone or Kind_SmartPhone)
            return 4;
        if (k.IsCrafting || k is Kind_Feather or Kind_RainbowFeather or Kind_StarPiece or Kind_SnowCrystal or Kind_Sakurapetal or Kind_AutumnLeaf
            or Kind_ShellDrift or Kind_LoveCrystal or Kind_Honeycomb or Kind_DreamGold)
            return 5;
        if (k is Kind_Fruit or Kind_Vegetable or Kind_Mushroom or Kind_CookingMaterial or Kind_Dishes or Kind_Drink or Kind_HarvestDish or Kind_Candy
            or Kind_Turnip or Kind_Icecandy or Kind_Tapioca or Kind_SouvenirChocolate or Kind_JuiceFuzzyapple or Kind_BdayCupcake or Kind_Candyfloss)
            return 6;
        if (k.IsFlower || k is Kind_FlowerSeed or Kind_Bush or Kind_BushSeedling or Kind_Tree or Kind_TreeSeedling or Kind_VegeSeedling or Kind_VegeTree
            or Kind_Weed or Kind_WeedLight or Kind_Vine)
            return 7;
        if (k == Kind_DIYRecipe) return -1;
        return 13;
    }

    public string Generar()
    {
        var player = TextosMac("PlayerStrings_es.txt");
        var home = TextosMac("HomeStrings_es.txt");
        var str = GameInfo.Strings;
        var sb = new StringBuilder();

        // Recetas: todas las de RecipeList (también las dos de la historia), por el nombre de lo que se fabrica.
        var names = str.itemlist;
        var recetas = RecipeList.Recipes
            .Select(z => (Indice: z.Key, Nombre: z.Value < names.Length && !string.IsNullOrWhiteSpace(names[z.Value]) ? names[z.Value] : $"#{z.Value}"))
            .OrderBy(z => Comun.Orden(z.Nombre), StringComparer.Ordinal).ToList();
        sb.AppendLine("// Las recetas por el nombre de lo que se fabrica (historia: el juego la da en la historia y «aprender todas» no la toca).");
        sb.AppendLine("struct RecetaConNombre { uint16_t indice; bool historia; const char* nombre; };");
        sb.AppendLine("inline constexpr RecetaConNombre RECETAS_NOMBRES[] = {");
        foreach (var r in recetas)
            sb.AppendLine($"    {{{r.Indice}, {(r.Indice is RecipeList.BridgeConstructionKit or RecipeList.CampsiteConstructionKit ? "true" : "false")}, {Comun.C(r.Nombre)}}},");
        sb.AppendLine("};");

        // Catálogo: lo mismo que lista CatalogWindow, por nombre, con su categoría.
        var ofs = new PersonalOffsets30();
        var max = (ofs.ItemRemakeCollectBit - ofs.ItemCollectBit) * 8;
        var skip = GameLists.NoCheckReceived.ToArray();
        var catalogo = str.ItemDataSource
            .Where(z => z.Value is > 0 and < Item.NONE && z.Value < max && Comun.EsNombre(z.Text) && Array.IndexOf(skip, (ushort)z.Value) < 0)
            .Select(z => (Id: (ushort)z.Value, Nombre: z.Text, Cat: Categoria((ushort)z.Value)))
            .Where(z => z.Cat >= 0)
            .OrderBy(z => Comun.Orden(z.Nombre), StringComparer.Ordinal).ToList();
        sb.AppendLine("// Las categorías del catálogo (la 0 es «todo»).");
        sb.AppendLine($"inline constexpr const char* CATEGORIAS[] = {{{string.Join(", ", Categorias.Select(z => Comun.C(home[z])))}}};");
        sb.AppendLine("// El catálogo por nombre: el objeto, su categoría, sus colores (los bits «remake * 8 + i» de cada i de «colores» en los");
        sb.AppendLine("// colores recibidos; remake -1 si no tiene) y su nombre.");
        sb.AppendLine("struct CatalogoConNombre { uint16_t id; uint8_t categoria; int16_t remake; uint8_t colores; const char* nombre; };");
        sb.AppendLine("inline constexpr CatalogoConNombre CATALOGO_NOMBRES[] = {");
        var colorBits = ((ofs.MaxRemakeBitFlag + 7) / 8) * 8;
        foreach (var c in catalogo)
        {
            // Como CatalogWindow.SetReceived: cada color que tiene, si cabe en los colores recibidos.
            var remake = ItemRemakeUtil.GetRemakeIndex(c.Id);
            int mascara = 0;
            if (ItemRemakeInfoData.List.TryGetValue(remake, out var info))
            {
                for (int i = 0; i < ItemRemakeInfo.BodyColorCountMax; i++)
                {
                    if (info.HasBodyColor(i) && (remake * ItemRemakeInfo.BodyColorCountMax) + i < colorBits)
                        mascara |= 1 << i;
                }
            }
            else
            {
                remake = -1;
            }
            sb.AppendLine($"    {{{c.Id}, {c.Cat}, {remake}, {mascara}, {Comun.C(c.Nombre)}}},");
        }
        sb.AppendLine("};");

        // Reacciones: las válidas de ReactionsWindow, en tres grupos.
        var reacciones = Enum.GetValues<Reaction>().Where(z => z != Reaction.None && !z.ToString().StartsWith("UNUSED", StringComparison.Ordinal)).ToList();
        sb.AppendLine("// Las reacciones: su número, su grupo (0 emociones, 1 gestos y poses, 2 ejercicios y bailes) y su nombre.");
        sb.AppendLine($"inline constexpr const char* GRUPOS_REACCIONES[] = {{{Comun.C(player["ReactionGroupFeelings"])}, {Comun.C(player["ReactionGroupGestures"])}, {Comun.C(player["ReactionGroupMoves"])}}};");
        sb.AppendLine("struct ReaccionConNombre { uint8_t valor; uint8_t grupo; const char* nombre; };");
        sb.AppendLine("inline constexpr ReaccionConNombre REACCIONES_NOMBRES[] = {");
        foreach (var r in reacciones)
        {
            var grupo = r < Reaction.Scare ? 0 : r < Reaction.PostureWarmUp ? 1 : 2;
            var nombre = player.GetValueOrDefault($"Reaction{r}") ?? Separar(r.ToString());
            sb.AppendLine($"    {{{(byte)r}, {grupo}, {Comun.C(nombre)}}},");
        }
        sb.AppendLine("};");

        // Récords de millas Nook (LifeSupportAchievement), por su número.
        var records = LifeSupportAchievement.List.Values.OrderBy(z => z.Index).ToList();
        sb.AppendLine("// Los récords de millas Nook: número, cuántos sellos, lo que pide cada sello (0: sellos sueltos, un bit cada uno) y");
        sb.AppendLine("// el nombre.");
        sb.AppendLine("struct Record { uint16_t indice; uint8_t sellos; uint32_t umbral[5]; const char* nombre; };");
        sb.AppendLine("inline constexpr Record RECORDS[] = {");
        foreach (var a in records)
        {
            var nombre = player.GetValueOrDefault($"Record{a.Name}") ?? Separar(a.Name);
            sb.AppendLine($"    {{{a.Index}, {a.AchievementCount}, {{{a.Threshold1}, {a.Threshold2}, {a.Threshold3}, {a.Threshold4}, {a.Threshold5}}}, {Comun.C(nombre)}}},");
        }
        sb.AppendLine("};");

        // Frutas del pasaporte, en el orden de ProfileWindow.
        sb.AppendLine("// Las frutas del pasaporte.");
        sb.AppendLine("struct FrutaConNombre { uint16_t id; const char* nombre; };");
        sb.AppendLine("inline constexpr FrutaConNombre FRUTAS[] = {");
        foreach (var id in GameLists.Fruits)
            sb.AppendLine($"    {{{id}, {Comun.C(str.GetItemName(new Item(id)))}}},");
        sb.AppendLine("};");

        Comun.Hpp("jugador_listas.hpp", "Las listas de la sección Jugador con sus nombres: recetas, catálogo, reacciones, récords y frutas.", sb.ToString());
        return $"{recetas.Count} recetas, {catalogo.Count} objetos del catálogo, {reacciones.Count} reacciones, {records.Count} récords";
    }
}
