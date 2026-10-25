using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using NHSE.Core;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Opens the game the console is playing, over Wi-Fi, straight from the launcher. The game's memory only holds the
/// island and the player, so a copy of that island's save on the Mac (once, e.g. from JKSV) is used for the rest: it is
/// copied to NHSE's own folder (the original is never touched) and filled with what the game has.
/// </summary>
public static class LiveOpen
{
    /// <summary> Where the copies used for each island live (one folder per island). </summary>
    private static string Folder => Path.Combine(Program.AppDataPath, "directo");

    /// <summary>
    /// Asks for the console, finds the island and brings the game. Returns the save and the connection, or null.
    /// </summary>
    public static async Task<(HorizonSave Save, LiveGame Game)?> Open(Window owner)
    {
        var settings = Program.Settings.SysBot;
        var ip = new TextBox { Text = settings.IP, Width = 160, PlaceholderText = "192.168.0.10" };
        var port = new NumericUpDown { Value = settings.Port, Minimum = 1, Maximum = 65535, FormatString = "0", Width = 110 };
        var status = new TextBlock { TextWrapping = TextWrapping.Wrap, Classes = { "subtle" }, Text = HomeStrings.OpenLiveHint };
        var progress = new ProgressBar { Minimum = 0, Maximum = 1, Height = 6, IsVisible = false };
        var go = new Button { Content = HomeStrings.OpenLiveGo, Classes = { "accent" }, IsDefault = true };
        var cancel = new Button { Content = HomeStrings.Cancel, IsCancel = true };

        var row = new WrapPanel { ItemSpacing = 8, LineSpacing = 8 };
        row.Children.Add(new TextBlock { Text = HomeStrings.LiveConsole, VerticalAlignment = VerticalAlignment.Center });
        row.Children.Add(ip);
        row.Children.Add(port);
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, HorizontalAlignment = HorizontalAlignment.Right };
        buttons.Children.Add(cancel);
        buttons.Children.Add(go);
        var panel = new StackPanel { Margin = new Thickness(22, 18, 22, 18), Spacing = 12 };
        panel.Children.Add(Text(HomeStrings.OpenLive, "title"));
        panel.Children.Add(row);
        panel.Children.Add(progress);
        panel.Children.Add(status);
        panel.Children.Add(buttons);
        var dialog = new NhseWindow { Title = HomeStrings.OpenLive, Width = 480, SizeToContent = SizeToContent.Height, CanResize = false, Content = panel };
        dialog.Bind(Window.BackgroundProperty, dialog.GetResourceObservable("NhWindowBrush"));

        (HorizonSave, LiveGame)? result = null;
        void Say(string text, bool bad = false)
        {
            status.Text = text;
            if (bad)
                status.Foreground = Brushes.IndianRed;
            else
                status.ClearValue(TextBlock.ForegroundProperty);
        }
        cancel.Click += (_, _) => dialog.Close();
        go.Click += async (_, _) =>
        {
            go.IsEnabled = false;
            var game = new LiveGame();
            try
            {
                var address = (ip.Text ?? string.Empty).Trim();
                var p = (int)(port.Value ?? 6000);
                Say(HomeStrings.LiveConnecting);
                await Task.Run(() => game.Connect(address, p));
                settings.IP = address;
                settings.Port = p;
                Program.SaveSettings();

                var id = await Task.Run(game.ReadPlayerId);
                if (id is not { } who)
                {
                    Say(HomeStrings.OpenLiveNoIsland, true);
                    game.Dispose();
                    return;
                }
                var template = FindCopy(who.Town, who.Player) ?? await AskForCopy(dialog, who.Town, who.Player, who.TownName, Say);
                if (template is null)
                {
                    game.Dispose();
                    return;
                }
                var sav = HorizonSave.FromFolder(template);
                Say(HomeStrings.LiveGamePulling);
                progress.IsVisible = true;
                var problem = await Task.Run(() => game.Pull(sav, v => Dispatcher.UIThread.Post(() => progress.Value = v)));
                if (problem is not null)
                {
                    Say(problem, true);
                    game.Dispose();
                    return;
                }
                result = (sav, game);
                dialog.Close();
            }
            catch (Exception ex)
            {
                Say(LiveGame.Explain(ex), true);
                game.Dispose();
            }
            finally
            {
                go.IsEnabled = true;
                progress.IsVisible = false;
            }
        };
        await dialog.ShowDialog(owner);
        return result;
    }

    /// <summary> The folder NHSE keeps for an island (by its ID). </summary>
    private static string CopyFolder(uint town) => Path.Combine(Folder, town.ToString("X8"));

    /// <summary> NHSE's own copy of this island, if it was opened by network before. </summary>
    private static string? FindCopy(uint town, uint player)
    {
        var folder = CopyFolder(town);
        return IsIsland(folder, town, player) ? folder : TryCandidates(town, player);
    }

    /// <summary>
    /// A save of this island among the last one opened and the automatic backups; it is copied to NHSE's folder.
    /// </summary>
    private static string? TryCandidates(uint town, uint player)
    {
        var candidates = new List<string>();
        if (Program.Settings.LastFilePath is { Length: not 0 } last)
            candidates.Add(last);
        if (Directory.Exists(SaveFileLoader.BackupPath))
            candidates.AddRange(Directory.GetDirectories(SaveFileLoader.BackupPath).OrderByDescending(Directory.GetLastWriteTime));
        foreach (var c in candidates)
        {
            if (IsIsland(c, town, player))
                return Adopt(c, town);
        }
        return null;
    }

    internal static bool IsIsland(string folder, uint town, uint player)
    {
        try
        {
            if (!File.Exists(Path.Combine(folder, "main.dat")))
                return false;
            var sav = HorizonSave.FromFolder(folder);
            var pers = sav.Players[0].Personal;
            return pers.TownID == town && pers.PlayerID == player;
        }
        catch (Exception ex) when (ex is IOException or ArgumentException or InvalidOperationException or UnauthorizedAccessException)
        {
            return false;
        }
    }

    /// <summary> Copies a save of the island into NHSE's folder (the original is not used again). </summary>
    private static string Adopt(string source, uint town)
    {
        var dest = CopyFolder(town);
        if (Directory.Exists(dest))
            Directory.Delete(dest, true);
        Directory.CreateDirectory(Folder);
        FileUtil.CopyFolder(source, dest);
        return dest;
    }

    /// <summary> The first time, asks for the folder of a save of this island (from JKSV). </summary>
    private static async Task<string?> AskForCopy(Window owner, uint town, uint player, string townName, Action<string, bool> say)
    {
        if (await Dialogs.Prompt(MessageBoxButtons.YesNo, string.Format(HomeStrings.OpenLiveNeedCopy, townName)) != DialogResult.Yes)
            return null;
        var folder = await Pickers.OpenFolder(owner, HomeStrings.OpenLive);
        if (folder is null)
            return null;
        if (!IsIsland(folder, town, player))
        {
            say(string.Format(HomeStrings.OpenLiveWrongCopy, townName), true);
            return null;
        }
        return Adopt(folder, town);
    }
}
