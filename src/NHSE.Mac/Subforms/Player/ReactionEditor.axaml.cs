using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class ReactionEditor : NhseWindow
{
    private readonly Personal Personal;

    public ReactionEditor(Personal p)
    {
        InitializeComponent();
        this.TranslateInterface(GameInfo.CurrentLanguage);

        Personal = p;
        var manpu = p.Reactions;

        PG_Manpu.SelectedObject = manpu;
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        var val = (IReactionStore)PG_Manpu.SelectedObject!;
        Personal.Reactions = val;
        Close(true);
    }

    private void B_GiveAll_Click(object? sender, RoutedEventArgs e)
    {
        var val = (IReactionStore)PG_Manpu.SelectedObject!;
        val.AddMissingReactions();
        PG_Manpu.SelectedObject = val;
        Sound.Asterisk();
    }
}
