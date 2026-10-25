using Avalonia;
using Avalonia.Headless;
using NHSE.Mac;
using NHSE.Mac.Tests;

[assembly: AvaloniaTestApplication(typeof(TestApp))]

namespace NHSE.Mac.Tests;

public static class TestApp
{
    public static AppBuilder BuildAvaloniaApp() => AppBuilder.Configure<App>()
        .UseSkia()
        .UseHeadless(new AvaloniaHeadlessPlatformOptions { UseHeadlessDrawing = false });
}
