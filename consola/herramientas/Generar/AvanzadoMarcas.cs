using System.Text;
using NHSE.Core;

/// <summary>
/// avanzado_marcas.hpp: las marcas de progreso (event flags) de la isla y del jugador que conoce NHSE, con lo que
/// significa cada una en castellano, como las enseña la ventana «Progreso» de NHSE para Mac (EventFlagsWindow): la
/// explicación de src/NHSE.Mac/Resources/flags_es.tsv o, si no la hay, el nombre interno separado en palabras.
/// </summary>
public sealed class AvanzadoMarcas : ITabla
{
    // Cuántas marcas hay en la partida 3.0 (MainSave.EventFlagLand y PersonalOffsets30.EventFlagsPlayerLength / 2).
    private const int Isla = 0x400;
    private const int Jugador = 0x1000 / 2;

    public string Generar()
    {
        var textos = Explicaciones();
        var isla = EventFlagLand.List.Select(z => (z.Key, z.Value.MaxValue, z.Value.Name)).Where(z => z.Key < Isla).OrderBy(z => z.Key).ToList();
        var jugador = EventFlagPlayer.List.Select(z => (z.Key, z.Value.MaxValue, z.Value.Name)).Where(z => z.Key < Jugador).OrderBy(z => z.Key).ToList();

        var sb = new StringBuilder();
        sb.AppendLine("// Una marca: su número, su máximo (1 = sí o no; otro = un contador hasta ese número, o hasta 32.767 si es 0 o");
        sb.AppendLine("// menos), lo que significa y su nombre interno.");
        sb.AppendLine("struct MarcaConNombre { uint16_t indice; int16_t maximo; const char* texto; const char* interno; };");
        void Lista(string nombre, bool deIsla, List<(ushort Key, short Max, string Name)> lista)
        {
            sb.AppendLine($"constexpr MarcaConNombre {nombre}[] = {{");
            foreach (var (k, max, name) in lista)
            {
                var texto = textos.GetValueOrDefault((deIsla, k)) ?? SplitWords(name);
                sb.AppendLine($"    {{{k}, {max}, {Comun.C(texto)}, {Comun.C(name)}}},");
            }
            sb.AppendLine("};");
        }
        Lista("MARCAS_ISLA", true, isla);
        Lista("MARCAS_JUGADOR", false, jugador);
        Comun.Hpp("avanzado_marcas.hpp", "Las marcas de progreso de la isla y del jugador, con su explicación (Avanzado).", sb.ToString());
        return $"{isla.Count} marcas de la isla, {jugador.Count} del jugador";
    }

    /// <summary> Las explicaciones de NHSE para Mac (tipo, número, texto). </summary>
    private static Dictionary<(bool Isla, int Indice), string> Explicaciones()
    {
        var r = new Dictionary<(bool, int), string>();
        var ruta = Path.Combine(Comun.Raiz, "src/NHSE.Mac/Resources/flags_es.tsv");
        foreach (var linea in File.ReadAllLines(ruta))
        {
            var p = linea.Split('\t');
            if (p.Length < 3 || !int.TryParse(p[1], out var i))
                continue;
            r[(p[0] == "land", i)] = p[2];
        }
        return r;
    }

    /// <summary> Como PlayerUi.SplitWords de NHSE para Mac: «HasVisitedMuseum» → «Has visited museum». </summary>
    private static string SplitWords(string name)
    {
        var sb = new StringBuilder(name.Length + 8);
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
}
