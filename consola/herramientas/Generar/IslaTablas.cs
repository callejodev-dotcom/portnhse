using System.Globalization;
using System.Text;
using NHSE.Core;

/// <summary>
/// isla_tablas.hpp: lo de la sección «Isla» (sin el mapa ni las casas): dónde está cada cosa en main.dat 3.0, las piezas
/// del museo por salas (como MuseumWindow de NHSE para Mac), las frutas y los visitantes especiales.
/// </summary>
public sealed class IslaTablas : ITabla
{
    // Las salas del museo, en el orden de MuseumWindow.Wing.
    private static readonly string[] Salas = ["Peces", "Bichos", "Criaturas marinas", "Fósiles", "Arte"];

    private static bool EsDeLaSala(int sala, ItemKind k) => sala switch
    {
        0 => k == ItemKind.Kind_Fish,
        1 => k == ItemKind.Kind_Insect,
        2 => k == ItemKind.Kind_DiveFish,
        3 => k == ItemKind.Kind_Fossil,
        _ => k is ItemKind.Kind_Picture or ItemKind.Kind_Sculpture,
    };

    private static ushort[] DeLaSala(int sala) => sala switch
    {
        0 => GameLists.Fish.ToArray(),
        1 => GameLists.Bugs.ToArray(),
        2 => GameLists.Dive.ToArray(),
        3 => GameLists.Fossils.ToArray(),
        _ => GameLists.Art.ToArray(),
    };

    private static readonly CultureInfo Es = new("es");

    // Como IslandUi.ItemName: el nombre del juego con la primera letra en mayúscula.
    private static string NombreObjeto(ushort id)
    {
        var s = GameInfo.Strings.GetItemName(new Item(id));
        return s.Length == 0 ? s : char.ToUpper(s[0], Es) + s[1..];
    }

    // Los visitantes especiales por su número (VisitorNPC) y su nombre interno (IslandUi.VisitorCode).
    private static readonly string[] CodigosVisitantes = ["", "gul", "hgc", "cml", "gstA", "hgh", "bey", "chy", "skk", "slo", "fox", "gulB"];

    public string Generar()
    {
        var sb = new StringBuilder();
        var o = new MainSaveOffsets30();
        var p = new PersonalOffsets30();
        sb.AppendLine("// Posiciones en main.dat (3.0.x), de MainSaveOffsets30.");
        void Pos(string nombre, int valor, string que) => sb.AppendLine($"constexpr uint32_t {nombre} = 0x{valor:X}; // {que}");
        Pos("ISLA_HEMISFERIO", o.WeatherArea, "0 norte, 1 sur");
        Pos("ISLA_AEROPUERTO", o.AirportThemeColor, "0 azul, 1 amarillo, 2 naranja, 3 verde");
        Pos("ISLA_FRUTA", o.SpecialtyFruit, "la fruta de la isla (objeto, 2 bytes)");
        Pos("ISLA_FRUTA_HERMANA", o.SisterFruit, "la segunda fruta");
        Pos("ISLA_FLOR", o.SpecialtyFlower, "la flor de la isla (0-7)");
        Pos("ISLA_FLOR_HERMANA", o.SisterFlower, "la segunda flor");
        Pos("ISLA_BANDERAS_FRUTA", o.FruitFlags, "5 bytes: manzana, naranja, pera, melocotón, cereza");
        Pos("ISLA_CAMPING", o.GSaveCampSite, "estado: 2 vacía, 3 con alguien");
        Pos("ISLA_CAMPING_VECINO", o.GSaveNpcCamp, "variante y especie del campista");
        Pos("ISLA_CAMPING_FECHA", o.CampLastVisitTime, "año (2 bytes), mes, día");
        Pos("ISLA_MUSEO", o.Museum, "nivel, 1024 fechas, 1024 objetos y 1024 jugadores");
        Pos("ISLA_VISITANTES", o.Visitor, "GSaveVisitorNpc (0x78 bytes)");
        Pos("ISLA_TABLON", o.BulletinBoard, "GSaveBulletinBoard: 30 notas de 0x77D0 bytes desde +8");
        Pos("ISLA_NABOS", o.ShopKabu, "TurnipStonk: compra, 14 precios, tendencia y comienzo del pico");
        sb.AppendLine("// En personal.dat (3.0.x), de PersonalOffsets30.");
        Pos("PERSONAL_FRUTA", p.ProfileFruit, "la fruta del pasaporte");
        sb.AppendLine();

        // Museo: cada sala ordenada por su nombre sin tildes, como MuseumWindow.GetPieces (y así se donan con «Donar todos»).
        sb.AppendLine("// Las piezas del museo por salas (peces, bichos, criaturas marinas, fósiles y arte), cada sala por orden alfabético");
        sb.AppendLine("// como las enseña NHSE para Mac (sin falsificaciones).");
        sb.AppendLine($"constexpr const char* SALAS_MUSEO[] = {{{string.Join(", ", Salas.Select(Comun.C))}}};");
        sb.AppendLine("struct PiezaMuseo { uint16_t id; uint8_t sala; const char* nombre; };");
        sb.AppendLine("constexpr PiezaMuseo PIEZAS_MUSEO[] = {");
        var inicios = new List<int>();
        int total = 0;
        for (int sala = 0; sala < Salas.Length; sala++)
        {
            inicios.Add(total);
            var piezas = DeLaSala(sala).Distinct()
                .Where(id => EsDeLaSala(sala, ItemInfo.GetItemKind(id)))
                .Select(id => (Id: id, Nombre: NombreObjeto(id)))
                .OrderBy(z => Comun.Normalizar(z.Nombre), StringComparer.Ordinal)
                .ToList();
            foreach (var (id, nombre) in piezas)
                sb.AppendLine($"    {{{id}, {sala}, {Comun.C(nombre)}}},");
            total += piezas.Count;
        }
        sb.AppendLine("};");
        inicios.Add(total);
        sb.AppendLine($"// Dónde empieza cada sala en PIEZAS_MUSEO (y el total al final).");
        sb.AppendLine($"constexpr int INICIO_SALAS[] = {{{string.Join(", ", inicios)}}};");
        sb.AppendLine();

        // Frutas, en el orden de las banderas de fruta (FruitsFlowersWindow.FruitOrder).
        ushort[] frutas = [2213, 2214, 2285, 2286, 2287];
        sb.AppendLine("// Las cinco frutas, en el orden de las banderas de fruta.");
        sb.AppendLine("struct FrutaConNombre { uint16_t id; const char* nombre; };");
        sb.AppendLine($"constexpr FrutaConNombre FRUTAS[] = {{{string.Join(", ", frutas.Select(z => $"{{{z}, {Comun.C(NombreObjeto(z))}}}"))}}};");
        sb.AppendLine();

        // Visitantes especiales.
        var nombres = CodigosVisitantes.Select((c, i) => i == 0 ? "Nadie" : GameInfo.Strings.GetVillager(c)).ToArray();
        sb.AppendLine("// Los visitantes especiales por su número en la partida (0 = nadie).");
        sb.AppendLine($"constexpr const char* VISITANTES[] = {{{string.Join(", ", nombres.Select(Comun.C))}}};");

        Comun.Hpp("isla_tablas.hpp", "Lo de la isla: posiciones en la partida, piezas del museo, frutas y visitantes especiales.", sb.ToString());
        return $"{total} piezas del museo, {nombres.Length - 1} visitantes";
    }
}
