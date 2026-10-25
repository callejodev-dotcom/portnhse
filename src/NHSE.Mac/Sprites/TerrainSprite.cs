using System.Drawing;
using Avalonia.Media.Imaging;
using NHSE.Core;

namespace NHSE.Mac;

/// <summary>
/// Text to draw over a map viewport (building and terrain tile names), centered at <see cref="X"/>, <see cref="Y"/>.
/// GDI+ drew these into the bitmap; the Avalonia map control draws them on top of it.
/// </summary>
public readonly record struct MapLabel(string Text, int X, int Y, byte Alpha);

/// <summary>
/// Logic to build a viewport render of a subsection of a map (or the entire "map", assuming it is small enough).
/// Port of NHSE.Sprites' TerrainSprite: rectangles are written into the pixel buffer, text is returned as <see cref="MapLabel"/>s.
/// </summary>
public static class TerrainSprite
{
    private static readonly int Selected = Color.Red.ToArgb();
    private static readonly int Others = Color.Yellow.ToArgb();
    private static readonly int PlazaColor = Color.RosyBrown.ToArgb();

    public const int ColorOcean = unchecked((int)0xFF80D7C3);
    private const int ColorGrid1 = unchecked((int)0xFF888888u); // lighter
    private const int ColorGrid2 = unchecked((int)0xFF666666u); // darker

    // 6x5 in a 16x16 acre scale. Since we are in 32x32 scale for items, our upscale is "double".
    // Tiles are always rendered as 16x16 squares, to match the precomputed tile appearance bitmaps.
    private const int TileScale = 16;
    private const int TilesPerViewport = 16;

    // To display in a 32x32 viewport from 16x16
    private const int Scale = 2;
    private const int PlazaWidth = 6 * Scale;
    private const int PlazaHeight = 5 * Scale;

    private static void GenerateMapTerrainAndUpscale(int width, int height, MapMutator mut, Span<int> scale1, Span<int> scaleX, int imgScale)
    {
        // Load the terrain pixels, then upscale.
        var mgr = mut.Manager.LayerTerrain;
        LoadTerrainPixels(mgr, mut.Manager.ConfigTerrain, scale1);
        ImageUtil.ScalePixelImage(scale1, scaleX, width, height, imgScale);
    }

    /// <summary>
    /// Draws the map with all buildings and the plaza overlay onto the specified bitmap.
    /// </summary>
    /// <param name="map">The bitmap on which the map, buildings, and plaza will be rendered.</param>
    /// <param name="m">The map editor instance containing map data, building information, and scaling parameters.</param>
    /// <param name="scale1">Buffer for the 1px per tile terrain.</param>
    /// <param name="scaleX">Buffer for the upscaled terrain.</param>
    /// <param name="buildingIndex">The index of a specific building to highlight, or -1.</param>
    /// <returns>The same instance as the input bitmap.</returns>
    public static WriteableBitmap GetMapWithBuildings(WriteableBitmap map, MapEditor m, Span<int> scale1, Span<int> scaleX, int buildingIndex = -1)
    {
        var imgScale = m.MapScale * 2; // because terrain is 16px per tile, items are 32px per tile
        var (w, h) = (map.PixelSize.Width, map.PixelSize.Height);
        GenerateMapTerrainAndUpscale(w, h, m.Mutator, scale1, scaleX, imgScale);

        var plaza = m.Mutator.Manager.Plaza;
        DrawMapPlaza(scaleX, w, h, (ushort)plaza.X, (ushort)plaza.Z, imgScale);
        DrawMapBuildings(scaleX, w, h, m.Buildings.Buildings, imgScale, buildingIndex);
        map.SetBitmapData(scaleX);
        return map;
    }

    /// <summary>
    /// Renders the current map viewport onto the specified bitmap, including terrain, buildings and grid overlays.
    /// </summary>
    /// <returns>Building and terrain tile names to draw over the viewport.</returns>
    public static List<MapLabel> LoadViewport(WriteableBitmap img, MapEditor m,
        Span<int> scale1, Span<int> scaleX,
        int selectedBuildingIndex, byte transparencyBuilding, byte transTerrain)
    {
        // Convert from absolute to relative.
        var cfg = m.Mutator.Manager.ConfigTerrain;
        int absX = m.X / 2; // 32px => 16px basis
        int absY = m.Y / 2; // 32px => 16px basis
        var (relX, relY) = cfg.GetCoordinatesRelative(absX, absY);

        SetViewTerrainPixels(m.Terrain, cfg, relX, relY, scale1, scaleX, Scale);

        var (w, h) = (img.PixelSize.Width, img.PixelSize.Height);
        DrawViewPlaza(scaleX, w, h, m, transparencyBuilding);
        DrawViewBuildings(scaleX, w, h, m, selectedBuildingIndex, transparencyBuilding);

        // Apply Grid
        ItemLayerSprite.DrawGrid(scaleX, w, h, ColorGrid1, m.ViewScale); // minor
        ItemLayerSprite.DrawGrid(scaleX, w, h, ColorGrid2, m.ViewScale * 2); // major

        img.SetBitmapData(scaleX);

        var labels = new List<MapLabel>();
        // Text of Terrain Tile Names
        if (transTerrain != 0)
            AddViewTerrainTileNames(labels, m.Terrain, cfg, relX, relY, m.ViewScale * 2, transTerrain);

        // Text of Building Names
        if (transparencyBuilding != 0)
            AddViewBuildingNames(labels, m, transparencyBuilding);

        return labels;
    }

