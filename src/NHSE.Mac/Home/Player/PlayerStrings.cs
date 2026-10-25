// ReSharper disable AutoPropertyCanBeMadeGetOnly.Global
// Generated from one table with the Spanish file (Resources/PlayerStrings_es.txt); keep both in step.
using System.Reflection;

namespace NHSE.Mac;

/// <summary>
/// Texts of the friendly player windows (recipes, reactions, records, catalog, profile, mailbox).
/// Translated with <c>Resources/PlayerStrings_xx.txt</c>; languages without a file use these English defaults.
/// </summary>
public static class PlayerStrings
{
    /// <summary>
    /// A text by property name (ReactionHappiness, RecordCatchFish…), or null if there is none.
    /// </summary>
    public static string? GetText(string name) => typeof(PlayerStrings).GetProperty(name, BindingFlags.Public | BindingFlags.Static)?.GetValue(null) as string;

    // Shared
    public static string NothingFound { get; set; } = "Nothing matches the search.";
    public static string ShowAll { get; set; } = "All";
    public static string Favorite { get; set; } = "Favorite";
    public static string NoDate { get; set; } = "No date";

    // Recipes
    public static string RecipesHint { get; set; } = "Tick «Learned» on the recipes the player should have in the DIY app of the phone.";
    public static string SearchRecipe { get; set; } = "Search a recipe (e.g. table, axe…)";
    public static string ShowLearned { get; set; } = "Learned";
    public static string ShowMissing { get; set; } = "Not learned yet";
    public static string Learned { get; set; } = "Learned";
    public static string Made { get; set; } = "Made";
    public static string MadeHint { get; set; } = "Already crafted at least once.";
    public static string StoryRecipe { get; set; } = "Part of the story: the game teaches it at the right moment.";
    public static string ForgetAll { get; set; } = "Forget all";
    public static string ForgetAllAsk { get; set; } = "Forget every recipe? The player will have to learn them again.";
    public static string MarkLearnedAsMade { get; set; } = "Mark the learned ones as made";

    // Reactions
    public static string ReactionsHint { get; set; } = "Tick the reactions the player knows, and choose the eight on the wheel.";
    public static string ReactionGroupFeelings { get; set; } = "Feelings";
    public static string ReactionGroupGestures { get; set; } = "Gestures and poses";
    public static string ReactionGroupMoves { get; set; } = "Exercises and dances";
    public static string Wheel { get; set; } = "Reaction wheel";
    public static string WheelHint { get; set; } = "The eight reactions you have at hand.";
    public static string WheelEmpty { get; set; } = "(empty)";
    public static string ReactionsFull { get; set; } = "There is no room for more reactions in this save.";

