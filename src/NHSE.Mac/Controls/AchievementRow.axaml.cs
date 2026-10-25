using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using NHSE.Core;

namespace NHSE.Mac;

public partial class AchievementRow : UserControl
{
    public AchievementRow()
    {
        InitializeComponent();
        // The WinForms picker allowed DateTime.MinValue (empty date); here an empty date is an empty picker.
        CAL_Date.AddHandler(PointerPressedEvent, CAL_Date_MouseDown, RoutingStrategies.Tunnel);
    }

    public void LoadRow(AchievementList list, in int index, in int row)
    {
        if (!LifeSupportAchievement.List.TryGetValue(index, out var detail))
        {
            L_Threshold.Text = "N/A";
        }
        else
        {
            if (row >= detail.AchievementCount)
            {
                L_Threshold.Text = string.Empty;
            }
            else
            {
                var threshold = detail.GetThresholdValue(row);
                L_Threshold.Text = threshold <= 0 ? "N/A" : threshold.ToString();
            }
        }

        DateTime date = list.Date[index][row];
        CAL_Date.SelectedDate = date == DateTime.MinValue ? null : date;

        CHK_Read.IsChecked = list.Read[index][row];
    }

    public void SaveRow(AchievementList list, in int index, in int row)
    {
        list.Date[index][row] = CAL_Date.SelectedDate ?? DateTime.MinValue;
        list.Read[index][row] = CHK_Read.IsChecked == true;
    }

    public void ChangeCount(in int index, in int row, in uint count)
    {
        if (!LifeSupportAchievement.List.TryGetValue(index, out var detail))
            return;

        bool satisfied = detail.GetIsSatisfied(row, count);
        if (satisfied)
            L_Threshold.Foreground = Brushes.Red;
        else
            L_Threshold.ClearValue(TextBlock.ForegroundProperty);
    }

    private void CAL_Date_MouseDown(object? sender, PointerPressedEventArgs e)
    {
        if ((e.KeyModifiers & KeyModifiers.Alt) != 0 && e.GetCurrentPoint(CAL_Date).Properties.IsLeftButtonPressed)
        {
            CAL_Date.SelectedDate = null;
            e.Handled = true;
        }
    }
}
