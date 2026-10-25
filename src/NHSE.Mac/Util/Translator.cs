using System.Reflection;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.LogicalTree;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Port of NHSE.WinForms' WinFormsTranslator: reuses the upstream <c>lang_xx.txt</c> files, whose keys are
/// <c>FormName.ControlName</c>. Windows and controls keep the WinForms names so the same keys apply.
/// </summary>
public static class Translator
{
    private static readonly Dictionary<string, TranslationContext> Context = [];

    public static void TranslateInterface(this Window form, string lang) => TranslateForm(form, GetFormName(form), GetContext(lang));

    /// <summary>
    /// Translates a control hosted by a form, using the keys of that form (for content created after the form was translated).
    /// </summary>
    public static void TranslateInterface(this Control control, string formName, string lang) => TranslateForm(control, formName, GetContext(lang));

    /// <summary>
    /// Gets a translated string for a <see cref="key"/>, or the <see cref="fallback"/> if the current language has none.
    /// </summary>
    public static string Tr(string key, string fallback) => GetContext(GameInfo.CurrentLanguage).GetTranslatedText(key, fallback);

    public static string GetFormName(Control form)
    {
        var name = form.GetType().Name;
        var tick = name.IndexOf('`');
        return tick < 0 ? name : name[..tick];
    }

    private static string GetTranslationFileNameInternal(string lang) => $"lang_{lang}";

    private static TranslationContext GetContext(string lang)
    {
        if (Context.TryGetValue(lang, out var context))
            return context;

        var lines = GetTranslationFile(lang);
        Context.Add(lang, context = new TranslationContext(lines));
        return context;
    }

    private static void TranslateForm(Control form, string formname, TranslationContext context)
    {
        if (form is Window w)
            w.Title = context.GetTranslatedText(formname, w.Title ?? string.Empty);

        foreach (var c in GetTranslatableControls(form))
        {
            switch (c)
            {
                case TextBlock t when !string.IsNullOrEmpty(t.Name) && !string.IsNullOrWhiteSpace(t.Text):
                    t.Text = context.GetTranslatedText($"{formname}.{t.Name}", t.Text);
                    break;
                case HeaderedSelectingItemsControl { Header: string h } m when !string.IsNullOrEmpty(m.Name):
                    m.Header = context.GetTranslatedText($"{formname}.{m.Name}", h);
                    break;
                case HeaderedContentControl { Header: string h } g when !string.IsNullOrEmpty(g.Name):
                    g.Header = context.GetTranslatedText($"{formname}.{g.Name}", h);
                    break;
                case HeaderedItemsControl { Header: string h } hi when !string.IsNullOrEmpty(hi.Name):
                    hi.Header = context.GetTranslatedText($"{formname}.{hi.Name}", h);
                    break;
                case ContentControl { Content: string s } cc when !string.IsNullOrEmpty(cc.Name) && !string.IsNullOrWhiteSpace(s):
                    cc.Content = context.GetTranslatedText($"{formname}.{cc.Name}", s);
                    break;
            }
        }
    }

    private static IEnumerable<Control> GetTranslatableControls(Control root)
    {
        foreach (var z in root.GetSelfAndLogicalDescendants().OfType<Control>())
        {
            // undesirable to modify, ignore
            if (z is TextBox or ComboBox or NumericUpDown or ListBox or AutoCompleteBox)
                continue;
            yield return z;

            if (z.ContextMenu is { } cm)
            {
                foreach (var m in GetMenuItems(cm.Items))
                    yield return m;
            }
            if (z.ContextFlyout is MenuFlyout cf)
            {
                foreach (var m in GetMenuItems(cf.Items))
                    yield return m;
            }
            if (z is Button { Flyout: MenuFlyout bf })
            {
                foreach (var m in GetMenuItems(bf.Items))
                    yield return m;
            }
        }
    }

    private static IEnumerable<MenuItem> GetMenuItems(IEnumerable<object?> items)
    {
        foreach (var i in items.OfType<MenuItem>())
        {
            yield return i;
            foreach (var sub in GetMenuItems(i.Items))
                yield return sub;
        }
    }

    private static string[] GetTranslationFile(string lang)
    {
        var file = GetTranslationFileNameInternal(lang);
        if (ResourceUtil.IsStringListCached(file, out var result))
            return result;
        var txt = EmbeddedResources.GetText($"lang/{file}.txt");
        if (txt is null)
            return [];
        return ResourceUtil.LoadStringList(file, txt);
    }
}

/// <summary>
/// Copy of NHSE.WinForms' TranslationContext (key=value lookup).
/// </summary>
public sealed class TranslationContext
{
    public const char Separator = '=';
    private readonly Dictionary<string, string> Translation = [];

    public TranslationContext(string[] content, char separator = Separator)
    {
        foreach (var line in content)
        {
            var index = line.IndexOf(separator);
            if (index < 0)
                continue;
            var key = line[..index];
            Translation.TryAdd(key, line[(index + 1)..]);
        }
    }

    public string GetTranslatedText(string val, string fallback) => Translation.GetValueOrDefault(val, fallback);
}

/// <summary>
/// Access to the resources embedded from the NHSE submodule (sprites, translations, map assets).
/// </summary>
public static class EmbeddedResources
{
    private static readonly Assembly Assembly = typeof(EmbeddedResources).Assembly;
    private static readonly HashSet<string> Names = [.. Assembly.GetManifestResourceNames()];

    public static bool Exists(string name) => Names.Contains(name);

    public static Stream? Open(string name) => Names.Contains(name) ? Assembly.GetManifestResourceStream(name) : null;

    public static string? GetText(string name)
    {
        using var s = Open(name);
        if (s is null)
            return null;
        using var reader = new StreamReader(s);
        return reader.ReadToEnd();
    }

    public static IEnumerable<string> GetNames(string prefix) => Names.Where(z => z.StartsWith(prefix, StringComparison.Ordinal));
}
