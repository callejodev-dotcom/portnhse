using System.Net;
using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Threading;
using NHSE.Core;
using NHSE.Injection;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// Program tools and SysBot windows: none of them needs save data, and none of them touches the network here.
/// </summary>
public class GalleryProgram
{
    [AvaloniaFact]
    public void ImageFetcherWindow()
    {
        ImageFetcher.MessageHandler = new SizeOnlyHandler(57_312_345);
        try
        {
            var w = new ImageFetcher();
            w.Show();
            var size = w.FindControl<TextBlock>("L_FileSize")!;
            WaitFor(() => !string.IsNullOrEmpty(size.Text));
            Assert.Equal($"{57.312345:0.##}MB", size.Text);
            Gallery.Capture(w, "50-image-fetcher");
        }
        finally
        {
            ImageFetcher.MessageHandler = null;
        }
    }

    [AvaloniaFact]
    public void SysBotRAMEditWindow()
    {
        Program.Settings.SysBot.Prompted = true; // no SysBot info message box
        Gallery.Capture(new SysBotRAMEdit(InjectionType.Generic), "51-sysbot-ram-edit");
    }

    [AvaloniaFact]
    public void SysBotUIWindow()
    {
        Program.Settings.SysBot.Prompted = true;
        var items = Enumerable.Range(0, 40).Select(_ => new Item(Item.NONE)).ToArray();
        var sb = new SysBotController(InjectionType.Pouch);
        var ai = new AutoInjector(new PocketInjector(items, sb.Bot), _ => { }, _ => { });
        var ub = new USBBotController();
        var aiUSB = new AutoInjector(new PocketInjector(items, ub.Bot), _ => { }, _ => { });
        Gallery.Capture(new SysBotUI(ai, sb, aiUSB, ub), "52-sysbot-ui");
    }

    [AvaloniaFact]
    public void SimpleHexEditorWindow()
    {
        var bytes = Enumerable.Range(0, 1024).Select(i => (byte)(i * 7)).ToArray();
        var w = new SimpleHexEditor(bytes);
        Assert.Equal(bytes, w.Bytes);
        Gallery.Capture(w, "53-simple-hex-editor");
    }

    private static void WaitFor(Func<bool> done)
    {
        var until = DateTime.UtcNow.AddSeconds(5);
        while (!done() && DateTime.UtcNow < until)
        {
            Dispatcher.UIThread.RunJobs();
            Thread.Sleep(10);
        }
    }

    /// <summary>
    /// Answers every request with an empty body and the given Content-Length.
    /// </summary>
    private sealed class SizeOnlyHandler(long length) : HttpMessageHandler
    {
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
        {
            var content = new ByteArrayContent([]);
            content.Headers.ContentLength = length;
            return Task.FromResult(new HttpResponseMessage(HttpStatusCode.OK) { Content = content, RequestMessage = request });
        }
    }
}
