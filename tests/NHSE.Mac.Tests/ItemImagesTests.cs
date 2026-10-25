using System.Net;
using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Interactivity;
using Avalonia.Threading;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// The item image pack (berichan's raw.zip): ImageFetcher installs it and the grids use it.
/// Needs a local copy of the zip in NHSE_IMAGEZIP, served through the fetcher's HTTP handler (no network).
/// </summary>
public class ItemImagesTests
{
    [AvaloniaFact]
    public void ImageFetcherInstallsThePackAndTheInventoryUsesIt()
    {
        var zip = Environment.GetEnvironmentVariable("NHSE_IMAGEZIP");
        Assert.SkipUnless(File.Exists(zip), "NHSE_IMAGEZIP is not set");
        Assert.SkipUnless(TestSave.Available, "NHSE_TEST_SAVE is not set");

        ImageFetcher.MessageHandler = new FileHandler(zip!);
        try
        {
            var w = new ImageFetcher();
            w.Show();
            w.FindControl<Button>("B_Download")!.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            var until = DateTime.UtcNow.AddMinutes(3);
            var status = w.FindControl<TextBlock>("L_Status")!;
            while (status.Text != "Images installed successfully." && DateTime.UtcNow < until)
            {
                Dispatcher.UIThread.RunJobs();
                Thread.Sleep(50);
            }
            Gallery.Capture(w, "60-image-fetcher-done");
        }
        finally
        {
            ImageFetcher.MessageHandler = null;
        }

        Assert.True(Directory.EnumerateFiles(ItemSprite.PlatformAppDataImagePath, "*.png").Take(20000).Count() >= 20000);

        var sav = TestSave.LoadCopy(out _);
        var pers = sav.Players[0].Personal;
        Gallery.Capture(new PlayerItemEditor(pers.Pocket.Concat(pers.Bag).ToArray(), 10, 4), "61-inventory-images");
        Gallery.Capture(new PlayerItemEditor(pers.ItemChest, 10, 5), "62-storage-images");
    }

    /// <summary>
    /// Serves a local file for every request (HEAD gets only its length).
    /// </summary>
    private sealed class FileHandler(string path) : HttpMessageHandler
    {
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
        {
            var length = new FileInfo(path).Length;
            HttpContent content = request.Method == HttpMethod.Head ? new ByteArrayContent([]) : new StreamContent(File.OpenRead(path));
            content.Headers.ContentLength = length;
            return Task.FromResult(new HttpResponseMessage(HttpStatusCode.OK) { Content = content });
        }
    }
}
