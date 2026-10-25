using System.ComponentModel;

namespace NHSE.Mac;

/// <summary>
/// Row of the long flag lists (FlagEditor, LandFlagEditor, VillagerFlagEditor, VillagerMemoryEditor).
/// WinForms rewrote <c>ListBox.Items[i]</c> when a value changed; here the row text changes instead, so the
/// (virtualized) ListBox keeps its selection and only redraws the visible row.
/// </summary>
public sealed class FlagListItem(string text) : INotifyPropertyChanged
{
    private static readonly PropertyChangedEventArgs TextChanged = new(nameof(Text));

    public event PropertyChangedEventHandler? PropertyChanged;

    public string Text
    {
        get;
        set
        {
            if (field == value)
                return;
            field = value;
            PropertyChanged?.Invoke(this, TextChanged);
        }
    } = text;

    public override string ToString() => Text;

    /// <summary>
    /// Creates the rows of a list, one per index.
    /// </summary>
    public static FlagListItem[] Create(int count, Func<int, string> getText)
    {
        var result = new FlagListItem[count];
        for (int i = 0; i < result.Length; i++)
            result[i] = new FlagListItem(getText(i));
        return result;
    }

    /// <summary>
    /// Rewrites the text of every row (after importing a whole list); only the rows that changed are redrawn.
    /// </summary>
    public static void Refresh(FlagListItem[] rows, Func<int, string> getText)
    {
        for (int i = 0; i < rows.Length; i++)
            rows[i].Text = getText(i);
    }
}
