// ReSharper disable AutoPropertyCanBeMadeGetOnly.Global
namespace NHSE.Mac;

/// <summary>
/// Texts of the friendly home window (plain language, no save-format jargon).
/// Translated with <c>Resources/HomeStrings_xx.txt</c>; languages without a file use these English defaults.
/// </summary>
public static class HomeStrings
{
    // Sidebar
    public static string NavVillagers { get; set; } = "Villagers";
    public static string NavIsland { get; set; } = "Island";
    public static string NavMuseum { get; set; } = "Museum & catalog";
    public static string NavDesigns { get; set; } = "Designs";
    public static string NavAdvanced { get; set; } = "Advanced";
    public static string SaveButton { get; set; } = "Save changes";
    public static string StatusSaved { get; set; } = "No unsaved changes";
    public static string StatusDirty { get; set; } = "You have unsaved changes";
    public static string StatusJustSaved { get; set; } = "Saved at {0}";
    public static string StatusBackupOn { get; set; } = "A copy of the save was kept when you opened it.";
    public static string StatusBackupOff { get; set; } = "Backups are off (Options → Settings).";
    public static string AskSaveBeforeClose { get; set; } = "Do you want to save your changes before closing?";
    public static string ButtonSave { get; set; } = "Save";
    public static string ButtonDontSave { get; set; } = "Don't save";

    // Player page
    public static string IslandOf { get; set; } = "Island";
    public static string SectionMoney { get; set; } = "Money";
    public static string Wallet { get; set; } = "Wallet";
    public static string WalletHint { get; set; } = "In your pocket (max. 99,999)";
    public static string Bank { get; set; } = "Savings";
    public static string BankHint { get; set; } = "Bells in the ABD";
    public static string NookMiles { get; set; } = "Nook Miles";
    public static string NookMilesEarned { get; set; } = "{0} earned in total";
    public static string Poki { get; set; } = "Poki";
    public static string PokiHint { get; set; } = "Hotel currency";
    public static string HotelTickets { get; set; } = "Hotel tickets";
    public static string HotelTicketsHint { get; set; } = "To spend at the hotel";
    public static string SectionItems { get; set; } = "Items";
    public static string Pockets { get; set; } = "Pockets";
    public static string PocketsHint { get; set; } = "{0} items";
    public static string Storage { get; set; } = "Home storage";
    public static string StorageHint { get; set; } = "{0} of {1} spaces in use";
    public static string RecycleBin { get; set; } = "Recycle box";
    public static string RecycleBinHint { get; set; } = "Items left at Resident Services";
    public static string SectionCollections { get; set; } = "Collections";
    public static string Recipes { get; set; } = "DIY recipes";
    public static string RecipesHint { get; set; } = "{0} of {1} learned";
    public static string Reactions { get; set; } = "Reactions";
    public static string ReactionsHint { get; set; } = "{0} of {1} learned";
    public static string Achievements { get; set; } = "Nook Miles achievements";
    public static string AchievementsHint { get; set; } = "Records and stamps";
    public static string Mail { get; set; } = "Mailbox";
    public static string MailHint { get; set; } = "{0} letters";
    public static string Catalog { get; set; } = "Catalog";
    public static string CatalogHint { get; set; } = "Items marked as received";
    public static string Profile { get; set; } = "Profile";
    public static string ProfileHint { get; set; } = "Birthday, passport fruit…";
    public static string LearnAll { get; set; } = "Learn all";
    public static string Open { get; set; } = "Open";
    public static string Edit { get; set; } = "Edit";
    public static string ChangePhotoHint { get; set; } = "Passport photo";
    public static string PlayerOf { get; set; } = "Player {0} of {1}";
    public static string RenameHint { get; set; } = "The name changes everywhere it is saved (letters, villagers' memories…).";

    // Villagers page
    public static string VillagersTitle { get; set; } = "Your villagers";
    public static string VillagersHint { get; set; } = "Pick a villager to change them.";
    public static string Personality { get; set; } = "Personality";
    public static string Catchphrase { get; set; } = "Catchphrase";
    public static string CatchphraseOriginal { get; set; } = "Original";
    public static string MovingOut { get; set; } = "Moving out";
    public static string MovingOutHint { get; set; } = "They will leave the island. For the game to really remove them, also set flag 024 (ForceMoveOut) to 1 in Advanced.";
    public static string ReplaceVillager { get; set; } = "Swap for another villager…";
    public static string ReplaceVillagerHint { get; set; } = "The new villager moves into the same house.";
    public static string VillagerFurniture { get; set; } = "Furniture";
    public static string VillagerClothes { get; set; } = "Clothes";
    public static string VillagerFriendship { get; set; } = "Friendship";
    public static string VillagerRoom { get; set; } = "Walls & floor";
    public static string VillagerHouse { get; set; } = "House";
    public static string ExportVillager { get; set; } = "Export…";
    public static string ImportVillager { get; set; } = "Import…";
    public static string EmptyPlot { get; set; } = "Empty plot";
    public static string PickerTitle { get; set; } = "Choose a villager";
    public static string PickerSearch { get; set; } = "Search by name";
    public static string PickerChoose { get; set; } = "Swap";
    public static string Cancel { get; set; } = "Cancel";
    public static string SwapDone { get; set; } = "{0} now lives on your island instead of {1}.";
    public static string SwapNeedsUpdate { get; set; } = "This save is from an old version of the game. Open it in the game once to update it, then try again.";

