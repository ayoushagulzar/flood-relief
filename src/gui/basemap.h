// this file declares Basemap -> the real OpenStreetMap tiles behind the map.
// at zoom z the world is cut into 2^z x 2^z tiles of 256 x 256 pixels.
// tiles are saved in cache/z/x/y.png so they download only once.
// HashTable "z/x/y" -> index in the tiles array (my own hash table again)
#pragma once // this prevents multiple inclusions of the header file.

#include <string>

#include "raylib.h"

#include "core/dynamic_array.h"
#include "core/hash_table.h"
#include "net/http.h"

class Basemap {
public:
    static constexpr int kRefZoom = 6; // our map coordinates are pixels at zoom 6
    static constexpr int kMinTileZoom = 5;
    static constexpr int kMaxTileZoom = 10; // tiles from zoom 5 to 10

    Basemap();
    ~Basemap();
    Basemap(const Basemap&) = delete; // no copies
    Basemap& operator=(const Basemap&) = delete;

    void init(); // after the window is open
    void unload(); // before the window closes

    void draw(const Camera2D& cam, Rectangle mapRect); // draws all visible tiles

    int tileZoomFor(float camZoom) const; // camera zoom -> tile zoom
    int loadedCount() const { return loaded_; }
    int pendingDownloads() const { return downloader_.pending(); }

private:
    enum class State { Queued, OnDisk, Loaded, Failed }; // downloading -> saved -> loaded as texture
    struct Tile {
        int z = 0, x = 0, y = 0;
        State state = State::Queued;
        Texture2D texture{};
    };

    static std::string keyFor(int z, int x, int y); // "z/x/y"
    static std::string pathFor(int z, int x, int y); // "cache/z/x/y.png"
    static float tileWorldSize(int z);

    void collectDownloads(); // get finished downloads from the thread
    int ensure(int z, int x, int y, int& downloadsLeft); // finds the tile or adds it (and starts its download)
    bool drawTile(int z, int x, int y, int fromZ, int fromX, int fromY); // draws one tile (or a part of its parent tile)

    HashTable index_; // "z/x/y" -> index in tiles_
    DynamicArray<Tile> tiles_;
    net::TileDownloader downloader_;
    Shader fadeShader_{}; // makes the tiles pale
    bool shaderOk_ = false;
    int loadsThisFrame_ = 0; // max 2 per frame so it doesn't lag
    int loaded_ = 0;
};
