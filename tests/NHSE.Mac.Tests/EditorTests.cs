using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The main editor writes what is typed in it back to valid save files.
/// </summary>
public class EditorTests
{
    [AvaloniaFact]
    public void SaveWritesValuesAndValidHashes()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var sav = TestSave.LoadCopy(out var folder);
        var editor = new Editor(sav);
        editor.Show();
        Dispatcher.UIThread.RunJobs();

        editor.FindControl<NumericUpDown>("NUD_Wallet")!.Value = 12345;
        editor.FindControl<NumericUpDown>("NUD_NookMiles")!.Value = 4321;
        editor.FindControl<ComboBox>("CB_Hemisphere")!.SelectedIndex = 1;
        editor.SaveToFiles();
        editor.Close();

        var reloaded = HorizonSave.FromFolder(folder);
        Assert.Empty(reloaded.GetInvalidHashes());
        Assert.Equal(12345u, reloaded.Players[0].Personal.Wallet.Value);
        Assert.Equal(4321u, reloaded.Players[0].Personal.NookMiles.Value);
        Assert.Equal(Hemisphere.Southern, reloaded.Main.Hemisphere);
    }

    [AvaloniaFact]
    public void InterfaceUsesTheWinFormsTranslations()
    {
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");
        var editor = new Editor(TestSave.LoadCopy(out _));
        var previous = GameInfo.CurrentLanguage;
        try
        {
            App.SetLanguage(GameLanguage.GetLanguageIndex("es"));
            Assert.Equal("Editar Objetos", editor.FindControl<Button>("B_EditPlayerItems")!.Content);
            Assert.Equal("Jugadores", editor.FindControl<TabItem>("Tab_Players")!.Header);
            Assert.Equal("Sí", MessageStrings.MsgDialogButtonYes);
            Assert.Equal("Arrastra aquí la carpeta de tu partida", MacStrings.LauncherDrop);

            App.SetLanguage(GameLanguage.GetLanguageIndex("en"));
            Assert.Equal("Edit Items", editor.FindControl<Button>("B_EditPlayerItems")!.Content);
            Assert.Equal("Yes", MessageStrings.MsgDialogButtonYes);
            Assert.Equal("Drop your save folder here", MacStrings.LauncherDrop);
        }
        finally
        {
            App.SetLanguage(GameLanguage.GetLanguageIndex(previous));
        }
    }
}
