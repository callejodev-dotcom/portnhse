using System.Text;
using System.Text.RegularExpressions;
using NHSE.Core;

/// <summary> tablas.hpp: flores y malas hierbas del suelo de la isla. </summary>
public sealed class Flores : ITabla
{
    public string Generar()
    {
        var names = GameInfo.Strings.itemlist;
        string Name(FieldItemDefinition d) => d.HeldItemId < names.Length && !string.IsNullOrWhiteSpace(names[d.HeldItemId]) ? names[d.HeldItemId] : d.Name;
        var flowers = FieldItemList.Items.Where(z => z.Value.Kind.IsFlower).OrderBy(z => z.Key).ToList();
        var weeds = FieldItemList.Items.Where(z => z.Value.Kind.IsWeed).OrderBy(z => z.Key).ToList();
        // Florecidas: la fase 3 («PltFlwRose3Red») o las que no tienen fases.
        bool Bloom(FieldItemDefinition d) => !Regex.IsMatch(d.Name, @"[12][A-Z][a-z]+$") && !new[] { "brote", "capullo", "yema", "semilla" }.Any(p => Name(d).StartsWith(p));
        var sb = new StringBuilder();
        sb.AppendLine($"constexpr uint16_t FLORES[] = {{{string.Join(", ", flowers.Select(z => $"0x{z.Key:X4}"))}}};");
        sb.AppendLine($"constexpr uint16_t MALAS_HIERBAS[] = {{{string.Join(", ", weeds.Select(z => $"0x{z.Key:X4}"))}}};");
        sb.AppendLine("struct FlorConNombre { uint16_t id; const char* nombre; };");
        sb.AppendLine("// Las florecidas, para elegir con qué cambiar, por orden alfabético.");
        sb.AppendLine("constexpr FlorConNombre FLORES_ABIERTAS[] = {");
        foreach (var f in flowers.Where(z => Bloom(z.Value)).OrderBy(z => Name(z.Value), Comun.Es))
            sb.AppendLine($"    {{0x{f.Key:X4}, {Comun.C(Name(f.Value))}}},");
        sb.AppendLine("};");
        Comun.Hpp("tablas.hpp", "Flores y malas hierbas del suelo de la isla.", sb.ToString());
        return $"{flowers.Count} flores, {weeds.Count} malas hierbas, {flowers.Count(z => Bloom(z.Value))} abiertas";
    }
}
