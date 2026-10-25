using System.IO.Compression;
using System.Runtime.CompilerServices;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Threading;
using NHSE.Core;

// The gallery tests replace the network (ImageFetcher.MessageHandler) and the settings.
[assembly: InternalsVisibleTo("NHSE.Mac.Tests")]

namespace NHSE.Mac;

public partial class ImageFetcher : NhseWindow
{
    private const string Filename = "image.zip";
    private const int MaxBufferSize = 8192;
    private static string ZipFilePath => Path.Combine(ItemSprite.PlatformAppDataPath, Filename);

    /// <summary>
    /// Handler for the HTTP requests instead of the network (tests only).
    /// </summary>
    internal static HttpMessageHandler? MessageHandler { get; set; }

    private readonly string[] AllHosts;
    private bool Downloading;

    public ImageFetcher()
    {
        InitializeComponent();
        L_Status.Text = L_FileSize.Text = string.Empty;

        var splitHosts = AllHosts = LoadHosts();

        CB_HostSelect.SetComboItems(splitHosts.Select(CleanUrl));

        CB_HostSelect.SelectedIndex = 0; // set outside of initialise to update filesize via HEAD response

        CheckFileStatusLabel();
    }

    private static string[] LoadHosts()
    {
        var hosts = EmbeddedResources.GetText("network/hosts_images.txt") ?? string.Empty;
        var splitHosts = hosts.Split(["\r", "\n", "\r\n"], StringSplitOptions.RemoveEmptyEntries);

        return splitHosts;
    }

    private static HttpClient CreateClient() => MessageHandler is { } handler ? new HttpClient(handler, false) : new HttpClient();

    protected override void OnClosing(WindowClosingEventArgs e)
    {
        // WinForms hides the close box while downloading.
        if (Downloading && e.CloseReason == WindowCloseReason.WindowClosing)
            e.Cancel = true;
        base.OnClosing(e);
    }

    private async void B_Download_Click(object? sender, RoutedEventArgs e)
    {
        try
        {
            var path = ItemSprite.PlatformAppDataPath;
            var hostSelected = AllHosts[CB_HostSelect.SelectedIndex];

            SetUIDownloadState(false);

            L_Status.Text = "Downloading...";

            if (!Directory.Exists(path))
                Directory.CreateDirectory(path);

            await DownloadFile(hostSelected, progress => Dispatcher.UIThread.Post(() => PBar_MultiUse.Value = progress));

            PBar_MultiUse.Value = 100;
            L_Status.Text = "Unzipping...";
        }
        catch (Exception ex)
        {
            await ShowError(ex);
            SetUIDownloadState(true);
            return;
        }
        await UnzipFile();
    }

    /// <summary>
    /// Downloads the file off the UI thread, reporting the percentage downloaded.
    /// </summary>
    private static async Task DownloadFile(string url, Action<int> report)
    {
        using var httpClient = CreateClient();
        using var response = await httpClient.GetAsync(url, HttpCompletionOption.ResponseHeadersRead).ConfigureAwait(false);
        response.EnsureSuccessStatusCode();

        var totalBytes = response.Content.Headers.ContentLength ?? -1L;
        var canReportProgress = totalBytes > 0;

        await using var contentStream = await response.Content.ReadAsStreamAsync().ConfigureAwait(false);
        await using var fileStream = new FileStream(ZipFilePath, FileMode.Create, FileAccess.Write, FileShare.None, MaxBufferSize, true);
        var buffer = new byte[MaxBufferSize];
        long totalRead = 0;
        int lastProgress = -1;
        int read;
        while ((read = await contentStream.ReadAsync(buffer).ConfigureAwait(false)) > 0)
        {
            await fileStream.WriteAsync(buffer.AsMemory(0, read)).ConfigureAwait(false);
            totalRead += read;
            if (!canReportProgress)
                continue;
            int progress = (int)Math.Min(totalRead * 100 / totalBytes, 100);
            if (progress == lastProgress)
                continue;
            lastProgress = progress;
            report(progress);
        }
    }

    private async Task UnzipFile()
    {
        try
        {
            var outputFolderPath = ItemSprite.PlatformAppDataImagePath;

            await Task.Run(() =>
            {
                if (Directory.Exists(outputFolderPath)) // overwrite existing
                    Directory.Delete(outputFolderPath, true);

                Directory.CreateDirectory(outputFolderPath);

                ZipFile.ExtractToDirectory(ZipFilePath, outputFolderPath);
            });

            SetUIDownloadState(true, true);
        }
        catch (Exception ex)
        {
            await ShowError(ex);
            SetUIDownloadState(true);
        }
    }

    private static Task<DialogResult> ShowError(Exception ex)
    {
        return ex.InnerException is { } inner
            ? Dialogs.Error(ex.Message, inner.Message)
            : Dialogs.Error(ex.Message);
    }

    private void SetUIDownloadState(bool val, bool success = false)
    {
        Downloading = !val;
        B_Download.IsEnabled = val;
        PBar_MultiUse.Value = 0;

        L_Status.Text = success ? "Images installed successfully." : string.Empty;

        CheckFileStatusLabel();

        if (success)
        {
            ItemSprite.Initialize(GameInfo.GetStrings("en").itemlist);
            ItemSprite.ClearCache();
        }

        if (File.Exists(ZipFilePath))
            File.Delete(ZipFilePath);
    }

    private async void CB_HostSelect_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        await CheckNetworkFileSizeAsync();
    }

    private async Task CheckNetworkFileSizeAsync()
    {
        try
        {
            L_FileSize.Text = string.Empty;
            var index = CB_HostSelect.SelectedIndex;
            if ((uint)index >= AllHosts.Length)
                return;
            var host = AllHosts[index];
            using var httpClient = CreateClient();
            // Only the headers: the size is all that is needed.
            using var httpInitialResponse = await httpClient.GetAsync(host, HttpCompletionOption.ResponseHeadersRead);
            var hdr = httpInitialResponse.Content.Headers.ContentLength;

            if (hdr == null)
            {
                L_FileSize.Text = "Failed.";
                return;
            }
            var totalSizeBytes = hdr.Value;
            var totalSizeMb = totalSizeBytes / 1e+6;
            L_FileSize.Text = $"{totalSizeMb:0.##}MB";
        }
        catch (Exception ex)
        {
            L_FileSize.Text = ex.Message;
        }
    }

    private static string CleanUrl(string url)
    {
        var uri = new Uri(url);
        return uri.Segments.Length < 2 ? url : $"{uri.Host}/{uri.Segments[1]}";
    }

    private void CheckFileStatusLabel() => L_ImgStatus.IsVisible = ItemSprite.SingleSpriteExists;
}