    private static void AddViewBuildingNames(List<MapLabel> labels, MapEditor m, byte transparency)
    {
        foreach (var b in m.Buildings.Buildings)
        {
            var (x, y) = m.GetViewCoordinatesBuilding(b.X, b.Y);
            const int cellsAbove = -2; // Show label inside the building square, 2 cells inside.
            y -= (m.ViewScale * cellsAbove);

            var name = b.BuildingType.ToString();
            labels.Add(new MapLabel(name, x, y - (m.ViewScale * 2), transparency));
        }
    }

    private static void DrawViewBuildings(Span<int> data, int w, int h, MapEditor m, int selectedBuildingIndex, byte transBuild)
    {
        var buildings = m.Buildings.Buildings;
        for (var i = 0; i < buildings.Count; i++)
        {
            var b = buildings[i];
            if (b.BuildingType == BuildingType.None)
                continue;
            var color = selectedBuildingIndex == i ? Selected : Others;
            if (transBuild != byte.MaxValue)
                color = (color & 0x00FFFFFF) | (transBuild << 24);
            DrawViewBuilding(data, w, h, m, b, color, m.ViewScale);
        }
    }

    private static void LoadTerrainPixels(LayerTerrain mgr, LayerPositionConfig cfg, Span<int> pixels)
    {
        var (shiftX, shiftY) = cfg.GetCoordinatesAbsolute();

        // Iterate through the relative positions within the layer.
        // Then, map to absolute positions in the bitmap with the configured shift.
        var width = cfg.LayerTotalWidth;
        var height = cfg.LayerTotalHeight;
        var mapWidth = cfg.MapTotalWidth; // 1px scale

        // Populate the image, with each pixel being a single tile.
        // Only need to render the layer's width/height, as the rest is ocean/unable to be changed.
        for (int y = 0; y < height; y++)
        {
            var absY = y + shiftY;
            for (int x = 0; x < width; x++)
            {
                var absX = x + shiftX;
                var color = mgr.GetTileColor(x, y, 0, 0);
                var offset = (absY * mapWidth) + absX;
                pixels[offset] = color;
            }
        }

        var mapHeight = cfg.MapTotalHeight;
        // Render each exterior acre, just in case it was changed from a sea acre.
        // If the acre (x,y) is a customizable terrain acre, skip it since we already did it above.
        for (int y = 0; y < mapHeight; y += TileScale)
        {
            for (int x = 0; x < mapWidth; x += TileScale)
            {
                var (relX, relY) = cfg.GetCoordinatesRelative(x, y);
                if (cfg.IsCoordinateValidRelative(relX, relY))
                    continue; // already done

                // Render the acre template (usually sea).
                var acreTemplate = mgr.GetAcreTemplate(relX, relY);
                for (int tileY = 0; tileY < TileScale; tileY++)
                {
                    for (int tileX = 0; tileX < TileScale; tileX++)
                    {
                        var color = AcreTileColor.GetAcreTileColor(acreTemplate, tileX, tileY);
                        if (color == -0x1000000) // transparent (dynamic)
                            color = Color.ForestGreen.ToArgb(); // just in case; it's invalid anyway.
                        var offset = ((y + tileY) * mapWidth) + (x + tileX);
                        pixels[offset] = color;
                    }
                }
            }
        }
    }

    private static void DrawMapPlaza(Span<int> data, int w, int h, ushort px, ushort py, int imgScale)
    {
        int width = imgScale * PlazaWidth;
        int height = imgScale * PlazaHeight;
        ImageUtil.FillRectangle(data, w, h, px, py, width, height, PlazaColor);
    }