    public static string PersonalityLazy { get; set; } = "Lazy";
    public static string PersonalityJock { get; set; } = "Jock";
    public static string PersonalityCranky { get; set; } = "Cranky";
    public static string PersonalitySmug { get; set; } = "Smug";
    public static string PersonalityNormal { get; set; } = "Normal";
    public static string PersonalityPeppy { get; set; } = "Peppy";
    public static string PersonalitySnooty { get; set; } = "Snooty";
    public static string PersonalitySisterly { get; set; } = "Sisterly";

    // Island page
    public static string IslandTitle { get; set; } = "Your island";
    public static string Hemisphere { get; set; } = "Hemisphere";
    public static string HemisphereNorth { get; set; } = "Northern";
    public static string HemisphereSouth { get; set; } = "Southern";
    public static string HemisphereHint { get; set; } = "Changes the seasons, fish and bugs.";
    public static string AirportColor { get; set; } = "Airport color";
    public static string IslandLife { get; set; } = "Island life";
    public static string MapTitle { get; set; } = "Map";
    public static string MapHint { get; set; } = "Items on the ground, terrain, paths and buildings.";
    public static string FruitFlowers { get; set; } = "Native fruit & flower";
    public static string FruitFlowersHint { get; set; } = "Your island's fruit and your sister island's.";
    public static string Turnips { get; set; } = "Turnip prices";
    public static string TurnipsHint { get; set; } = "Daisy Mae's price and this week's prices.";
    public static string Campsite { get; set; } = "Campsite";
    public static string CampsiteHint { get; set; } = "Who visits the campsite.";
    public static string Visitors { get; set; } = "Special visitors";
    public static string VisitorsHint { get; set; } = "Who comes to the plaza each day.";
    public static string PlayerHouses { get; set; } = "Player houses";
    public static string PlayerHousesHint { get; set; } = "Size, exterior and the furniture in each room.";
    public static string Bulletin { get; set; } = "Bulletin board";
    public static string BulletinHint { get; set; } = "Notes on the plaza board.";
    public static string WeatherSeed { get; set; } = "Weather seed";
    public static string WeatherSeedHint { get; set; } = "Decides the weather of every day. Only change it if you know what you are doing.";

    // Museum page
    public static string MuseumTitle { get; set; } = "Museum";
    public static string MuseumHint { get; set; } = "Donated fish, bugs, sea creatures, fossils and art.";

    // Designs page
    public static string DesignsTitle { get; set; } = "Custom designs";
    public static string Patterns { get; set; } = "My designs";
    public static string PatternsHint { get; set; } = "The normal design slots.";
    public static string ProDesigns { get; set; } = "PRO designs";
    public static string ProDesignsHint { get; set; } = "Clothes made with the PRO editor.";
    public static string FlagDesign { get; set; } = "Island flag";
    public static string FlagDesignHint { get; set; } = "The design on the flag by Resident Services.";
    public static string TailorDesigns { get; set; } = "Able Sisters";
    public static string TailorDesignsHint { get; set; } = "Designs shown at the tailor's.";

    // Advanced page
    public static string AdvancedTitle { get; set; } = "Advanced";
    public static string AdvancedHint { get; set; } = "Tools that edit the raw save data. A wrong value can break your save: keep a backup.";
    public static string ClassicEditor { get; set; } = "Classic editor";
    public static string ClassicEditorHint { get; set; } = "Every value as in the Windows version of NHSE.";
    public static string PlayerFlags { get; set; } = "Player flags";
    public static string ProgressSection { get; set; } = "Progress (flags)";
    public static string IslandFlags { get; set; } = "Island flags";
    public static string VerifyHashes { get; set; } = "Check the save";
    public static string VerifyHashesHint { get; set; } = "Verifies the checksums of every file.";
    public static string DumpDecrypted { get; set; } = "Export decrypted";
    public static string LoadDecrypted { get; set; } = "Import decrypted";
    public static string DumpDecryptedHint { get; set; } = "Saves the game files unencrypted into a folder, for other tools.";
    public static string LoadDecryptedHint { get; set; } = "Loads unencrypted game files from a folder into this save.";
    public static string TourWeatherSeed { get; set; } = "Mystery islands";
    public static string TourWeatherSeedHint { get; set; } = "Decides the weather of the islands you visit with a Nook Miles Ticket.";
    public static string RamEdit { get; set; } = "Live editing (SysBot)";
    public static string RamEditHint { get; set; } = "Reads and writes the game's memory over Wi-Fi. Needs sys-botbase on the console.";
    public static string ItemImages { get; set; } = "Item images";
    public static string ItemImagesHint { get; set; } = "Downloads the pictures of every item (92 MB).";

