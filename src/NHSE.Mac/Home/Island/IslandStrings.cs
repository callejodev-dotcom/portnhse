// ReSharper disable AutoPropertyCanBeMadeGetOnly.Global
namespace NHSE.Mac;

/// <summary>
/// Texts of the friendly island and museum windows (turnips, visitors, campsite, fruit and flowers, museum).
/// Translated with <c>Resources/IslandStrings_xx.txt</c>; languages without a file use these English defaults.
/// </summary>
public static class IslandStrings
{
    // Shared
    public static string Nobody { get; set; } = "Nobody";
    public static string Bells { get; set; } = "{0} Bells";
    public static string DayMorning { get; set; } = "{0} morning";
    public static string DayAfternoon { get; set; } = "{0} afternoon";
    public static string Morning { get; set; } = "Morning";
    public static string Afternoon { get; set; } = "Afternoon";

    // Island page
    public static string AirportBlue { get; set; } = "Blue";
    public static string AirportYellow { get; set; } = "Yellow";
    public static string AirportOrange { get; set; } = "Orange";
    public static string AirportGreen { get; set; } = "Green";
    public static string TurnipsSummary { get; set; } = "Daisy Mae: {0} Bells · {1}";
    public static string CampsiteSummaryLocked { get; set; } = "You don't have one yet";
    public static string CampsiteSummaryEmpty { get; set; } = "Nobody is camping";
    public static string CampsiteSummary { get; set; } = "{0} is camping";
    public static string VisitorsSummaryNone { get; set; } = "Nobody this week";
    public static string MuseumSummary { get; set; } = "{0} of {1} pieces donated";

    // Turnips
    public static string TurnipsTitle { get; set; } = "Turnip prices";
    public static string TurnipsHint { get; set; } = "What Daisy Mae charges on Sunday and what the shop pays for them each day of this week.";
    public static string BuyTitle { get; set; } = "Daisy Mae's price";
    public static string BuyHint { get; set; } = "What a turnip costs on Sunday morning. In the game it is between 90 and 110 Bells.";
    public static string WeekTitle { get; set; } = "Selling prices this week";
    public static string WeekHint { get; set; } = "What Timmy and Tommy pay for each turnip. The morning price lasts until noon. On Sundays they don't buy turnips.";
    public static string BestPrice { get; set; } = "Best price this week: {0}, {1} Bells.";
    public static string ChartLegend { get; set; } = "Green bars: you earn more than you paid Daisy Mae.";
    public static string AllPrices { get; set; } = "Set every price to";
    public static string PatternTitle { get; set; } = "This week's trend";
    public static string PatternHint { get; set; } = "The game picks it on Sunday. Changing it doesn't change the prices above: the game uses it to choose next week's trend.";
    public static string PatternFluctuating { get; set; } = "Fluctuating";
    public static string PatternFluctuatingHint { get; set; } = "Goes up and down all week, sometimes a little above what you paid.";
    public static string PatternLargeSpike { get; set; } = "Large spike";
    public static string PatternLargeSpikeHint { get; set; } = "Falls for a few days and then shoots up: up to six times what you paid.";
    public static string PatternDecreasing { get; set; } = "Decreasing";
    public static string PatternDecreasingHint { get; set; } = "Goes down all week. Better to sell them on another island.";
    public static string PatternSmallSpike { get; set; } = "Small spike";
    public static string PatternSmallSpikeHint { get; set; } = "Falls and then rises a little: up to about twice what you paid.";
    public static string SpikeStart { get; set; } = "The spike starts on";

    // Visitors
    public static string VisitorsTitle { get; set; } = "Special visitors";
    public static string VisitorsHint { get; set; } = "Who comes to your island each day of this week.";
    public static string ThisWeek { get; set; } = "This week";
    public static string SundayNote { get; set; } = "Daisy Mae comes on Sunday mornings.";
    public static string SaturdayNote { get; set; } = "K.K. plays on Saturday nights.";
    public static string ConcertsTitle { get; set; } = "K.K.'s concerts";
    public static string ConcertsHint { get; set; } = "The days the game has a concert in the plaza noted down, a normal one or a birthday one.";
    public static string ConcertNormal { get; set; } = "Concert";
    public static string ConcertBirthday { get; set; } = "Birthday";
    public static string VisitorsAdvancedHint { get; set; } = "Everything the game keeps about this week's visitors, as it is in the save.";

