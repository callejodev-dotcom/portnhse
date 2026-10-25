using System.Net.Sockets;
using System.Text;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// The game being played, over Wi-Fi (sys-botbase on the console). In the console's memory the island and the first
/// player are kept exactly as in the save files (checked on ACNH 3.0.3), so the open save can be filled with what is in
/// the game, edited with the whole app, and only the bytes that changed sent back.
/// </summary>
public sealed class LiveGame : IDisposable
{
    /// <summary> Where the island (main.dat) starts in the game's memory. </summary>
    public const uint MainAddress = 0xB19FC270;

    /// <summary> Where the first player (personal.dat) starts in the game's memory. </summary>
    public const uint PlayerAddress = 0xB2783AB0;

    private const int ReadChunk = 0x40000;
    private const int WriteChunk = 0x4000;

    private TcpClient? Client;
    private NetworkStream? Stream;

    private byte[]? MainBefore;
    private byte[]? PlayerBefore;

    public bool Connected => Client?.Connected == true;

    /// <summary> True once the open save holds what is in the game. </summary>
    public bool Pulled => MainBefore is not null;

    public void Connect(string ip, int port)
    {
        Dispose();
        Client = new TcpClient { ReceiveTimeout = 20000, SendTimeout = 20000, NoDelay = true };
        Client.Connect(ip, port);
        Stream = Client.GetStream();
    }

    public void Dispose()
    {
        Stream?.Dispose();
        Client?.Dispose();
        Stream = null;
        Client = null;
    }

    private void Send(string command)
    {
        var bytes = Encoding.ASCII.GetBytes(command + "\r\n");
        Stream!.Write(bytes);
    }

    private string ReadLine()
    {
        var sb = new StringBuilder();
        var buffer = new byte[0x10000];
        while (true)
        {
            var n = Stream!.Read(buffer);
            if (n == 0)
                throw new IOException("The console closed the connection.");
            var text = Encoding.ASCII.GetString(buffer, 0, n);
            var end = text.IndexOf('\n');
            if (end >= 0)
            {
                sb.Append(text.AsSpan(0, end));
                return sb.ToString().TrimEnd('\r');
            }
            sb.Append(text);
        }
    }

    /// <summary> Reads part of the game's memory, a piece at a time. </summary>
    public byte[] Read(uint address, int length, Action<double>? progress = null)
    {
        var result = new byte[length];
        for (int done = 0; done < length;)
        {
            var size = Math.Min(ReadChunk, length - done);
            Send($"peek 0x{address + (uint)done:X8} {size}");
            Convert.FromHexString(ReadLine()).CopyTo(result, done);
            done += size;
            progress?.Invoke((double)done / length);
        }
        return result;
    }

    /// <summary> Writes into the game's memory, a piece at a time. </summary>
    public void Write(uint address, ReadOnlySpan<byte> data)
    {
        for (int done = 0; done < data.Length;)
        {
            var size = Math.Min(WriteChunk, data.Length - done);
            Send($"poke 0x{address + (uint)done:X8} 0x{Convert.ToHexString(data.Slice(done, size))}");
            done += size;
        }
        // sys-botbase answers nothing to a write: ask something small to know they are all done.
        Send("getVersion");
        ReadLine();
    }

    /// <summary> Presses a button of the console's controller (sys-botbase's own controller). </summary>
    private void Press(string button) => Send($"click {button}");

    /// <summary>
    /// Saves the game and loads the island again, as − → «Guardar y salir» → A on the title screen does, so the game
    /// shows what it only reads when the island loads (the size of the house…). Takes about a minute.
    /// </summary>
    /// <param name="step">Told what is happening at each step.</param>
    public async Task Reload(Action<string> step)
    {
        step(HomeStrings.LiveReloadSaving);
        Press("MINUS");          // «¿Qué quieres hacer?»
        await Task.Delay(1500);
        Press("A");
        await Task.Delay(1500);
        Press("A");              // «Guardar y salir»: saves
        await Task.Delay(10000);
        Press("A");              // «Se han guardado los datos. ¡Hasta pronto!»
        step(HomeStrings.LiveReloadTitle);
        await Task.Delay(20000);
        // The title screen takes a while: A a few times (while it is still loading they do nothing).
        step(HomeStrings.LiveReloadEnter);
        for (int i = 0; i < 4; i++)
        {
            Press("A");
            await Task.Delay(4000);
        }
        // Give the controller back to the player.
        Send("detachController");
        step(HomeStrings.LiveReloadDone);
    }

