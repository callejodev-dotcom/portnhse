using System.Text;
using NHSE.Core;
using static NHSE.Core.ItemKind;

/// <summary>
/// mapa_objetos.hpp: lo que el mapa de la isla y las casas necesitan de cada objeto, como GridItems de NHSE para Mac: cuánto
/// ocupa, si se gira, su nombre (para «quitar todos los iguales») y su color en el mapa; y las cosas plantadas en el suelo
/// (árboles, flores, vallas, rocas...: los objetos ≥ 60000) con el nombre que les da el Mac.
/// </summary>
public sealed class MapaObjetos : ITabla
{
    /// <summary> Textos del mapa del Mac en español (Resources/MapStrings_es.txt). </summary>
    public static readonly Dictionary<string, string> Textos = LeerTextos();

    private static Dictionary<string, string> LeerTextos()
    {
        var r = new Dictionary<string, string>();
        foreach (var l in File.ReadAllLines(Path.Combine(Comun.Raiz, "src/NHSE.Mac/Resources/MapStrings_es.txt")))
        {
            var i = l.IndexOf('=');
            if (i > 0)
                r[l[..i]] = l[(i + 1)..];
        }
        return r;
    }

    /// <summary> Como GridItems.GetFieldName del Mac. </summary>
    public static string NombrePlanta(FieldItemDefinition def)
    {
        var held = def.HeldItemId;
        var names = GameInfo.Strings.itemlist;
        if (held != def.Index && held < names.Length && !string.IsNullOrWhiteSpace(names[held]))
            return names[held];
        var k = def.Kind;
        if (def.Name.Contains("Stump", StringComparison.Ordinal))
            return Textos["Stump"];
        if (k.IsTree)
            return Textos["Tree"];
        if (k.IsStone)
            return Textos["Rock"];
        if (k == FieldItemKind.UnitIconHole)
            return Textos["Hole"];
        if (k.IsWeed)
            return Textos["Weeds"];
        if (k.IsFlower)
            return Textos["Flower"];
        if (k.IsBush)
            return Textos["Bush"];
        if (k.IsFence)
            return Textos["Fence"];
        if (k is >= FieldItemKind.LadderKitA and <= FieldItemKind.LadderKitD)
            return Textos["Ladder"];
        return Textos["Plant"];
    }

    /// <summary> Como ItemSearchBox.GetCategory del Mac (0 todo... 13 otros). </summary>
    private static int Categoria(ushort id)
    {
        var k = ItemInfo.GetItemKind(id);
        if (k == Kind_Fish) return 7;
        if (k == Kind_Insect) return 8;
        if (k == Kind_DiveFish) return 9;
        if (k is Kind_Fossil or Kind_FossilUnknown) return 10;
        if (k is Kind_Picture or Kind_PictureFake or Kind_Sculpture or Kind_SculptureFake) return 11;
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
            return 12;
        if (k == Kind_DIYRecipe) return 13;
        return 0;
    }

    /// <summary> Como GridItems.CanRotate del Mac, solo por su clase (lo demás se mira en la consola). </summary>
    private static bool Girable(ushort id)
    {
        var kind = ItemInfo.GetItemKind(id);
        return kind.IsFurniture || kind is Kind_Fence or Kind_Counter or Kind_Pillar or Kind_Rug
            or Kind_RugMyDesign or Kind_Poster or Kind_Gyroid or Kind_Picture or Kind_PictureFake
            or Kind_Sculpture or Kind_SculptureFake or Kind_Bromide or Kind_Lantern or Kind_Windmill
            or Kind_HousePost or Kind_XmasDeco or Kind_FtrWall;
    }

    /// <summary> Cómo se forma el nombre en GameStrings.GetItemName(Item): 0 solo el nombre, 1 receta o botella (+ lo que
    /// se fabrica), 2 fósil sin identificar (+ el fósil), 3 flor con genes (+ los genes), 4 árbol (+ lo que da). </summary>
    private static int TipoNombre(ushort id)
    {
        var kind = ItemInfo.GetItemKind(id);
        if (kind.IsFlowerGene(id)) return 3;
        if (kind is Kind_DIYRecipe or Kind_MessageBottle) return 1;
        if (kind == Kind_FossilUnknown) return 2;
        if (kind == Kind_Tree) return 4;
        return 0;
    }

    /// <summary> Con este número NHSE falla (lee fuera de su tabla); no se usa. </summary>
    private static readonly int ItemTamanos = (int)new FileInfo(Path.Combine(Comun.Raiz, "NHSE/NHSE.Core/Resources/byte/item_size.bin")).Length;

    private static (int W, int H) Tamano(ushort id)
    {
        if (id == ItemTamanos)
            return (2, 2);
        var s = ItemInfo.GetItemSize(id);
        return (s.Width, s.Height);
    }