    // Reaction names
    public static string ReactionHappiness { get; set; } = "Happiness";
    public static string ReactionLaughter { get; set; } = "Laughter";
    public static string ReactionJoy { get; set; } = "Joy";
    public static string ReactionLove { get; set; } = "Love";
    public static string ReactionGlee { get; set; } = "Glee";
    public static string ReactionAggravation { get; set; } = "Aggravation";
    public static string ReactionWorry { get; set; } = "Worry";
    public static string ReactionSighing { get; set; } = "Sighing";
    public static string ReactionThought { get; set; } = "Thought";
    public static string ReactionSadness { get; set; } = "Sadness";
    public static string ReactionDistress { get; set; } = "Distress";
    public static string ReactionSorrow { get; set; } = "Sorrow";
    public static string ReactionAmazed { get; set; } = "Amazed";
    public static string ReactionSurprise { get; set; } = "Surprise";
    public static string ReactionShocked { get; set; } = "Shocked";
    public static string ReactionCold_Chill { get; set; } = "Cold chill";
    public static string ReactionFearful { get; set; } = "Fearful";
    public static string ReactionAgreement { get; set; } = "Agreement";
    public static string ReactionInspiration { get; set; } = "Inspiration";
    public static string ReactionCuriosity { get; set; } = "Curiosity";
    public static string ReactionHeartbreak { get; set; } = "Heartbreak";
    public static string ReactionSleepy { get; set; } = "Sleepy";
    public static string ReactionBashfulness { get; set; } = "Bashfulness";
    public static string ReactionResignation { get; set; } = "Resignation";
    public static string ReactionMischief { get; set; } = "Mischief";
    public static string ReactionDelight { get; set; } = "Delight";
    public static string ReactionSneezing { get; set; } = "Sneezing";
    public static string ReactionEncouraging { get; set; } = "Encouraging";
    public static string ReactionGreetings { get; set; } = "Greetings";
    public static string ReactionPride { get; set; } = "Pride";
    public static string ReactionSmirking { get; set; } = "Smirking";
    public static string ReactionSheepishness { get; set; } = "Sheepishness";
    public static string ReactionShyness { get; set; } = "Shyness";
    public static string ReactionDisagreement { get; set; } = "Disagreement";
    public static string ReactionMistaken { get; set; } = "Mistaken";
    public static string ReactionFlourish { get; set; } = "Flourish";
    public static string ReactionDaydreaming { get; set; } = "Daydreaming";
    public static string ReactionShowmanship { get; set; } = "Showmanship";
    public static string ReactionDozing { get; set; } = "Dozing";
    public static string ReactionIntense { get; set; } = "Intense";
    public static string ReactionPleased { get; set; } = "Pleased";
    public static string ReactionApologetic { get; set; } = "Apologetic";
    public static string ReactionConfident { get; set; } = "Confident";
    public static string ReactionBewilderment { get; set; } = "Bewilderment";
    public static string ReactionScare { get; set; } = "Scare";
    public static string ReactionHaunt { get; set; } = "Haunt";
    public static string ReactionSitDown { get; set; } = "Sit down";
    public static string ReactionYoga { get; set; } = "Yoga";
    public static string ReactionHereYouGo { get; set; } = "Here you go";
    public static string ReactionWorkOut { get; set; } = "Work out";
    public static string ReactionTakeAPicture { get; set; } = "Take a picture";
    public static string ReactionSniffSniff { get; set; } = "Sniff sniff";
    public static string ReactionTada { get; set; } = "Ta-da!";
    public static string ReactionWaveGoodbye { get; set; } = "Wave goodbye";
    public static string ReactionExcited { get; set; } = "Excited";
    public static string ReactionConfetti { get; set; } = "Confetti";
    public static string ReactionViva { get; set; } = "Viva";
    public static string ReactionLetsGo { get; set; } = "Let's go";
    public static string ReactionFeelinIt { get; set; } = "Feelin' it";
    public static string ReactionSayCheese { get; set; } = "Say cheese";
    public static string ReactionEager { get; set; } = "Eager";
    public static string ReactionPostureWarmUp { get; set; } = "Posture warm-up";
    public static string ReactionArmCircles { get; set; } = "Arm circles";
    public static string ReactionSideBends { get; set; } = "Side bends";
    public static string ReactionBodyTwists { get; set; } = "Body twists";
    public static string ReactionWideArmStretch { get; set; } = "Wide arm stretch";
    public static string ReactionUpperBodyCircles { get; set; } = "Upper-body circles";
    public static string ReactionJump { get; set; } = "Jump";
    public static string ReactionHula { get; set; } = "Hula";
    public static string ReactionDoubleWave { get; set; } = "Double wave";
    public static string ReactionListeningEars { get; set; } = "Listening ears";
    public static string ReactionFlex { get; set; } = "Flex";
    public static string ReactionJammin { get; set; } = "Jammin'";
    public static string ReactionStretch { get; set; } = "Stretch";
    public static string ReactionBehold { get; set; } = "Behold";
    public static string ReactionWorkIt { get; set; } = "Work it";
    public static string ReactionActNatural { get; set; } = "Act natural";
    public static string ReactionGroovingHop { get; set; } = "Grooving hop";
    public static string ReactionGrooveRight { get; set; } = "Groove right";
    public static string ReactionGrooveLeft { get; set; } = "Groove left";
    public static string ReactionSoakItIn { get; set; } = "Soak it in";
    public static string ReactionShimmy { get; set; } = "Shimmy";
    public static string ReactionAirplane { get; set; } = "Airplane";
    public static string ReactionTwistyDance { get; set; } = "Twisty dance";
    public static string ReactionArmSwingDance { get; set; } = "Arm-swing dance";
    public static string ReactionIslandStomp { get; set; } = "Island stomp";
    public static string ReactionSideToSide { get; set; } = "Side-to-side";
    public static string ReactionTurnipPatch { get; set; } = "Turnip patch";

