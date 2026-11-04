using System.Text;
using NHSE.Core;

/// <summary>
/// jugador_correo.hpp: lo que hace falta para el buzón, como MailWindow de NHSE para Mac: los personajes que mandan
/// cartas, los papeles de carta, los envoltorios y las cajas, con sus nombres en español.
/// </summary>
public sealed class JugadorCorreo : ITabla
{
    // NHSE solo tiene los envoltorios y las cajas en inglés: aquí van en castellano (los papeles de carta se quedan como
    // en NHSE).
    private static readonly Dictionary<string, string> Colores = new()
    {
        ["Yellow"] = "amarillo", ["Pink"] = "rosa", ["Orange"] = "naranja", ["Chartreuse"] = "verde claro", ["Green"] = "verde",
        ["Mint"] = "menta", ["LightBlue"] = "azul claro", ["Purple"] = "morado", ["Navy"] = "azul marino", ["Blue"] = "azul",
        ["White"] = "blanco", ["Red"] = "rojo", ["Gold"] = "dorado", ["Brown"] = "marrón", ["Gray"] = "gris", ["Black"] = "negro",
        ["Festive"] = "festivo",
    };

    private static readonly Dictionary<string, string> Nombres = new()
    {
        ["None"] = "Ninguno", ["Present"] = "Bolsa de regalo", ["Lucky Red Envelope"] = "Sobre rojo de la suerte",
        ["Bokjumeoni Pouch"] = "Bolsita bokjumeoni", ["Otoshidama Envelope"] = "Sobre otoshidama",
        ["Nook Shopping Box"] = "Caja de Nook Shopping", ["Hotel Box"] = "Caja del hotel", ["Paradise Planning Box"] = "Caja de Paradise Planning",
    };

    private static string Castellano(string s)
    {
        const string papel = "Wrapping Paper ";
        if (s.StartsWith(papel, StringComparison.Ordinal) && Colores.TryGetValue(s[papel.Length..], out var color))
            return "Papel de regalo " + color;
        return Nombres.GetValueOrDefault(s, s);
    }

    public string Generar()
    {
        var str = GameInfo.Strings;
        var sb = new StringBuilder();

        // Personajes: todos (para saber quién manda una carta) y si salen en el menú para elegir (sin repetir nombre).
        string Nombre(Mail.SenderNPCInfo npc) => npc.NameConversion == "npc_mom" ? "Mamá" : str.GetVillager(npc.NameConversion);
        var vistos = new HashSet<string>();
        var enMenu = Mail.SenderNPCData.OrderBy(z => Nombre(z.Value), Comun.Es).Where(z => vistos.Add(z.Value.NameConversion)).Select(z => z.Key).ToHashSet();
        sb.AppendLine("// Los personajes que mandan cartas (su número va donde la especie de un vecino), por nombre; «menu»: se puede elegir");
        sb.AppendLine("// (los que salen dos veces, solo una).");
        sb.AppendLine("struct Personaje { uint8_t numero; bool menu; const char* nombre; };");
        sb.AppendLine("inline constexpr Personaje PERSONAJES[] = {");
        foreach (var (key, npc) in Mail.SenderNPCData.OrderBy(z => Nombre(z.Value), Comun.Es))
            sb.AppendLine($"    {{{key}, {(enMenu.Contains(key) ? "true" : "false")}, {Comun.C(Nombre(npc))}}},");
        sb.AppendLine("};");

        void Lista(string titulo, string nombre, IEnumerable<(int Codigo, string Clave)> filas)
        {
            sb.AppendLine($"// {titulo}");
            sb.AppendLine($"inline constexpr Opcion {nombre}[] = {{");
            foreach (var (codigo, clave) in filas)
                sb.AppendLine($"    {{{codigo}, {Comun.C(Castellano(str.GetMail(clave)))}}},");
            sb.AppendLine("};");
        }
        sb.AppendLine("struct Opcion { uint16_t codigo; const char* nombre; };");
        Lista("Los papeles de carta.", "PAPELES_CARTA", Mail.LetterStationerySkin.OrderBy(z => z.Value.index).Select(z => (z.Key, z.Value.name)));
        Lista("Los envoltorios del regalo de una carta.", "ENVOLTORIOS", ItemWrappingExtended.ItemWrappingSkin.OrderBy(z => z.Value.index).Select(z => (z.Key, z.Value.name)));
        Lista("Las cajas de un paquete.", "CAJAS", ItemWrappingExtended.ItemBoxSkin.OrderBy(z => z.Value.index).Select(z => (z.Key, z.Value.name)));

        Comun.Hpp("jugador_correo.hpp", "El buzón: quién manda las cartas, papeles de carta, envoltorios y cajas.", sb.ToString());
        return $"{Mail.SenderNPCData.Count} personajes, {Mail.LetterStationerySkin.Count} papeles de carta";
    }
}
