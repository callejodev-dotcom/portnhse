using Avalonia.Controls;
using Avalonia.Interactivity;
using NHSE.Core;
using NHSE.Injection;

namespace NHSE.Mac;

public partial class SysBotRAMEdit : NhseWindow
{
    private readonly SysBotController Bot;
    private bool IsClosed;

    public SysBotRAMEdit() : this(InjectionType.Generic) { }

    public SysBotRAMEdit(InjectionType type)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);
        Bot = new SysBotController(type);
        RamOffset.Text = Bot.GetDefaultOffset().ToString("X8");

        TB_IP.Text = Bot.IP;
        TB_Port.Text = Bot.Port;

        // Shown once the window is up, so that the message box sits on top of it.
        Opened += async (_, _) => await Bot.PopPrompt();
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

        IsClosed = true;
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

    private async void B_Edit_Click(object? sender, RoutedEventArgs e)
    {
        var offset = StringUtil.GetHexValue(RamOffset.Text);
        if (offset == 0)
        {
            await Dialogs.Error(MessageStrings.MsgInvalidHexValue);
            return;
        }

        var length = (int)(NUD_Offset.Value ?? 0);
        try
        {
            await Bot.HexEdit(this, offset, length);
        }
        catch (Exception ex)
        {
            // WinForms showed these through its unhandled exception dialog (lost connection, etc.).
            await Dialogs.Error(ex.Message);
        }
    }
}