    // Campsite
    public static string CampsiteTitle { get; set; } = "Campsite";
    public static string CampsiteHint { get; set; } = "Choose who is staying at the campsite.";
    public static string CampsiteLocked { get; set; } = "You don't have a campsite yet";
    public static string CampsiteLockedHint { get; set; } = "When Tom Nook gives it to you and you place it on the island, you can choose here who stays in it.";
    public static string Camping { get; set; } = "Is staying at the campsite.";
    public static string NotCamping { get; set; } = "Nobody is staying at the campsite.";
    public static string ChooseCamper { get; set; } = "Choose camper…";
    public static string NoCamper { get; set; } = "Leave it empty";
    public static string CampDate { get; set; } = "Day of the visit";
    public static string CampDateHint { get; set; } = "Campers only stay for a day: use today's date to find them when you open the game.";
    public static string AlreadyResident { get; set; } = "{0} already lives on your island.";

    // Fruit and flowers
    public static string FruitTitle { get; set; } = "Your island's fruit and flowers";
    public static string FruitHint { get; set; } = "The fruit and flowers that grow on their own on your island.";
    public static string Fruit { get; set; } = "Fruit";
    public static string Flowers { get; set; } = "Flowers";
    public static string NativeFruit { get; set; } = "Your island's fruit";
    public static string NativeFruitHint { get; set; } = "The one the trees had when you arrived. It is also on your passport.";
    public static string SecondFruit { get; set; } = "Second fruit";
    public static string SecondFruitHint { get; set; } = "The one on mystery islands (Nook Miles Ticket) and in Mom's letters.";
    public static string NativeFlower { get; set; } = "Your island's flower";
    public static string NativeFlowerHint { get; set; } = "The one that grows on its own on your island.";
    public static string SecondFlower { get; set; } = "Second flower";
    public static string SecondFlowerHint { get; set; } = "The other flower of your island.";
    public static string FruitAdvancedHint { get; set; } = "Only to use a fruit that is not in the pictures: its item number.";
    public static string NativeFruitNumber { get; set; } = "Your island's fruit";
    public static string SecondFruitNumber { get; set; } = "Second fruit";
    public static string FlowerWindflower { get; set; } = "Windflowers";
    public static string FlowerHyacinth { get; set; } = "Hyacinths";
    public static string FlowerTulip { get; set; } = "Tulips";
    public static string FlowerRose { get; set; } = "Roses";
    public static string FlowerCosmos { get; set; } = "Cosmos";
    public static string FlowerLily { get; set; } = "Lilies";
    public static string FlowerMum { get; set; } = "Mums";
    public static string FlowerPansy { get; set; } = "Pansies";

    // Museum
    public static string MuseumTitle { get; set; } = "Museum";
    public static string MuseumHint { get; set; } = "Tick what is already on display. Unticking a piece takes it out of the museum.";
    public static string FishCount { get; set; } = "{0} of {1} fish donated";
    public static string BugsCount { get; set; } = "{0} of {1} bugs donated";
    public static string SeaCount { get; set; } = "{0} of {1} sea creatures donated";
    public static string FossilsCount { get; set; } = "{0} of {1} fossils donated";
    public static string ArtCount { get; set; } = "{0} of {1} works of art donated";
    public static string DonateAll { get; set; } = "Donate all";
    public static string DonateEverything { get; set; } = "Donate everything…";
    public static string DonateEverythingAsk { get; set; } = "Donate every piece still missing from the museum?";
    public static string DonateEverythingAsk2 { get; set; } = "{0} pieces will be donated today by {1}.";
    public static string DonatedOnBy { get; set; } = "Donated on {0} by {1}";
    public static string DonatedOn { get; set; } = "Donated on {0}";
    public static string DonatedBy { get; set; } = "Donated by {0}";
    public static string Donated { get; set; } = "Donated";
    public static string NotDonated { get; set; } = "Not donated yet";
    public static string FilterAll { get; set; } = "All";
    public static string FilterMissing { get; set; } = "Missing";
    public static string FilterDonated { get; set; } = "Donated";
    public static string SearchPiece { get; set; } = "Search by name";
    public static string NothingFound { get; set; } = "Nothing here.";
    public static string ClassicMuseum { get; set; } = "Dates and more…";
    public static string ClassicMuseumHint { get; set; } = "The classic editor: change the date and who donated each piece, export or import the museum.";

    // Bulletin board
    public static string BulletinHint { get; set; } = "The notes pinned on the plaza board, newest first. You can change what they say.";
    public static string BulletinEmpty { get; set; } = "There are no notes on the board.";
    public static string BulletinResidentServices { get; set; } = "Resident Services";
    public static string BulletinHandwritten { get; set; } = "A note handwritten by {0}. Handwriting can only be seen in the game.";
    public static string BulletinSignature { get; set; } = "Signed";
    public static string BulletinSummary { get; set; } = "{0} notes · the latest from {1}";
    public static string BulletinSummaryEmpty { get; set; } = "No notes";
}
