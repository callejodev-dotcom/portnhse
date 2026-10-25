using Avalonia.Controls;
using NHSE.Injection;

namespace NHSE.Mac;

public sealed class SysBotController(InjectionType type)
{
    public readonly SysBot Bot = new();

    private static SysBotSettings Config => Program.Settings.SysBot;

    public string IP => Config.IP;
    public string Port => Config.Port.ToString();

    public async Task<bool> Connect(string ip, string port)
    {
        if (!int.TryParse(port, out var p))
            p = 6000;

        try
        {
            Bot.Connect(ip, p);
        }
        catch (Exception ex)
        {
            await Dialogs.Error(ex.Message);
            return false;
        }

        Config.IP = ip;
        Config.Port = p;

        return true;
    }

    public uint GetDefaultOffset() => type switch
    {
        InjectionType.Generic => Config.GenericOffset,
        InjectionType.Pouch => Config.PouchOffset,
        _ => throw new ArgumentOutOfRangeException(nameof(type), type, null),
    };

    public void SetOffset(uint value)
    {
        switch (type)
        {
            case InjectionType.Generic: Config.GenericOffset = value; break;
            case InjectionType.Pouch: Config.PouchOffset = value; break;
            default: return;
        }
    }

    public async Task HexEdit(Window owner, uint offset, int length)
    {
        var read = ReadBytes(offset, length);
        var ram = new SimpleHexEditor(read);
        if (!await ram.ShowDialog<bool>(owner))
            return;

        var write = ram.Bytes;
        if (read.Length != write.Length)
        {
            var prompt = await Dialogs.Prompt(MessageBoxButtons.OKCancel,
                string.Format(MessageStrings.MsgDataSizeMismatchRAM, read.Length, write.Length),
                MessageStrings.MsgAskWriteAnyway);

            if (prompt != DialogResult.OK)
                return;
        }

        WriteBytes(ram.Bytes, offset);
        SetOffset(offset);
        Sound.Asterisk();
    }

    public async Task PopPrompt()
    {
        if (Config.Prompted)
            return;

        await Dialogs.Alert(MessageStrings.MsgSysBotInfo, MessageStrings.MsgSysBotRequired);
        Config.Prompted = true;
    }

    public void WriteBytes(byte[] data, uint offset)
    {
        Bot.WriteBytes(data, offset);
        SetOffset(offset);
    }

    public byte[] ReadBytes(uint offset, int length)
    {
        var result = Bot.ReadBytes(offset, length);
        SetOffset(offset);
        return result;
    }
}
