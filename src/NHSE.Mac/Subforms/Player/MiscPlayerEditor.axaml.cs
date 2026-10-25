using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class MiscPlayerEditor : NhseWindow
{
    private readonly Player Player;
    private readonly MainSave Save;

    public MiscPlayerEditor(Player p, MainSave s)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);
        Player = p;
        Save = s;

        var profileFruit = ComboItemUtil.GetArray(GameLists.Fruits, GameInfo.Strings.itemlistdisplay);
        RIS_ProfileFruit.Initialize(profileFruit);

        LoadPlayer();
    }

    private void LoadPlayer()
    {
        var p = Player;
        var pers = p.Personal;

        _ = Save;

        var bd = pers.Birthday;
        NUD_BirthDay.Value = bd.Day;
        NUD_BirthMonth.Value = bd.Month;

        CHK_ProfileMadeVillage.IsChecked = pers.ProfileIsMakeVillage;

        RIS_ProfileFruit.Value = pers.ProfileFruit;

        // An empty date (DateTime.MinValue) is shown as an empty picker.
        DateTime timestamp = pers.ProfileTimestamp;
        CAL_ProfileTimestamp.SelectedDate = timestamp == DateTime.MinValue ? null : timestamp;
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        SavePlayer();
        Close(true);
    }

    private void SavePlayer()
    {
        var p = Player;
        var pers = p.Personal;

        var bd = pers.Birthday;
        bd.Day = (byte)(NUD_BirthDay.Value ?? 0);
        bd.Month = (byte)(NUD_BirthMonth.Value ?? 0);

        pers.Birthday = bd;
        pers.ProfileBirthday = bd;
        pers.ProfileIsMakeVillage = CHK_ProfileMadeVillage.IsChecked == true;

        pers.ProfileFruit = RIS_ProfileFruit.Value;

        pers.ProfileTimestamp = CAL_ProfileTimestamp.SelectedDate ?? DateTime.MinValue;
    }
}
