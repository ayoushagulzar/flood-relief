// basemap.cpp - OSM tile cache, downloads and drawing.
#include "gui/basemap.h"

#include <cmath>

#include "gui/theme.h"

namespace {

constexpr int kMaxDownloadsPerFrame = 2;
constexpr int kMaxLoadsPerFrame = 2;
constexpr int kMaxPendingDownloads = 6;  // OSM policy: be gentle with their servers

// OSM tiles are colourful. This shader turns them into a pale, almost
// grey map so our burgundy roads and symbols stand out on top:
//   1. mostly grey: mix the colour with its own brightness (luminance)
//   2. lighter: mix the result with near-white by `fade`
// Water stays a little darker than land, so the coast is still visible.
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

}  // namespace

Basemap::Basemap() : index_(256), downloader_("FloodRelief/1.0 (DSA student project; github.com/ayoushagulzar/flood-relief)") {}

Basemap::~Basemap() = default;

void Basemap::init() {
    fadeShader_ = LoadShaderFromMemory(nullptr, kFadeFragment);
    shaderOk_ = IsShaderValid(fadeShader_);
    if (shaderOk_) {
        float f = theme::kTileFade;
        SetShaderValue(fadeShader_, GetShaderLocation(fadeShader_, "fade"), &f, SHADER_UNIFORM_FLOAT);
    }
}

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

int Basemap::tileZoomFor(float camZoom) const {
    // camera zoom 1.0 = reference zoom 6; every doubling = one tile zoom level.
    // Rounding up (with a small bias) picks the sharper tile set: the start
    // view (camera zoom ~0.65) uses zoom 6 tiles, as planned.
    int z = static_cast<int>(std::ceil(kRefZoom + std::log2(camZoom) - 0.25f));
    if (z < kMinTileZoom) z = kMinTileZoom;
    if (z > kMaxTileZoom) z = kMaxTileZoom;
    return z;
}

void Basemap::collectDownloads() {
    net::DownloadResult r;
    while (downloader_.popResult(r)) {
        int i = -1;
        if (!index_.find(r.key, i)) continue;
        tiles_[static_cast<std::size_t>(i)].state = r.ok ? State::OnDisk : State::Failed;
    }
}

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
            t.state = State::Failed;  // corrupt file: keep the map usable
        }
    }
    return i;
}

// Draws tile (z,x,y) using the texture of tile (fromZ,fromX,fromY), which is
// either the same tile or one of its parents (then only a part of it).
bool Basemap::drawTile(int z, int x, int y, int fromZ, int fromX, int fromY) {
    int i = -1;
    if (!index_.find(keyFor(fromZ, fromX, fromY), i)) return false;
    const Tile& t = tiles_[static_cast<std::size_t>(i)];
    if (t.state != State::Loaded) return false;

    int levels = z - fromZ;          // 0 = same tile, 1 = parent ...
    float part = 256.0f / static_cast<float>(1 << levels);
    float sx = static_cast<float>(x - (fromX << levels)) * part;
    float sy = static_cast<float>(y - (fromY << levels)) * part;
    float ws = tileWorldSize(z);
    Rectangle src{sx, sy, part, part};
    // +0.5 world unit overlap hides hairline seams between tiles
    Rectangle dst{static_cast<float>(x) * ws, static_cast<float>(y) * ws, ws + 0.5f, ws + 0.5f};
    DrawTexturePro(t.texture, src, dst, {0, 0}, 0, WHITE);
    return true;
}

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
    // Centre tiles first, so the part you look at arrives first.
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    int radius = (x1 - x0 > y1 - y0 ? x1 - x0 : y1 - y0) / 2 + 1;
    for (int r = 0; r <= radius; ++r) {
        for (int y = cy - r; y <= cy + r; ++y) {
            for (int x = cx - r; x <= cx + r; ++x) {
                if (x < x0 || x > x1 || y < y0 || y > y1) continue;
                if (std::abs(x - cx) != r && std::abs(y - cy) != r) continue;  // ring only
                ensure(z, x, y, downloadsLeft);
                if (drawTile(z, x, y, z, x, y)) continue;
                for (int up = 1; up <= 3 && z - up >= 0; ++up) {  // parent fallback
                    if (drawTile(z, x, y, z - up, x >> up, y >> up)) break;
                }
            }
        }
    }
    if (shaderOk_) EndShaderMode();
}
