// ReSharper disable AutoPropertyCanBeMadeGetOnly.Global
namespace NHSE.Mac;

/// <summary>
/// Texts of the friendly island map and house windows (plain language, no save-format jargon).
/// Translated with <c>Resources/MapStrings_xx.txt</c>; languages without a file use these English defaults.
/// </summary>
public static class MapStrings
{
    // Island map window
    public static string MapTitle { get; set; } = "Island map";
    public static string Painting { get; set; } = "Drawing the island…";
    public static string MapHint { get; set; } = "Click a spot to see what is there and change it. Drag to move around; ⌘ + scroll or the buttons to zoom.";
    public static string ZoomIn { get; set; } = "Zoom in";
    public static string ZoomOut { get; set; } = "Zoom out";
    public static string ZoomFit { get; set; } = "See the whole island";
    public static string ShowGrid { get; set; } = "Grid";
    public static string ShowGridHint { get; set; } = "Shows a line around each square";
    public static string LayerGround { get; set; } = "On the ground";
    public static string LayerGroundHint { get; set; } = "Trees, flowers, fences, furniture and everything standing on the ground.";
    public static string LayerTop { get; set; } = "On top of things";
    public static string LayerTopHint { get; set; } = "Things placed on top of other furniture, like whatever sits on an outdoor table.";
    public static string Cancel { get; set; } = "Cancel";
    public static string Save { get; set; } = "Save";
    public static string NavigationHint { get; set; } = "Drag to move · ⌘ + scroll to zoom · Delete removes what is selected";

    // Side panel
    public static string NothingSelected { get; set; } = "Choose a spot";
    public static string NothingSelectedHint { get; set; } = "Click anywhere on the map to see what is there. You can put something new on an empty spot, or change or remove what is there.";
    public static string OnYourIsland { get; set; } = "On your island";
    public static string CountTrees { get; set; } = "Trees";
    public static string CountFlowers { get; set; } = "Flowers";
    public static string CountFurniture { get; set; } = "Furniture and items";
    public static string CountFences { get; set; } = "Fences";
    public static string CountWeeds { get; set; } = "Weeds";
    public static string EmptySpace { get; set; } = "Empty spot";
    public static string EmptySpaceHint { get; set; } = "There is nothing here. Choose something to put here.";
    public static string Remove { get; set; } = "Remove";
    public static string Rotate { get; set; } = "Turn";
    public static string RotateHint { get; set; } = "Turns it a quarter to the right";
    public static string PutHere { get; set; } = "Put here";
    public static string ChangeFor { get; set; } = "Change it for";
    public static string KindItems { get; set; } = "Items";
    public static string KindPlants { get; set; } = "Plants, fences and rocks";
    public static string SearchPlant { get; set; } = "Search…";
    public static string AdvancedOptions { get; set; } = "Advanced options";
    public static string Apply { get; set; } = "Apply";
    public static string Position { get; set; } = "Position: {0}, {1}";
    public static string WhereGround { get; set; } = "On the ground";
    public static string WhereTop { get; set; } = "On top of other things";
    public static string Buried { get; set; } = "Buried in the ground";
    public static string DoesNotFit { get; set; } = "It doesn't fit here: {0} is in the way.";
    public static string OutOfMap { get; set; } = "It doesn't fit here: it would stick out of the edge.";
    public static string AskUnsupported { get; set; } = "Some things are on top of nothing (there is no furniture under them). Save anyway?";
    public static string BuildingHere { get; set; } = "This spot belongs to a building. Buildings are moved in the game, at Resident Services.";

    // Plants and other things on the ground
    public static string Tree { get; set; } = "Tree";
    public static string Stump { get; set; } = "Tree stump";
    public static string Rock { get; set; } = "Rock";
    public static string Hole { get; set; } = "Hole";
    public static string Weeds { get; set; } = "Weeds";
    public static string Flower { get; set; } = "Flower";
    public static string Bush { get; set; } = "Bush";
    public static string Fence { get; set; } = "Fence";
    public static string Ladder { get; set; } = "Ladder";
    public static string Plant { get; set; } = "Plant";

    // Buildings
    public static string ResidentServices { get; set; } = "Resident Services";
    public static string NooksCranny { get; set; } = "Nook's Cranny";
    public static string Museum { get; set; } = "Museum";
    public static string Airport { get; set; } = "Airport";
    public static string AbleSisters { get; set; } = "Able Sisters";
    public static string Campsite { get; set; } = "Campsite";
    public static string Bridge { get; set; } = "Bridge";
    public static string Incline { get; set; } = "Incline";
    public static string ReddsBoat { get; set; } = "Redd's boat";
    public static string Studio { get; set; } = "Photo studio";
    public static string Hotel { get; set; } = "Hotel";
    public static string Plaza { get; set; } = "Plaza";
    public static string HouseOf { get; set; } = "{0}'s house";
    public static string EmptyLot { get; set; } = "Empty lot";
    public static string Building { get; set; } = "Building";

