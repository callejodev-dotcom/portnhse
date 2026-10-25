using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using LibUsbDotNet;
using NHSE.Injection;

namespace NHSE.Mac;

/// <summary>
/// USB injection through sys-botbase's usb-botbase. Needs libusb on macOS (brew install libusb).
/// </summary>
public sealed class USBBotController
{
    public readonly USBBot Bot = new();

    private static bool LibUsbUsed;

    static USBBotController()
    {
        // LibUsbDotNet imports "libusb-1.0", which the default search doesn't find in Homebrew's folder on Apple silicon.
        try
        {
            NativeLibrary.SetDllImportResolver(typeof(UsbDevice).Assembly, ResolveLibUsb);
        }
        catch (InvalidOperationException)
        {
            // A resolver is already set for that assembly.
        }
    }

    private static readonly string[] LibUsbPaths =
    [
        "/opt/homebrew/lib/libusb-1.0.dylib", // Homebrew, Apple silicon
        "/usr/local/lib/libusb-1.0.dylib", // Homebrew, Intel
        "/opt/local/lib/libusb-1.0.dylib", // MacPorts
    ];

    private static IntPtr ResolveLibUsb(string name, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (!OperatingSystem.IsMacOS() || !name.StartsWith("libusb-1.0", StringComparison.Ordinal))
            return IntPtr.Zero;
        foreach (var path in LibUsbPaths)
        {
            if (File.Exists(path) && NativeLibrary.TryLoad(path, out var handle))
                return handle;
        }
        return IntPtr.Zero; // default search
    }

    public async Task<bool> Connect()
    {
        if (!LibUsbUsed)
        {
            LibUsbUsed = true;
            // libusb's event thread would keep the process alive after quitting.
            if (Application.Current?.ApplicationLifetime is IControlledApplicationLifetime lifetime)
                lifetime.Exit += (_, _) => ExitLibUsb();
        }

        try
        {
            return Bot.Connect();
        }
        catch (Exception ex) when (IsLibUsbMissing(ex))
        {
            await Dialogs.Error(MacStrings.MsgLibUsbMissing);
            return false;
        }
        catch (Exception ex)
        {
            await Dialogs.Error(ex.Message);
            return false;
        }
    }

    /// <summary>
    /// Checks if the exception comes from the native libusb library not being installed.
    /// </summary>
    private static bool IsLibUsbMissing(Exception? ex)
    {
        for (; ex is not null; ex = ex.InnerException)
        {
            if (ex is DllNotFoundException or EntryPointNotFoundException)
                return true;
        }
        return false;
    }

    public void Disconnect() => Bot.Disconnect();

    /// <summary>
    /// Closes the device (if still open) and frees libusb, once the window that uses it is closed.
    /// </summary>
    public void Release()
    {
        if (!LibUsbUsed)
            return;
        try
        {
            if (Bot.Connected)
                Bot.Disconnect();
        }
        catch (Exception ex)
        {
            Debug.WriteLine(ex.Message);
        }
        ExitLibUsb();
    }

    private static void ExitLibUsb()
    {
        try
        {
            UsbDevice.Exit();
        }
        catch (Exception ex)
        {
            Debug.WriteLine(ex.Message);
        }
    }

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
