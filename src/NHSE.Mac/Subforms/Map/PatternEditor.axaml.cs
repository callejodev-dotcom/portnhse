using System.Collections.ObjectModel;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

public partial class PatternEditor : NhseWindow
{
    private readonly DesignPattern[] Patterns;
    private readonly Player Player;
    private readonly ObservableCollection<string> Items = [];

    private int Index;
    private const int scale = 8;

    public PatternEditor(DesignPattern[] patterns, Player player)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);
        Patterns = patterns;
        Player = player;

        foreach (var p in patterns)
            Items.Add(GetPatternSummary(p));
        LB_Items.ItemsSource = Items;

        LB_Items.SelectedIndex = 0;
        SetCM_DLDesign();
    }

    private void SetCM_DLDesign()
    {
        // The "All" entries only make sense with a list of designs.
        var all = Items.Count != 0;
        B_DumpDesignAll.IsVisible = all;
        B_LoadDesignAll.IsVisible = all;
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e) => Close(true);

    private void LB_Items_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (LB_Items.SelectedIndex < 0)
            return;
        LoadPattern(Patterns[Index = LB_Items.SelectedIndex]);
    }

    private static string GetPatternSummary(DesignPattern p) => p.DesignName;

    private async void B_DumpDesign_Click(object? sender, RoutedEventArgs e)
    {
        if (sender == B_DumpDesignAll)
        {
            var folder = await Pickers.OpenFolder(this);
            if (folder is null || !Directory.Exists(folder))
                return;
            bool indexed = await Dialogs.Prompt(MessageBoxButtons.YesNo, MessageStrings.MsgAskExportResultsWithFileIndex) == DialogResult.Yes;
            Patterns.Dump(folder, indexed);
            return;
        }

        var original = Patterns[Index];
        var name = original.DesignName;
        var path = await Pickers.SaveFile(this, "New Horizons Design Pattern (*.nhd)|*.nhd|All files (*.*)|*.*", $"{name}.nhd");
        if (path is null)
            return;

        var d = original;
        await File.WriteAllBytesAsync(path, d.Data.ToArray());
    }

    private async void B_LoadDesign_Click(object? sender, RoutedEventArgs e)
    {
        if (sender == B_LoadDesignAll)
        {
            var folder = await Pickers.OpenFolder(this);
            if (folder is null || !Directory.Exists(folder))
                return;
            var result = await Dialogs.Prompt(MessageBoxButtons.YesNoCancel, MessageStrings.MsgAskUpdateValues);
            Patterns.Load(folder, Player.Personal, result == DialogResult.Yes);
            foreach (var p in Patterns)
                UpdateUsageCompatibility(p);
            LoadPattern(Patterns[Index]);
            RepopulateList(Index);
            return;
        }

        var original = Patterns[Index];
        var name = original.DesignName;
        var file = await Pickers.OpenFile(this, "New Horizons Design Pattern (*.nhd)|*.nhd|All files (*.*)|*.*", $"{name}.nhd");
        if (file is null)
            return;

        await ImportDesign(file);
    }

    /// <summary>
    /// Replaces the selected design with the one in the <see cref="file"/> (a .nhd).
    /// </summary>
    /// <returns>True if it was imported.</returns>
    public async Task<bool> ImportDesign(string file)
    {
        var original = Patterns[Index];
        var expectLength = original.Data.Length;
        var fi = new FileInfo(file);
        if (fi.Length != expectLength)
        {
            var msg = string.Format(MessageStrings.MsgDataSizeMismatchImport, fi.Length, expectLength);
            await Dialogs.Error(MessageStrings.MsgCanceling, msg);
            return false;
        }

        var data = await File.ReadAllBytesAsync(file);
        var d = new DesignPattern(data);
        var player0 = Player.Personal;
        if (!d.IsOriginatedFrom(player0))
        {
            var notHost = string.Format(MessageStrings.MsgDataDidNotOriginateFromHost_0, player0.PlayerName);
            var result = await Dialogs.Prompt(MessageBoxButtons.YesNoCancel, notHost, MessageStrings.MsgAskUpdateValues);
            if (result == DialogResult.Cancel)
                return false;
            if (result == DialogResult.Yes)
                d.ChangeOrigins(player0, d.Data);
        }

        UpdateUsageCompatibility(d);

        Patterns[Index] = d;
        LoadPattern(d);
        RepopulateList(Index);
        return true;
    }

    private static void UpdateUsageCompatibility(DesignPattern d)
    {
        if (d.UsageCompatibility is (0xEE00 or 0xEE02)) // known valid values (00=non-transparent, 02=transparent)
            return;

        using var image = d.GetImage();
        if (ImageUtil.ContainsTransparentPixels(image))
            d.UsageCompatibility = 0xEE02; // set to transparent
        else
            d.UsageCompatibility = 0xEE00; // reset to default value (non-transparent)
    }

    private void RepopulateList(int index)
    {
        Items.Clear();
        foreach (var p in Patterns)
            Items.Add(GetPatternSummary(p));
        LB_Items.SelectedIndex = index;
    }

    private void LoadPattern(DesignPattern designPattern)
    {
        PB_Pattern.Source = ResizeImage(designPattern.GetImage(), scale);
        PB_Palette.Source = ResizeImage(designPattern.GetPalette(), 10); // 150x10
        L_PatternName.Text = designPattern.DesignName + Environment.NewLine +
                             designPattern.TownName + Environment.NewLine +
                             designPattern.PlayerName;
    }

    /// <summary>
    /// Enlarges a pixel image by an integer factor with nearest neighbor (ImageUtil.ResizeImage of NHSE.Sprites), consuming the original.
    /// </summary>
    internal static WriteableBitmap ResizeImage(WriteableBitmap image, int factor)
    {
        using (image)
        {
            var (w, h) = (image.PixelSize.Width, image.PixelSize.Height);
            var scaled = ImageUtil.ScalePixelImage(image.GetPixels(), factor, w, h, out var sw, out var sh);
            return ImageUtil.GetBitmap(scaled, sw, sh);
        }
    }

    private void PB_Pattern_MouseEnter(object? sender, PointerEventArgs e) => BG_Pattern.Background = Brushes.GreenYellow;
    private void PB_Pattern_MouseLeave(object? sender, PointerEventArgs e) => BG_Pattern.Background = null;

    private async void Menu_SavePNG_Click(object? sender, RoutedEventArgs e)
    {
        if (PB_Pattern.Source is not Bitmap bmp)
        {
            await Dialogs.Alert(MessageStrings.MsgNoPictureLoaded);
            return;
        }

        var name = Patterns[Index].DesignName;
        var path = await Pickers.SaveFile(this, "png file (*.png)|*.png|All files (*.*)|*.*", $"{name}.png");
        if (path is null)
            return;

        bmp.Save(path, new PngBitmapEncoderOptions());
    }
}
