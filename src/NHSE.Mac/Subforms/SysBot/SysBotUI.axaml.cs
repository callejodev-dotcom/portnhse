using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Threading;
using NHSE.Core;
using NHSE.Injection;

namespace NHSE.Mac;

public partial class SysBotUI : NhseWindow
{
    private readonly AutoInjector Injector;
    private readonly SysBotController Bot;
    private readonly AutoInjector InjectorUSB;
    private readonly USBBotController BotUSB;

    private readonly DispatcherTimer TIM_Interval = new() { Interval = TimeSpan.FromMilliseconds(5000) };
    private bool AutoReading;
    private bool IsClosed;

    public SysBotUI(AutoInjector injector, SysBotController c, AutoInjector injectorUSB, USBBotController b)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);
        Bot = c;
        Injector = injector;
        BotUSB = b;
        InjectorUSB = injectorUSB;

        RamOffset.TextChanged += RamOffset_TextChanged;
        RamOffsetUSB.TextChanged += RamOffsetUSB_TextChanged;
        CHK_AutoWrite.IsCheckedChanged += CHK_AutoWrite_CheckedChanged;
        CHK_AutoRead.IsCheckedChanged += CHK_AutoRead_CheckedChanged;
        CHK_Validate.IsCheckedChanged += CHK_Validate_CheckedChanged;

        var offset = Bot.GetDefaultOffset();
        offset = offset > 0 ? offset : StringUtil.GetHexValue(RamOffset.Text);
        Injector.SetWriteOffset(offset);

        RamOffset.Text = offset.ToString("X8");
        RamOffsetUSB.Text = offset.ToString("X8");

        TB_IP.Text = Bot.IP;
        TB_Port.Text = Bot.Port;

        // Shown once the window is up, so that the message box sits on top of it.
        Opened += async (_, _) => await Bot.PopPrompt();

        TIM_Interval.Tick += async (_, _) => await AutoRead();
    }

    /// <summary>
    /// Timer read, off the UI thread (the injector's read callback marshals its own UI updates).
    /// </summary>
    private async Task AutoRead()
    {
        if (AutoReading)
            return;
        AutoReading = true;
        try
        {
            await Task.Run(() => Injector.Read());
        }
        catch (Exception ex)
        {
            TurnOffAuto();
            await Dialogs.Error(ex.Message);
        }
        finally
        {
            AutoReading = false;
        }
    }

    private async void B_Connect_Click(object? sender, RoutedEventArgs e)
    {
        B_Connect.IsEnabled = false;
        bool connected;
        try
        {
            connected = await Bot.Connect(TB_IP.Text ?? string.Empty, TB_Port.Text ?? string.Empty);
        }
        finally
        {
            B_Connect.IsEnabled = true;
        }
        if (!connected)
            return;
        if (IsClosed) // closed while connecting
        {
            Disconnect();
            return;
        }
        GB_Inject.IsEnabled = true;
    }

    protected override void OnClosing(WindowClosingEventArgs e)
    {
        base.OnClosing(e);
        if (e.Cancel)
            return;

        TurnOffAuto();
        TIM_Interval.Stop();
        IsClosed = true;
        BotUSB.Release();
        if (!Bot.Bot.Connected)
            return;

        Disconnect();
    }

    private void Disconnect()
    {
        try
        {
            Bot.Bot.Disconnect();
        }
        catch (Exception ex)
        {
            Console.WriteLine(ex.Message);
        }
    }

    private async void B_WriteCurrent_Click(object? sender, RoutedEventArgs e)
    {
        var offset = StringUtil.GetHexValue(RamOffset.Text);
        if (offset == 0)
        {
            await Dialogs.Error(MessageStrings.MsgInvalidHexValue);
            return;
        }

        Injector.SetWriteOffset(offset);

        try
        {
            var spoof = UiUtil.ModifierKeys == UiUtil.CommandModifier;
            var result = await Task.Run(() => Write(Injector, spoof));
            if (result == InjectionResult.Success)
                return;
            await Dialogs.Alert(result.ToString());
        }
        catch (Exception ex)
        {
            await Dialogs.Error(ex.Message);
        }
        TurnOffAuto();
    }

    private static InjectionResult Write(AutoInjector injector, bool spoofInventoryWrite)
    {
        if (injector.Injector is not PocketInjector p)
            return injector.Write(true);

        p.SpoofInventoryWrite = spoofInventoryWrite;
        try
        {
            return injector.Write(true);
        }
        finally
        {
            p.SpoofInventoryWrite = false;
        }
    }

    private async void B_ReadCurrent_Click(object? sender, RoutedEventArgs e)
    {
        try
        {
            var result = await Task.Run(() => Injector.Read(true));
            if (result == InjectionResult.Success)
                return;
            await Dialogs.Alert(result.ToString());
        }
        catch (Exception ex)
        {
            await Dialogs.Error(ex.Message);
        }
        TurnOffAuto();
    }

    private void TurnOffAuto()
    {
        if (CHK_AutoRead.IsChecked == true)
            CHK_AutoRead.IsChecked = false;
        if (CHK_AutoWrite.IsChecked == true)
            CHK_AutoWrite.IsChecked = false;
    }

    private void CHK_AutoWrite_CheckedChanged(object? sender, RoutedEventArgs e) => Injector.AutoInjectEnabled = CHK_AutoWrite.IsChecked == true;
    private void CHK_AutoRead_CheckedChanged(object? sender, RoutedEventArgs e) => TIM_Interval.IsEnabled = CHK_AutoRead.IsChecked == true;
    private void CHK_Validate_CheckedChanged(object? sender, RoutedEventArgs e) => Injector.ValidateEnabled = CHK_Validate.IsChecked == true;

    private async void RamOffset_TextChanged(object? sender, TextChangedEventArgs e)
    {
        var offset = StringUtil.GetHexValue(RamOffset.Text);
        if (offset == 0)
        {
            await Dialogs.Error(MessageStrings.MsgInvalidHexValue);
            return;
        }

        Injector.SetWriteOffset(offset);
        Bot.SetOffset(offset);
    }

    private async void ReadUSB_Click(object? sender, RoutedEventArgs e)
    {
        if (!await BotUSB.Connect())
            return;

        var offset = StringUtil.GetHexValue(RamOffsetUSB.Text);
        if (offset == 0)
        {
            await Dialogs.Error(MessageStrings.MsgInvalidHexValue);
            return;
        }

        InjectorUSB.SetWriteOffset(offset);
        Bot.SetOffset(offset);

        try
        {
            var result = await Task.Run(() => InjectorUSB.Read(true));
            if (result == InjectionResult.Success)
                return;
            await Dialogs.Alert(result.ToString());
        }
        catch (Exception ex)
        {
            await Dialogs.Error(ex.Message);
        }

        BotUSB.Disconnect();
    }

    private async void WriteUSB_Click(object? sender, RoutedEventArgs e)
    {
        if (!await BotUSB.Connect())
            return;

        var offset = StringUtil.GetHexValue(RamOffsetUSB.Text);
        if (offset == 0)
        {
            await Dialogs.Error(MessageStrings.MsgInvalidHexValue);
            return;
        }

        InjectorUSB.SetWriteOffset(offset);
        Bot.SetOffset(offset);

        try
        {
            var result = await Task.Run(() => InjectorUSB.Write(true));
            if (result == InjectionResult.Success)
                return;
            await Dialogs.Alert(result.ToString());
        }
        catch (Exception ex)
        {
            await Dialogs.Error(ex.Message);
        }

        BotUSB.Disconnect();
    }

    private async void RamOffsetUSB_TextChanged(object? sender, TextChangedEventArgs e)
    {
        var offset = StringUtil.GetHexValue(RamOffsetUSB.Text);
        if (offset == 0)
        {
            await Dialogs.Error(MessageStrings.MsgInvalidHexValue);
            return;
        }

        Injector.SetWriteOffset(offset);
        Bot.SetOffset(offset);
    }
}