    /// <summary> Color de una planta por su nombre interno («PltFlwRose3Red», «PltTreeApple»...). </summary>
    private static uint ColorPlanta(FieldItemDefinition d)
    {
        var n = d.Name;
        (string Palabra, uint Color)[] colores =
        [
            ("Black", 0x4A3F4F), ("White", 0xF4F1EA), ("Yellow", 0xF2D24B), ("Pink", 0xF29BB8), ("Orange", 0xF0954A),
            ("Blue", 0x6F98E0), ("Purple", 0xA77BD0), ("Green", 0x9CCB5E), ("Red", 0xD9443F), ("Gold", 0xE8C043),
        ];
        var k = d.Kind;
        if (k.IsTree)
        {
            if (n.Contains("Cedar")) return 0x2F6B45;
            if (n.Contains("Palm") || n.Contains("Coconut")) return 0x5DA845;
            if (n.Contains("Bamboo")) return 0x86C25A;
            if (n.Contains("Apple")) return 0x4D9A3E;
            return 0x48963C;
        }
        foreach (var (p, c) in colores)
            if (n.Contains(p, StringComparison.Ordinal)) return c;
        if (k.IsBush)
        {
            if (n.Contains("Azalea")) return 0xE07AA6;
            if (n.Contains("Hibiscus")) return 0xD94A4A;
            if (n.Contains("Hydrangea")) return 0x7D9BE0;
            if (n.Contains("Osmanthus")) return 0xF0A848;
            if (n.Contains("Camellia")) return 0xD84A5A;
            if (n.Contains("Holly")) return 0x3E8A4A;
            if (n.Contains("Tea")) return 0xF2F0E2;
            return 0x5A9E4A;
        }
        if (k.IsWeed) return 0x6FA83E;
        if (k.IsFence) return 0xA87A4E;
        if (k.IsStone) return 0x9C9EA3;
        if (k.IsFlower) return 0xE6A4C4;
        if (k.IsPlant) return 0x7CBF52;
        return 0xB49A72;
    }

    /// <summary> Cómo se dibuja (como IslandMapView.GetLook): 0 normal, 1 árbol, 2 arbusto, 3 flor, 4 tocón, 5 roca, 6 hoyo, 7 hierba. </summary>
    private static int Aspecto(FieldItemDefinition d)
    {
        var k = d.Kind;
        if (d.Name.Contains("Stump", StringComparison.Ordinal)) return 4;
        if (k.IsTree) return 1;
        if (k.IsBush) return 2;
        if (k.IsStone) return 5;
        if (k == FieldItemKind.UnitIconHole) return 6;
        if (k.IsWeed) return 7;
        return k.IsPlant ? 3 : 0;
    }

    /// <summary> Para ordenar y quitar: 1 mala hierba, 2 flor, 3 árbol, 4 arbusto, 5 valla, 6 hoyo, 7 roca, 0 lo demás. </summary>
    private static int Clase(FieldItemKind k)
    {
        if (k.IsWeed) return 1;
        if (k.IsFlower) return 2;
        if (k.IsTree) return 3;
        if (k.IsBush) return 4;
        if (k.IsFence) return 5;
        if (k == FieldItemKind.UnitIconHole) return 6;
        if (k.IsStone) return 7;
        return 0;
    }

