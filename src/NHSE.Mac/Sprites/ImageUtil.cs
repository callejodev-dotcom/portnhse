using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Port of NHSE.Sprites' ImageUtil: the pixel buffers are the same 32bpp ARGB ints (BGRA bytes in memory),
/// written into Avalonia <see cref="WriteableBitmap"/>s instead of GDI+ bitmaps.
/// </summary>
public static class ImageUtil
{
    extension(DesignPattern bg)
    {
        public WriteableBitmap GetImage() => GetBitmap(bg.GetBitmap(), DesignPattern.Width, DesignPattern.Height);

        public WriteableBitmap GetPalette() => GetBitmap24(bg.GetPaletteBitmap(), DesignPattern.PaletteColorCount, 1);
    }

    extension(DesignPatternPRO bg)
    {
        public WriteableBitmap GetImage(int sheet) => GetBitmap(bg.GetBitmap(sheet), DesignPatternPRO.Width, DesignPatternPRO.Height);

        public WriteableBitmap GetPalette() => GetBitmap24(bg.GetPaletteBitmap(), DesignPatternPRO.PaletteColorCount, 1);
    }

    extension(WriteableBitmap bmp)
    {
        /// <summary>
        /// Copies the bitmap's pixels into <see cref="data"/> (width * height ints).
        /// </summary>
        public void GetBitmapData(Span<int> data)
        {
            using var fb = bmp.Lock();
            var w = fb.Size.Width;
            for (int y = 0; y < fb.Size.Height; y++)
            {
                var row = GetRow(fb, y);
                MemoryMarshal.Cast<byte, int>(row)[..w].CopyTo(data.Slice(y * w, w));
            }
        }

        /// <summary>
        /// Copies <see cref="data"/> (width * height ints) into the bitmap's pixels.
        /// </summary>
        public void SetBitmapData(ReadOnlySpan<int> data)
        {
            using var fb = bmp.Lock();
            var w = fb.Size.Width;
            for (int y = 0; y < fb.Size.Height; y++)
            {
                var row = GetRow(fb, y);
                data.Slice(y * w, w).CopyTo(MemoryMarshal.Cast<byte, int>(row));
            }
        }

        public void SetBitmapData(ReadOnlySpan<byte> data) => bmp.SetBitmapData(MemoryMarshal.Cast<byte, int>(data));

        /// <summary>
        /// Gets a copy of the pixels as a new array.
        /// </summary>
        public int[] GetPixels()
        {
            var result = new int[bmp.PixelSize.Width * bmp.PixelSize.Height];
            bmp.GetBitmapData(result);
            return result;
        }
    }

    private static unsafe Span<byte> GetRow(ILockedFramebuffer fb, int y)
        => new((byte*)fb.Address + ((long)y * fb.RowBytes), fb.Size.Width * 4);

    public static WriteableBitmap Create(int width, int height)
        => new(new PixelSize(width, height), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Unpremul);

    public static WriteableBitmap GetBitmap(ReadOnlySpan<int> data, int width, int height)
    {
        var bmp = Create(width, height);
        bmp.SetBitmapData(data);
        return bmp;
    }

    public static WriteableBitmap GetBitmap(ReadOnlySpan<byte> data, int width, int height)
        => GetBitmap(MemoryMarshal.Cast<byte, int>(data), width, height);

    /// <summary>
    /// 24bpp BGR data (Format24bppRgb in GDI+) to an opaque bitmap.
    /// </summary>
    public static WriteableBitmap GetBitmap24(ReadOnlySpan<byte> data, int width, int height)
    {
        var pixels = new int[width * height];
        for (int i = 0; i < pixels.Length; i++)
            pixels[i] = unchecked((int)0xFF000000) | (data[(i * 3) + 2] << 16) | (data[(i * 3) + 1] << 8) | data[i * 3];
        return GetBitmap(pixels, width, height);
    }

    public static bool ContainsTransparentPixels(WriteableBitmap image)
    {
        foreach (var p in image.GetPixels())
        {
            if ((uint)p >> 24 < 255) // check alpha
                return true;
        }
        return false;
    }

    /// <summary>
    /// Decodes an image file (png, jpg…) to 32bpp ARGB pixels, scaled to <see cref="width"/> x <see cref="height"/>.
    /// </summary>
    public static int[] GetPixels(Stream stream, int width, int height)
    {
        using var src = new Bitmap(stream);
        using var rt = new RenderTargetBitmap(new PixelSize(width, height));
        using (var ctx = rt.CreateDrawingContext())
            ctx.DrawImage(src, new Rect(0, 0, width, height));
        // The render target is premultiplied RGBA: let Avalonia convert it into a BGRA (ARGB ints) unpremultiplied bitmap.
        using var result = Create(width, height);
        using (var fb = result.Lock())
            rt.CopyPixels(fb);
        return result.GetPixels();
    }

