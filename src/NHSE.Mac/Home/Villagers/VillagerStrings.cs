// ReSharper disable AutoPropertyCanBeMadeGetOnly.Global
namespace NHSE.Mac;

/// <summary>
/// Texts of the friendly villager page and its windows (friendship, walls and floor).
/// Translated with <c>Resources/VillagerStrings_xx.txt</c>; languages without a file use these English defaults.
/// </summary>
public static class VillagerStrings
{
    // Villagers page
    public static string NobodyLivesHere { get; set; } = "Nobody lives here yet.";
    public static string MovingOutHint { get; set; } = "They will pack their things and leave the island.";
    public static string MovingOutBadge { get; set; } = "Moving out";
    public static string FriendshipWith { get; set; } = "with {0}";
    public static string NotMetYet { get; set; } = "Hasn't met {0} yet.";
    public static string Crafting { get; set; } = "DIY";
    public static string CraftingNow { get; set; } = "Crafting {0} right now, done at {1}.";
    public static string VillagerFile { get; set; } = "File";
    public static string FileHint { get; set; } = "To take them to another island or keep a copy.";
    public static string ExportHint { get; set; } = "Saves this villager to a file, to take them to another island or keep a copy.";
    public static string ImportHint { get; set; } = "Replaces this villager with one saved in a file. The house stays the same.";
    public static string ImportWrongFile { get; set; } = "This file is not an Animal Crossing: New Horizons villager, or it is from a version that can't be converted.";
    public static string ImportOtherIsland { get; set; } = "This villager comes from another island ({0}). Do you want to make them yours, so they remember you and your island? That is the usual choice.";
    public static string ImportDone { get; set; } = "{0} has been imported.";
    public static string EveryoneMovesOut { get; set; } = "Everyone moves out…";
    public static string EveryoneMovesOutAsk { get; set; } = "All your villagers will pack their things and leave the island. Are you sure?";
    public static string EveryoneMovesOutDone { get; set; } = "Done: {0} villagers are moving out. Save the changes for it to happen in the game.";
    public static string BestFriendsEveryVillager { get; set; } = "Best friends with every villager…";
    public static string BestFriendsEveryVillagerAsk { get; set; } = "Every villager will be best friends (255 points, the maximum) with every player they know. Are you sure?";
    public static string BestFriendsEveryVillagerDone { get; set; } = "Done: all your villagers are now best friends with everyone.";
    public static string ExportAll { get; set; } = "Export every villager to a folder…";
    public static string ExportAllDone { get; set; } = "{0} villagers were saved in the folder.";

    // Friendship window
    public static string FriendshipTitle { get; set; } = "Friendship with {0}";
    public static string FriendshipHint { get; set; } = "How {0} gets on with each player. Click a heart or move the bar.";
    public static string FriendshipHowTitle { get; set; } = "How friendship works";
    public static string FriendshipHow { get; set; } = "Each villager keeps 0 to 255 friendship points with every player, shown here as 7 hearts. It goes up when you talk to them every day and give them presents (even more on their birthday), and goes down if you hit them with the net. The more hearts, the more presents they give you; with many hearts they may even give you their photo.";
    public static string FriendshipPoints { get; set; } = "{0} of 255 points";
    public static string FriendshipLevel0 { get; set; } = "Upset with you";
    public static string FriendshipLevel1 { get; set; } = "Just met";
    public static string FriendshipLevel2 { get; set; } = "Acquaintances";
    public static string FriendshipLevel3 { get; set; } = "Friends";
    public static string FriendshipLevel4 { get; set; } = "Good friends";
    public static string FriendshipLevel5 { get; set; } = "Very good friends";
    public static string FriendshipLevel6 { get; set; } = "Close friends";
    public static string FriendshipLevel7 { get; set; } = "Best friends";
    public static string HeartTip { get; set; } = "{0} ({1} points or more)";
    public static string FromYourIsland { get; set; } = "Player of your island";
    public static string FromOtherIsland { get; set; } = "Visitor from {0}";
    public static string Nickname { get; set; } = "Calls you";
    public static string NicknameHint { get; set; } = "Leave it empty and they will use your name.";
    public static string BestFriendsAll { get; set; } = "Best friends with everyone";
    public static string NobodyMet { get; set; } = "{0} hasn't met any player yet.";
    public static string ClassicMemories { get; set; } = "Open the classic memory editor…";
    public static string ClassicMemoriesHint { get; set; } = "Everything {0} remembers about each player: greetings, visits, conversations…";

    // Walls & floor window
    public static string RoomTitle { get; set; } = "{0}'s walls & floor";
    public static string RoomHint { get; set; } = "Click the wall or the floor, then choose the new one in the list on the right.";
    public static string RoomWall { get; set; } = "Wall";
    public static string RoomFloor { get; set; } = "Floor";
    public static string RoomAccent { get; set; } = "Accent wall";
    public static string RoomAccentHint { get; set; } = "One of the walls, with a different wallpaper.";
    public static string RoomChangeWall { get; set; } = "Choose another wall";
    public static string RoomChangeFloor { get; set; } = "Choose another floor";
    public static string RoomChangeAccent { get; set; } = "Choose another accent wall";
    public static string RoomPickWallHint { get; set; } = "Click a wallpaper to put it up.";
    public static string RoomPickFloorHint { get; set; } = "Click a flooring to put it down.";
    public static string RoomSearch { get; set; } = "Search by name (wood, brick…)";
    public static string RoomShowAccent { get; set; } = "Show the accent wall";
    public static string RoomAdvancedHint { get; set; } = "The values of the room as the game stores them.";

    // House
    public static string HouseTitle { get; set; } = "{0}’s house";
    public static string HouseHint { get; set; } = "How {0}’s house looks from outside and what hangs on its door.";
    public static string HouseNotFound { get; set; } = "{0} has no house on the island yet.";
    public static string HouseOutside { get; set; } = "From outside";
    public static string DoorDecoration { get; set; } = "Door decoration";
    public static string DoorDecorationHint { get; set; } = "What hangs on the door: wreaths, seasonal decorations…";
    public static string NoDoorDecoration { get; set; } = "Nothing on the door.";
    public static string ChangeDoorDecoration { get; set; } = "Choose something to hang on the door:";
    public static string RemoveDoorDecoration { get; set; } = "Remove";
    public static string HouseAdvancedHint { get; set; } = "The values of the house as the game stores them.";
    public static string HouseWrongFile { get; set; } = "“{0}” is not a villager house file (.nhvh).";
}
