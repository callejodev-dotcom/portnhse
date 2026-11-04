using System.Text;
using NHSE.Core;

/// <summary>
/// vecino_marcas.hpp: las marcas sueltas de un vecino (EventFlagVillager, las de VillagerFlagEditor) y las de lo que
/// recuerda de cada jugador (EventFlagVillagerMemoryPlayer, las de VillagerMemoryEditor), con lo que significa cada una
/// en castellano (herramientas/marcas_vecino_es.tsv) y su nombre interno.
/// </summary>
public sealed class VecinoMarcas : ITabla
{
    public string Generar()
    {
        var textos = Explicaciones();
        var sb = new StringBuilder();
        sb.AppendLine("// Una marca con nombre: su número, el valor con que empieza, su máximo, lo que significa y su nombre interno.");
        sb.AppendLine("struct MarcaDeVecino { uint16_t indice; int32_t inicial; int32_t maximo; const char* texto; const char* interno; };");
        void Lista(string nombre, string tipo, IEnumerable<(ushort Indice, int Inicial, int Maximo, string Nombre)> marcas)
        {
            sb.AppendLine($"constexpr MarcaDeVecino {nombre}[] = {{");
            foreach (var (i, ini, max, n) in marcas.OrderBy(z => z.Indice))
            {
                if (!textos.TryGetValue((tipo, i), out var t))
                    throw new InvalidOperationException($"Falta la explicación de la marca {tipo} {i} ({n}) en marcas_vecino_es.tsv");
                sb.AppendLine($"    {{{i}, {ini}, {max}, {Comun.C(t)}, {Comun.C(n)}}},");
            }
            sb.AppendLine("};");
        }
        Lista("MARCAS_VECINO", "vecino", EventFlagVillager.List.Values.Select(z => (z.Index, (int)z.Value1, (int)z.Value2, z.Name)));
        Lista("MARCAS_RECUERDO", "recuerdo", EventFlagVillagerMemoryPlayer.List.Values.Select(z => (z.Index, (int)z.Initial, (int)z.Maximum, z.Name)));
        Comun.Hpp("vecino_marcas.hpp", "Las marcas sueltas de cada vecino y las de lo que recuerda de cada jugador, con su explicación.", sb.ToString());
        return $"{EventFlagVillager.List.Count} marcas del vecino, {EventFlagVillagerMemoryPlayer.List.Count} de sus recuerdos";
    }

    private static Dictionary<(string Tipo, int Indice), string> Explicaciones()
    {
        var r = new Dictionary<(string, int), string>();
        foreach (var linea in File.ReadAllLines(Path.Combine(Comun.Raiz, "consola/herramientas/marcas_vecino_es.tsv")))
        {
            var p = linea.Split('\t');
            if (p.Length < 3 || linea.StartsWith('#') || !int.TryParse(p[1], out var i))
                continue;
            r[(p[0], i)] = p[2];
        }
        return r;
    }
}
