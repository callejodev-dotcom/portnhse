using System.Windows.Input;

namespace NHSE.Mac;

/// <summary>
/// Minimal <see cref="ICommand"/> for native menu items.
/// </summary>
public sealed class RelayCommand(Action execute) : ICommand
{
    public RelayCommand(Func<Task?> execute) : this(() => { _ = execute(); }) { }

    public event EventHandler? CanExecuteChanged { add { } remove { } }
    public bool CanExecute(object? parameter) => true;
    public void Execute(object? parameter) => execute();
}
