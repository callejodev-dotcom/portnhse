// ReSharper disable AutoPropertyCanBeMadeGetOnly.Global
namespace NHSE.Mac;

/// <summary>
/// Texts of the friendly designs page and window.
/// Translated with <c>Resources/DesignStrings_xx.txt</c>; languages without a file use these English defaults.
/// </summary>
public static class DesignStrings
{
    // Page
    public static string SlotsUsed { get; set; } = "{0} of {1} slots used.";
    public static string PatternsHint { get; set; } = "Patterns you can put on the ground, on walls, on clothes, umbrellas…";
    public static string ProHint { get; set; } = "Clothes made with the PRO editor, with front, back and sleeves.";
    public static string FlagHint { get; set; } = "The design flying on the flag by Resident Services.";
    public static string TailorHint { get; set; } = "The designs shown on the mannequins at the Able Sisters.";

    // Window
    public static string WindowHint { get; set; } = "Choose a design to see it large. You can rename it, save it as a picture or bring one from a file.";
    public static string FlagWindowHint { get; set; } = "You can rename it, save it as a picture or change it for one from a file.";
    public static string Empty { get; set; } = "Empty";
    public static string NoName { get; set; } = "(no name)";
    public static string EmptySlot { get; set; } = "This slot is empty. Bring a design from a file to fill it.";
    public static string Name { get; set; } = "Name";
    public static string Colors { get; set; } = "Colours";
    public static string MadeBy { get; set; } = "Made by {0} on {1}.";
    public static string FromGame { get; set; } = "A sample design that comes with the game.";
    public static string NotMine { get; set; } = "Someone else made it, so the game won’t let you change it. Make it yours to be able to.";
    public static string MakeMine { get; set; } = "Make it mine";
    public static string IsMineNow { get; set; } = "Now it’s yours.";
    public static string SavePicture { get; set; } = "Save as picture…";
    public static string Export { get; set; } = "Save to a file…";
    public static string Import { get; set; } = "Change for a file…";
    public static string ExportAll { get; set; } = "Save all in a folder…";
    public static string ImportFolder { get; set; } = "Load from a folder…";
    public static string DropHint { get; set; } = "Tip: drag {0} files here to put them in the chosen slot and the ones after it.";
    public static string FileNormal { get; set; } = "New Horizons design";
    public static string FilePro { get; set; } = "New Horizons PRO design";
    public static string WrongFile { get; set; } = "“{0}” is not a {1} file.";
    public static string AskMakeMine { get; set; } = "{0} made this design. Do you want it to be yours? Then you can change it in the game.";
    public static string AskMakeMineMany { get; set; } = "Some of these designs were made by other people. Do you want them to be yours? Then you can change them in the game.";
    public static string NoneInFolder { get; set; } = "There are no {0} files in that folder.";
    public static string Loaded { get; set; } = "{0} designs loaded.";
    public static string Saved { get; set; } = "{0} designs saved in the folder.";
}
