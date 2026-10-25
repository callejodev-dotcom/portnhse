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

        var close = new Button { Content = HomeStrings.Close, IsCancel = true, MinWidth = 100, HorizontalAlignment = HorizontalAlignment.Right };
        close.Click += (_, _) => Close();

        var root = new StackPanel { Margin = new Thickness(22, 18, 22, 18), Spacing = 12 };
        root.Children.Add(Text(HomeStrings.LiveGameTitle, "title"));
        root.Children.Add(Text(HomeStrings.LiveGameHint, "subtle"));
        root.Children.Add(Card(steps));
        root.Children.Add(notes);
        root.Children.Add(close);
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
            Say(string.Format(HomeStrings.LiveError, ex.Message), true);
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
        if (sent < 0)
        {
            Say(HomeStrings.LiveGameMoved, true);
            Push.IsEnabled = false;
            return;
        }
        Sound.Asterisk();
        ReloadIsland.IsEnabled = true;
        Say(sent == 0 ? HomeStrings.LiveSame : string.Format(HomeStrings.LiveGamePushed, sent.ToString("N0")));
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

    protected override void OnClosed(EventArgs e)
    {
        Game.Dispose();
        base.OnClosed(e);
    }
}
