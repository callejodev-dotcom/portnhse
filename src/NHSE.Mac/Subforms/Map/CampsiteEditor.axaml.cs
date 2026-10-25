using Avalonia.Controls;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class CampsiteEditor : NhseWindow
{
    private readonly HorizonSave SAV;
    private bool CampsiteUnlocked;
    private string GetUpdatedVillagerInternalName() => VillagerUtil.GetInternalVillagerName((VillagerSpecies)(int)(NUD_Species.Value ?? 0), (int)(NUD_Variant.Value ?? 0));
    private void ChangeVillager(object? sender, NumericUpDownValueChangedEventArgs e) => ChangeVillager();
    private void ChangeVillager()
    {
        var name = GetUpdatedVillagerInternalName();
        L_InternalName.Text = name;
        L_ExternalName.Text = GameInfo.Strings.GetVillager(name);
        PB_Villager.Source = VillagerSprite.GetVillagerSprite(name);
    }

    public CampsiteEditor(HorizonSave sav)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);

        SAV = sav;

        CheckCampsiteStatus();

        LoadCampsiteVisitor();
    }

    private void CheckCampsiteStatus()
    {
        switch (SAV.Main.CampsiteStatus)
        {
            case 01:
                CampsiteUnlocked = false;
                CB_CampsiteOccupied.IsChecked = false;
                break;
            case 02:
                CampsiteUnlocked = true;
                CB_CampsiteOccupied.IsChecked = false;
                break;
            case 03:
                CampsiteUnlocked = true;
                CB_CampsiteOccupied.IsChecked = true;
                break;
        }

        B_Save.IsEnabled = CampsiteUnlocked;
        L_CampsiteUnlockStatus.IsEnabled = !CampsiteUnlocked;
        if (CampsiteUnlocked)
            L_CampsiteUnlockStatus.Text = null;
        L_CampsiteUnlockStatus.IsVisible = !CampsiteUnlocked; // no empty line above the check box
        CB_CampsiteOccupied.IsEnabled = CampsiteUnlocked;
        L_Camper_ExternalName.IsEnabled = CampsiteUnlocked;
        L_ExternalName.IsEnabled = CampsiteUnlocked;
        PB_Villager.IsEnabled = CampsiteUnlocked;
        L_Camper_InternalName.IsEnabled = CampsiteUnlocked;
        L_InternalName.IsEnabled = CampsiteUnlocked;
        L_Camper_Edit.IsEnabled = CampsiteUnlocked;
        L_Species.IsEnabled = CampsiteUnlocked;
        NUD_Species.IsEnabled = CampsiteUnlocked;
        L_Variant.IsEnabled = CampsiteUnlocked;
        NUD_Variant.IsEnabled = CampsiteUnlocked;
    }

    private void LoadCampsiteVisitor()
    {
        NUD_Species.Value = SAV.Main.CampsiteVillagerSpeciesID;
        NUD_Variant.Value = SAV.Main.CampsiteVillagerVariantID;
        var name = GetUpdatedVillagerInternalName();
        L_InternalName.Text = name;
        L_ExternalName.Text = GameInfo.Strings.GetVillager(name);
        PB_Villager.Source = VillagerSprite.GetVillagerSprite(name);
        CAL_CampTimestamp.SelectedDate = DateTime.Now;
    }

    private void SaveCampsiteVisitor()
    {
        if (CampsiteUnlocked)
        {
            switch (CB_CampsiteOccupied.IsChecked == true)
            {
                case true:
                    SAV.Main.CampsiteStatus = 0x03;
                    SAV.Main.CampsiteVillagerVariantID = (byte)(NUD_Variant.Value ?? 0);
                    SAV.Main.CampsiteVillagerSpeciesID = (byte)(NUD_Species.Value ?? 0);
                    SAV.Main.CampTimestamp = CAL_CampTimestamp.SelectedDate ?? DateTime.Now;
                    break;
                case false:
                    // The game performs a check and corrects status based on
                    // villagerID or status - but let's clean up anyway.
                    SAV.Main.CampsiteStatus = 0x02;
                    SAV.Main.CampsiteVillagerVariantID = 0x00;
                    SAV.Main.CampsiteVillagerSpeciesID = 0x23;
                    break;
            }
        }
    }

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        SaveCampsiteVisitor();
        Close(true);
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);
}