    private static void DrawMapBuildings(Span<int> data, int w, int h, IReadOnlyList<Building> buildings, int imgScale, int selectedBuildingIndex = -1)
    {
        for (int i = 0; i < buildings.Count; i++)
        {
            var b = buildings[i];
            if (b.BuildingType == 0)
                continue;

            var (width, height) = b.BuildingType.GetDimensions();
            var color = selectedBuildingIndex == i ? Selected : Others;
            ImageUtil.FillRectangle(data, w, h, (int)b.X, (int)b.Y, imgScale * width, imgScale * height, color);
        }
    }

    private static void DrawViewBuilding(Span<int> data, int w, int h, MapEditor m, Building b, int color, int imgScale)
    {
        var (x, y) = m.GetViewCoordinatesBuilding(b.X, b.Y);
        var type = b.BuildingType;
        var (width, height) = type.GetDimensions();
        x -= (width / 2) * m.ViewScale * 2;
        y -= (height / 2) * m.ViewScale * 2;

        // Draw the building.
        ImageUtil.FillRectangle(data, w, h, x, y, width * imgScale * 2, height * imgScale * 2, color);
    }

    private static void SetViewTerrainPixels(LayerTerrain t, LayerPositionConfig cfg, int relX, int relY, Span<int> data, Span<int> scaleX, int imgScale)
    {
        GetViewTerrain1(t, cfg, relX, relY, data);
        ImageUtil.ScalePixelImage(data, scaleX, TilesPerViewport * TileScale * imgScale, TilesPerViewport * TileScale * imgScale, imgScale);
    }

    private static void GetViewTerrain1(LayerTerrain t, LayerPositionConfig cfg, int relX, int relY, Span<int> data)
    {
        for (int tileY = 0; tileY < TilesPerViewport; tileY++)
        {
            var actY = tileY + relY;
            for (int tileX = 0; tileX < TilesPerViewport; tileX++)
            {
                var actX = relX + tileX;
                var acreTemplate = t.GetAcreTemplate(actX, actY);
                if (!cfg.IsCoordinateValidRelative(actX, actY))
                {
                    // Fill tile's square with a solid color.
                    // Ensure coordinates are positive (for modulo later).
                    if (actX < 0)
                        actX += TilesPerViewport;
                    if (actY < 0)
                        actY += TilesPerViewport;

                    for (int pixelY = 0; pixelY < TileScale; pixelY++)
                    {
                        var index = (tileY * TileScale + pixelY) * (TilesPerViewport * TileScale) + tileX * TileScale;
                        for (int pixelX = 0; pixelX < TileScale; pixelX++)
                        {
                            var color = AcreTileColor.GetAcreTileColor(acreTemplate, actX % 16, actY % 16);
                            if (color == -0x1000000) // transparent (dynamic)
                                color = Color.ForestGreen.ToArgb(); // just in case; it's invalid anyway.
                            data[index] = color;
                            index++;
                        }
                    }
                }
                else
                {
                    // Fill tile's square from terrain data.
                    var tile = t.GetTile(actX, actY);
                    for (int pixelY = 0; pixelY < TileScale; pixelY++)
                    {
                        var index = (tileY * TileScale + pixelY) * (TilesPerViewport * TileScale) + tileX * TileScale;
                        for (int pixelX = 0; pixelX < TileScale; pixelX++)
                        {
                            data[index] = t.GetTileColor(acreTemplate, tile, actX, actY, pixelX, pixelY);
                            index++;
                        }
                    }
                }
            }
        }
    }

    private static void AddViewTerrainTileNames(List<MapLabel> labels, LayerTerrain t, LayerPositionConfig cfg,
        int relX, int relY, int scale, byte transparency)
    {
        // iterate over every tile in the view
        for (int y = 0; y < TilesPerViewport; y++)
        {
            var actY = relY + y;
            var centerY = (y * scale) + (scale / 2);
            for (int x = 0; x < TilesPerViewport; x++)
            {
                var actX = relX + x;
                if (!cfg.IsCoordinateValidRelative(actX, actY))
                    continue;

                var tile = t.GetTile(actX, actY);
                var centerX = (x * scale) + (scale / 2);
                var name = TerrainTileColor.GetTileName(tile);
                labels.Add(new MapLabel(name, centerX, centerY, transparency));
            }
        }
    }

    private static void DrawViewPlaza(Span<int> data, int w, int h, MapEditor m, byte transparency)
    {
        var plaza = m.Mutator.Manager.Plaza;
        var (x, y) = m.GetViewCoordinatesBuilding(plaza.X, plaza.Z);

        var scale = m.ViewScale * 2;
        var width = scale * PlazaWidth;
        var height = scale * PlazaHeight;

        var color = PlazaColor;
        if (transparency != byte.MaxValue)
            color = (color & 0x00FFFFFF) | (transparency << 24);
        ImageUtil.FillRectangle(data, w, h, x, y, width, height, color);
    }
}
