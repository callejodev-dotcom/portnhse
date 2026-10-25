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
    private string Ip = string.Empty;
    private int Port;
    private NetworkStream? Stream;

    private byte[]? MainBefore;
    private byte[]? PlayerBefore;

    public bool Connected => Client?.Connected == true;

    /// <summary> True once the open save holds what is in the game. </summary>
    public bool Pulled => MainBefore is not null;

    public void Connect(string ip, int port)
    {
        Dispose();
        Ip = ip;
        Port = port;
        Client = new TcpClient { ReceiveTimeout = 20000, SendTimeout = 20000, NoDelay = true };
        // Without a limit, a console that is off or asleep takes half a minute to fail.
        try
        {
            if (!Client.ConnectAsync(ip, port).Wait(TimeSpan.FromSeconds(5)))
                throw new ConsoleUnreachableException(ip);
        }
        catch (Exception ex) when (ex is ConsoleUnreachableException or AggregateException { InnerException: SocketException })
        {
            Dispose();
            throw ex as ConsoleUnreachableException ?? new ConsoleUnreachableException(ip);
        }
        Stream = Client.GetStream();
    }

    /// <summary>
    /// Connects again if the console dropped the connection (it went to sleep, the Wi-Fi came back…), so a window left
    /// open keeps working.
    /// </summary>
    private void EnsureConnected()
    {
        if (Stream is not null)
        {
            try
            {
                Send("getVersion");
                ReadLine();
                return;
            }
            catch (Exception ex) when (ex is IOException or SocketException or ObjectDisposedException)
            {
                // Connect again below.
            }
        }
        if (Ip.Length == 0)
            throw new IOException(HomeStrings.LiveNotConnected);
        Connect(Ip, Port);
    }

    /// <summary> What to tell the player when talking to the console failed. </summary>
    public static string Explain(Exception ex) => ex is ConsoleUnreachableException ? ex.Message : string.Format(HomeStrings.LiveError, ex.Message);

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
        Poke(address, data);
        Sync();
    }

    /// <summary> Sends the writes without waiting for them (see <see cref="Sync"/>). </summary>
    private void Poke(uint address, ReadOnlySpan<byte> data)
    {
        for (int done = 0; done < data.Length;)
        {
            var size = Math.Min(WriteChunk, data.Length - done);
            Send($"poke 0x{address + (uint)done:X8} 0x{Convert.ToHexString(data.Slice(done, size))}");
            done += size;
        }
    }

    /// <summary> sys-botbase answers nothing to a write: asks something small, to know the writes before are all done. </summary>
    private void Sync()
    {
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
        await Task.Run(EnsureConnected);
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

    /// <summary> Where the player's identity (island and player IDs and names) is in personal.dat of 3.0. </summary>
    private const int PersonalId = 0xC248;

    /// <summary>
    /// Who is playing: the island and player of the first player, read from the game's memory; null if the game is not
    /// on an island (or not this version).
    /// </summary>
    public (uint Town, string TownName, uint Player, string PlayerName)? ReadPlayerId()
    {
        var d = Read(PlayerAddress + PersonalId, 0x38);
        var town = BitConverter.ToUInt32(d, 0);
        var player = BitConverter.ToUInt32(d, 0x1C);
        var townName = StringUtil.GetString(d, 4, 10);
        var playerName = StringUtil.GetString(d, 0x20, 10);
        if (town == 0 || player == 0 || townName.Length == 0 || playerName.Length == 0)
            return null;
        return (town, townName, player, playerName);
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
        EnsureConnected();
        int mainLength = sav.Main.Data.Length, playerLength = sav.Players[0].Personal.Data.Length;
        var total = (double)(mainLength + playerLength);
        var gameMain = Read(MainAddress, mainLength, p => progress?.Invoke(p * mainLength / total));
        var gamePlayer = Read(PlayerAddress, playerLength, p => progress?.Invoke((mainLength + (p * playerLength)) / total));
        var main = sav.Main.Data;
        var player = sav.Players[0].Personal.Data;

        // The same player (island and player IDs and names), and the island keeps most of its data between the save on
        // the Mac and the game (most of a save is zeros, so a likeness alone is not enough).
        var id = sav.Players[0].Personal.Offsets.PersonalId;
        if (!gamePlayer.AsSpan(id, 0x38).SequenceEqual(player.Slice(id, 0x38)) || Likeness(gamePlayer, player) < 0.5 || Likeness(gameMain, main) < 0.5)
            return HomeStrings.LiveGameOther;

        LiveBackups.Save(gameMain, gamePlayer, sav.Players[0].Personal, HomeStrings.LiveBackupPulled);
        LiveBackups.SaveLatest(gameMain, gamePlayer, sav.Players[0].Personal);
        gameMain.CopyTo(main);
        gamePlayer.CopyTo(player);
        MainBefore = gameMain;
        PlayerBefore = gamePlayer;
        return null;
    }

    /// <summary>
    /// What sending did: how many bytes were written and in how many places, and in how many of those the console does
    /// not have them when read back. <see cref="Moved"/>: nothing was sent, the game no longer has the player where it was.
    /// </summary>
    public sealed record SendResult(bool Moved, int Bytes, int Places, int Wrong)
    {
        public static readonly SendResult Nothing = new(false, 0, 0, 0);
        public bool Checked => !Moved && Wrong == 0;
    }

    /// <summary>
    /// Sends to the game the bytes of the island and the first player that changed since they were brought, after checking
    /// that the game still has them where they were, and reads them back to check the console has them.
    /// </summary>
    public SendResult Push(HorizonSave sav)
    {
        if (MainBefore is null || PlayerBefore is null)
            return SendResult.Nothing;
        EnsureConnected();
        var personal = sav.Players[0].Personal;
        var id = personal.Offsets.PersonalId;
        var check = Read(PlayerAddress + (uint)id, 0x38);
        if (!check.AsSpan().SequenceEqual(PlayerBefore.AsSpan(id, 0x38)))
            return new SendResult(true, 0, 0, 0);

        // The game as it is now, in case it has to go back.
        if (!sav.Main.Data.SequenceEqual(MainBefore) || !personal.Data.SequenceEqual(PlayerBefore))
            LiveBackups.Save(MainBefore, PlayerBefore, personal, HomeStrings.LiveBackupBeforeSend);
        var mainRuns = Send(MainAddress, sav.Main.Data, MainBefore);
        var playerRuns = Send(PlayerAddress, personal.Data, PlayerBefore);
        if (mainRuns.Count + playerRuns.Count == 0)
            return SendResult.Nothing;
        Sync();
        var wrong = Verify(MainAddress, mainRuns, MainBefore) + Verify(PlayerAddress, playerRuns, PlayerBefore);
        LiveBackups.SaveLatest(MainBefore, PlayerBefore, personal);
        var runs = mainRuns.Concat(playerRuns).ToList();
        return new SendResult(false, runs.Sum(z => z.Length), runs.Count, wrong);
    }

    /// <summary>
    /// Reads back the places just written (close ones in one read) and counts those the console does not have. Those keep
    /// what the console has as the known state, so sending again writes them again.
    /// </summary>
    private int Verify(uint address, List<(int Start, int Length)> runs, byte[] before)
    {
        int wrong = 0;
        for (int r = 0; r < runs.Count; r++)
        {
            int first = r, start = runs[r].Start, end = runs[r].Start + runs[r].Length;
            while (r + 1 < runs.Count && runs[r + 1].Start - end < 0x800)
            {
                r++;
                end = runs[r].Start + runs[r].Length;
            }
            var read = Read(address + (uint)start, end - start);
            for (int k = first; k <= r; k++)
            {
                var (s, length) = runs[k];
                var has = read.AsSpan(s - start, length);
                if (has.SequenceEqual(before.AsSpan(s, length)))
                    continue;
                has.CopyTo(before.AsSpan(s));
                wrong++;
            }
        }
        return wrong;
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
    /// <returns>Where they were written.</returns>
    private List<(int Start, int Length)> Send(uint address, ReadOnlySpan<byte> now, byte[] before)
    {
        var runs = new List<(int, int)>();
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
            Poke(address + (uint)i, now[i..end]);
            now[i..end].CopyTo(before.AsSpan(i));
            runs.Add((i, end - i));
            i = end;
        }
        return runs;
    }
}

/// <summary> The console did not answer at all (off, asleep, another network, no sys-botbase). </summary>
public sealed class ConsoleUnreachableException(string ip) : IOException(string.Format(HomeStrings.LiveUnreachable, ip));
