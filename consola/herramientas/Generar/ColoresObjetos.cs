using System.Text;
using NHSE.Core;

/// <summary>
/// objetos_colores.hpp: el color de fondo de cada objeto en sus casillas, como NHSE para Mac (ItemColor: un color por
/// clase de objeto).
/// </summary>
public sealed class ColoresObjetos : ITabla
{
    public string Generar()
    {
        var n = GameInfo.Strings.itemlist.Length;
        var kinds = Enumerable.Range(0, n).Select(i => ItemInfo.GetItemKind((ushort)i)).ToArray();
        var distintas = kinds.Distinct().OrderBy(k => (int)k).ToList();
        var sb = new StringBuilder();
        sb.AppendLine($"constexpr int NUM_COLORES_OBJETO = {n};");
        sb.AppendLine("// La clase de cada objeto (índice en COLORES_CLASE), por su número.");
        sb.AppendLine($"inline constexpr uint8_t CLASE_OBJETO[] = {{{Comun.Numeros(kinds.Select(k => distintas.IndexOf(k)))}}};");
        sb.AppendLine("// Su color, 0xRRGGBB.");
        sb.AppendLine($"inline constexpr uint32_t COLORES_CLASE[] = {{{Comun.Numeros(distintas.Select(k => { var c = k == ItemKind.Unknown ? System.Drawing.Color.LimeGreen : ColorUtil.GetColor((int)k); return $"0x{c.R:X2}{c.G:X2}{c.B:X2}"; }))}}};");
        Comun.Hpp("objetos_colores.hpp", "El color de fondo de cada objeto en sus casillas (ItemColor de NHSE).", sb.ToString());
        return $"{n} objetos, {distintas.Count} colores";
    }
}