    public static int[] ScalePixelImage(ReadOnlySpan<int> data, int imgScale, int imgWidthSingle, int imgHeightSingle, out int imgWidthUpscaled, out int imgHeightUpscaled)
    {
        imgWidthUpscaled = imgScale * imgWidthSingle;
        imgHeightUpscaled = imgScale * imgHeightSingle;
        var scaled = new int[imgWidthUpscaled * imgHeightUpscaled];
        ScalePixelImage(data, scaled, imgWidthUpscaled, imgHeightUpscaled, imgScale);
        return scaled;
    }

    public static void ScalePixelImage(ReadOnlySpan<int> data, Span<int> scaled, int imgWidth, int imgHeight, int imgScale)
    {
        // For each pixel, copy to the X indexes, then block copy the row to the other rows.
        int i = 0;
        for (int y = 0; y < imgHeight; y += imgScale)
        {
            // Fill the X pixels
            var baseIndex = y * imgWidth;
            for (int x = 0; x < imgWidth; x += imgScale)
            {
                var v = data[i];
                var xi = baseIndex + x;
                for (int x1 = 0; x1 < imgScale; x1++)
                    scaled[xi + x1] = v;
                i++;
            }

            // Copy entire pixel row down
            for (int y1 = 1; y1 < imgScale; y1++)
            {
                var src = scaled.Slice(baseIndex, imgWidth);
                var dest = scaled.Slice(baseIndex + (y1 * imgWidth), imgWidth);
                src.CopyTo(dest);
            }
        }
    }

    /// <summary>
    /// Sets a bitwise and of the requested transparency; this is assuming the pixel value is 0xFF_xx_xx_xx. Single operation laziness!
    /// </summary>
    public static void ClampAllTransparencyTo(Span<int> data, int trans)
    {
        for (int i = 0; i < data.Length; i++)
            data[i] &= trans;
    }

    public static void ClampAllTransparencyTo(Span<byte> data, byte trans)
    {
        for (int i = 0; i < data.Length; i += 4)
            data[i + 3] &= trans;
    }

    /// <summary>
    /// Fills a rectangle of an ARGB pixel buffer, clipped to the image (GDI+ FillRectangle replacement).
    /// </summary>
    public static void FillRectangle(Span<int> data, int imgWidth, int imgHeight, int x, int y, int width, int height, int color)
    {
        var x0 = Math.Max(0, x);
        var y0 = Math.Max(0, y);
        var x1 = Math.Min(imgWidth, x + width);
        var y1 = Math.Min(imgHeight, y + height);
        if (x0 >= x1 || y0 >= y1)
            return;

        var alpha = (uint)color >> 24;
        for (int py = y0; py < y1; py++)
        {
            var row = data.Slice((py * imgWidth) + x0, x1 - x0);
            if (alpha == 0xFF)
                row.Fill(color);
            else
                BlendInto(row, color, alpha);
        }
    }

    /// <summary>
    /// Draws a 1px rectangle outline, clipped to the image (GDI+ DrawRectangle replacement).
    /// </summary>
    public static void DrawRectangle(Span<int> data, int imgWidth, int imgHeight, int x, int y, int width, int height, int color)
    {
        FillRectangle(data, imgWidth, imgHeight, x, y, width + 1, 1, color);
        FillRectangle(data, imgWidth, imgHeight, x, y + height, width + 1, 1, color);
        FillRectangle(data, imgWidth, imgHeight, x, y, 1, height + 1, color);
        FillRectangle(data, imgWidth, imgHeight, x + width, y, 1, height + 1, color);
    }

    private static void BlendInto(Span<int> row, int color, uint alpha)
    {
        var inv = 255 - alpha;
        uint sr = ((uint)color >> 16) & 0xFF, sg = ((uint)color >> 8) & 0xFF, sb = (uint)color & 0xFF;
        for (int i = 0; i < row.Length; i++)
        {
            var d = (uint)row[i];
            var r = ((sr * alpha) + ((d >> 16 & 0xFF) * inv)) / 255;
            var g = ((sg * alpha) + ((d >> 8 & 0xFF) * inv)) / 255;
            var b = ((sb * alpha) + ((d & 0xFF) * inv)) / 255;
            var a = Math.Max(alpha, d >> 24);
            row[i] = (int)((a << 24) | (r << 16) | (g << 8) | b);
        }
    }
}