    // Item window
    public static string ItemsHint { get; set; } = "Click a space to choose it, then pick the item on the right. Drag items to move them.";
    public static string EmptySlot { get; set; } = "Empty space";
    public static string NothingSelected { get; set; } = "Choose a space on the left.";
    public static string ChangeItem { get; set; } = "Put an item";
    public static string SearchItem { get; set; } = "Search by name (e.g. apple, sofa…)";
    public static string SearchItemShort { get; set; } = "Search…";
    public static string Quantity { get; set; } = "How many";
    public static string MaxButton { get; set; } = "Max";
    public static string Variant { get; set; } = "Color";
    public static string Fabric { get; set; } = "Pattern";
    public static string RecipeOf { get; set; } = "Recipe for";
    public static string FossilOf { get; set; } = "Fossil";
    public static string Gift { get; set; } = "Wrapped as a gift";
    public static string GiftPaper { get; set; } = "Wrapping paper";
    public static string ClearSlot { get; set; } = "Empty this space";
    public static string FillAll { get; set; } = "Copy to every space";
    public static string FillAllAsk { get; set; } = "Put {0} in every space of this page?";
    public static string Sort { get; set; } = "Sort";
    public static string SortAZ { get; set; } = "By name";
    public static string SortType { get; set; } = "By type";
    public static string Clear { get; set; } = "Empty";
    public static string ClearAll { get; set; } = "Everything";
    public static string ClearClothes { get; set; } = "Clothes";
    public static string ClearMaterials { get; set; } = "Crafting materials";
    public static string ClearFurniture { get; set; } = "Furniture";
    public static string ClearBugs { get; set; } = "Bugs";
    public static string ClearFish { get; set; } = "Fish";
    public static string ClearDive { get; set; } = "Sea creatures";
    public static string TipAmount { get; set; } = "Amount: {0}";
    public static string More { get; set; } = "More";
    public static string ExportItems { get; set; } = "Export to a file…";
    public static string ImportItems { get; set; } = "Import from a file…";
    public static string AdvancedOptions { get; set; } = "Advanced options";
    public static string Apply { get; set; } = "Apply";
    public static string PageOf { get; set; } = "Page {0} of {1}";
    public static string Done { get; set; } = "Save";
    public static string ColorYellow { get; set; } = "Yellow";
    public static string ColorPink { get; set; } = "Pink";
    public static string ColorOrange { get; set; } = "Orange";
    public static string ColorLightGreen { get; set; } = "Light green";
    public static string ColorGreen { get; set; } = "Green";
    public static string ColorMint { get; set; } = "Mint";
    public static string ColorLightBlue { get; set; } = "Light blue";
    public static string ColorPurple { get; set; } = "Purple";
    public static string ColorNavy { get; set; } = "Navy";
    public static string ColorBlue { get; set; } = "Blue";
    public static string ColorWhite { get; set; } = "White";
    public static string ColorRed { get; set; } = "Red";
    public static string ColorGold { get; set; } = "Gold";
    public static string ColorBrown { get; set; } = "Brown";
    public static string ColorGray { get; set; } = "Gray";
    public static string ColorBlack { get; set; } = "Black";
    public static string CatAll { get; set; } = "All";
    public static string CatFurniture { get; set; } = "Furniture";
    public static string CatWalls { get; set; } = "Walls & floors";
    public static string CatClothes { get; set; } = "Clothes";
    public static string CatTools { get; set; } = "Tools";
    public static string CatMaterials { get; set; } = "Materials";
    public static string CatFood { get; set; } = "Food";
    public static string CatPlants { get; set; } = "Plants";
    public static string CatFish { get; set; } = "Fish";
    public static string CatBugs { get; set; } = "Bugs";
    public static string CatSea { get; set; } = "Sea creatures";
    public static string CatFossils { get; set; } = "Fossils";
    public static string CatArt { get; set; } = "Art";
    public static string CatRecipes { get; set; } = "DIY recipes";
    public static string CatOther { get; set; } = "Other";
}
