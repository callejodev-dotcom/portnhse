using System.Net;
using System.Net.Sockets;
using System.Text;

namespace NHSE.Mac.Tests;

/// <summary>
/// A stand-in for sys-botbase on the console: answers peek, poke and getVersion over the same text protocol, on a copy of
/// the island and the player kept where the game keeps them. Can drop the connection, as a console going to sleep does.
/// </summary>
internal sealed class FakeConsole : IDisposable
{
    private readonly TcpListener Listener = new(IPAddress.Loopback, 0);
    private readonly Dictionary<uint, byte[]> Memory = [];
    private TcpClient? Current;

    public int Port => ((IPEndPoint)Listener.LocalEndpoint).Port;
    public int Connections { get; private set; }
    public List<string> Pressed { get; } = [];

    /// <summary> Takes the writes without doing them, as a write that went wrong. </summary>
    public bool IgnorePokes { get; set; }

    public FakeConsole(byte[] main, byte[] player)
    {
        Memory[LiveGame.MainAddress] = main;
        Memory[LiveGame.PlayerAddress] = player;
        Listener.Start();
        _ = Task.Run(Serve);
    }

    public byte[] Main => Memory[LiveGame.MainAddress];
    public byte[] Player => Memory[LiveGame.PlayerAddress];

    /// <summary> Closes the connection from the console's side. </summary>
    public void Drop() => Current?.Close();

    public void Dispose()
    {
        Current?.Close();
        Listener.Stop();
    }

    private async Task Serve()
    {
        while (true)
        {
            TcpClient client;
            try
            {
                client = await Listener.AcceptTcpClientAsync();
            }
            catch (Exception ex) when (ex is SocketException or ObjectDisposedException)
            {
                return;
            }
            Current = client;
            Connections++;
            try
            {
                using var reader = new StreamReader(client.GetStream(), Encoding.ASCII);
                var stream = client.GetStream();
                while (await reader.ReadLineAsync() is { } line)
                {
                    var answer = Answer(line.Split(' '));
                    if (answer is not null)
                        await stream.WriteAsync(Encoding.ASCII.GetBytes(answer + "\r\n"));
                }
            }
            catch (Exception ex) when (ex is IOException or SocketException or ObjectDisposedException)
            {
                // Dropped: wait for the next connection.
            }
        }
    }

    private string? Answer(string[] c)
    {
        switch (c[0])
        {
            case "getVersion":
                return "2.6";
            case "peek":
                {
                    var (block, at) = Find(Convert.ToUInt32(c[1], 16));
                    return Convert.ToHexString(block, at, int.Parse(c[2]));
                }
            case "poke":
                {
                    if (IgnorePokes)
                        return null;
                    var (block, at) = Find(Convert.ToUInt32(c[1], 16));
                    Convert.FromHexString(c[2][2..]).CopyTo(block, at);
                    return null;
                }
            default:
                Pressed.Add(string.Join(' ', c));
                return null;
        }
    }

    private (byte[] Block, int At) Find(uint address)
    {
        foreach (var (start, block) in Memory)
        {
            if (address >= start && address < start + block.Length)
                return (block, (int)(address - start));
        }
        throw new InvalidOperationException($"Nothing at {address:X8}");
    }
}
