using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using NHSE.Core;
using NHSE.Injection;

namespace NHSE.Mac;

/// <summary>
/// The pockets of the player in the running game, over Wi-Fi (sys-botbase on the console): the usual items window with
/// a bar to connect, bring the pockets from the game and send them back. Sending is only possible after bringing them,
/// and the game's data is checked first, so nothing is written where the pockets are not.
/// </summary>
public static class LivePockets
{
    /// <summary> Where the first item of the pockets is in the memory of ACNH 3.0.x (works on 3.0.3). </summary>
    private const uint DefaultOffset = 0xB27BB758;

    public static ItemsWindow Create()
    {
        var items = Enumerable.Range(0, 40).Select(_ => new Item(Item.NONE)).ToArray();
        var bot = new SysBot();
        var settings = Program.Settings.SysBot;
        var injector = new PocketInjector(items, bot) { WriteOffset = settings.PouchOffset > 0 ? settings.PouchOffset : DefaultOffset };

        var ip = new TextBox { Text = settings.IP, Width = 150, PlaceholderText = "192.168.0.10" };
        var port = new NumericUpDown { Value = settings.Port, Minimum = 1, Maximum = 65535, FormatString = "0", Width = 110 };
        var connect = new Button { Content = HomeStrings.LiveConnect };
        var read = new Button { Content = HomeStrings.LiveRead, IsEnabled = false, Classes = { "accent" } };
        var write = new Button { Content = HomeStrings.LiveWrite, IsEnabled = false };
        var status = new TextBlock { Text = HomeStrings.LiveNotConnected, VerticalAlignment = VerticalAlignment.Center, TextWrapping = TextWrapping.Wrap, Classes = { "subtle" } };
        ItemsWindow? window = null;

        void Say(string text, bool bad = false)
        {
            status.Text = text;
            if (bad)
                status.Foreground = Brushes.IndianRed;
            else
                status.ClearValue(TextBlock.ForegroundProperty);
        }

        async Task Run(Button b, Func<Task> work)
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
            }
        }

        connect.Click += async (_, _) => await Run(connect, async () =>
        {
            var address = (ip.Text ?? string.Empty).Trim();
            var p = (int)(port.Value ?? 6000);
            Say(HomeStrings.LiveConnecting);
            if (bot.Connected)
                bot.Disconnect();
            await Task.Run(() => bot.Connect(address, p));
            settings.IP = address;
            settings.Port = p;
            Program.SaveSettings();
            read.IsEnabled = true;
            Say(HomeStrings.LiveConnected);
        });

        read.Click += async (_, _) => await Run(read, async () =>
        {
            var result = await Task.Run(injector.Read);
            if (result is InjectionResult.Success or InjectionResult.Same)
            {
                window?.Reload();
                write.IsEnabled = true;
                Say(HomeStrings.LiveReadDone);
            }
            else
            {
                Say(HomeStrings.LiveNotFound, true);
            }
        });

        write.Click += async (_, _) => await Run(write, async () =>
        {
            var result = await Task.Run(injector.Write);
            switch (result)
            {
                case InjectionResult.Success:
                    Sound.Asterisk();
                    Say(HomeStrings.LiveWriteDone);
                    break;
                case InjectionResult.Same:
                    Say(HomeStrings.LiveSame);
                    break;
                default:
                    Say(HomeStrings.LiveNotFound, true);
                    break;
            }
        });

        var row = new WrapPanel { ItemSpacing = 8, LineSpacing = 8 };
        row.Children.Add(new TextBlock { Text = HomeStrings.LiveConsole, VerticalAlignment = VerticalAlignment.Center });
        row.Children.Add(ip);
        row.Children.Add(port);
        row.Children.Add(connect);
        var actions = new WrapPanel { ItemSpacing = 8, LineSpacing = 8 };
        actions.Children.Add(read);
        actions.Children.Add(write);
        actions.Children.Add(status);
        var panel = new StackPanel { Spacing = 8 };
        panel.Children.Add(row);
        panel.Children.Add(actions);
        var card = new Border { Classes = { "card" }, Child = panel, Margin = new Thickness(0, 12, 0, 0), Padding = new Thickness(14, 10) };

        window = new ItemsWindow(items, 10, 4, HomeStrings.LiveTitle, HomeStrings.LiveHint, card, closeOnly: true);
        window.Closed += (_, _) =>
        {
            if (bot.Connected)
                bot.Disconnect();
        };
        return window;
    }
}
