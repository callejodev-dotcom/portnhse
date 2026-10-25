using NHSE.Injection;

namespace NHSE.Mac;

/// <summary>
/// USB injection through sys-botbase's usb-botbase. Needs libusb on macOS (brew install libusb).
/// </summary>
public sealed class USBBotController
{
    public readonly USBBot Bot = new();

    public async Task<bool> Connect()
    {
        try
        {
            return Bot.Connect();
        }
        catch (Exception ex)
        {
            await Dialogs.Error(ex.Message);
            return false;
        }
    }

    public void Disconnect() => Bot.Disconnect();

    public void WriteBytes(byte[] data, uint offset)
    {
        Bot.WriteBytes(data, offset);
    }

    public byte[] ReadBytes(uint offset, int length)
    {
        var result = Bot.ReadBytes(offset, length);
        return result;
    }
}
