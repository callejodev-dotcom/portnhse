using System.Globalization;
using System.Text;

/// <summary> Lo común a todas las tablas: dónde van y cómo se escriben. </summary>
public static class Comun
{
    public static readonly string Raiz = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "../../../../../.."));
    public static readonly string Core = Path.Combine(Raiz, "consola/source/core");
    public static readonly string Romfs = Path.Combine(Raiz, "consola/romfs");
    public static readonly StringComparer Es = StringComparer.Create(new CultureInfo("es-ES"), true);

    public const string Cabecera = "// Generado desde NHSE (kwsch/NHSE) con consola/herramientas/Generar: no editar a mano.";

    /// <summary> Texto como cadena de C++. </summary>
    public static string C(string s) => "\"" + s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n").Replace("\r", "") + "\"";

    /// <summary> En minúsculas y sin tildes, como busca NHSE («sofa» encuentra «sofá»). </summary>
    public static string Normalizar(string s)
    {
        var d = s.ToLowerInvariant().Normalize(NormalizationForm.FormD);
        var sb = new StringBuilder(d.Length);
        foreach (var c in d)
        {
            if (CharUnicodeInfo.GetUnicodeCategory(c) != UnicodeCategory.NonSpacingMark)
                sb.Append(c);
        }
        return sb.ToString();
    }

    private static readonly char[] NoLetra = [.. "¡!¿?()[]\"'«»-—–.,:;#*+ "];

    /// <summary> Para ordenar: sin tildes y sin los signos del principio («¡», «(»). </summary>
    public static string Orden(string nombre) => Normalizar(nombre).TrimStart(NoLetra);

    public static bool EsNombre(string? text) => !string.IsNullOrWhiteSpace(text) && !text.StartsWith("(Item #", StringComparison.Ordinal) && text.Any(char.IsLetterOrDigit);

    /// <summary> Escribe un .hpp en consola/source/core con la cabecera, el namespace y el contenido. </summary>
    public static void Hpp(string nombre, string descripcion, string contenido)
    {
        var sb = new StringBuilder();
        sb.AppendLine(Cabecera);
        foreach (var l in descripcion.Split('\n'))
            sb.AppendLine("// " + l);
        sb.AppendLine("#pragma once");
        sb.AppendLine("#include <cstdint>");
        sb.AppendLine("namespace acnh {");
        sb.Append(contenido);
        sb.AppendLine("} // namespace acnh");
        File.WriteAllText(Path.Combine(Core, nombre), sb.ToString());
    }

    /// <summary> Una lista de números para C++, en líneas no muy largas. </summary>
    public static string Numeros<T>(IEnumerable<T> valores)
    {
        var sb = new StringBuilder();
        int n = 0;
        foreach (var v in valores)
        {
            sb.Append(v).Append(", ");
            if (++n % 24 == 0)
                sb.Append("\n    ");
        }
        return sb.ToString().TrimEnd(' ', '\n');
    }
}
