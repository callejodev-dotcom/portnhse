// Genera las tablas de NHSE para Switch desde NHSE (kwsch/NHSE), con los nombres en español. Cada tabla es una clase
// que implementa ITabla (un archivo por tabla): este programa las busca y las genera todas.
// Uso: dotnet run --project consola/herramientas/Generar
using System.Reflection;
using NHSE.Core;

GameInfo.SetLanguage2Char(3);
var tablas = Assembly.GetExecutingAssembly().GetTypes()
    .Where(t => typeof(ITabla).IsAssignableFrom(t) && !t.IsInterface && !t.IsAbstract)
    .OrderBy(t => t.Name, StringComparer.Ordinal);
foreach (var t in tablas)
{
    var tabla = (ITabla)Activator.CreateInstance(t)!;
    Console.WriteLine($"{t.Name}: {tabla.Generar()}");
}

/// <summary> Una tabla generada: escribe sus archivos y devuelve un resumen de una línea. </summary>
public interface ITabla
{
    string Generar();
}
