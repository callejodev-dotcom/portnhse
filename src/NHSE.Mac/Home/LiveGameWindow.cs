using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using static NHSE.Mac.HomeUi;

namespace NHSE.Mac;

/// <summary>
/// Edits the game being played over Wi-Fi: brings the island and the first player from the console into the open save,
/// and sends back what was changed in any part of the app. Stays open beside the home window while editing.
/// </summary>
public sealed class LiveGameWindow : NhseWindow
{
    private readonly HomeContext Ctx;
    private readonly Action Reload;
    private readonly LiveGame Game = new();
    private readonly TextBlock Status = new() { TextWrapping = TextWrapping.Wrap, Classes = { "subtle" } };
    private readonly ProgressBar Progress = new() { Minimum = 0, Maximum = 1, IsVisible = false, Height = 6 };
    private readonly Button Pull = new() { Content = HomeStrings.LiveGamePull, IsEnabled = false, Classes = { "accent" } };
    private readonly Button Push = new() { Content = HomeStrings.LiveGamePush, IsEnabled = false, Classes = { "accent" } };
    private readonly Button ReloadIsland = new() { Content = HomeStrings.LiveReload, IsEnabled = false };

    /// <param name="reload">Shows the home window again with what was brought from the game.</param>
    public LiveGameWindow(HomeContext ctx, Action reload)
    {
        Ctx = ctx;
        Reload = reload;
        Title = HomeStrings.LiveGameTitle;
        Width = 520;
        SizeToContent = SizeToContent.Height;
        CanResize = false;

        var settings = Program.Settings.SysBot;
        var ip = new TextBox { Text = settings.IP, Width = 150, PlaceholderText = "192.168.0.10" };
        var port = new NumericUpDown { Value = settings.Port, Minimum = 1, Maximum = 65535, FormatString = "0", Width = 110 };
        var connect = new Button { Content = HomeStrings.LiveConnect };
        connect.Click += async (_, _) => await Run(connect, async () =>
        {
            var address = (ip.Text ?? string.Empty).Trim();
            var p = (int)(port.Value ?? 6000);
            Say(HomeStrings.LiveConnecting);
            await Task.Run(() => Game.Connect(address, p));
            settings.IP = address;
            settings.Port = p;
            Program.SaveSettings();
            Pull.IsEnabled = true;
            ReloadIsland.IsEnabled = true;
            Say(HomeStrings.LiveGameConnected);
        });
        Pull.Click += async (_, _) => await Run(Pull, DoPull);
        Push.Click += async (_, _) => await Run(Push, DoPush);
        ReloadIsland.Click += async (_, _) => await Run(ReloadIsland, DoReload);

        var row = new WrapPanel { ItemSpacing = 8, LineSpacing = 8 };
        row.Children.Add(new TextBlock { Text = HomeStrings.LiveConsole, VerticalAlignment = VerticalAlignment.Center });
        row.Children.Add(ip);
        row.Children.Add(port);
        row.Children.Add(connect);

        var steps = new StackPanel { Spacing = 10 };
        steps.Children.Add(Step("1", HomeStrings.LiveGameStep1, row));
        steps.Children.Add(Step("2", HomeStrings.LiveGameStep2, Pull));
        steps.Children.Add(Step("3", HomeStrings.LiveGameStep3, null));
        steps.Children.Add(Step("4", HomeStrings.LiveGameStep4, Push));
        steps.Children.Add(Step("5", HomeStrings.LiveGameStep5, ReloadIsland));
        steps.Children.Add(Progress);
        steps.Children.Add(Status);

        var notes = new Border { Classes = { "card" }, Padding = new Thickness(12, 8), Child = new TextBlock { Text = HomeStrings.LiveGameNotes, TextWrapping = TextWrapping.Wrap, FontSize = 12 } };

        var close = new Button { Content = HomeStrings.Close, IsCancel = true, MinWidth = 100 };
        close.Click += (_, _) => Close();
        var copies = PlayerUi.MenuButton(HomeStrings.LiveBackups,
            (HomeStrings.LiveBackupUse, () => _ = ChooseCopy()),
            (HomeStrings.LiveBackupFolder, () =>
            {
                Directory.CreateDirectory(LiveBackups.Folder);
                UiUtil.OpenInFinder(LiveBackups.Folder);
            }));
        var bottom = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto") };
        bottom.Children.Add(copies);
        Grid.SetColumn(close, 1);
        bottom.Children.Add(close);

        var root = new StackPanel { Margin = new Thickness(22, 18, 22, 18), Spacing = 12 };
        root.Children.Add(Text(HomeStrings.LiveGameTitle, "title"));
        root.Children.Add(Text(HomeStrings.LiveGameHint, "subtle"));
        root.Children.Add(Card(steps));
        root.Children.Add(notes);
        root.Children.Add(bottom);
        Content = root;
        this.Bind(BackgroundProperty, this.GetResourceObservable("NhWindowBrush"));
        Say(HomeStrings.LiveNotConnected);
    }

