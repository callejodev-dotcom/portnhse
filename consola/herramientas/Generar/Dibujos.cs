using System.Text;
using NHSE.Core;

/// <summary>
/// herramientas/dibujos.tsv: para cada objeto, de qué objeto es su dibujo (los del suelo, ≥ 60000, usan el del objeto
/// que dan) y su icono de menú (el dibujo genérico cuando no hay uno propio). Con esto, herramientas/dibujos.py hace
/// romfs/iconos.bin y el paquete de dibujos de la SD.
/// </summary>
public sealed class Dibujos : ITabla
{
    public string Generar()
    {
        var names = GameInfo.Strings.itemlist;
        var sb = new StringBuilder();
        sb.AppendLine("# objeto\tdibujo\ticono");
        int n = 0;
        for (int i = 1; i < Item.EXTENSION; i++)
        {
            var id = (ushort)i;
            var sprite = id;
            if (id >= names.Length)
            {
                if (!FieldItemList.Items.TryGetValue(id, out var d))
                    continue;
                if (d.HeldItemId < names.Length)
                    sprite = d.HeldItemId;
            }
            else if (!Comun.EsNombre(names[id]))
                continue;
            var icon = ItemInfo.GetMenuIcon(sprite);
            sb.Append(id).Append('\t').Append(sprite).Append('\t').Append(icon).Append('\n');
            n++;
        }
        File.WriteAllText(Path.Combine(Comun.Raiz, "consola/herramientas/dibujos.tsv"), sb.ToString());
        return $"{n} objetos con dibujo";
    }
}
