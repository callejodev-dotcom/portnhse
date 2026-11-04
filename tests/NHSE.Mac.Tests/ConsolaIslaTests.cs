using Avalonia.Controls;
using Avalonia.Headless.XUnit;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using NHSE.Core;
using Xunit;

namespace NHSE.Mac.Tests;

/// <summary>
/// La isla de NHSE para Switch (sin el mapa ni las casas): cada cambio deja la partida byte a byte como las ventanas de
/// Isla y Museo de NHSE para Mac.
/// </summary>
public partial class ConsolaTests
{
    /// <summary> Guarda una ventana de las de la isla como su botón «Hecho». </summary>
    private static void Hecho(Window w)
    {
        w.Show();
        Dispatcher.UIThread.RunJobs();
        var save = w.GetVisualDescendants().OfType<Button>().Single(z => z.IsDefault);
        save.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        Dispatcher.UIThread.RunJobs();
    }

    [Fact]
    public void IslaHemisferioComoNhse() => Both(sav => sav.Main.Hemisphere = Hemisphere.Southern, "isla-hemisferio", "1");

    [Fact]
    public void IslaAeropuertoComoNhse() => Both(sav => sav.Main.AirportThemeColor = AirportColor.Green, "isla-aeropuerto", "3");

    [AvaloniaFact]
    public void IslaFrutasYFloresComoNhse()
    {
        var (_, after) = Both(sav =>
        {
            var w = new FruitsFlowersWindow(sav.Players, 0, sav.Main);
            w.NativeFruit.Value = 2213; // manzana
            w.SecondFruit.Value = 2287; // cereza
            w.NativeFlower.Value = IslandFlowers.Rose;
            w.SecondFlower.Value = IslandFlowers.Lily;
            Hecho(w);
        }, "isla-frutas", "2213", "2287", "3", "5", "0");
        Assert.Equal(2213, after.Players[0].Personal.ProfileFruit);
    }

    [AvaloniaFact]
    public void IslaFrutaPorNumeroComoNhse()
    {
        // Una fruta que no es de las cinco (el número de objeto de «Opciones avanzadas»): ninguna bandera de fruta.
        Skip();
        Both(sav =>
        {
            var w = new FruitsFlowersWindow(sav.Players, 0, sav.Main);
            w.NativeFruit.Value = 2290;
            Hecho(w);
        }, "isla-frutas", "2290", Isla(s => s.Main.SisterFruit), Isla(s => (int)s.Main.SpecialtyFlower), Isla(s => (int)s.Main.SisterFlower), "0");
    }

    private static string Isla(Func<HorizonSave, int> leer) => leer(TestSave.LoadCopy(out _)).ToString();

    [AvaloniaFact]
    public void IslaCampistaComoNhse()
    {
        var (_, after) = Both(sav =>
        {
            var w = new CampsiteWindow(sav.Main);
            Assert.True(w.Unlocked);
            w.SetCamper("cat01");
            w.VisitDate = new DateTime(2026, 7, 20);
            Hecho(w);
        }, "isla-camping", "cat01", "2026-07-20");
        Assert.Equal(3, after.Main.CampsiteStatus);
    }

    [AvaloniaFact]
    public void IslaCampingVacioComoNhse()
    {
        Both(sav =>
        {
            var w = new CampsiteWindow(sav.Main);
            w.SetCamper(null);
            Hecho(w);
        }, "isla-camping", "nadie");
    }

    [AvaloniaFact]
    public void IslaVisitanteComoNhse()
    {
        // El lunes viene Betunio (Kicks).
        Both(sav =>
        {
            var v = sav.Main.Visitor;
            var w = new VisitorsWindow(v);
            var lunes = w.DayBox(DayOfWeek.Monday);
            lunes.SelectedItem = lunes.Items.OfType<VisitorsWindow.Choice>().Single(z => z.Value == VisitorNPC.Kicks);
            Hecho(w);
            sav.Main.Visitor = v;
        }, "isla-visitante", "1", "8");
    }

    [Fact]
    public void IslaConciertoComoNhse()
    {
        // Lo que hace la casilla del concierto de cumpleaños del sábado en VisitorsWindow.
        Both(sav =>
        {
            var v = sav.Main.Visitor;
            v.IsBirthdayLive[6] = true;
            sav.Main.Visitor = v;
        }, "isla-concierto", "6", "cumple", "1");
    }

    [AvaloniaFact]
    public void IslaTendenciaNabosComoNhse()
    {
        Both(sav =>
        {
            var t = sav.Main.Turnips;
            var w = new TurnipsWindow(t);
            w.PatternButton(TurnipPattern.SmallSpike).IsChecked = true;
            Hecho(w);
            t.FeverStart = 6; // «El pico empieza el miércoles por la mañana»
            sav.Main.Turnips = t;
        }, "isla-tendencia", "3", "6");
    }

