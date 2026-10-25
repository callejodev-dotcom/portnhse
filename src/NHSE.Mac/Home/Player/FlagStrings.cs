// ReSharper disable AutoPropertyCanBeMadeGetOnly.Global
namespace NHSE.Mac;

/// <summary>
/// Texts of the friendly event flags window. Translated with <c>Resources/FlagStrings_xx.txt</c>; the explanation of each
/// flag is in <c>Resources/flags_xx.tsv</c>.
/// </summary>
public static class FlagStrings
{
    public static string PlayerTitle { get; set; } = "{0}’s progress";
    public static string IslandTitle { get; set; } = "Island progress";
    public static string PlayerHint { get; set; } = "What the game remembers about this player: conversations, tasks, first times…";
    public static string IslandHint { get; set; } = "What the game remembers about the island: buildings, events, visits…";
    public static string Warning { get; set; } = "These are notes the game keeps about what already happened. Changing them can make conversations or events repeat or be skipped: make a copy of your save first.";
    public static string Search { get; set; } = "Search (museum, Tom Nook, K.K.…)";
    public static string FilterAll { get; set; } = "All";
    public static string FilterOn { get; set; } = "On";
    public static string FilterOff { get; set; } = "Off";
    public static string FilterChanged { get; set; } = "Changed";
    public static string Yes { get; set; } = "Yes";
    public static string Summary { get; set; } = "{0} of {1} shown · {2} changed";
    public static string Undo { get; set; } = "Undo all changes";
    public static string Export { get; set; } = "Save to a file…";
    public static string Import { get; set; } = "Load from a file…";
}
