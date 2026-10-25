using Avalonia.Controls;
using Avalonia.Interactivity;
using NHSE.Core;

namespace NHSE.Mac;

public partial class VillagerDIYTimerEditor : NhseWindow
{
    private readonly List<ComboItem> Recipes = GameInfo.Strings.CreateItemDataSource(RecipeList.Recipes, false);

    private readonly IVillager Villager;

    private byte Hour, Minute, Second;
    private ushort RecipeIndex;
    private bool Loading = true;

    public VillagerDIYTimerEditor(IVillager v)
    {
        Villager = v;
        Hour = Villager.DIYEndHour;
        Minute = Villager.DIYEndMinute;
        Second = Villager.DIYEndSecond;

        InitializeComponent();

        // Both hour and minute will be 255 in most cases when villager is idling.
        // If another villager's end time is earlier, this villager will not craft UNLESS the villager is crafting an "event" item, in which case it seems it will take priority.
        bool idleState = Hour == byte.MaxValue || Minute == byte.MaxValue;
        DT_Time.SelectedTime = idleState ? TimeSpan.Zero : new TimeSpan(Hour, Minute, Second);
        Loading = false;

        CHK_Crafting.IsChecked = DT_Time.IsEnabled = !idleState;

        CB_Recipe.SetComboItems(Recipes);

        RecipeIndex = Villager.DIYRecipeIndex;
        CB_Recipe.SelectedValue = (int)RecipeIndex;
    }

    private void CB_Recipe_SelectedIndexChanged(object? sender, SelectionChangedEventArgs e)
    {
        var val = UiUtil.GetIndex(CB_Recipe);
        RecipeIndex = (ushort)Math.Max(0, Math.Min(ushort.MaxValue, val));
    }

    private void B_Cancel_Click(object? sender, RoutedEventArgs e) => Close(false);

    private void B_Save_Click(object? sender, RoutedEventArgs e)
    {
        Villager.DIYEndHour = Hour;
        Villager.DIYEndMinute = Minute;
        Villager.DIYEndSecond = Second;
        Villager.DIYRecipeIndex = RecipeIndex;
        Close(true);
    }

    private TimeSpan Time => DT_Time.SelectedTime ?? TimeSpan.Zero;

    private void CHK_Crafting_CheckedChanged(object? sender, RoutedEventArgs e)
    {
        bool crafting = CHK_Crafting.IsChecked == true;
        Hour = crafting ? (byte)Time.Hours : byte.MaxValue;
        Minute = crafting ? (byte)Time.Minutes : byte.MaxValue;
        if (crafting)
            Second = (byte)Time.Seconds;
        DT_Time.IsEnabled = crafting;
    }

    private void DT_Time_ValueChanged(object? sender, TimePickerSelectedValueChangedEventArgs e)
    {
        // WinForms also got here when the constructor set the initial time, which turned an idle timer (255) into 00:00 on save.
        if (Loading)
            return;
        Hour = (byte)Time.Hours;
        Minute = (byte)Time.Minutes;
        Second = (byte)Time.Seconds;
    }
}