    // Nook Miles+ records
    public static string RecordsHint { get; set; } = "Choose a record to see how far you are and the stamps it gives. «Max» completes it.";
    public static string SearchRecord { get; set; } = "Search a record (e.g. fish, turnips…)";
    public static string ShowInProgress { get; set; } = "In progress";
    public static string ShowCompleted { get; set; } = "Completed";
    public static string RecordsDone { get; set; } = "{0} of {1} records completed";
    public static string NextStamp { get; set; } = "{0} → next stamp at {1}";
    public static string AllStamps { get; set; } = "{0} · every stamp earned";
    public static string StepsDone { get; set; } = "{0} of {1}";
    public static string Stamps { get; set; } = "Stamps";
    public static string StampAt { get; set; } = "Stamp at {0}";
    public static string StampStep { get; set; } = "Stamp {0}";
    public static string StampEarnedOn { get; set; } = "earned on {0}";
    public static string RecordStepsHint { get; set; } = "Each stamp is one thing to do. Click a stamp to mark it.";
    public static string ResetRecord { get; set; } = "Start this record again";
    public static string StampDates { get; set; } = "Dates of the stamps";
    public static string StampDatesHint { get; set; } = "The day each stamp was earned. You can leave them as they are.";
    public static string CompleteAll { get; set; } = "Complete all";
    public static string CompleteAllAsk { get; set; } = "Complete every record and earn all its stamps?";
    public static string ResetAll { get; set; } = "Start every record again";
    public static string ResetAllAsk { get; set; } = "Set every record back to zero and remove all stamps?";

