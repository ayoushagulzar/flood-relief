// basemap.h - real OpenStreetMap tiles behind the graph.
//
// Tile maths (Web Mercator, what OSM uses): at zoom z the world is
// 256 x 2^z pixels and is cut into 2^z x 2^z tiles of 256 x 256.
// Our world coordinates are pixels at the REFERENCE zoom (6), so a tile at
// zoom z covers 256 x 2^(6 - z) world units.
//
// Cache: HashTable "z/x/y" -> index into DynamicArray<Tile>.
//   disk:   cache/z/x/y.png  (downloaded once, reused forever)
//   memory: one Texture2D per tile that has been loaded
// Per frame we start at most 2 downloads and load at most 2 PNGs, so the
// UI never stalls. While a tile is missing we draw its nearest loaded
// parent tile, stretched (blurry for a moment, never an empty hole).
#pragma once

#include <string>

#include "raylib.h"

#include "core/dynamic_array.h"
#include "core/hash_table.h"
#include "net/http.h"

class Basemap {
public:
    static constexpr int kRefZoom = 6;
    static constexpr int kMinTileZoom = 5;
    static constexpr int kMaxTileZoom = 10;

    Basemap();
    ~Basemap();
    Basemap(const Basemap&) = delete;
    Basemap& operator=(const Basemap&) = delete;

    void init();    // after InitWindow(): loads the dark-map shader
    void unload();  // before CloseWindow(): frees textures + shader

    // Call inside BeginMode2D(cam). mapRect is the map area on screen.
    void draw(const Camera2D& cam, Rectangle mapRect);

    int tileZoomFor(float camZoom) const;
    int loadedCount() const { return loaded_; }
    int pendingDownloads() const { return downloader_.pending(); }

private:
    enum class State { Queued, OnDisk, Loaded, Failed };
    struct Tile {
        int z = 0, x = 0, y = 0;
        State state = State::Queued;
        Texture2D texture{};
    };

    static std::string keyFor(int z, int x, int y);
    static std::string pathFor(int z, int x, int y);
    static float tileWorldSize(int z);

    void collectDownloads();
    // Returns the tile index, creating the entry (and a download) if needed.
    int ensure(int z, int x, int y, int& downloadsLeft);
    bool drawTile(int z, int x, int y, int fromZ, int fromX, int fromY);

    HashTable index_;
    DynamicArray<Tile> tiles_;
    net::TileDownloader downloader_;
    Shader darkShader_{};
    bool shaderOk_ = false;
    int loadsThisFrame_ = 0;
    int loaded_ = 0;
};