    public string Generar()
    {
        var display = GameInfo.Strings.itemlistdisplay;
        // Hasta donde llegan los nombres y los tamaños (más allá, 2×2 y sin nombre, como NHSE).
        int n = display.Length;
        while (n < Item.FieldItemMin && Tamano((ushort)n) is var t && t != (2, 2))
            n++;
        // Grupos de nombres: dos cosas con el mismo nombre (como lo dice el Mac) van en el mismo grupo.
        var grupos = new Dictionary<string, int>(StringComparer.Ordinal) { [""] = 0 };
        var nombres = new List<string> { "" };
        int Grupo(string s)
        {
            if (!grupos.TryGetValue(s, out var g))
            {
                grupos[s] = g = nombres.Count;
                nombres.Add(s);
            }
            return g;
        }

        var info = new List<uint>();
        var grupoDe = new List<int>();
        for (int i = 0; i < n; i++)
        {
            var id = (ushort)i;
            var (w, h) = Tamano(id);
            if (w > 15 || h > 15)
                throw new Exception($"tamaño {w}x{h} de {id}");
            uint v = (uint)w | ((uint)h << 4) | (Girable(id) ? 1u << 8 : 0) | ((uint)Categoria(id) << 9) | ((uint)TipoNombre(id) << 13);
            info.Add(v);
            grupoDe.Add(FieldItemList.Items.ContainsKey(id) || i >= display.Length ? 0 : Grupo(display[i]));
        }
        // Lo que no está en la tabla ocupa 2×2 (como NHSE: tamaño desconocido): compruébalo.
        for (int i = n; i < 65534; i++)
        {
            if (i != ItemTamanos && Tamano((ushort)i) != (2, 2))
                throw new Exception($"{i} no es 2x2");
        }

        var plantas = FieldItemList.Items.Values.OrderBy(z => z.Index).ToList();
        var sb = new StringBuilder();
        sb.AppendLine($"constexpr int MAPA_INFO_N = {n};");
        sb.AppendLine("// Por objeto (id < MAPA_INFO_N): bits 0-3 ancho y 4-7 alto en casillas de objeto, 8 se puede girar, 9-12 su");
        sb.AppendLine("// categoría (0 otros, 1 muebles, 2 paredes y suelos, 3 ropa, 4 herramientas, 5 materiales, 6 comida, 7 peces,");
        sb.AppendLine("// 8 bichos, 9 criaturas marinas, 10 fósiles, 11 arte, 12 plantas, 13 recetas), 13-15 cómo se forma su nombre.");
        sb.AppendLine($"constexpr uint16_t MAPA_INFO[] = {{{Comun.Numeros(info)}}};");
        sb.AppendLine("// Su grupo de nombre (MAPA_NOMBRES): las cosas que se llaman igual van en el mismo.");

        // Las plantas con su nombre como el Mac (comparten grupos con los objetos que se llaman igual).
        var filas = new List<string>();
        foreach (var d in plantas)
        {
            var nombre = NombrePlanta(d);
            filas.Add($"    {{{d.Index}, {Clase(d.Kind)}, {Aspecto(d)}, {Grupo(nombre)}, 0x{ColorPlanta(d):X6}}},");
        }
        sb.AppendLine($"constexpr uint16_t MAPA_GRUPO[] = {{{Comun.Numeros(grupoDe)}}};");
        sb.AppendLine("// Las cosas plantadas en el suelo (id ≥ 60000), por id: clase (1 mala hierba, 2 flor, 3 árbol, 4 arbusto, 5 valla,");
        sb.AppendLine("// 6 hoyo, 7 roca, 0 otra), cómo se dibuja (0 normal, 1 árbol, 2 arbusto, 3 flor, 4 tocón, 5 roca, 6 hoyo, 7 hierba),");
        sb.AppendLine("// su grupo de nombre y su color.");
        sb.AppendLine("struct Planta { uint16_t id; uint8_t clase; uint8_t aspecto; uint16_t grupo; uint32_t color; };");
        sb.AppendLine("constexpr Planta PLANTAS[] = {");
        foreach (var f in filas)
            sb.AppendLine(f);
        sb.AppendLine("};");

        // Para elegir qué plantar: una por nombre, árboles y arbustos primero (como FieldItemPicker del Mac).
        var vistos = new HashSet<string>();
        var lista = new List<(ushort Id, string Nombre, int Grupo)>();
        foreach (var d in plantas)
        {
            var nombre = NombrePlanta(d);
            if (!vistos.Add(nombre))
                continue;
            var k = d.Kind;
            var g = k.IsTree ? 0 : k.IsBush ? 1 : k.IsFlower ? 2 : k.IsFence ? 3 : k.IsStone ? 4 : 5;
            lista.Add((d.Index, nombre, g));
        }
        lista = [.. lista.OrderBy(z => z.Grupo).ThenBy(z => Comun.Normalizar(z.Nombre), StringComparer.Ordinal)];
        sb.AppendLine("// Para plantar: una por nombre, como la lista del Mac (árboles, arbustos, flores, vallas, rocas y lo demás).");
        sb.AppendLine("struct PlantaParaElegir { uint16_t id; const char* nombre; const char* clave; };");
        sb.AppendLine("constexpr PlantaParaElegir PLANTAS_LISTA[] = {");
        foreach (var p in lista)
            sb.AppendLine($"    {{{p.Id}, {Comun.C(p.Nombre)}, {Comun.C(Comun.Normalizar(p.Nombre))}}},");
        sb.AppendLine("};");

        // Recetas: qué objeto sale de cada una (para el nombre de las recetas del suelo).
        sb.AppendLine("// Las recetas: número de receta y el objeto que se fabrica.");
        sb.AppendLine("struct RecetaObjeto { uint16_t receta; uint16_t objeto; };");
        sb.AppendLine("constexpr RecetaObjeto MAPA_RECETAS[] = {");
        foreach (var (r, o) in RecipeList.Recipes.OrderBy(z => z.Key))
            sb.AppendLine($"    {{{r}, {o}}},");
        sb.AppendLine("};");
        sb.AppendLine($"constexpr uint16_t MAPA_CONCHAS[] = {{{Comun.Numeros(GameLists.Shells.ToArray())}}};");

        sb.AppendLine("// El nombre de cada grupo.");
        sb.AppendLine("constexpr const char* MAPA_NOMBRES[] = {");
        foreach (var s in nombres)
            sb.AppendLine($"    {Comun.C(s)},");
        sb.AppendLine("};");
        Comun.Hpp("mapa_objetos.hpp", "Los objetos en el mapa de la isla y en las casas: cuánto ocupan, si se giran, su nombre y su color.", sb.ToString());
        return $"{n} objetos, {plantas.Count} plantas ({lista.Count} para elegir), {nombres.Count} nombres";
    }
}
