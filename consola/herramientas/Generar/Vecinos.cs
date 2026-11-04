using System.IO.Compression;
using System.Text;
using NHSE.Core;
using NHSE.Villagers;

/// <summary> vecinos.hpp (los vecinos por nombre) y romfs/vecinos.bin (los datos de cada uno y su casa, comprimidos). </summary>
public sealed class Vecinos : ITabla
{
    public string Generar()
    {
        var resDir = Path.Combine(Comun.Raiz, "NHSE/NHSE.Villagers/Resources");
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
        var ruta = Path.Combine(Comun.Romfs, "vecinos.bin");
        using (var f = File.Create(ruta))
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
        var species = Enum.GetValues<VillagerSpecies>().OrderBy(z => (int)z).ToList();
        sb.AppendLine("// Las especies por su número (el primer byte de cada vecino), para su nombre interno («kal09»).");
        sb.AppendLine($"constexpr const char* ESPECIES[] = {{{string.Join(", ", Enumerable.Range(0, (int)species.Max() + 1).Select(i => $"\"{(Enum.IsDefined((VillagerSpecies)i) ? ((VillagerSpecies)i).ToString() : "")}\""))}}};");
        sb.AppendLine("struct VecinoConNombre { const char* interno; const char* nombre; };");
        sb.AppendLine("constexpr VecinoConNombre LISTA_VECINOS[] = {");
        foreach (var n in names.Select(n => (n, GameInfo.Strings.GetVillager(n))).OrderBy(z => z.Item2, Comun.Es))
            sb.AppendLine($"    {{\"{n.n}\", {Comun.C(n.Item2)}}},");
        sb.AppendLine("};");
        Comun.Hpp("vecinos.hpp", "Los vecinos con su nombre en español, por orden alfabético.", sb.ToString());
        return $"{names.Count} vecinos, {new FileInfo(ruta).Length / 1024} KB";
    }
}