    /// <summary> Las piezas del museo de MuseumWindow, con los nombres en español (como las enseña la Switch). </summary>
    private static (int Sala, ushort Id)[] PiezasEnEspanol()
    {
        var es = GameInfo.GetStrings("es");
        var cultura = new System.Globalization.CultureInfo("es");
        string Nombre(ushort id)
        {
            var s = es.GetItemName(new Item(id));
            return s.Length == 0 ? s : char.ToUpper(s[0], cultura) + s[1..];
        }
        return [.. Enum.GetValues<MuseumWindow.Wing>().SelectMany(wing => MuseumWindow.GetPieces(wing)
            .OrderBy(p => ItemSearchBox.Normalize(Nombre(p.Id)), StringComparer.Ordinal)
            .Select(p => ((int)wing, p.Id)))];
    }

    [Fact]
    public void IslaMuseoTieneLasPiezasDeNhse()
    {
        Skip();
        TestSave.LoadCopy(out var folder);
        var salida = Run(folder, "isla-museo-lista").Split('\n').Where(z => z.StartsWith("pieza ", StringComparison.Ordinal))
            .Select(z => z.Split(' ')).Select(z => (int.Parse(z[1]), ushort.Parse(z[2]))).ToArray();
        Assert.Equal(PiezasEnEspanol(), salida);
    }

    /// <summary> Lo mismo que marcar o desmarcar piezas en MuseumWindow y pulsar «Hecho». </summary>
    private static void Museo(HorizonSave sav, Action<MuseumWindow> cambiar)
    {
        var museum = sav.Main.Museum;
        var w = new MuseumWindow(museum, sav.Players, 0);
        cambiar(w);
        Hecho(w);
        sav.Main.Museum = museum;
    }

    private static MuseumWindow.Piece Pieza(MuseumWindow w, ushort id) =>
        Enum.GetValues<MuseumWindow.Wing>().SelectMany(w.Pieces).Single(z => z.Id == id);

    private static ushort PrimeraPieza(bool donada)
    {
        var sav = TestSave.LoadCopy(out _);
        var dadas = sav.Main.Museum.GetItems().Select(z => z.ItemId).ToHashSet();
        return PiezasEnEspanol().Select(z => z.Id).First(z => dadas.Contains(z) == donada);
    }

    [AvaloniaFact]
    public void IslaDonarComoNhse()
    {
        Skip();
        var id = PrimeraPieza(false);
        Both(sav => Museo(sav, w => w.SetDonated(Pieza(w, id), true)), "isla-donar", id.ToString(), "0");
    }

    [AvaloniaFact]
    public void IslaRetirarComoNhse()
    {
        Skip();
        var id = PrimeraPieza(true);
        Both(sav => Museo(sav, w => w.SetDonated(Pieza(w, id), false)), "isla-retirar", id.ToString());
    }

    [AvaloniaFact]
    public void IslaDonarTodoComoNhse()
    {
        // «Donar todo»: lo que falta, sala a sala y en el orden en que se ven (en español).
        Skip();
        var piezas = PiezasEnEspanol();
        Both(sav => Museo(sav, w =>
        {
            foreach (var (_, id) in piezas)
                w.SetDonated(Pieza(w, id), true);
            Assert.All(Enum.GetValues<MuseumWindow.Wing>(), wing => Assert.Equal(w.Progress(wing).Total, w.Progress(wing).Donated));
        }), "isla-donar-sala", "todo", "0");
    }

    private static int PrimeraNota()
    {
        var sav = TestSave.LoadCopy(out _);
        var w = new BulletinWindow(sav.Main);
        var notas = sav.Main.Bulletin.Stock.Buffer;
        var i = w.Shown.FirstOrDefault(z => !BulletinWindow.IsHandwritten(notas[z]) && notas[z].Body.Length > 40, -1);
        Assert.SkipWhen(i < 0, "la partida de prueba no tiene notas escritas");
        return i;
    }

    [AvaloniaFact]
    public void IslaNotaComoNhse()
    {
        Skip();
        var hueco = PrimeraNota();
        const string texto = "¡Hola, isla!\nMañana hay fiesta en la plaza ★";
        Both(sav =>
        {
            var w = new BulletinWindow(sav.Main);
            w.SetText(hueco, texto);
            w.SaveAndClose();
        }, "isla-nota", hueco.ToString(), texto.Replace("\n", "\\n"));
    }

    [AvaloniaFact]
    public void IslaNotaLargaSeCortaComoNhse()
    {
        Skip();
        var hueco = PrimeraNota();
        var texto = string.Concat(Enumerable.Repeat("Ñandú ", 50)); // 300 letras: se queda en 239
        var (_, after) = Both(sav =>
        {
            var w = new BulletinWindow(sav.Main);
            w.SetText(hueco, texto);
            w.SaveAndClose();
        }, "isla-nota", hueco.ToString(), texto);
        Assert.Equal(239, after.Main.Bulletin.Stock.Buffer[hueco].Body.Length);
    }

    [AvaloniaFact]
    public void IslaFirmaComoNhse()
    {
        // Lo que hace la casilla «Firma» de BulletinWindow.
        Skip();
        var hueco = PrimeraNota();
        Both(sav =>
        {
            var board = sav.Main.Bulletin;
            var stock = board.Stock;
            var notas = stock.Buffer;
            notas[hueco].Footer = "Carli";
            stock.Buffer = notas;
            board.Stock = stock;
            sav.Main.Bulletin = board;
        }, "isla-firma", hueco.ToString(), "Carli");
    }
}
