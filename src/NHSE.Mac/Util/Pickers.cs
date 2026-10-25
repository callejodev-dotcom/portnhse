using Avalonia;
using Avalonia.Controls;
using Avalonia.Platform.Storage;

namespace NHSE.Mac;

/// <summary>
/// Open/Save/Folder dialogs. Filters use the WinForms syntax so ported code can keep them as they were:
/// <c>"New Horizons Inventory (*.nhi)|*.nhi|All files (*.*)|*.*"</c>.
/// </summary>
public static class Pickers
{
    /// <summary>
    /// Shows an open file dialog; returns the chosen path, or null if canceled.
    /// </summary>
    public static async Task<string?> OpenFile(Visual owner, string filter, string? fileName = null, string? title = null)
    {
        var sp = GetProvider(owner);
        if (sp is null)
            return null;

        var result = await sp.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = title,
            AllowMultiple = false,
            SuggestedFileName = fileName,
            FileTypeFilter = ParseFilter(filter),
            SuggestedStartLocation = await GetStartFolder(sp),
        });
        return Remember(result.Count == 0 ? null : result[0].TryGetLocalPath());
    }

    /// <summary>
    /// Shows a save file dialog; returns the chosen path, or null if canceled.
    /// </summary>
    public static async Task<string?> SaveFile(Visual owner, string filter, string? fileName = null, string? title = null)
    {
        var sp = GetProvider(owner);
        if (sp is null)
            return null;

        var types = ParseFilter(filter);
        var ext = types.SelectMany(z => z.Patterns ?? []).Select(GetExtension).FirstOrDefault(z => z is not null);
        var result = await sp.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = title,
            SuggestedFileName = fileName,
            DefaultExtension = ext,
            FileTypeChoices = types,
            ShowOverwritePrompt = true,
            SuggestedStartLocation = await GetStartFolder(sp),
        });
        return Remember(result?.TryGetLocalPath());
    }

    /// <summary>
    /// Shows a folder selection dialog; returns the chosen path, or null if canceled.
    /// </summary>
    public static async Task<string?> OpenFolder(Visual owner, string? title = null)
    {
        var sp = GetProvider(owner);
        if (sp is null)
            return null;

        var result = await sp.OpenFolderPickerAsync(new FolderPickerOpenOptions
        {
            Title = title,
            AllowMultiple = false,
            SuggestedStartLocation = await GetStartFolder(sp),
        });
        var path = result.Count == 0 ? null : result[0].TryGetLocalPath();
        if (path is not null)
            LastFolder = path;
        return path;
    }

    /// <summary>
    /// Folder of the last file chosen in any dialog, to start the next one there.
    /// </summary>
    private static string? LastFolder;

    private static string? Remember(string? path)
    {
        if (path is not null)
            LastFolder = Path.GetDirectoryName(path);
        return path;
    }

    private static async Task<IStorageFolder?> GetStartFolder(IStorageProvider sp)
    {
        if (LastFolder is null || !Directory.Exists(LastFolder))
            return null;
        return await sp.TryGetFolderFromPathAsync(LastFolder);
    }

    private static IStorageProvider? GetProvider(Visual owner) => (TopLevel.GetTopLevel(owner) ?? UiUtil.GetActiveWindow())?.StorageProvider;

    private static string? GetExtension(string pattern)
    {
        var ext = Path.GetExtension(pattern).TrimStart('.');
        return ext.Length == 0 || ext.Contains('*') ? null : ext;
    }

    /// <summary>
    /// Converts a WinForms filter string into Avalonia file types.
    /// </summary>
    public static List<FilePickerFileType> ParseFilter(string filter)
    {
        var parts = filter.Split('|');
        var result = new List<FilePickerFileType>();
        for (int i = 0; i + 1 < parts.Length; i += 2)
        {
            var patterns = parts[i + 1].Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            if (patterns.Contains("*.*"))
                patterns = ["*"];
            result.Add(new FilePickerFileType(parts[i]) { Patterns = patterns });
        }
        return result;
    }
}