    // House window
    public static string HouseTitle { get; set; } = "Houses";
    public static string HouseHint { get; set; } = "Choose a room and click a spot to see what is there, change it, turn it or remove it.";
    public static string RoomMain { get; set; } = "Main room";
    public static string RoomBack { get; set; } = "Back room";
    public static string RoomLeft { get; set; } = "Left room";
    public static string RoomRight { get; set; } = "Right room";
    public static string RoomUpstairs { get; set; } = "Upstairs";
    public static string RoomBasement { get; set; } = "Basement";
    public static string EditFloor { get; set; } = "Furniture";
    public static string EditFloorHint { get; set; } = "Everything standing on the floor.";
    public static string EditTables { get; set; } = "On tables";
    public static string EditTablesHint { get; set; } = "Small things placed on top of furniture, like a lamp on a table.";
    public static string EditRugs { get; set; } = "Rugs";
    public static string EditRugsHint { get; set; } = "Rugs lie under the furniture.";
    public static string EditWalls { get; set; } = "Walls";
    public static string EditWallsHint { get; set; } = "Things hanging on the walls. Choose a wall with the buttons above.";
    public static string EditCeiling { get; set; } = "Ceiling";
    public static string EditCeilingHint { get; set; } = "Lamps and things hanging from the ceiling.";
    public static string WallBack { get; set; } = "Back wall";
    public static string WallLeft { get; set; } = "Left wall";
    public static string WallFront { get; set; } = "Front wall";
    public static string WallRight { get; set; } = "Right wall";
    public static string WhereFloor { get; set; } = "On the floor";
    public static string WhereTable { get; set; } = "On top of furniture";
    public static string WhereRug { get; set; } = "Rug";
    public static string WhereWall { get; set; } = "On the wall";
    public static string WhereCeiling { get; set; } = "Hanging from the ceiling";
    public static string HouseSection { get; set; } = "The house";
    public static string HouseSize { get; set; } = "Size";
    public static string Roof { get; set; } = "Roof";
    public static string Walls { get; set; } = "Walls";
    public static string Door { get; set; } = "Door";
    public static string RoomSection { get; set; } = "This room";
    public static string Wallpaper { get; set; } = "Wallpaper";
    public static string Flooring { get; set; } = "Flooring";
    public static string Change { get; set; } = "Change";
    public static string ChooseWallpaper { get; set; } = "Choose a wallpaper or flooring";
    public static string RoomPickHint { get; set; } = "Click anything in the room to change it, turn it or remove it, or an empty spot to put something there. Choose above what to work on: furniture, what is on the tables, rugs, walls or ceiling.";
    public static string OutOfRoom { get; set; } = "It doesn't fit here: it would stick out of the room.";
    public static string LevelTent { get; set; } = "Tent";
    public static string LevelHouse { get; set; } = "House";
    public static string LevelBigger { get; set; } = "Bigger main room";
    public static string LevelBack { get; set; } = "With back room";
    public static string LevelLeft { get; set; } = "With left room";
    public static string LevelRight { get; set; } = "With right room";
    public static string LevelUpstairs { get; set; } = "With upstairs";
    public static string LevelBasement { get; set; } = "With basement";
    public static string EditFlags { get; set; } = "House events…";
    public static string ExportHouse { get; set; } = "Export house…";
    public static string ImportHouse { get; set; } = "Import house…";
    public static string ExportRoom { get; set; } = "Export room…";
    public static string ImportRoom { get; set; } = "Import room…";

    // Roof, wall and door looks
    public static string StyleStandard { get; set; } = "Classic";
    public static string StyleThatched { get; set; } = "Thatched";
    public static string StyleStone { get; set; } = "Stone";
    public static string StyleWesternTile { get; set; } = "Western tiles";
    public static string StyleSlate { get; set; } = "Slate";
    public static string StyleMetal { get; set; } = "Metal";
    public static string StyleTile { get; set; } = "Tiles";
    public static string StyleWoodTile { get; set; } = "Wood shingles";
    public static string StyleWoodPanel { get; set; } = "Wood panels";
    public static string StyleJapanese { get; set; } = "Japanese";
    public static string StyleStoneWeight { get; set; } = "Weighted with stones";
    public static string StyleWood { get; set; } = "Wood";
    public static string StyleWoodFrame { get; set; } = "Timber frame";
    public static string StyleWoodSiding { get; set; } = "Wood siding";
    public static string StyleBrick { get; set; } = "Brick";
    public static string StyleStucco { get; set; } = "Stucco";
    public static string StyleLog { get; set; } = "Log";
    public static string StyleOriental { get; set; } = "Oriental";
    public static string StyleSoil { get; set; } = "Adobe";
    public static string StyleTent { get; set; } = "Tent";
    public static string StyleForSale { get; set; } = "For sale";
    public static string StyleSoldOut { get; set; } = "Sold";
    public static string StyleMoving { get; set; } = "Moving";
    public static string StyleIron { get; set; } = "Iron";
    public static string StyleIronParts { get; set; } = "Iron fittings";
    public static string StyleRelief { get; set; } = "Carved relief";
    public static string StyleWindow { get; set; } = "With window";
    public static string StyleIronGrill { get; set; } = "Iron grille";
    public static string StyleChinese { get; set; } = "Chinese";
    public static string StyleSimple { get; set; } = "Simple";
    public static string StyleCarving { get; set; } = "Carved";
    public static string StyleRoundWindow { get; set; } = "Round window";
    public static string StyleTallWindow { get; set; } = "Tall window";
    public static string StyleLattice { get; set; } = "Lattice";
    public static string StylePlain { get; set; } = "Plain";
    public static string StyleOther { get; set; } = "Other";
    public static string ColorNumber { get; set; } = "color {0}";
    public static string DoorRound { get; set; } = "arched";
}