    /// <summary> Share of equal bytes, to tell whether the memory read is this island and player. </summary>
    private static double Likeness(ReadOnlySpan<byte> a, ReadOnlySpan<byte> b)
    {
        int same = 0;
        for (int i = 0; i < a.Length; i++)
        {
            if (a[i] == b[i])
                same++;
        }
        return (double)same / a.Length;
    }

    /// <summary>
    /// Fills the open save with the island and the first player as they are in the game (only in memory: the files are
    /// not touched). Refuses when what is read does not look like this save (another island, another game version).
    /// </summary>
    /// <returns>Null if done, or why not.</returns>
    public string? Pull(HorizonSave sav, Action<double>? progress = null)
    {
        int mainLength = sav.Main.Data.Length, playerLength = sav.Players[0].Personal.Data.Length;
        var total = (double)(mainLength + playerLength);
        var gameMain = Read(MainAddress, mainLength, p => progress?.Invoke(p * mainLength / total));
        var gamePlayer = Read(PlayerAddress, playerLength, p => progress?.Invoke((mainLength + (p * playerLength)) / total));
        var main = sav.Main.Data;
        var player = sav.Players[0].Personal.Data;

        // The same island keeps most of its data between the save on the Mac and the game.
        if (Likeness(gamePlayer, player) < 0.5 || Likeness(gameMain, main) < 0.5)
            return HomeStrings.LiveGameOther;

        LiveBackups.Save(gameMain, gamePlayer, sav.Players[0].Personal, HomeStrings.LiveBackupPulled);
        gameMain.CopyTo(main);
        gamePlayer.CopyTo(player);
        MainBefore = gameMain;
        PlayerBefore = gamePlayer;
        return null;
    }

    /// <summary>
    /// Sends to the game the bytes of the island and the first player that changed since they were brought, after checking
    /// that the game still has them where they were.
    /// </summary>
    /// <returns>How many bytes were sent, or -1 if the game no longer looks like what was brought.</returns>
    public int Push(HorizonSave sav)
    {
        if (MainBefore is null || PlayerBefore is null)
            return 0;
        var personal = sav.Players[0].Personal;
        var id = personal.Offsets.PersonalId;
        var check = Read(PlayerAddress + (uint)id, 0x38);
        if (!check.AsSpan().SequenceEqual(PlayerBefore.AsSpan(id, 0x38)))
            return -1;

        // The game as it is now, in case it has to go back.
        if (!sav.Main.Data.SequenceEqual(MainBefore) || !personal.Data.SequenceEqual(PlayerBefore))
            LiveBackups.Save(MainBefore, PlayerBefore, personal, HomeStrings.LiveBackupBeforeSend);
        int sent = Send(MainAddress, sav.Main.Data, MainBefore);
        sent += Send(PlayerAddress, personal.Data, PlayerBefore);
        return sent;
    }

    /// <summary>
    /// Puts a kept copy into the open save, to send it to the game: brings what the game has now first, so that only what
    /// differs from the copy is sent.
    /// </summary>
    /// <returns>Null if done, or why not.</returns>
    public string? UseCopy(HorizonSave sav, string path, Action<double>? progress = null)
    {
        var main = sav.Main.Data.Length;
        var player = sav.Players[0].Personal.Data.Length;
        if (LiveBackups.Read(path, main, player) is not { } copy)
            return HomeStrings.LiveBackupWrong;
        if (Pull(sav, progress) is { } problem)
            return problem;
        copy.Main.CopyTo(sav.Main.Data);
        copy.Player.CopyTo(sav.Players[0].Personal.Data);
        return null;
    }

    /// <summary> Writes the runs of bytes that differ (close runs joined), and keeps them as the new state. </summary>
    private int Send(uint address, ReadOnlySpan<byte> now, byte[] before)
    {
        int sent = 0;
        for (int i = 0; i < now.Length;)
        {
            if (now[i] == before[i])
            {
                i++;
                continue;
            }
            int end = i, gap = 0;
            for (int j = i; j < now.Length && gap < 32; j++)
            {
                if (now[j] != before[j])
                {
                    end = j + 1;
                    gap = 0;
                }
                else
                {
                    gap++;
                }
            }
            Write(address + (uint)i, now[i..end]);
            now[i..end].CopyTo(before.AsSpan(i));
            sent += end - i;
            i = end;
        }
        return sent;
    }
}
