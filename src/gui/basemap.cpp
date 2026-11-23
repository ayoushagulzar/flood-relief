// this file has the code of Basemap (tile cache, downloads and drawing the tiles)
#include "gui/basemap.h"

#include <cmath>

#include "gui/theme.h"

namespace {

constexpr int kMaxDownloadsPerFrame = 2; // limits so the window never lags
constexpr int kMaxLoadsPerFrame = 2;
constexpr int kMaxPendingDownloads = 6;

// shader (small program that runs on the GPU) that makes the OSM tiles grey and pale
// so our roads and symbols are easier to see
const char* kFadeFragment = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float fade;
out vec4 finalColor;
void main() {
    vec4 c = texture(texture0, fragTexCoord);
    float lum = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    vec3 grey = mix(c.rgb, vec3(lum), 0.8);
    vec3 pale = mix(grey, vec3(1.0, 0.985, 0.985), fade);
    finalColor = vec4(pale, c.a) * fragColor * colDiffuse;
}
)";

}

// OSM asks every app to send a User-Agent with its name
Basemap::Basemap() : index_(256), downloader_("FloodRelief/1.0 (DSA student project; github.com/ayoushagulzar/flood-relief)") {}

Basemap::~Basemap() = default;

// shader can only be loaded after the window is open
void Basemap::init() {
    fadeShader_ = LoadShaderFromMemory(nullptr, kFadeFragment);
    shaderOk_ = IsShaderValid(fadeShader_);
    if (shaderOk_) {
        float f = theme::kTileFade;
        SetShaderValue(fadeShader_, GetShaderLocation(fadeShader_, "fade"), &f, SHADER_UNIFORM_FLOAT);
    }
}

// free every texture
void Basemap::unload() {
    for (std::size_t i = 0; i < tiles_.size(); ++i) {
        if (tiles_[i].state == State::Loaded) UnloadTexture(tiles_[i].texture);
    }
    tiles_.clear();
    index_.clear();
    if (shaderOk_) UnloadShader(fadeShader_);
    shaderOk_ = false;
    loaded_ = 0;
}

std::string Basemap::keyFor(int z, int x, int y) {
    return std::to_string(z) + "/" + std::to_string(x) + "/" + std::to_string(y);
}

std::string Basemap::pathFor(int z, int x, int y) { return "cache/" + keyFor(z, x, y) + ".png"; }

float Basemap::tileWorldSize(int z) { return 256.0f * std::pow(2.0f, static_cast<float>(kRefZoom - z)); }

// camera zoom 1.0 = tile zoom 6, every x2 zoom = one more tile level
int Basemap::tileZoomFor(float camZoom) const {
    int z = static_cast<int>(std::ceil(kRefZoom + std::log2(camZoom) - 0.25f));
    if (z < kMinTileZoom) z = kMinTileZoom;
    if (z > kMaxTileZoom) z = kMaxTileZoom;
    return z;
}

// finished downloads -> mark the tile as on disk or failed
void Basemap::collectDownloads() {
    net::DownloadResult r;
    while (downloader_.popResult(r)) {
        int i = -1;
        if (!index_.find(r.key, i)) continue;
        tiles_[static_cast<std::size_t>(i)].state = r.ok ? State::OnDisk : State::Failed;
    }
}

// looks up "z/x/y" in the hash table. new tile -> add it and download it if it's not in cache.
// if the png is on disk, load it as a texture
int Basemap::ensure(int z, int x, int y, int& downloadsLeft) {
    std::string key = keyFor(z, x, y);
    int i = -1;
    if (!index_.find(key, i)) {
        bool onDisk = FileExists(pathFor(z, x, y).c_str());
        if (!onDisk) {
            if (downloadsLeft <= 0 || downloader_.pending() >= kMaxPendingDownloads) return -1;
            --downloadsLeft;
            net::DownloadJob job;
            job.key = key;
            job.url = "https://tile.openstreetmap.org/" + key + ".png";
            job.path = pathFor(z, x, y);
            downloader_.enqueue(job);
        }
        Tile t;
        t.z = z;
        t.x = x;
        t.y = y;
        t.state = onDisk ? State::OnDisk : State::Queued;
        tiles_.pushBack(t);
        i = static_cast<int>(tiles_.size()) - 1;
        index_.insert(key, i);
    }
    Tile& t = tiles_[static_cast<std::size_t>(i)];
    if (t.state == State::OnDisk && loadsThisFrame_ < kMaxLoadsPerFrame) {
        ++loadsThisFrame_;
        t.texture = LoadTexture(pathFor(z, x, y).c_str());
        if (t.texture.id != 0) {
            SetTextureFilter(t.texture, TEXTURE_FILTER_BILINEAR);
            t.state = State::Loaded;
            ++loaded_;
        } else {
            t.state = State::Failed;
        }
    }
    return i;
}

// draws tile z/x/y using the texture of the same tile or a part of its parent
bool Basemap::drawTile(int z, int x, int y, int fromZ, int fromX, int fromY) {
    int i = -1;
    if (!index_.find(keyFor(fromZ, fromX, fromY), i)) return false;
    const Tile& t = tiles_[static_cast<std::size_t>(i)];
    if (t.state != State::Loaded) return false;

    int levels = z - fromZ;
    float part = 256.0f / static_cast<float>(1 << levels);
    float sx = static_cast<float>(x - (fromX << levels)) * part;
    float sy = static_cast<float>(y - (fromY << levels)) * part;
    float ws = tileWorldSize(z);
    Rectangle src{sx, sy, part, part};
    Rectangle dst{static_cast<float>(x) * ws, static_cast<float>(y) * ws, ws + 0.5f, ws + 0.5f};
    DrawTexturePro(t.texture, src, dst, {0, 0}, 0, WHITE);
    return true;
}

// draws all tiles the camera can see. if a tile is missing we draw its parent (blurry but no hole)
void Basemap::draw(const Camera2D& cam, Rectangle mapRect) {
    loadsThisFrame_ = 0;
    collectDownloads();

    int z = tileZoomFor(cam.zoom);
    int n = 1 << z;
    float ws = tileWorldSize(z);
    Vector2 tl = GetScreenToWorld2D({mapRect.x, mapRect.y}, cam);
    Vector2 br = GetScreenToWorld2D({mapRect.x + mapRect.width, mapRect.y + mapRect.height}, cam);
    int x0 = static_cast<int>(std::floor(tl.x / ws)), x1 = static_cast<int>(std::floor(br.x / ws));
    int y0 = static_cast<int>(std::floor(tl.y / ws)), y1 = static_cast<int>(std::floor(br.y / ws));
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > n - 1) x1 = n - 1;
    if (y1 > n - 1) y1 = n - 1;

    if (shaderOk_) BeginShaderMode(fadeShader_);
    int downloadsLeft = kMaxDownloadsPerFrame;
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    int radius = (x1 - x0 > y1 - y0 ? x1 - x0 : y1 - y0) / 2 + 1;
    for (int r = 0; r <= radius; ++r) {
        for (int y = cy - r; y <= cy + r; ++y) {
            for (int x = cx - r; x <= cx + r; ++x) {
                if (x < x0 || x > x1 || y < y0 || y > y1) continue;
                if (std::abs(x - cx) != r && std::abs(y - cy) != r) continue;
                ensure(z, x, y, downloadsLeft);
                if (drawTile(z, x, y, z, x, y)) continue;
                for (int up = 1; up <= 3 && z - up >= 0; ++up) {
                    if (drawTile(z, x, y, z - up, x >> up, y >> up)) break;
                }
            }
        }
    }
    if (shaderOk_) EndShaderMode();
}