    private static Control Step(string number, string text, Control? action)
    {
        var g = new Grid { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 10 };
        var badge = new Border { Width = 24, Height = 24, CornerRadius = new CornerRadius(12), VerticalAlignment = VerticalAlignment.Top, Child = new TextBlock { Text = number, Foreground = Brushes.White, FontWeight = FontWeight.SemiBold, HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center } };
        badge.Bind(Border.BackgroundProperty, badge.GetResourceObservable("NhGreenBrush"));
        g.Children.Add(badge);
        var p = new StackPanel { Spacing = 6 };
        p.Children.Add(new TextBlock { Text = text, TextWrapping = TextWrapping.Wrap, VerticalAlignment = VerticalAlignment.Center });
        if (action is not null)
        {
            action.HorizontalAlignment = HorizontalAlignment.Left;
            p.Children.Add(action);
        }
        Grid.SetColumn(p, 1);
        g.Children.Add(p);
        return g;
    }

    private void Say(string text, bool bad = false)
    {
        Status.Text = text;
        if (bad)
            Status.Foreground = Brushes.IndianRed;
        else
            Status.ClearValue(TextBlock.ForegroundProperty);
    }

    private async Task Run(Button b, Func<Task> work)
    {
        b.IsEnabled = false;
        try
        {
            await work();
        }
        catch (Exception ex)
        {
            Say(LiveGame.Explain(ex), true);
        }
        finally
        {
            b.IsEnabled = true;
            Progress.IsVisible = false;
        }
    }

    private async Task DoPull()
    {
        Say(HomeStrings.LiveGamePulling);
        Progress.Value = 0;
        Progress.IsVisible = true;
        var problem = await Task.Run(() => Game.Pull(Ctx.SAV, p => Avalonia.Threading.Dispatcher.UIThread.Post(() => Progress.Value = p)));
        if (problem is not null)
        {
            Say(problem, true);
            return;
        }
        Reload();
        Push.IsEnabled = true;
        Sound.Asterisk();
        Say(HomeStrings.LiveGamePulled);
    }

    private async Task DoPush()
    {
        Say(HomeStrings.LiveGamePushing);
        var sent = await Task.Run(() => Game.Push(Ctx.SAV));
        if (sent.Moved)
        {
            Say(HomeStrings.LiveGameMoved, true);
            Push.IsEnabled = false;
            return;
        }
        if (sent.Wrong != 0)
        {
            Say(string.Format(HomeStrings.LiveVerifyFailed, sent.Wrong), true);
            return;
        }
        Sound.Asterisk();
        ReloadIsland.IsEnabled = true;
        Say(sent.Bytes == 0 ? HomeStrings.LiveSame : string.Format(HomeStrings.LiveGamePushed, sent.Places.ToString("N0")));
    }

    private async Task DoReload()
    {
        if (await Dialogs.Prompt(MessageBoxButtons.YesNo, HomeStrings.LiveReloadAsk) != DialogResult.Yes)
            return;
        Push.IsEnabled = false;
        Pull.IsEnabled = false;
        await Game.Reload(text => Avalonia.Threading.Dispatcher.UIThread.Post(() => Say(text)));
        // What was brought belongs to the game before it loaded again: bring it again before changing more.
        Pull.IsEnabled = true;
    }

    /// <summary>
    /// Lists the kept copies; the chosen one goes into the open save, ready to be sent to the game with «Send the changes».
    /// </summary>
    private async Task ChooseCopy()
    {
        var list = LiveBackups.List();
        if (list.Count == 0)
        {
            await Dialogs.Alert(HomeStrings.LiveBackupNone);
            return;
        }
        if (!Game.Connected)
        {
            await Dialogs.Alert(HomeStrings.LiveNotConnected);
            return;
        }
        var box = new ListBox { ItemsSource = list.Select(LiveBackups.Describe).ToArray(), SelectedIndex = 0, Height = 260 };
        var dialog = new Window
        {
            Title = HomeStrings.LiveBackupUse,
            Width = 460,
            SizeToContent = SizeToContent.Height,
            CanResize = false,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
        };
        var ok = new Button { Content = HomeStrings.LiveBackupUseThis, Classes = { "accent" }, IsDefault = true };
        var cancel = new Button { Content = HomeStrings.Cancel, IsCancel = true };
        ok.Click += (_, _) => dialog.Close(true);
        cancel.Click += (_, _) => dialog.Close(false);
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, HorizontalAlignment = HorizontalAlignment.Right };
        buttons.Children.Add(cancel);
        buttons.Children.Add(ok);
        var panel = new StackPanel { Margin = new Thickness(18), Spacing = 10 };
        panel.Children.Add(Text(HomeStrings.LiveBackupUseHint, "subtle"));
        panel.Children.Add(box);
        panel.Children.Add(buttons);
        dialog.Content = panel;
        dialog.Bind(BackgroundProperty, dialog.GetResourceObservable("NhWindowBrush"));
        if (!await dialog.ShowDialog<bool>(this) || box.SelectedIndex < 0)
            return;

        var path = list[box.SelectedIndex].Path;
        await Run(Pull, async () =>
        {
            Say(HomeStrings.LiveGamePulling);
            Progress.Value = 0;
            Progress.IsVisible = true;
            var problem = await Task.Run(() => Game.UseCopy(Ctx.SAV, path, p => Avalonia.Threading.Dispatcher.UIThread.Post(() => Progress.Value = p)));
            if (problem is not null)
            {
                Say(problem, true);
                return;
            }
            Reload();
            Push.IsEnabled = true;
            Say(HomeStrings.LiveBackupReady);
        });
    }

    protected override void OnClosed(EventArgs e)
    {
        Game.Dispose();
        base.OnClosed(e);
    }
}
