using System.Diagnostics;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Loader for <see cref="HorizonSave"/> files.
/// </summary>
public static class SaveFileLoader
{
    private const string BackupFolderName = "bak";
    public static string BackupPath => Path.Combine(Program.AppDataPath, BackupFolderName);

    /// <summary>
    /// Attempts to load a <see cref="HorizonSave"/> file from the specified path.
    /// </summary>
    /// <param name="path">The file system path to the save folder, its main.dat, or a .zip of it.</param>
    /// <returns>The loaded save file, or null if it could not be loaded (the user has been told why).</returns>
    public static async Task<HorizonSave?> TryGetSaveFile(string path)
    {
        try
        {
            return await TryGetSaveFileInternal(path);
        }
        catch (Exception ex)
        {
            Debug.WriteLine(ex);
            await Dialogs.Error(ex.Message);
            return null;
        }
    }

    private static async Task<HorizonSave?> TryGetSaveFileInternal(string path)
    {
        if (Directory.Exists(path))
            return await OpenSaveFile(path);

        // Load zip files differently
        var ext = Path.GetExtension(path);
        if (ext.Equals(".zip", StringComparison.OrdinalIgnoreCase))
        {
            var length = new FileInfo(path).Length;
            const int maxSize = 20 * 1024 * 1024;
            if (length < maxSize)
            {
                var sav = HorizonSave.FromZip(path);
                return await TryValidateSaveFile(sav) ? sav : null;
            }
        }
        // Try loading from the parent folder if it's the .dat file
        else if (ext.Equals(".dat", StringComparison.OrdinalIgnoreCase))
        {
            var dir = Path.GetDirectoryName(path);
            if (Directory.Exists(dir))
                return await OpenSaveFile(dir);
        }

        // Nice try, we failed.
        await Dialogs.Error(MessageStrings.MsgSaveDataImportFail, MessageStrings.MsgSaveDataImportSuggest);
        return null;
    }

    private static async Task<HorizonSave?> OpenSaveFile(string path)
    {
        var sav = HorizonSave.FromFolder(path);
        if (!await TryValidateSaveFile(sav))
            return null;

        var settings = Program.Settings;
        settings.LastFilePath = path;
        await CheckBackupPrompt(settings);

        if (settings.AutomaticBackup)
            BackupSaveFile(sav, path, BackupPath);

        return sav;
    }

    private static async Task CheckBackupPrompt(ApplicationSettings settings)
    {
        if (settings.BackupPrompted)
            return;

        settings.BackupPrompted = true;
        var line1 = string.Format(MacStrings.MsgBackupMac, UiUtil.ShortPath(BackupPath));
        var line2 = MessageStrings.MsgBackupCreateQuestion;
        var prompt = await Dialogs.Prompt(MessageBoxButtons.YesNo, line1, line2);
        settings.AutomaticBackup = prompt == DialogResult.Yes;
        Program.SaveSettings();
    }

    private static async Task<bool> TryValidateSaveFile(HorizonSave file)
    {
        if (!file.ValidateSizes())
        {
            var prompt = await Dialogs.Prompt(MessageBoxButtons.YesNo, MessageStrings.MsgSaveDataSizeMismatch, MessageStrings.MsgAskContinue);
            if (prompt != DialogResult.Yes)
                return false;
        }
        if (file.GetInvalidHashes().Any())
        {
            var prompt = await Dialogs.Prompt(MessageBoxButtons.YesNo, MessageStrings.MsgSaveDataHashMismatch, MessageStrings.MsgAskContinue);
            if (prompt != DialogResult.Yes)
                return false;
        }
        return true;
    }

    private static void BackupSaveFile(HorizonSave file, string path, string bak)
    {
        Directory.CreateDirectory(bak);
        var dest = Path.Combine(bak, file.GetBackupFolderTitle());
        if (!Directory.Exists(dest))
            FileUtil.CopyFolder(path, dest);
    }
}