    // Record names
    public static string RecordCatchFish { get; set; } = "Fish caught";
    public static string RecordCatchInsect { get; set; } = "Bugs caught";
    public static string RecordCatchFishContinuously { get; set; } = "Best fishing streak";
    public static string RecordFillFishList { get; set; } = "Kinds of fish found";
    public static string RecordFillInsectList { get; set; } = "Kinds of bugs found";
    public static string RecordShootDownBalloon { get; set; } = "Balloons popped";
    public static string RecordPlantFlowerSeed { get; set; } = "Flower seeds planted";
    public static string RecordPlantFruit { get; set; } = "Kinds of fruit planted";
    public static string RecordPlantTreeSeedling { get; set; } = "Saplings planted";
    public static string RecordBuyKabu { get; set; } = "Bought turnips";
    public static string RecordHowmuchSellKabu { get; set; } = "Bells earned with turnips";
    public static string RecordHowmuchBuyItem { get; set; } = "Bells spent shopping";
    public static string RecordBuyItemRcm { get; set; } = "Bought at Timmy and Tommy's shop";
    public static string RecordSellItemRcm { get; set; } = "Sold at Timmy and Tommy's shop";
    public static string RecordRemakeFurniture { get; set; } = "Items customized";
    public static string RecordFillCatalog { get; set; } = "Items in the catalog";
    public static string RecordBuyItemCatalog { get; set; } = "Nook Shopping orders";
    public static string RecordGotoTotakekeShow { get; set; } = "K.K. concerts watched";
    public static string RecordVisitAnotherIsland { get; set; } = "Visits to other islands";
    public static string RecordInviteFriend { get; set; } = "Friends invited to your island";
    public static string RecordDayPlayed { get; set; } = "Days played";
    public static string RecordWaterPlant { get; set; } = "Times watering";
    public static string RecordBeStungbyWasp { get; set; } = "Stung by wasps";
    public static string RecordCatchSeminonukegara { get; set; } = "Found a cicada shell";
    public static string RecordCatchNomi { get; set; } = "Fleas caught on villagers";
    public static string RecordBeStungbyPoisonousInsect { get; set; } = "Fainted from a scary bug";
    public static string RecordFillReactionList { get; set; } = "Reactions learned";
    public static string RecordCatchTrash { get; set; } = "Trash fished up";
    public static string RecordPayImmigrationCost { get; set; } = "Paid the moving fee";
    public static string RecordRepayLoan { get; set; } = "Paid off a loan";
    public static string RecordUseMydesign { get; set; } = "Used a custom design";
    public static string RecordUseMydesignPRO { get; set; } = "Used a PRO design";
    public static string RecordSellWeed { get; set; } = "Weeds sold";
    public static string RecordCollectGoldTool { get; set; } = "Golden tools collected";
    public static string RecordGetSRankonHHA { get; set; } = "S rank for your home";
    public static string RecordAttendFishingConvention { get; set; } = "Fishing tourneys (one per season)";
    public static string RecordAttendInsectConvention { get; set; } = "Bug-Offs (June to September)";
    public static string RecordHelpGul { get; set; } = "Times you helped Gulliver";
    public static string RecordHelpGst { get; set; } = "Times you helped Wisp";
    public static string RecordMakePerfectSnowball { get; set; } = "Perfect snowmen made";
    public static string RecordReachMyBirthday { get; set; } = "Celebrated your birthday";
    public static string RecordCelebrateVillagersBithday { get; set; } = "Villager birthdays celebrated";
    public static string RecordAttendCountdownParty { get; set; } = "New Year's countdown";
    public static string RecordBreakTool { get; set; } = "Tools broken";
    public static string RecordSendLetter { get; set; } = "Letters sent";
    public static string RecordMakePitfall { get; set; } = "Set a pitfall";
    public static string RecordFallintoPitfall { get; set; } = "Fell into a pitfall";
    public static string RecordImmigratetoIsland { get; set; } = "Moved to the island";
    public static string RecordBeFriendwithVillager { get; set; } = "Villagers who are close friends";
    public static string RecordFillRecipeList { get; set; } = "DIY recipes collected";
    public static string RecordBootPhone { get; set; } = "Times you opened the NookPhone";
    public static string RecordStrikeRock8Times { get; set; } = "Hit a money rock 8 times";
    public static string RecordAchieveAppQuest { get; set; } = "Nook Miles+ tasks done";
    public static string RecordAchieveVillagersQuest { get; set; } = "Favors done for villagers";
    public static string RecordDigBell { get; set; } = "Dug up Bells";
    public static string RecordDigFossil { get; set; } = "Dug up a fossil";
    public static string RecordJudgeFossil { get; set; } = "Fossils assessed";
    public static string RecordDigShell { get; set; } = "Clams dug up";
    public static string RecordPutFurnitureOutside { get; set; } = "Furniture placed outdoors";
    public static string RecordStrikeWood { get; set; } = "Wood chopped from trees";
    public static string RecordGreetAllVillager { get; set; } = "Days you greeted every villager";
    public static string RecordSellShell { get; set; } = "Shells sold";
    public static string RecordSellFruit { get; set; } = "Fruit sold";
    public static string RecordCatchBeeContinuously { get; set; } = "Caught 5 wasps in a row";
    public static string RecordDIYTool { get; set; } = "Tools crafted";
    public static string RecordDIYFurniture { get; set; } = "Furniture crafted";
    public static string RecordTakePicture { get; set; } = "Took a photo";
    public static string RecordBootPhoneBeginner { get; set; } = "Opened the NookPhone (first times)";
    public static string RecordHouseStorageItem { get; set; } = "Items in home storage";
    public static string RecordPlaceFurnitureMyHouse { get; set; } = "Furniture displayed at home";
    public static string RecordFallFurnitureLeaf { get; set; } = "Furniture shaken from trees";
    public static string RecordDropPresentinWater { get; set; } = "Balloon present fell in the water";
    public static string RecordReformMyHome { get; set; } = "Home makeovers";
    public static string RecordExtendMyHome { get; set; } = "Home expansions";
    public static string RecordPostMessageBoard { get; set; } = "Wrote on the bulletin board";
    public static string RecordUseCloset { get; set; } = "Changed clothes in the wardrobe";
    public static string RecordPrayShootingStar { get; set; } = "Wishes on shooting stars";
    public static string RecordChangeSymbol { get; set; } = "Changed the island flag and tune";
    public static string RecordUpdatePassport { get; set; } = "Updated your passport";
    public static string RecordModifyIsland { get; set; } = "Island designing tried (paths, water, cliffs)";
    public static string RecordBuildFence { get; set; } = "Fences placed";
    public static string RecordDonateFake { get; set; } = "Tried to donate a fake";
    public static string RecordBuyatTsunekichiShop { get; set; } = "Art bought from Redd";
    public static string RecordPlantBushSeedling { get; set; } = "Bush starts planted";
    public static string RecordFillSeafoodList { get; set; } = "Kinds of sea creatures found";
    public static string RecordGiveScallopsToSeo { get; set; } = "Scallops given to Pascal";
    public static string RecordCatchSeafood { get; set; } = "Sea creatures caught";
    public static string RecordPlantVegetableSeedling { get; set; } = "Vegetable starts planted";
    public static string RecordHarvestVegetable { get; set; } = "Vegetables harvested";
    public static string RecordVisitKappeiTour { get; set; } = "Boat tours with Kapp'n";
    public static string RecordFillGyroidList { get; set; } = "Gyroids in the catalog";
    public static string RecordFillCookingList { get; set; } = "Cooking recipes learned";
    public static string RecordDigGyroid { get; set; } = "Dug up a gyroid";
    public static string RecordCookFood { get; set; } = "Dishes cooked";
    public static string RecordDrinkCoffee { get; set; } = "Coffees at the café";
    public static string RecordJoinRadioGymnastics { get; set; } = "Group stretching sessions";
    public static string RecordDIYTrade { get; set; } = "DIY trades";
    public static string RecordGuideNoRoomNPC { get; set; } = "Visitors taken to the hotel";
    public static string RecordCoordinateHotelRoom { get; set; } = "Hotel rooms decorated";

    // Catalog
    public static string CatalogHint { get; set; } = "Items marked as received can be ordered again from Nook Shopping. Marking an item marks all its colors.";
    public static string CatalogCount { get; set; } = "{0} of {1} received";
    public static string Received { get; set; } = "Received";
    public static string MarkShown { get; set; } = "Mark all";
    public static string UnmarkShown { get; set; } = "Unmark all";
    public static string UnmarkShownAsk { get; set; } = "Unmark the {0} items in the list?";

    // Profile
    public static string ProfileHint { get; set; } = "What your passport shows.";
    public static string IslandName { get; set; } = "Island {0}";
    public static string Birthday { get; set; } = "Birthday";
    public static string BirthdayHint { get; set; } = "Villagers celebrate it with you that day.";
    public static string BirthdayPattern { get; set; } = "{m} {d}";
    public static string PassportFruit { get; set; } = "Favorite fruit";
    public static string PassportFruitHint { get; set; } = "The fruit shown on your passport.";
    public static string Registered { get; set; } = "Registration date";
    public static string RegisteredHint { get; set; } = "The day you got your passport.";
    public static string Founder { get; set; } = "Founded the island";
    public static string FounderHint { get; set; } = "Marks the first player of the island (the resident representative) on the passport. Leave it as it is unless you know you need it.";

    // Mailbox
    public static string MailHint { get; set; } = "Choose a letter to read it or change it. You can attach a gift or write a new letter.";
    public static string MailEmpty { get; set; } = "There are no letters in the mailbox.";
    public static string MailEmptyHint { get; set; } = "The mailbox is empty. Press «Write a letter» to add one.";
    public static string ChooseLetter { get; set; } = "Choose a letter on the left.";
    public static string NewLetter { get; set; } = "Write a letter";
    public static string MailFull { get; set; } = "The mailbox is full. Delete a letter first.";
    public static string From { get; set; } = "From {0}";
    public static string ChangeSender { get; set; } = "Change sender";
    public static string SenderVillager { get; set; } = "A villager…";
    public static string SenderCharacter { get; set; } = "A special character";
    public static string SenderMe { get; set; } = "{0} (you)";
    public static string SenderMom { get; set; } = "Mom";
    public static string SomePlayer { get; set; } = "A player";
    public static string SomeoneElse { get; set; } = "Someone";
    public static string Unread { get; set; } = "Not read yet";
    public static string LetterTo { get; set; } = "To";
    public static string LetterMessage { get; set; } = "Message";
    public static string LetterSignature { get; set; } = "Signature";
    public static string Gift { get; set; } = "Gift";
    public static string NoGift { get; set; } = "No gift";
    public static string AddGift { get; set; } = "Add a gift";
    public static string ChangeGift { get; set; } = "Choose another gift";
    public static string RemoveGift { get; set; } = "Remove";
    public static string Read { get; set; } = "Read";
    public static string LetterDate { get; set; } = "Date";
    public static string DeleteLetter { get; set; } = "Delete this letter";
    public static string DeleteLetterAsk { get; set; } = "Delete the letter from {0}?";
    public static string Kind { get; set; } = "Kind";
    public static string KindLetter { get; set; } = "Letter";
    public static string KindParcel { get; set; } = "Parcel";
    public static string Paper { get; set; } = "Writing paper";
    public static string Wrapping { get; set; } = "Gift wrapping";
    public static string BoxKind { get; set; } = "Box";
    public static string ReceiverName { get; set; } = "Addressed to";
    public static string ReceiverIsland { get; set; } = "Island";
    public static string ExportLetter { get; set; } = "Export this letter…";
    public static string ExportAllLetters { get; set; } = "Export every letter…";
    public static string ImportLetter { get; set; } = "Import a letter…";
    public static string ImportLetterWrong { get; set; } = "That file is not a letter exported by NHSE.";
}
