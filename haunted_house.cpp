// =====================================================================
//  3D HAUNTED HOUSE - "The Last Lantern"   (OpenGL + GLU + GLUT/freeglut)
//  Chapter I : Blackwood Manor  (gothic Victorian mansion edition)
// ---------------------------------------------------------------------
//  GOAL : Find the 3 golden keys hidden in the mansion, then escape
//         through the iron gate behind you (south of the yard).
//  DANGER: Ghosts hunt you. Shine your flashlight on them to drive
//          them back. Darkness and ghost touches drain your SANITY.
//
//  CONTROLS
//   W A S D  : move            SPACE (hold) : sprint (uses stamina)
//   Arrows   : look around     F : flashlight      E : open/close door
//   M        : minimap         H : help            P : pause
//   ENTER    : start/restart   R : restart         Q / ESC : quit
//
//  Build (Linux):   g++ haunted_house.cpp -o haunted -lGL -lGLU -lglut
//  Build (MinGW):   g++ haunted_house.cpp -o haunted -lfreeglut -lopengl32 -lglu32
//
//  All textures (stone, shingles, wood, wallpaper, grass, mist) are
//  generated procedurally at start-up - no image files are needed.
// =====================================================================
#include <GL/glut.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <vector>

const float PI = 3.14159265f;
float rad(float d) { return d * PI / 180.0f; }
float frand(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }

// ------------------------------------------------------------ game state
enum GameState { MENU, PLAYING, PAUSED, WON, LOST };
GameState state = MENU;

int winW = 1100, winH = 750;

// player
float px, py, pz, yaw, pitch, bobPhase;
float sanity, battery, stamina;
bool flashlight, doorOpen, showHelp = true, showMap = true;
float doorAngle;
int keyCount;
float playTime;
float hurtFlash;

// world fx (all mutated only in update(), never in rendering)
float flicker = 0.7f, lightning = 0.0f, nextLightning = 5.0f, gateAngle = 0.0f;
float insideFactor = 0.0f;          // 0 = outside, 1 = inside the mansion (smoothed)
float shakeX = 0.0f, shakeY = 0.0f; // camera shake offsets
float timeSec = 0.0f;               // animation clock
int lastTime = 0;

bool keyDownArr[256] = { false };
bool arrows[256] = { false };

char statusMessage[160] = "";
float statusTimer = 0.0f;

void setStatus(const char* msg, float seconds = 3.0f)
{
    snprintf(statusMessage, sizeof(statusMessage), "%s", msg);
    statusTimer = seconds;
}

// ------------------------------------------------------------ textures
GLuint texStone, texShingle, texWood, texWallpaper, texGround, texFog;
GLUquadric* quad = nullptr;

// ------------------------------------------------------------ collision
struct AABB { float x0, z0, x1, z1; };
std::vector<AABB> walls;
const AABB DOOR_BOX = { -1.0f, 4.89f, 1.0f, 5.11f };
const AABB GATE_BOX = { -1.5f, 14.85f, 1.5f, 15.15f };

// ------------------------------------------------------------ items
enum ItemType { ITEM_KEY, ITEM_BATTERY };
struct Item { float x, y, z; ItemType type; bool taken; const char* note; };
std::vector<Item> items;

// ------------------------------------------------------------ ghosts
struct Ghost {
    float x, y, z;
    float speed;
    int wp;
    float stun, alpha, hitCooldown;
    bool active;
};
Ghost ghosts[2];

const float WAYPOINTS[][2] = {
    {0,1}, {3.5f,3.5f}, {-3.0f,3.0f}, {-3.6f,-1.6f}, {3.6f,-1.6f},
    {0,-4}, {-3.9f,-4.2f}, {3.9f,-4.2f}, {0,8.5f}, {-6,9}, {4,9}
};
const int WP_COUNT = sizeof(WAYPOINTS) / sizeof(WAYPOINTS[0]);

// gravestones: x, z, tilt, type (0 = rounded slab, 1 = cross)
const float GRAVES[][4] = {
    {-6,10,8,0}, {-4.3f,12.2f,-6,1}, {6.2f,10.8f,4,0}, {4.6f,12.8f,-10,1},
    {-7.5f,7.8f,15,0}, {-5.6f,13.6f,0,1}, {7.4f,9.0f,-5,1}, {-9.0f,9.8f,3,0}
};
const int GRAVE_COUNT = sizeof(GRAVES) / sizeof(GRAVES[0]);
// trees: x, z, scale, leafy(1) / bare(0)
const float TREES[][4] = {
    {-10,2,1.4f,0}, {11.5f,6,1.7f,1}, {-9,-7,1.2f,0}, {10.5f,-7,1.3f,1},
    {-12,11,1.4f,1}, {12,12.5f,1.2f,0}, {-13,-1,1.6f,1}, {13,0,1.3f,0}
};
const int TREE_COUNT = sizeof(TREES) / sizeof(TREES[0]);

// windows: x, y, z, rotationY, width, height, glow (0 = dark), arched
struct WindowDef { float x, y, z, rot, w, h, glow; bool arched; };
const WindowDef WINDOWS[] = {
    // ground floor front (under the porch)
    {-2.7f, 2.4f, 5.11f, 0, 0.9f, 1.15f, 0.65f, true},
    { 2.9f, 2.4f, 5.11f, 0, 0.9f, 1.15f, 0.80f, true},
    // second floor front
    {-2.7f, 6.2f, 5.11f, 0, 0.8f, 1.3f, 0.00f, true},
    {-0.8f, 6.2f, 5.11f, 0, 0.8f, 1.3f, 0.00f, true},
    { 1.0f, 6.2f, 5.11f, 0, 0.8f, 1.3f, 0.85f, true},
    { 2.6f, 6.2f, 5.11f, 0, 0.8f, 1.3f, 0.95f, true},
    // left side
    {-5.11f, 2.4f, -1.5f, -90, 0.8f, 1.1f, 0.40f, true},
    {-5.11f, 6.2f, -2.0f, -90, 0.8f, 1.3f, 0.00f, true},
    {-5.11f, 6.2f,  1.5f, -90, 0.8f, 1.3f, 0.70f, true},
    // right side (above the east wing)
    { 5.11f, 6.4f, -2.5f, 90, 0.8f, 1.2f, 0.75f, true},
    { 5.11f, 6.4f,  0.5f, 90, 0.8f, 1.2f, 0.00f, true},
    // east wing side
    { 9.0f, 1.9f, -1.0f, 90, 0.8f, 1.2f, 0.75f, true},
    { 9.0f, 1.9f, -3.0f, 90, 0.8f, 1.2f, 0.00f, true},
    // back
    {-2.0f, 6.2f, -5.11f, 180, 0.8f, 1.3f, 0.00f, true},
    { 2.0f, 2.4f, -5.11f, 180, 0.8f, 1.1f, 0.00f, true},
    // main tower (faces front-left)
    {-5.92f, 2.2f, 5.92f, -45, 0.6f, 1.0f, 0.60f, true},
    {-5.92f, 5.6f, 5.92f, -45, 0.6f, 1.1f, 0.85f, true},
    {-5.92f, 8.4f, 5.92f, -45, 0.5f, 0.9f, 0.00f, true},
    // small tower on the east wing
    { 8.2f, 4.6f, 2.82f, 0, 0.5f, 0.9f, 0.70f, true},
    { 8.2f, 6.6f, 2.82f, 0, 0.45f, 0.8f, 0.00f, true},
    // bay window
    { 6.3f, 1.7f, 2.81f, 0, 1.0f, 1.8f, 0.95f, true},
    { 5.49f, 1.7f, 2.4f, -90, 0.4f, 1.6f, 0.80f, false},
    { 7.11f, 1.7f, 2.4f, 90, 0.4f, 1.6f, 0.80f, false},
};
const int WINDOW_COUNT = sizeof(WINDOWS) / sizeof(WINDOWS[0]);

// precomputed decorative data (filled in init, never randomised while drawing)
float starDir[120][3];
float embers[40][3];

// =====================================================================
//  PROCEDURAL TEXTURES
// =====================================================================
float hash2(int x, int y, int seed)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + (unsigned)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xffff) / 65535.0f;
}
float valueNoise(float x, float y, int period, int seed)
{
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    auto w = [period](int a) { return ((a % period) + period) % period; };
    float a = hash2(w(xi), w(yi), seed), b = hash2(w(xi + 1), w(yi), seed);
    float c = hash2(w(xi), w(yi + 1), seed), d = hash2(w(xi + 1), w(yi + 1), seed);
    return a + (b - a) * sx + (c - a) * sy + (a - b - c + d) * sx * sy;
}
float fbm(float x, float y, int period, int seed)
{
    float s = 0, amp = 0.5f, tot = 0;
    for (int o = 0; o < 4; o++) {
        s += amp * valueNoise(x, y, period, seed + o);
        tot += amp; x *= 2; y *= 2; period *= 2; amp *= 0.5f;
    }
    return s / tot;
}
unsigned char toByte(float v) { return (unsigned char)(std::max(0.0f, std::min(1.0f, v)) * 255); }

GLuint uploadTexture(int size, int channels, const std::vector<unsigned char>& data)
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLenum fmt = channels == 1 ? GL_LUMINANCE : GL_LUMINANCE_ALPHA;
    gluBuild2DMipmaps(GL_TEXTURE_2D, channels, size, size, fmt, GL_UNSIGNED_BYTE, data.data());
    return id;
}

void buildTextures()
{
    const int S = 256;
    std::vector<unsigned char> img(S * S), img2(S * S * 2);

    // cut stone blocks (slate-grey mansion walls)
    for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) {
        int row = y / 32, off = (row % 2) * 32;
        int lx = (x + off) % 64, ly = y % 32;
        bool mortar = lx < 3 || ly < 3;
        float blockVar = hash2(((x + off) % S) / 64, row, 11) * 0.22f;
        float n = fbm(x / 16.0f, y / 16.0f, 16, 3);
        float v = mortar ? 0.28f + 0.1f * n : 0.52f + blockVar + 0.28f * (n - 0.5f);
        if (!mortar && (lx < 5 || ly < 5)) v *= 0.85f;   // bevel shading
        img[y * S + x] = toByte(v);
    }
    texStone = uploadTexture(S, 1, img);

    // roof shingles
    for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) {
        int row = y / 16, off = (row % 2) * 16;
        int lx = (x + off) % 32, ly = y % 16;
        float var = hash2(((x + off) % S) / 32, row, 21) * 0.2f;
        float n = fbm(x / 32.0f, y / 32.0f, 8, 5);
        float v = 0.3f + 0.35f * (ly / 16.0f) + var + 0.2f * (n - 0.5f);
        if (ly < 2 || lx < 1) v = 0.12f;
        img[y * S + x] = toByte(v);
    }
    texShingle = uploadTexture(S, 1, img);

    // wooden planks
    for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) {
        int plank = y / 32, ly = y % 32;
        float n = fbm(x / 32.0f, y / 8.0f, 8, 7);
        float grain = 0.5f + 0.15f * std::sin(x / (float)S * 2 * PI * 4 + n * 9 + plank);
        float v = grain + hash2(plank, 0, 31) * 0.15f - 0.07f;
        if (ly < 2) v = 0.12f;
        img[y * S + x] = toByte(v);
    }
    texWood = uploadTexture(S, 1, img);

    // faded damask wallpaper
    for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) {
        float stripe = ((x / 16) % 2) ? 0.62f : 0.55f;
        float dx = (x % 32) - 16.0f, dy = (y % 48) - 24.0f;
        float motif = (std::fabs(dx) * 1.5f + std::fabs(dy) < 14) ? -0.08f : 0.0f;
        float stain = fbm(x / 64.0f, y / 64.0f, 4, 9);
        img[y * S + x] = toByte(stripe + motif - 0.3f * std::max(0.0f, stain - 0.5f));
    }
    texWallpaper = uploadTexture(S, 1, img);

    // grass / earth
    for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) {
        float n = fbm(x / 32.0f, y / 32.0f, 8, 13);
        float fine = hash2(x, y, 17);
        img[y * S + x] = toByte(0.3f + 0.5f * n + 0.18f * fine);
    }
    texGround = uploadTexture(S, 1, img);

    // soft mist (luminance + alpha)
    for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) {
        float n = fbm(x / 32.0f, y / 32.0f, 8, 23);
        img2[(y * S + x) * 2] = 255;
        img2[(y * S + x) * 2 + 1] = toByte((n - 0.3f) * 2.0f);
    }
    texFog = uploadTexture(S, 2, img2);
}

// =====================================================================
//  WORLD SETUP
// =====================================================================
void buildColliders()
{
    walls.clear();
    // exterior walls (front wall is split for the doorway)
    walls.push_back({ -5.11f, -5.11f, -4.89f, 5.11f });
    walls.push_back({ 4.89f, -5.11f,  5.11f, 5.11f });
    walls.push_back({ -5.11f, -5.11f,  5.11f, -4.89f });
    walls.push_back({ -5.11f,  4.89f, -1.0f,  5.11f });
    walls.push_back({ 1.0f,   4.89f,  5.11f, 5.11f });
    // interior walls (with passage gaps in the middle)
    walls.push_back({ -5.0f, -0.375f, -1.2f, -0.225f });
    walls.push_back({ 1.2f, -0.375f,  5.0f, -0.225f });
    walls.push_back({ -5.0f, -2.975f, -1.0f, -2.825f });
    walls.push_back({ 1.0f, -2.975f,  5.0f, -2.825f });
    // furniture
    walls.push_back({ -3.75f, 1.6f, -2.05f, 2.6f });   // table
    walls.push_back({ 2.65f, 1.6f,  3.85f, 2.4f });   // cabinet
    walls.push_back({ -3.4f, -4.2f, -2.2f, -3.4f });   // wardrobe
    walls.push_back({ 2.5f, -4.1f,  3.1f, -3.5f });   // chest
    // mansion exterior: main tower, east wing, bay window, small tower
    walls.push_back({ -6.3f, 3.7f, -3.7f, 6.3f });
    walls.push_back({ 5.0f, -4.0f, 9.0f, 2.0f });
    walls.push_back({ 5.5f, 2.0f, 7.1f, 2.8f });
    walls.push_back({ 7.35f, 1.15f, 9.05f, 2.85f });
    // porch columns and railings
    const float cols[4] = { -3.5f, -1.5f, 1.5f, 4.7f };
    for (int i = 0; i < 4; i++) walls.push_back({ cols[i] - 0.15f, 7.05f, cols[i] + 0.15f, 7.35f });
    walls.push_back({ -3.5f, 7.1f, -1.5f, 7.3f });
    walls.push_back({ 1.5f, 7.1f,  4.7f, 7.3f });
    // fence line at z = 15 with a gate gap
    walls.push_back({ -18.0f, 14.85f, -1.5f, 15.15f });
    walls.push_back({ 1.5f, 14.85f, 18.0f, 15.15f });
    // trees and graves
    for (int i = 0; i < TREE_COUNT; i++) {
        float s = TREES[i][2] * 0.25f;
        walls.push_back({ TREES[i][0] - s, TREES[i][1] - s, TREES[i][0] + s, TREES[i][1] + s });
    }
    for (int i = 0; i < GRAVE_COUNT; i++)
        walls.push_back({ GRAVES[i][0] - 0.45f, GRAVES[i][1] - 0.2f, GRAVES[i][0] + 0.45f, GRAVES[i][1] + 0.2f });
}

void buildDecorations()
{
    for (int i = 0; i < 120; i++) {               // stars in the upper sky
        float az = frand(0, 2 * PI), el = frand(rad(18), rad(85));
        starDir[i][0] = std::cos(el) * std::sin(az);
        starDir[i][1] = std::sin(el);
        starDir[i][2] = std::cos(el) * std::cos(az);
    }
    for (int i = 0; i < 40; i++) {                // drifting embers around the graveyard
        embers[i][0] = frand(-10, 10);
        embers[i][1] = frand(0.2f, 1.6f);
        embers[i][2] = frand(7.5f, 14.5f);
    }
}

void resetGame()
{
    px = 0; py = 1.65f; pz = 11.5f; yaw = 0; pitch = 6; bobPhase = 0;
    sanity = 100; battery = 100; stamina = 100;
    flashlight = true; doorOpen = false; doorAngle = 0; gateAngle = 0;
    keyCount = 0; playTime = 0; hurtFlash = 0;

    items.clear();
    items.push_back({ -3.9f, 0.55f, -4.3f, ITEM_KEY, false, "Key 1: 'The children hid it where the wardrobe weeps...'" });
    items.push_back({ 3.25f, 1.65f, 2.0f, ITEM_KEY, false, "Key 2: Dusty, cold to the touch. Something is watching." });
    items.push_back({ 4.2f, 0.45f, -1.6f, ITEM_KEY, false, "Key 3: The final key! RUN to the iron gate!" });
    items.push_back({ -2.6f, 0.95f, 2.1f, ITEM_BATTERY, false, "Battery found (+50%)." });
    items.push_back({ 4.2f, 0.25f, -4.3f, ITEM_BATTERY, false, "Battery found (+50%)." });
    items.push_back({ -4.3f, 0.25f, 0.6f, ITEM_BATTERY, false, "Battery found (+50%)." });

    ghosts[0] = { 1.4f, 1.25f, -2.0f, 1.55f, 0, 0, 0.7f, 0, true };
    ghosts[1] = { -3.6f, 1.25f, -4.0f, 1.75f, 5, 0, 0.0f, 0, false };

    setStatus("Find 3 golden keys inside the mansion, then escape through the gate behind you.", 6);
}

// =====================================================================
//  DRAWING HELPERS
// =====================================================================
struct V3 { float x, y, z; };

// Subdivided, textured quad. Subdivision makes the spot/point lights look smooth.
void face(V3 o, V3 u, V3 v, V3 n, float lu, float lv, float tile)
{
    const float cell = 1.0f;
    int nu = std::max(1, std::min(64, (int)std::ceil(lu / cell)));
    int nv = std::max(1, std::min(64, (int)std::ceil(lv / cell)));
    glNormal3f(n.x, n.y, n.z);
    glBegin(GL_QUADS);
    for (int i = 0; i < nu; i++) for (int j = 0; j < nv; j++) {
        float a[2] = { lu * i / nu, lu * (i + 1) / nu };
        float b[2] = { lv * j / nv, lv * (j + 1) / nv };
        const int ord[4][2] = { {0,0}, {1,0}, {1,1}, {0,1} };
        for (int k = 0; k < 4; k++) {
            float aa = a[ord[k][0]], bb = b[ord[k][1]];
            glTexCoord2f(aa / tile, bb / tile);
            glVertex3f(o.x + u.x * aa + v.x * bb, o.y + u.y * aa + v.y * bb, o.z + u.z * aa + v.z * bb);
        }
    }
    glEnd();
}

void texBox(float x, float y, float z, float sx, float sy, float sz,
    float r, float g, float b, GLuint tex, float tile)
{
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, tex);
    glColor3f(r, g, b);
    glPushMatrix(); glTranslatef(x, y, z);
    float hx = sx / 2, hy = sy / 2, hz = sz / 2;
    face({ -hx,-hy, hz }, { 1,0,0 }, { 0,1,0 }, { 0,0,1 }, sx, sy, tile);
    face({ hx,-hy,-hz }, { -1,0,0 }, { 0,1,0 }, { 0,0,-1 }, sx, sy, tile);
    face({ hx,-hy, hz }, { 0,0,-1 }, { 0,1,0 }, { 1,0,0 }, sz, sy, tile);
    face({ -hx,-hy,-hz }, { 0,0,1 }, { 0,1,0 }, { -1,0,0 }, sz, sy, tile);
    face({ -hx, hy, hz }, { 1,0,0 }, { 0,0,-1 }, { 0,1,0 }, sx, sz, tile);
    face({ -hx,-hy,-hz }, { 1,0,0 }, { 0,0,1 }, { 0,-1,0 }, sx, sz, tile);
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
}

void box(float x, float y, float z, float sx, float sy, float sz, float r, float g, float b)
{
    glPushMatrix();
    glTranslatef(x, y, z); glScalef(sx, sy, sz);
    glColor3f(r, g, b); glutSolidCube(1.0);
    glPopMatrix();
}
void sphere(float x, float y, float z, float s, float r, float g, float b)
{
    glPushMatrix(); glTranslatef(x, y, z); glColor3f(r, g, b);
    glutSolidSphere(s, 20, 16); glPopMatrix();
}
void glowHalo(float x, float y, float z, float radius, float r, float g, float b, float a)
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
    glPushMatrix(); glTranslatef(x, y, z);
    glColor4f(r, g, b, a); glutSolidSphere(radius, 16, 12);
    glColor4f(r, g, b, a * 0.5f); glutSolidSphere(radius * 1.8f, 16, 12);
    glPopMatrix();
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// colours of the mansion
const float STONE[3] = { 0.58f, 0.61f, 0.66f };
const float ROOFC[3] = { 0.26f, 0.28f, 0.34f };
const float TRIM[3] = { 0.30f, 0.31f, 0.34f };

// Steep gable roof with its ridge along local X, centred at the origin.
void gableRoofX(float w, float d, float base, float h, float ov)
{
    float hw = w / 2, hd = d / 2;
    float sl = std::sqrt(h * h + hd * hd);
    float vy = h / sl, vz = hd / sl;

    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texShingle);
    glColor3f(ROOFC[0], ROOFC[1], ROOFC[2]);
    face({ -hw - ov, base - vy * ov,  hd + vz * ov }, { 1,0,0 }, { 0,vy,-vz }, { 0,vz, vy }, w + 2 * ov, sl + ov, 1.6f);
    face({ hw + ov, base - vy * ov, -hd - vz * ov }, { -1,0,0 }, { 0,vy, vz }, { 0,vz,-vy }, w + 2 * ov, sl + ov, 1.6f);

    glBindTexture(GL_TEXTURE_2D, texStone);
    glColor3f(STONE[0], STONE[1], STONE[2]);
    glBegin(GL_TRIANGLES);
    glNormal3f(1, 0, 0);
    glTexCoord2f(hd / 2, base / 2);       glVertex3f(hw, base, hd);
    glTexCoord2f(-hd / 2, base / 2);       glVertex3f(hw, base, -hd);
    glTexCoord2f(0, (base + h) / 2);       glVertex3f(hw, base + h, 0);
    glNormal3f(-1, 0, 0);
    glTexCoord2f(-hd / 2, base / 2);       glVertex3f(-hw, base, -hd);
    glTexCoord2f(hd / 2, base / 2);       glVertex3f(-hw, base, hd);
    glTexCoord2f(0, (base + h) / 2);       glVertex3f(-hw, base + h, 0);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    // dark barge boards along the gable edges + iron finials
    for (int s = -1; s <= 1; s += 2) {
        float gx = s * (hw + 0.02f);
        glDisable(GL_LIGHTING); glColor3f(0.08f, 0.08f, 0.1f); glLineWidth(3);
        glBegin(GL_LINE_STRIP);
        glVertex3f(gx, base - 0.05f, hd + 0.05f); glVertex3f(gx, base + h + 0.05f, 0); glVertex3f(gx, base - 0.05f, -hd - 0.05f);
        glEnd();
        glLineWidth(1); glEnable(GL_LIGHTING);
        box(gx, base + h + 0.4f, 0, 0.06f, 0.8f, 0.06f, 0.1f, 0.1f, 0.12f);
        sphere(gx, base + h + 0.85f, 0, 0.07f, 0.12f, 0.12f, 0.14f);
    }
}

void setTextureScale(float s, float t)
{
    glMatrixMode(GL_TEXTURE); glLoadIdentity(); glScalef(s, t, 1); glMatrixMode(GL_MODELVIEW);
}
void resetTextureMatrix()
{
    glMatrixMode(GL_TEXTURE); glLoadIdentity(); glMatrixMode(GL_MODELVIEW);
}

// Octagonal stone tower with a tall witch-hat spire.
void tower(float x, float z, float r, float height, float spireH)
{
    glPushMatrix();
    glTranslatef(x, 0, z); glRotatef(-90, 1, 0, 0);   // quadric +Z becomes world +Y
    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, texStone);
    setTextureScale(2 * PI * r / 2.0f, height / 2.0f);
    glColor3f(STONE[0], STONE[1], STONE[2]);
    gluCylinder(quad, r, r, height, 8, 10);

    // cornice ring at the top of the tower
    glPushMatrix(); glTranslatef(0, 0, height - 0.3f);
    glColor3f(TRIM[0], TRIM[1], TRIM[2]);
    gluCylinder(quad, r + 0.12f, r + 0.12f, 0.3f, 8, 1);
    glPopMatrix();

    // spire
    glTranslatef(0, 0, height);
    glBindTexture(GL_TEXTURE_2D, texShingle);
    setTextureScale(2 * PI * r / 1.4f, spireH / 1.4f);
    glColor3f(ROOFC[0], ROOFC[1], ROOFC[2]);
    gluDisk(quad, 0, r + 0.3f, 8, 1);
    gluCylinder(quad, r + 0.3f, 0.0, spireH, 8, 8);
    resetTextureMatrix();
    glDisable(GL_TEXTURE_2D);

    // iron finial
    glTranslatef(0, 0, spireH - 0.1f);
    glColor3f(0.1f, 0.1f, 0.12f);
    gluCylinder(quad, 0.05, 0.0, 1.0, 6, 1);
    glTranslatef(0, 0, 0.35f); gluSphere(quad, 0.08, 8, 6);
    glPopMatrix();
}

void chimney(float x, float z, float baseY, float h)
{
    texBox(x, baseY + h / 2, z, 0.8f, h, 0.8f, 0.45f, 0.38f, 0.36f, texStone, 1.2f);
    box(x, baseY + h + 0.08f, z, 1.0f, 0.16f, 1.0f, TRIM[0], TRIM[1], TRIM[2]);
    box(x - 0.18f, baseY + h + 0.3f, z, 0.18f, 0.4f, 0.18f, 0.3f, 0.2f, 0.18f);
    box(x + 0.18f, baseY + h + 0.3f, z, 0.18f, 0.4f, 0.18f, 0.3f, 0.2f, 0.18f);
}

// Window with stone surround, sill, mullions and (optionally) warm interior glow.
void windowAt(const WindowDef& wd, int idx)
{
    float glow = wd.glow;
    bool attic = false;
    if (glow > 0) glow *= (0.85f + 0.15f * std::sin(timeSec * 1.7f + idx * 2.3f)) * (0.75f + 0.25f * flicker);

    glPushMatrix();
    glTranslatef(wd.x, wd.y, wd.z); glRotatef(wd.rot, 0, 1, 0);
    float w = wd.w, h = wd.h;
    // surround + sill
    box(0, 0, 0.04f, w + 0.24f, h + 0.24f, 0.1f, 0.42f, 0.43f, 0.46f);
    box(0, -h / 2 - 0.15f, 0.1f, w + 0.4f, 0.08f, 0.2f, 0.5f, 0.5f, 0.52f);
    if (wd.arched) {   // stone arch cap
        glPushMatrix(); glTranslatef(0, h / 2, 0.09f);
        glColor3f(0.42f, 0.43f, 0.46f); glNormal3f(0, 0, 1);
        glBegin(GL_TRIANGLE_FAN); glVertex3f(0, 0, 0);
        for (int i = 0; i <= 12; i++) { float a = PI * i / 12; glVertex3f(std::cos(a) * (w / 2 + 0.12f), std::sin(a) * (w / 2 + 0.12f), 0); }
        glEnd(); glPopMatrix();
    }

    glDisable(GL_LIGHTING);
    float r = 0.03f, g = 0.05f, b = 0.07f;
    if (glow > 0) { r = glow; g = glow * 0.72f; b = glow * 0.3f; }
    (void)attic;
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex3f(-w / 2, -h / 2, 0.1f); glVertex3f(w / 2, -h / 2, 0.1f);
    glVertex3f(w / 2, h / 2, 0.1f);   glVertex3f(-w / 2, h / 2, 0.1f);
    glEnd();
    if (wd.arched) {
        glBegin(GL_TRIANGLE_FAN); glVertex3f(0, h / 2, 0.1f);
        for (int i = 0; i <= 12; i++) { float a = PI * i / 12; glVertex3f(std::cos(a) * w / 2, h / 2 + std::sin(a) * w / 2, 0.1f); }
        glEnd();
    }
    // mullions
    glColor3f(0.06f, 0.04f, 0.03f);
    float m = 0.025f;
    glBegin(GL_QUADS);
    glVertex3f(-m, -h / 2, 0.11f); glVertex3f(m, -h / 2, 0.11f);
    glVertex3f(m, h / 2 + (wd.arched ? w / 2 : 0), 0.11f); glVertex3f(-m, h / 2 + (wd.arched ? w / 2 : 0), 0.11f);
    glVertex3f(-w / 2, h * 0.1f - m, 0.11f); glVertex3f(w / 2, h * 0.1f - m, 0.11f);
    glVertex3f(w / 2, h * 0.1f + m, 0.11f);  glVertex3f(-w / 2, h * 0.1f + m, 0.11f);
    glEnd();
    glEnable(GL_LIGHTING);
    glPopMatrix();

    if (glow > 0.3f) {   // soft light bleeding out of lit windows
        float rr = rad(wd.rot);
        glowHalo(wd.x + std::sin(rr) * 0.2f, wd.y, wd.z + std::cos(rr) * 0.2f, std::max(w, h) * 0.45f,
            1.0f, 0.65f, 0.25f, 0.07f * glow);
    }
}

// =====================================================================
//  ENVIRONMENT
// =====================================================================
void skyDome()
{
    glDisable(GL_LIGHTING); glDisable(GL_FOG); glDepthMask(GL_FALSE);
    glPushMatrix(); glTranslatef(px, py, pz);
    const int SEG = 32, RINGS = 10; const float R = 60;
    float L = lightning * 0.35f;
    for (int r = 0; r < RINGS; r++) {
        float e0 = rad(-20 + 110.0f * r / RINGS), e1 = rad(-20 + 110.0f * (r + 1) / RINGS);
        glBegin(GL_QUAD_STRIP);
        for (int s = 0; s <= SEG; s++) {
            float az = 2 * PI * s / SEG;
            float ee[2] = { e0, e1 };
            for (int k = 0; k < 2; k++) {
                float t = std::max(0.0f, std::sin(ee[k]));
                t = std::sqrt(t);
                // teal horizon -> near-black zenith, like a misty moonlit night
                glColor3f(0.17f * (1 - t) + 0.015f * t + L, 0.25f * (1 - t) + 0.03f * t + L, 0.27f * (1 - t) + 0.05f * t + L);
                glVertex3f(R * std::cos(ee[k]) * std::sin(az), R * std::sin(ee[k]), R * std::cos(ee[k]) * std::cos(az));
            }
        }
        glEnd();
    }
    glPopMatrix();
    glDepthMask(GL_TRUE); glEnable(GL_FOG); glEnable(GL_LIGHTING);
}

void moonAndStars()
{
    glDisable(GL_LIGHTING); glDisable(GL_FOG); glDepthMask(GL_FALSE);
    glPushMatrix(); glTranslatef(px, py, pz);
    // stars
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 120; i++) {
        float tw = 0.5f + 0.5f * std::sin(timeSec * 2.0f + i * 1.7f);
        glColor3f(0.55f + 0.3f * tw, 0.6f + 0.3f * tw, 0.75f + 0.2f * tw);
        glVertex3f(starDir[i][0] * 50, starDir[i][1] * 50, starDir[i][2] * 50);
    }
    glEnd();
    // moon with a misty halo (sits behind the mansion, upper right)
    glPushMatrix(); glTranslatef(14, 22, -38);
    glColor3f(0.95f, 0.96f, 0.88f); glutSolidSphere(2.2, 28, 20);
    glColor3f(0.82f, 0.84f, 0.78f);
    glPushMatrix(); glTranslatef(-0.6f, 0.5f, 1.8f); glutSolidSphere(0.45, 12, 10); glPopMatrix();
    glPushMatrix(); glTranslatef(0.7f, -0.4f, 1.9f); glutSolidSphere(0.3, 12, 10); glPopMatrix();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(0.5f, 0.7f, 0.75f, 0.12f); glutSolidSphere(4.5, 24, 16);
    glColor4f(0.4f, 0.6f, 0.65f, 0.06f); glutSolidSphere(8.0, 24, 16);
    glDisable(GL_BLEND);
    glPopMatrix();
    glPopMatrix();
    glDepthMask(GL_TRUE); glEnable(GL_FOG); glEnable(GL_LIGHTING);
}

void ground()
{
    texBox(0, -0.12f, 0, 80, 0.24f, 80, 0.20f, 0.27f, 0.22f, texGround, 6.0f);
    // stone path from the porch steps to the gate
    for (float z = 8.1f; z < 15; z += 0.85f)
        texBox(std::sin(z) * 0.15f, 0.015f, z, 1.5f, 0.05f, 0.7f, 0.42f, 0.42f, 0.44f, texStone, 2.0f);
}

void candle(float x, float y, float z, float phase)
{
    box(x, y + 0.09f, z, 0.06f, 0.18f, 0.06f, 0.85f, 0.82f, 0.7f);
    glDisable(GL_LIGHTING);
    float f = 0.8f + 0.2f * std::sin(timeSec * 20.0f + phase);
    glPushMatrix(); glTranslatef(x, y + 0.22f, z); glScalef(0.6f, 1.3f * f, 0.6f);
    glColor3f(1.0f, 0.6f * f, 0.1f); glutSolidSphere(0.035, 10, 8);
    glPopMatrix();
    glEnable(GL_LIGHTING);
    glowHalo(x, y + 0.23f, z, 0.12f, 1.0f, 0.55f, 0.15f, 0.12f * f);
}

void door()
{
    glPushMatrix();
    glTranslatef(-1.0f, 0, 5.08f); glRotatef(doorAngle, 0, 1, 0);
    texBox(0.95f, 1.15f, 0, 1.9f, 2.3f, 0.16f, 0.32f, 0.16f, 0.1f, texWood, 1.0f);
    box(0.95f, 1.65f, 0.09f, 1.4f, 0.7f, 0.03f, 0.16f, 0.06f, 0.03f);
    box(0.95f, 0.65f, 0.09f, 1.4f, 0.7f, 0.03f, 0.16f, 0.06f, 0.03f);
    sphere(1.6f, 1.15f, 0.12f, 0.07f, 0.85f, 0.58f, 0.12f);
    glPopMatrix();
    // door frame + transom glow
    box(-1.05f, 1.25f, 5.15f, 0.14f, 2.5f, 0.12f, TRIM[0], TRIM[1], TRIM[2]);
    box(1.05f, 1.25f, 5.15f, 0.14f, 2.5f, 0.12f, TRIM[0], TRIM[1], TRIM[2]);
    box(0, 2.55f, 5.15f, 2.24f, 0.14f, 0.12f, TRIM[0], TRIM[1], TRIM[2]);
    glDisable(GL_LIGHTING);
    glColor3f(0.7f * flicker + 0.1f, 0.45f * flicker + 0.05f, 0.15f);
    glBegin(GL_TRIANGLE_FAN); glVertex3f(0, 2.62f, 5.12f);
    for (int i = 0; i <= 12; i++) { float a = PI * i / 12; glVertex3f(std::cos(a) * 0.9f, 2.62f + std::sin(a) * 0.55f, 5.12f); }
    glEnd();
    glEnable(GL_LIGHTING);
}

// Ground floor shell and interior (the playable part of the mansion)
void houseInterior()
{
    texBox(0, 0.035f, 0, 10, 0.07f, 10, 0.42f, 0.3f, 0.22f, texWood, 1.5f);   // floor

    // exterior walls; front is split to create the doorway
    texBox(-3, 2.25f, 5, 4, 4.5f, 0.22f, STONE[0], STONE[1], STONE[2], texStone, 2.0f);
    texBox(3, 2.25f, 5, 4, 4.5f, 0.22f, STONE[0], STONE[1], STONE[2], texStone, 2.0f);
    texBox(0, 3.75f, 5, 2, 1.5f, 0.22f, STONE[0], STONE[1], STONE[2], texStone, 2.0f);
    texBox(-5, 2.25f, 0, 0.22f, 4.5f, 10, STONE[0], STONE[1], STONE[2], texStone, 2.0f);
    texBox(5, 2.25f, 0, 0.22f, 4.5f, 10, STONE[0], STONE[1], STONE[2], texStone, 2.0f);
    texBox(0, 2.25f, -5, 10, 4.5f, 0.22f, STONE[0], STONE[1], STONE[2], texStone, 2.0f);

    // interior walls (wallpaper), leaving passage gaps
    texBox(-3.1f, 2.1f, -0.3f, 3.8f, 4.2f, 0.15f, 0.42f, 0.30f, 0.32f, texWallpaper, 1.5f);
    texBox(3.1f, 2.1f, -0.3f, 3.8f, 4.2f, 0.15f, 0.42f, 0.30f, 0.32f, texWallpaper, 1.5f);
    texBox(-3.0f, 2.0f, -2.9f, 4.0f, 4.0f, 0.15f, 0.30f, 0.32f, 0.40f, texWallpaper, 1.5f);
    texBox(3.0f, 2.0f, -2.9f, 4.0f, 4.0f, 0.15f, 0.30f, 0.32f, 0.40f, texWallpaper, 1.5f);

    // ceiling
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texWood);
    glColor3f(0.18f, 0.13f, 0.1f);
    face({ -5, 4.48f, -5 }, { 1,0,0 }, { 0,0,1 }, { 0,-1,0 }, 10, 10, 2.0f);
    glDisable(GL_TEXTURE_2D);

    door();

    // table, legs and candles
    texBox(-2.9f, 0.75f, 2.1f, 1.7f, 0.16f, 1, 0.35f, 0.15f, 0.08f, texWood, 1.0f);
    box(-3.55f, 0.35f, 1.75f, 0.12f, 0.7f, 0.12f, 0.12f, 0.045f, 0.02f);
    box(-2.25f, 0.35f, 1.75f, 0.12f, 0.7f, 0.12f, 0.12f, 0.045f, 0.02f);
    box(-3.55f, 0.35f, 2.45f, 0.12f, 0.7f, 0.12f, 0.12f, 0.045f, 0.02f);
    box(-2.25f, 0.35f, 2.45f, 0.12f, 0.7f, 0.12f, 0.12f, 0.045f, 0.02f);
    candle(-3.4f, 0.83f, 2.0f, 0.0f);
    candle(-3.15f, 0.83f, 2.3f, 1.7f);

    // cabinet, wardrobe, chest
    texBox(3.25f, 0.7f, 2, 1.2f, 1.4f, 0.75f, 0.28f, 0.12f, 0.08f, texWood, 1.0f);
    box(3.25f, 1.45f, 2, 1.3f, 0.12f, 0.82f, 0.17f, 0.055f, 0.035f);
    texBox(-2.8f, 1, -3.8f, 1.2f, 2, 0.75f, 0.26f, 0.12f, 0.09f, texWood, 1.0f);
    box(-2.8f, 1, -3.39f, 0.95f, 1.7f, 0.04f, 0.17f, 0.07f, 0.04f);
    texBox(2.8f, 0.45f, -3.8f, 0.55f, 0.9f, 0.55f, 0.3f, 0.13f, 0.07f, texWood, 1.0f);

    // portrait with glowing eyes in the hall
    box(0, 2.6f, -0.2f, 0.9f, 1.1f, 0.05f, 0.18f, 0.12f, 0.05f);
    box(0, 2.6f, -0.17f, 0.7f, 0.9f, 0.03f, 0.06f, 0.05f, 0.06f);
    glDisable(GL_LIGHTING);
    float e = 0.4f + 0.6f * flicker;
    sphere(-0.1f, 2.75f, -0.15f, 0.03f, e, 0, 0); sphere(0.1f, 2.75f, -0.15f, 0.03f, e, 0, 0);

    // hanging ceiling lamp
    glColor3f(0.1f, 0.1f, 0.1f);
    glBegin(GL_LINES); glVertex3f(0, 4.48f, 1.5f); glVertex3f(0, 3.75f, 1.5f); glEnd();
    sphere(0, 3.7f, 1.5f, 0.12f, flicker, 0.75f * flicker, 0.35f * flicker);
    // cold blue orb in the rear room
    sphere(-2.7f, 3.2f, -3.9f, 0.1f, 0.3f, 0.45f, 1.0f);

    // cobwebs (thin lines in corners)
    glColor3f(0.45f, 0.45f, 0.5f);
    glBegin(GL_LINES);
    for (int i = 0; i < 6; i++) {
        float t = i * 0.12f;
        glVertex3f(-4.88f, 4.4f - t, -4.88f); glVertex3f(-4.88f + t, 4.4f, -4.88f);
        glVertex3f(4.88f, 4.4f - t, -4.88f); glVertex3f(4.88f - t, 4.4f, -4.88f);
    }
    glEnd(); glEnable(GL_LIGHTING);
    glowHalo(0, 3.7f, 1.5f, 0.25f, 1.0f, 0.7f, 0.3f, 0.15f * flicker);
}

void porchColumn(float x, float z)
{
    box(x, 0.2f, z, 0.32f, 0.26f, 0.32f, 0.45f, 0.46f, 0.48f);
    glPushMatrix(); glTranslatef(x, 0.3f, z); glRotatef(-90, 1, 0, 0);
    glColor3f(0.55f, 0.56f, 0.58f);
    gluCylinder(quad, 0.11, 0.11, 2.75, 10, 6);
    glPopMatrix();
    box(x, 3.0f, z, 0.32f, 0.16f, 0.32f, 0.45f, 0.46f, 0.48f);
}

void railing(float x0, float x1, float z)
{
    float cx = (x0 + x1) / 2, len = x1 - x0;
    box(cx, 0.95f, z, len, 0.07f, 0.1f, 0.3f, 0.3f, 0.32f);
    box(cx, 0.25f, z, len, 0.06f, 0.08f, 0.3f, 0.3f, 0.32f);
    for (float x = x0 + 0.2f; x < x1 - 0.1f; x += 0.22f)
        box(x, 0.6f, z, 0.04f, 0.7f, 0.04f, 0.32f, 0.32f, 0.34f);
}

void porch()
{
    texBox(0.6f, 0.07f, 6.3f, 8.6f, 0.14f, 2.4f, 0.32f, 0.27f, 0.24f, texWood, 1.5f);
    texBox(0, 0.04f, 7.75f, 3.0f, 0.08f, 0.7f, 0.45f, 0.45f, 0.47f, texStone, 1.5f);   // step

    const float cols[4] = { -3.5f, -1.5f, 1.5f, 4.7f };
    for (int i = 0; i < 4; i++) porchColumn(cols[i], 7.2f);
    railing(-3.5f, -1.5f, 7.2f);
    railing(1.5f, 4.7f, 7.2f);

    // sloped porch roof
    float a = 0.55f, b = 2.45f, l = std::sqrt(a * a + b * b);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texShingle);
    glColor3f(ROOFC[0], ROOFC[1], ROOFC[2]);
    face({ -3.9f, 3.1f, 7.55f }, { 1,0,0 }, { 0, a / l, -b / l }, { 0, b / l, a / l }, 9.0f, l, 1.4f);
    glDisable(GL_TEXTURE_2D);
    box(0.6f, 3.05f, 7.55f, 9.1f, 0.22f, 0.1f, TRIM[0], TRIM[1], TRIM[2]);
    // decorative brackets
    for (int i = 0; i < 4; i++) box(cols[i], 2.85f, 7.0f, 0.06f, 0.25f, 0.5f, TRIM[0], TRIM[1], TRIM[2]);

    // wall lanterns beside the door
    for (int s = -1; s <= 1; s += 2) {
        float x = s * 1.4f;
        box(x, 2.0f, 5.2f, 0.06f, 0.06f, 0.2f, 0.08f, 0.08f, 0.08f);
        box(x, 1.95f, 5.32f, 0.18f, 0.28f, 0.18f, 0.08f, 0.08f, 0.08f);
        glDisable(GL_LIGHTING);
        sphere(x, 1.95f, 5.32f, 0.08f, 1.0f, 0.75f * flicker + 0.2f, 0.3f);
        glEnable(GL_LIGHTING);
        glowHalo(x, 1.95f, 5.32f, 0.22f, 1.0f, 0.65f, 0.25f, 0.18f);
    }
}

// Everything above / around the ground floor: the gothic mansion silhouette.
void mansionExterior()
{
    // second storey (not playable, so it is a solid shell above the ceiling)
    texBox(0, 6.25f, 0, 10.22f, 3.5f, 10.22f, STONE[0], STONE[1], STONE[2], texStone, 2.0f);
    // string course between floors and cornice under the roof
    const float ys[2] = { 4.55f, 7.95f };
    for (int i = 0; i < 2; i++) {
        box(0, ys[i], 5.2f, 10.6f, 0.2f, 0.2f, TRIM[0], TRIM[1], TRIM[2]);
        box(0, ys[i], -5.2f, 10.6f, 0.2f, 0.2f, TRIM[0], TRIM[1], TRIM[2]);
        box(-5.2f, ys[i], 0, 0.2f, 0.2f, 10.6f, TRIM[0], TRIM[1], TRIM[2]);
        box(5.2f, ys[i], 0, 0.2f, 0.2f, 10.6f, TRIM[0], TRIM[1], TRIM[2]);
    }

    // main roof (ridge left-right)
    glPushMatrix(); gableRoofX(10.22f, 10.22f, 8.0f, 3.2f, 0.35f); glPopMatrix();
    // iron cresting along the main ridge
    for (float x = -4.8f; x <= 4.8f; x += 0.4f)
        box(x, 11.35f, 0, 0.04f, 0.3f, 0.04f, 0.08f, 0.08f, 0.1f);
    // tall front cross-gable (ridge front-back)
    glPushMatrix(); glTranslatef(1.8f, 0, 2.4f); glRotatef(90, 0, 1, 0);
    gableRoofX(5.42f, 4.4f, 8.0f, 3.8f, 0.3f);
    glPopMatrix();
    // small dormer on the left slope
    texBox(-2.7f, 8.9f, 3.6f, 1.3f, 1.4f, 1.6f, STONE[0], STONE[1], STONE[2], texStone, 2.0f);
    glPushMatrix(); glTranslatef(-2.7f, 0, 3.6f); glRotatef(90, 0, 1, 0);
    gableRoofX(1.6f, 1.3f, 9.6f, 0.9f, 0.12f);
    glPopMatrix();

    chimney(-3.4f, -2.2f, 8.5f, 4.0f);
    chimney(3.8f, -2.8f, 8.5f, 3.6f);

    // east wing with its own roof, bay window and small tower
    texBox(7.0f, 1.8f, -1.0f, 4.0f, 3.6f, 6.0f, STONE[0], STONE[1], STONE[2], texStone, 2.0f);
    box(7.0f, 3.6f, -1.0f, 4.3f, 0.16f, 6.3f, TRIM[0], TRIM[1], TRIM[2]);
    glPushMatrix(); glTranslatef(7.0f, 0, -1.0f); gableRoofX(4.0f, 6.0f, 3.6f, 1.8f, 0.25f); glPopMatrix();
    texBox(6.3f, 1.6f, 2.4f, 1.6f, 3.2f, 0.8f, STONE[0], STONE[1], STONE[2], texStone, 2.0f);
    texBox(6.3f, 3.3f, 2.45f, 1.85f, 0.2f, 1.0f, ROOFC[0], ROOFC[1], ROOFC[2], texShingle, 1.0f);

    tower(-5.0f, 5.0f, 1.3f, 10.0f, 5.0f);   // main tower, front-left
    tower(8.2f, 2.0f, 0.85f, 7.6f, 3.8f);   // little tower on the east wing

    // windows
    for (int i = 0; i < WINDOW_COUNT; i++) windowAt(WINDOWS[i], i);

    // round attic window in the cross gable - turns blood-red once you steal a key,
    // and a figure appears in it whenever lightning strikes
    glDisable(GL_LIGHTING);
    float ar = keyCount >= 1 ? 0.9f : 0.35f, ag = keyCount >= 1 ? 0.08f : 0.25f, ab = keyCount >= 1 ? 0.05f : 0.1f;
    glColor3f(0.4f, 0.41f, 0.44f);
    glBegin(GL_TRIANGLE_FAN); glVertex3f(1.8f, 9.6f, 5.14f);
    for (int i = 0; i <= 20; i++) { float a = 2 * PI * i / 20; glVertex3f(1.8f + std::cos(a) * 0.62f, 9.6f + std::sin(a) * 0.62f, 5.14f); }
    glEnd();
    glColor3f(ar, ag, ab);
    glBegin(GL_TRIANGLE_FAN); glVertex3f(1.8f, 9.6f, 5.16f);
    for (int i = 0; i <= 20; i++) { float a = 2 * PI * i / 20; glVertex3f(1.8f + std::cos(a) * 0.48f, 9.6f + std::sin(a) * 0.48f, 5.16f); }
    glEnd();
    if (lightning > 0.25f) {
        glColor3f(0, 0, 0);
        glBegin(GL_TRIANGLE_FAN); glVertex3f(1.8f, 9.75f, 5.17f);
        for (int i = 0; i <= 12; i++) { float a = 2 * PI * i / 12; glVertex3f(1.8f + std::cos(a) * 0.1f, 9.75f + std::sin(a) * 0.12f, 5.17f); }
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex3f(1.8f, 9.68f, 5.17f); glVertex3f(1.55f, 9.15f, 5.17f); glVertex3f(2.05f, 9.15f, 5.17f);
        glEnd();
    }
    glEnable(GL_LIGHTING);
    glowHalo(1.8f, 9.6f, 5.3f, 0.5f, ar, ag + 0.1f, ab + 0.05f, 0.1f);
}

void tree(float x, float z, float s, bool leafy)
{
    texBox(x, 0.9f * s, z, 0.35f * s, 1.8f * s, 0.35f * s, 0.25f, 0.18f, 0.14f, texWood, 0.6f);
    const float br[5][3] = { {40,0,1}, {-50,1,1}, {30,1,0}, {-35,0,1}, {55,1,-1} };
    for (int i = 0; i < 5; i++) {
        glPushMatrix(); glTranslatef(x, (1.3f + 0.15f * i) * s, z);
        glRotatef(i * 72.0f, 0, 1, 0); glRotatef(br[i][0], 0, 0, 1);
        box(0, 0.5f * s, 0, 0.09f * s, 1.1f * s, 0.09f * s, 0.12f, 0.08f, 0.06f);
        glTranslatef(0, 1.0f * s, 0); glRotatef(-br[i][0] * 0.8f, 1, 0, 0);
        box(0, 0.3f * s, 0, 0.05f * s, 0.6f * s, 0.05f * s, 0.12f, 0.08f, 0.06f);
        glPopMatrix();
    }
    if (leafy) {   // dense dark canopy like the big tree beside the mansion
        const float c[6][4] = { {0,2.6f,0,1.2f}, {0.8f,2.2f,0.3f,0.9f}, {-0.8f,2.3f,-0.2f,0.95f},
                                {0.2f,3.3f,0.4f,0.85f}, {-0.3f,2.0f,0.8f,0.8f}, {0.4f,2.4f,-0.8f,0.85f} };
        for (int i = 0; i < 6; i++)
            sphere(x + c[i][0] * s, c[i][1] * s, z + c[i][2] * s, c[i][3] * s, 0.05f, 0.09f, 0.07f);
    }
}

void gravestone(float x, float z, float tilt, int type, int idx)
{
    glPushMatrix(); glTranslatef(x, 0, z); glRotatef(tilt, 0, 0, 1);
    if (type == 0) {
        texBox(0, 0.45f, 0, 0.8f, 0.9f, 0.18f, 0.45f, 0.46f, 0.5f, texStone, 1.0f);
        glPushMatrix(); glTranslatef(0, 0.9f, 0); glScalef(0.4f, 0.2f, 0.09f);
        glColor3f(0.42f, 0.43f, 0.47f); glutSolidSphere(1, 14, 10); glPopMatrix();
        box(0, 0.6f, 0.1f, 0.08f, 0.35f, 0.02f, 0.15f, 0.15f, 0.17f);
        box(0, 0.66f, 0.1f, 0.25f, 0.07f, 0.02f, 0.15f, 0.15f, 0.17f);
    }
    else {
        texBox(0, 0.1f, 0, 0.7f, 0.2f, 0.4f, 0.4f, 0.41f, 0.45f, texStone, 1.0f);
        texBox(0, 0.8f, 0, 0.16f, 1.4f, 0.16f, 0.45f, 0.46f, 0.5f, texStone, 1.0f);
        texBox(0, 1.1f, 0, 0.75f, 0.16f, 0.16f, 0.45f, 0.46f, 0.5f, texStone, 1.0f);
    }
    glPopMatrix();
    if (idx % 2 == 0) candle(x + 0.3f, 0.0f, z + 0.3f, idx * 1.3f);
}

void fenceAndGate()
{
    for (float x = -18; x <= 18; x += 0.6f) {
        if (std::fabs(x) < 1.6f) continue;
        box(x, 0.7f, 15, 0.07f, 1.4f, 0.07f, 0.08f, 0.08f, 0.09f);
        box(x, 1.45f, 15, 0.03f, 0.12f, 0.03f, 0.12f, 0.12f, 0.12f);
    }
    box(-9.8f, 0.4f, 15, 16.4f, 0.06f, 0.05f, 0.08f, 0.08f, 0.09f);
    box(9.8f, 0.4f, 15, 16.4f, 0.06f, 0.05f, 0.08f, 0.08f, 0.09f);
    box(-9.8f, 1.1f, 15, 16.4f, 0.06f, 0.05f, 0.08f, 0.08f, 0.09f);
    box(9.8f, 1.1f, 15, 16.4f, 0.06f, 0.05f, 0.08f, 0.08f, 0.09f);
    // stone gate posts with lanterns
    texBox(-1.65f, 1.1f, 15, 0.35f, 2.2f, 0.35f, 0.45f, 0.45f, 0.48f, texStone, 1.0f);
    texBox(1.65f, 1.1f, 15, 0.35f, 2.2f, 0.35f, 0.45f, 0.45f, 0.48f, texStone, 1.0f);
    bool open = keyCount >= 3;
    float lr = open ? 0.2f : 1.0f, lg = open ? 1.0f : 0.25f, lb = open ? 0.35f : 0.1f;
    glDisable(GL_LIGHTING);
    sphere(-1.65f, 2.35f, 15, 0.12f, lr, lg, lb);
    sphere(1.65f, 2.35f, 15, 0.12f, lr, lg, lb);
    glEnable(GL_LIGHTING);
    glowHalo(-1.65f, 2.35f, 15, 0.16f, lr, lg, lb, 0.12f);
    glowHalo(1.65f, 2.35f, 15, 0.16f, lr, lg, lb, 0.12f);
    // two gate leaves that swing outward when unlocked
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 1.45f, 0, 15);
        glRotatef(-side * gateAngle, 0, 1, 0);
        for (int i = 0; i < 5; i++)
            box(-side * (0.14f + i * 0.28f), 0.95f, 0, 0.05f, 1.7f, 0.05f, 0.1f, 0.1f, 0.11f);
        box(-side * 0.72f, 0.4f, 0, 1.4f, 0.06f, 0.06f, 0.1f, 0.1f, 0.11f);
        box(-side * 0.72f, 1.6f, 0, 1.4f, 0.06f, 0.06f, 0.1f, 0.1f, 0.11f);
        glPopMatrix();
    }
}

// Layers of drifting ground mist (the thick fog hugging the lawn).
void groundMist()
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texFog);
    const float H[4] = { 0.15f, 0.42f, 0.75f, 1.1f };
    const float A[4] = { 0.62f, 0.45f, 0.3f, 0.16f };
    float k = 1.0f - 0.6f * insideFactor;
    const float S = 40, CELL = 5, TILE = 14;
    for (int i = 0; i < 4; i++) {
        glMatrixMode(GL_TEXTURE); glLoadIdentity();
        glTranslatef(timeSec * 0.012f * (i + 1) + i * 0.37f, timeSec * 0.007f * (i % 2 ? 1 : -1), 0);
        glMatrixMode(GL_MODELVIEW);
        float L = lightning * 0.4f;
        glColor4f(0.45f + L, 0.53f + L, 0.56f + L, A[i] * k);
        glNormal3f(0, 1, 0);
        glBegin(GL_QUADS);
        for (float x = -S; x < S; x += CELL) for (float z = -S; z < S; z += CELL) {
            glTexCoord2f(x / TILE, z / TILE);                   glVertex3f(x, H[i], z);
            glTexCoord2f(x / TILE, (z + CELL) / TILE);          glVertex3f(x, H[i], z + CELL);
            glTexCoord2f((x + CELL) / TILE, (z + CELL) / TILE); glVertex3f(x + CELL, H[i], z + CELL);
            glTexCoord2f((x + CELL) / TILE, z / TILE);          glVertex3f(x + CELL, H[i], z);
        }
        glEnd();
    }
    resetTextureMatrix();
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE); glDisable(GL_BLEND); glEnable(GL_LIGHTING);
}

void drawEmbers()
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
    glPointSize(3.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 40; i++) {
        float a = 0.5f + 0.5f * std::sin(timeSec * 3.0f + i);
        glColor4f(1.0f, 0.55f, 0.15f, a);
        glVertex3f(embers[i][0] + 0.3f * std::sin(timeSec * 0.5f + i),
            embers[i][1] + 0.4f * std::sin(timeSec * 0.7f + i * 1.3f),
            embers[i][2] + 0.3f * std::cos(timeSec * 0.4f + i));
    }
    glEnd();
    glDepthMask(GL_TRUE); glDisable(GL_BLEND); glEnable(GL_LIGHTING);
}

void drawKeyItem(float x, float y, float z, float rot)
{
    glPushMatrix(); glTranslatef(x, y, z); glRotatef(rot, 0, 1, 0);
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 0.82f, 0.2f);
    glPushMatrix(); glTranslatef(-0.12f, 0, 0); glutSolidTorus(0.025, 0.07, 10, 16); glPopMatrix();
    glPushMatrix(); glTranslatef(0.08f, 0, 0); glScalef(0.26f, 0.035f, 0.035f); glutSolidCube(1); glPopMatrix();
    glPushMatrix(); glTranslatef(0.17f, -0.04f, 0); glScalef(0.035f, 0.06f, 0.035f); glutSolidCube(1); glPopMatrix();
    glPushMatrix(); glTranslatef(0.11f, -0.035f, 0); glScalef(0.03f, 0.04f, 0.035f); glutSolidCube(1); glPopMatrix();
    glEnable(GL_LIGHTING);
    glPopMatrix();
    glowHalo(x, y, z, 0.2f, 1.0f, 0.75f, 0.2f, 0.18f);
}

void drawBatteryItem(float x, float y, float z, float rot)
{
    glPushMatrix(); glTranslatef(x, y, z); glRotatef(rot, 0, 1, 0);
    glDisable(GL_LIGHTING);
    box(0, 0, 0, 0.1f, 0.22f, 0.1f, 0.1f, 0.75f, 0.25f);
    box(0, 0.13f, 0, 0.05f, 0.04f, 0.05f, 0.8f, 0.8f, 0.8f);
    glEnable(GL_LIGHTING);
    glPopMatrix();
    glowHalo(x, y, z, 0.16f, 0.2f, 1.0f, 0.4f, 0.15f);
}

void drawItems()
{
    for (size_t i = 0; i < items.size(); i++) {
        const Item& it = items[i];
        if (it.taken) continue;
        float bob = 0.05f * std::sin(timeSec * 2.5f + i);
        if (it.type == ITEM_KEY) drawKeyItem(it.x, it.y + bob, it.z, timeSec * 90);
        else drawBatteryItem(it.x, it.y + bob, it.z, timeSec * 60);
    }
}

void drawGhost(const Ghost& g)
{
    if (!g.active || g.alpha <= 0.01f) return;
    float face = std::atan2(px - g.x, pz - g.z) * 180.0f / PI;

    glDisable(GL_LIGHTING); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    // soft blob shadow on the floor
    glColor4f(0, 0, 0, 0.35f * g.alpha);
    glBegin(GL_TRIANGLE_FAN); glVertex3f(g.x, 0.09f, g.z);
    for (int i = 0; i <= 16; i++) { float a = 2 * PI * i / 16; glVertex3f(g.x + std::cos(a) * 0.55f, 0.09f, g.z + std::sin(a) * 0.55f); }
    glEnd();

    glPushMatrix();
    glTranslatef(g.x, g.y + 0.15f * std::sin(timeSec * 3 + g.x), g.z);
    glRotatef(face, 0, 1, 0);
    bool angry = g.stun <= 0;
    glColor4f(0.78f, 0.88f, 1, g.alpha);
    glPushMatrix(); glTranslatef(0, 0.35f, 0); glScalef(0.75f, 1, 0.55f); glutSolidSphere(0.65, 22, 18); glPopMatrix();
    glColor4f(0.72f, 0.84f, 1, g.alpha * 0.85f);
    for (int i = -1; i <= 1; i++) {
        glPushMatrix();
        glTranslatef(i * 0.28f, -0.12f + 0.05f * std::sin(timeSec * 6 + i), 0);
        glutSolidSphere(0.25, 14, 12);
        glPopMatrix();
    }
    if (angry) {
        glPushMatrix(); glTranslatef(-0.45f, 0.3f, 0.35f); glScalef(0.12f, 0.12f, 0.5f); glutSolidSphere(1, 10, 8); glPopMatrix();
        glPushMatrix(); glTranslatef(0.45f, 0.3f, 0.35f); glScalef(0.12f, 0.12f, 0.5f); glutSolidSphere(1, 10, 8); glPopMatrix();
    }
    glDepthMask(GL_TRUE);
    glColor4f(angry ? 1.0f : 0.3f, 0.02f, angry ? 0.02f : 0.8f, std::min(1.0f, g.alpha + 0.3f));
    glPushMatrix(); glTranslatef(-0.20f, 0.45f, 0.49f); glutSolidSphere(0.075, 12, 10); glPopMatrix();
    glPushMatrix(); glTranslatef(0.20f, 0.45f, 0.49f); glutSolidSphere(0.075, 12, 10); glPopMatrix();
    glColor4f(0.05f, 0.0f, 0.05f, g.alpha);
    glPushMatrix(); glTranslatef(0, 0.18f, 0.5f); glScalef(1, 1.4f, 0.4f); glutSolidSphere(0.09, 12, 10); glPopMatrix();
    glPopMatrix();
    glDisable(GL_BLEND); glEnable(GL_LIGHTING);
}

// =====================================================================
//  LIGHTING
// =====================================================================
void flashlightLight()   // called with identity modelview -> follows camera
{
    if (flashlight && battery > 0) {
        float strength = battery < 15 ? (0.5f + 0.5f * (std::sin(timeSec * 50.0f) > 0)) : 1.0f;
        GLfloat fp[] = { 0,0,0,1 }, fd[] = { 0,0,-1 };
        GLfloat fcol[] = { 0.95f * strength, 0.92f * strength, 0.78f * strength, 1 };
        glLightfv(GL_LIGHT1, GL_POSITION, fp); glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, fd);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, fcol);
        glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 30);
        glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 16);
        glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0.6f);
        glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.04f);
        glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.008f);
        glEnable(GL_LIGHT1);
    }
    else glDisable(GL_LIGHT1);
}

void pointLight(GLenum id, float x, float y, float z, float r, float g, float b, float c, float l, float q)
{
    GLfloat p[] = { x, y, z, 1 }, d[] = { r, g, b, 1 };
    glLightfv(id, GL_POSITION, p); glLightfv(id, GL_DIFFUSE, d);
    glLightf(id, GL_CONSTANT_ATTENUATION, c);
    glLightf(id, GL_LINEAR_ATTENUATION, l);
    glLightf(id, GL_QUADRATIC_ATTENUATION, q);
    glEnable(id);
}

void worldLights()       // called after gluLookAt -> fixed in world
{
    float out = 1.0f - insideFactor;
    GLfloat ambient[] = { 0.03f + 0.03f * out, 0.03f + 0.04f * out, 0.045f + 0.04f * out, 1 };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    // cold teal moonlight (dimmer when you are inside) + lightning
    float L = lightning * (1.0f - 0.6f * insideFactor);
    GLfloat mp[] = { 0.35f, 0.8f, -0.5f, 0 };
    GLfloat md[] = { 0.06f + 0.22f * out + L, 0.07f + 0.28f * out + L, 0.11f + 0.30f * out + L, 1 };
    glLightfv(GL_LIGHT0, GL_POSITION, mp); glLightfv(GL_LIGHT0, GL_DIFFUSE, md);

    // flickering ceiling lamp
    pointLight(GL_LIGHT2, 0, 3.6f, 1.5f, 0.95f * flicker, 0.72f * flicker, 0.42f * flicker, 0.7f, 0.12f, 0.035f);
    // cold blue light in the rear room
    pointLight(GL_LIGHT3, -2.7f, 3.2f, -3.9f, 0.12f, 0.18f, 0.45f, 0.8f, 0.16f, 0.0f);
    // red light near the first ghost
    if (ghosts[0].active)
        pointLight(GL_LIGHT4, ghosts[0].x, ghosts[0].y + 0.5f, ghosts[0].z, 0.35f * ghosts[0].alpha, 0.05f, 0.08f, 1.0f, 0.5f, 0.0f);
    else glDisable(GL_LIGHT4);
    // warm porch lanterns
    pointLight(GL_LIGHT5, 0, 2.2f, 6.2f, 0.9f, 0.6f, 0.28f, 0.5f, 0.25f, 0.09f);
    // warm glow spilling from the bay window onto the lawn
    pointLight(GL_LIGHT6, 6.3f, 1.4f, 3.8f, 0.8f, 0.52f, 0.22f, 0.6f, 0.3f, 0.12f);

    float fogD = 0.05f - lightning * 0.025f + 0.015f * insideFactor;
    glFogf(GL_FOG_DENSITY, std::max(0.01f, fogD));
}

// =====================================================================
//  HUD
// =====================================================================
void drawText(float x, float y, const char* text, void* font = GLUT_BITMAP_8_BY_13)
{
    glRasterPos2f(x, y);
    for (const char* c = text; *c; ++c) glutBitmapCharacter(font, *c);
}
int textWidth(const char* text, void* font)
{
    int w = 0; for (const char* c = text; *c; ++c) w += glutBitmapWidth(font, *c);
    return w;
}
void drawTextCentered(float y, const char* text, void* font)
{
    float w = textWidth(text, font) * 1000.0f / winW;   // HUD space is 1000 wide
    drawText(500 - w / 2, y, text, font);
}
void rect(float x0, float y0, float x1, float y1, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS); glVertex2f(x0, y0); glVertex2f(x1, y0); glVertex2f(x1, y1); glVertex2f(x0, y1); glEnd();
}
void bar(float x, float y, const char* label, float value, float r, float g, float b)
{
    rect(x, y, x + 200, y + 14, 0.1f, 0.1f, 0.1f, 0.7f);
    rect(x + 2, y + 2, x + 2 + 196 * std::max(0.0f, value) / 100.0f, y + 12, r, g, b, 0.9f);
    glColor3f(0.9f, 0.9f, 0.9f);
    drawText(x + 208, y + 2, label);
}

void minimap()
{
    float mx0 = 800, my0 = 450, mw = 180, mh = 240;   // world x[-9,9], z[-7,17]
    rect(mx0 - 4, my0 - 4, mx0 + mw + 4, my0 + mh + 4, 0, 0, 0, 0.6f);
    auto MX = [&](float x) { return mx0 + (x + 9) / 18.0f * mw; };
    auto MY = [&](float z) { return my0 + mh - (z + 7) / 24.0f * mh; };
    for (size_t i = 0; i < walls.size(); i++) {
        const AABB& w = walls[i];
        if (w.x1 < -9 || w.x0 > 9 || w.z1 < -7 || w.z0 > 17) continue;
        rect(MX(std::max(-9.0f, w.x0)), MY(std::min(17.0f, w.z1)), MX(std::min(9.0f, w.x1)), MY(std::max(-7.0f, w.z0)),
            0.6f, 0.55f, 0.5f, 0.9f);
    }
    for (size_t i = 0; i < items.size(); i++) {
        if (items[i].taken) continue;
        float x = MX(items[i].x), y = MY(items[i].z);
        if (items[i].type == ITEM_KEY) rect(x - 3, y - 3, x + 3, y + 3, 1, 0.8f, 0.1f, 1);
        else rect(x - 2, y - 2, x + 2, y + 2, 0.2f, 1, 0.4f, 1);
    }
    for (int i = 0; i < 2; i++) if (ghosts[i].active) {
        float x = MX(ghosts[i].x), y = MY(ghosts[i].z);
        rect(x - 3, y - 3, x + 3, y + 3, 1, 0.1f, 0.1f, 0.9f);
    }
    float x = MX(px), y = MY(pz), yr = rad(yaw);
    float fx = std::sin(yr), fy = std::cos(yr);   // screen y up = world -z
    glColor3f(1, 1, 1);
    glBegin(GL_TRIANGLES);
    glVertex2f(x + fx * 8, y + fy * 8);
    glVertex2f(x - fy * 4 - fx * 4, y + fx * 4 - fy * 4);
    glVertex2f(x + fy * 4 - fx * 4, y - fx * 4 - fy * 4);
    glEnd();
    glColor3f(0.8f, 0.8f, 0.8f);
    drawText(mx0, my0 - 18, "MAP (M)");
}

void hud()
{
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST); glDisable(GL_FOG);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0, 1000, 0, 700);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

    char buf[160];

    // full-screen effects
    if (lightning > 0.05f) rect(0, 0, 1000, 700, 0.8f, 0.85f, 1.0f, lightning * 0.25f);
    if (hurtFlash > 0) rect(0, 0, 1000, 700, 0.7f, 0, 0, hurtFlash * 0.5f);
    if (state == PLAYING && sanity < 40) {
        float pulse = (0.5f + 0.5f * std::sin(timeSec * 6.0f)) * (40 - sanity) / 40.0f;
        rect(0, 0, 1000, 40, 0.6f, 0, 0, pulse * 0.6f);   rect(0, 660, 1000, 700, 0.6f, 0, 0, pulse * 0.6f);
        rect(0, 0, 40, 700, 0.6f, 0, 0, pulse * 0.6f);    rect(960, 0, 1000, 700, 0.6f, 0, 0, pulse * 0.6f);
    }

    if (state == MENU) {
        rect(0, 0, 1000, 700, 0, 0, 0, 0.25f);
        rect(0, 200, 1000, 540, 0, 0, 0, 0.5f);
        glColor3f(0.9f, 0.1f, 0.1f);
        drawTextCentered(500, "T H E   L A S T   L A N T E R N", GLUT_BITMAP_TIMES_ROMAN_24);
        glColor3f(0.75f, 0.8f, 0.85f);
        drawTextCentered(470, "Chapter I  -  Blackwood Manor", GLUT_BITMAP_HELVETICA_18);
        glColor3f(0.85f, 0.85f, 0.9f);
        drawTextCentered(420, "Three golden keys lie hidden inside the Blackwood mansion.", GLUT_BITMAP_HELVETICA_18);
        drawTextCentered(392, "Find them all and escape through the iron gate before your mind breaks.", GLUT_BITMAP_HELVETICA_18);
        drawTextCentered(364, "Your flashlight repels the ghosts... but the battery will not last forever.", GLUT_BITMAP_HELVETICA_18);
        glColor3f(0.7f, 0.75f, 0.9f);
        drawTextCentered(310, "WASD move | Arrows look | SPACE sprint | F flashlight | E door | M map | P pause", GLUT_BITMAP_HELVETICA_12);
        glColor3f(1, 0.8f, 0.3f);
        if (((int)(timeSec * 2)) % 2) drawTextCentered(245, "Press ENTER to begin", GLUT_BITMAP_TIMES_ROMAN_24);
    }
    else if (state == WON || state == LOST) {
        rect(0, 0, 1000, 700, 0, 0, 0, 0.7f);
        if (state == WON) { glColor3f(0.3f, 1, 0.4f); drawTextCentered(420, "YOU ESCAPED BLACKWOOD MANOR!", GLUT_BITMAP_TIMES_ROMAN_24); }
        else {
            glColor3f(1, 0.1f, 0.1f); drawTextCentered(420, "YOUR MIND HAS SHATTERED...", GLUT_BITMAP_TIMES_ROMAN_24);
            glColor3f(0.8f, 0.6f, 0.6f); drawTextCentered(385, "You are now part of the house forever.", GLUT_BITMAP_HELVETICA_18);
        }
        glColor3f(0.9f, 0.9f, 0.9f);
        snprintf(buf, sizeof(buf), "Time: %d:%02d    Keys found: %d / 3    Sanity left: %d%%",
            (int)playTime / 60, (int)playTime % 60, keyCount, (int)std::max(0.0f, sanity));
        drawTextCentered(340, buf, GLUT_BITMAP_HELVETICA_18);
        glColor3f(1, 0.8f, 0.3f);
        drawTextCentered(280, "Press ENTER to play again  |  ESC to quit", GLUT_BITMAP_HELVETICA_18);
    }
    else {
        bar(18, 90, "SANITY", sanity, 0.8f, 0.15f, 0.2f);
        bar(18, 70, battery > 0 ? (flashlight ? "BATTERY (ON)" : "BATTERY (OFF)") : "BATTERY DEAD", battery, 0.95f, 0.85f, 0.3f);
        bar(18, 50, "STAMINA", stamina, 0.3f, 0.6f, 1.0f);

        glColor3f(1, 0.82f, 0.25f);
        snprintf(buf, sizeof(buf), "KEYS: %d / 3", keyCount);
        drawText(18, 120, buf, GLUT_BITMAP_HELVETICA_18);
        glColor3f(0.8f, 0.8f, 0.8f);
        snprintf(buf, sizeof(buf), "TIME %d:%02d", (int)playTime / 60, (int)playTime % 60);
        drawText(18, 145, buf);

        glColor3f(0.75f, 0.85f, 1.0f);
        drawText(18, 22, keyCount < 3 ? "Objective: Search the mansion for the golden keys."
            : "Objective: ESCAPE through the iron gate (green lanterns) south of the mansion!");

        if (statusTimer > 0) {
            glColor4f(1.0f, 0.75f, 0.35f, std::min(1.0f, statusTimer));
            drawTextCentered(180, statusMessage, GLUT_BITMAP_HELVETICA_18);
        }

        float ddx = px, ddz = pz - 5.0f;
        if (ddx * ddx + ddz * ddz < 4.0f) {
            glColor3f(1, 1, 1);
            drawTextCentered(320, doorOpen ? "[E] Close door" : "[E] Open door", GLUT_BITMAP_HELVETICA_18);
        }

        if (showHelp) {
            rect(10, 520, 330, 690, 0, 0, 0, 0.5f);
            glColor3f(0.78f, 0.86f, 1.0f);
            drawText(18, 674, "CONTROLS (H to hide)");
            drawText(18, 656, "W/S/A/D : move     SPACE : sprint");
            drawText(18, 638, "Arrow keys : look around");
            drawText(18, 620, "F : flashlight (repels ghosts)");
            drawText(18, 602, "E : open/close front door");
            drawText(18, 584, "M : minimap    P : pause");
            drawText(18, 566, "R : restart    Q/ESC : quit");
            drawText(18, 540, "Darkness inside drains sanity!");
        }
        if (showMap) minimap();

        glColor3f(1.0f, 0.12f, 0.12f);
        glBegin(GL_LINES);
        glVertex2i(493, 350); glVertex2i(507, 350);
        glVertex2i(500, 343); glVertex2i(500, 357);
        glEnd();

        if (state == PAUSED) {
            rect(0, 0, 1000, 700, 0, 0, 0, 0.6f);
            glColor3f(1, 1, 1);
            drawTextCentered(360, "PAUSED - press P to continue", GLUT_BITMAP_TIMES_ROMAN_24);
        }
    }

    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING); glEnable(GL_FOG);
}

// =====================================================================
//  DISPLAY
// =====================================================================
void display()
{
    float L = lightning * 0.3f;
    glClearColor(0.12f + L, 0.17f + L, 0.18f + L, 1);
    GLfloat fogC[] = { 0.11f * (1 - insideFactor) + 0.02f * insideFactor + L,
                       0.16f * (1 - insideFactor) + 0.02f * insideFactor + L,
                       0.17f * (1 - insideFactor) + 0.03f * insideFactor + L, 1 };
    glFogfv(GL_FOG_COLOR, fogC);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    flashlightLight();

    float yr = rad(yaw), pr = rad(pitch);
    float dx = std::sin(yr) * std::cos(pr), dy = std::sin(pr), dz = -std::cos(yr) * std::cos(pr);
    gluLookAt(px + shakeX, py + shakeY, pz, px + dx + shakeX, py + dy + shakeY, pz + dz, 0, 1, 0);
    worldLights();

    skyDome();
    moonAndStars();
    ground();
    houseInterior();
    mansionExterior();
    porch();
    fenceAndGate();
    for (int i = 0; i < TREE_COUNT; i++) tree(TREES[i][0], TREES[i][1], TREES[i][2], TREES[i][3] > 0.5f);
    for (int i = 0; i < GRAVE_COUNT; i++) gravestone(GRAVES[i][0], GRAVES[i][1], GRAVES[i][2], (int)GRAVES[i][3], i);
    drawItems();
    groundMist();
    drawEmbers();
    drawGhost(ghosts[0]); drawGhost(ghosts[1]);
    hud();
    glutSwapBuffers();
}

void reshape(int w, int h)
{
    if (h == 0) h = 1;
    winW = w; winH = h;
    glViewport(0, 0, w, h); glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(70.0, (double)w / (double)h, 0.1, 150.0);
    glMatrixMode(GL_MODELVIEW);
}

// =====================================================================
//  GAME LOGIC
// =====================================================================
bool overlaps(const AABB& b, float x, float z, float r)
{
    return x + r > b.x0 && x - r < b.x1 && z + r > b.z0 && z - r < b.z1;
}
bool blocked(float x, float z)
{
    const float r = 0.28f;
    if (x < -18 || x > 18 || z < -18 || z > 18) return true;
    for (size_t i = 0; i < walls.size(); i++) if (overlaps(walls[i], x, z, r)) return true;
    if (doorAngle > -70 && overlaps(DOOR_BOX, x, z, r)) return true;
    if (gateAngle < 60 && overlaps(GATE_BOX, x, z, r)) return true;
    return false;
}
bool insideHouse() { return px > -5 && px < 5 && pz > -5 && pz < 5; }

void updatePlayer(float dt)
{
    float y = rad(yaw), fx = std::sin(y), fz = -std::cos(y), rx = std::cos(y), rz = std::sin(y);
    float mx = 0, mz = 0;
    if (keyDownArr['w']) { mx += fx; mz += fz; }
    if (keyDownArr['s']) { mx -= fx; mz -= fz; }
    if (keyDownArr['a']) { mx -= rx; mz -= rz; }
    if (keyDownArr['d']) { mx += rx; mz += rz; }
    float len = std::sqrt(mx * mx + mz * mz);
    bool moving = len > 0.01f;
    bool sprint = keyDownArr[' '] && stamina > 0 && moving;

    if (moving) {
        float speed = sprint ? 6.5f : 3.6f;
        mx = mx / len * speed * dt; mz = mz / len * speed * dt;
        if (!blocked(px + mx, pz)) px += mx;
        if (!blocked(px, pz + mz)) pz += mz;
        bobPhase += dt * (sprint ? 14.0f : 9.0f);
    }
    py = 1.65f + (moving ? 0.045f * std::sin(bobPhase) : 0.0f);

    if (sprint) stamina = std::max(0.0f, stamina - 32 * dt);
    else stamina = std::min(100.0f, stamina + 16 * dt);

    if (arrows[GLUT_KEY_LEFT])  yaw -= 80 * dt;
    if (arrows[GLUT_KEY_RIGHT]) yaw += 80 * dt;
    if (arrows[GLUT_KEY_UP])    pitch += 60 * dt;
    if (arrows[GLUT_KEY_DOWN])  pitch -= 60 * dt;
    pitch = std::max(-75.0f, std::min(75.0f, pitch));

    if (flashlight && battery > 0) {
        battery = std::max(0.0f, battery - 1.1f * dt);
        if (battery <= 0) { flashlight = false; setStatus("Your flashlight died! Find a battery!", 4); }
    }

    bool nearGhost = false;
    for (int i = 0; i < 2; i++) if (ghosts[i].active) {
        float gx = ghosts[i].x - px, gz = ghosts[i].z - pz;
        if (gx * gx + gz * gz < 25) nearGhost = true;
    }
    if (insideHouse() && !(flashlight && battery > 0)) sanity -= 2.2f * dt;
    else if (nearGhost) sanity -= 0.8f * dt;
    else sanity = std::min(100.0f, sanity + 1.2f * dt);

    for (size_t i = 0; i < items.size(); i++) {
        Item& it = items[i];
        if (it.taken) continue;
        float ix = it.x - px, iz = it.z - pz;
        if (ix * ix + iz * iz < 1.1f * 1.1f) {
            it.taken = true;
            if (it.type == ITEM_KEY) {
                keyCount++;
                setStatus(it.note, 4.5f);
                if (keyCount == 2) {
                    ghosts[1].active = true;
                    setStatus("Key 2 found... another presence AWAKENS!", 4.5f);
                }
                if (keyCount == 3) {
                    ghosts[0].speed += 0.5f; ghosts[1].speed += 0.5f;
                    lightning = 1.0f;
                }
            }
            else {
                battery = std::min(100.0f, battery + 50);
                setStatus(it.note, 2.5f);
            }
        }
    }

    if (keyCount >= 3 && pz > 15.8f && std::fabs(px) < 2.0f) state = WON;
    if (sanity <= 0) { sanity = 0; state = LOST; }
}

void updateGhosts(float dt)
{
    float yr = rad(yaw), vfx = std::sin(yr), vfz = -std::cos(yr);
    bool lightOn = flashlight && battery > 0;

    for (int gi = 0; gi < 2; gi++) {
        Ghost& g = ghosts[gi];
        if (!g.active) continue;
        if (g.alpha < 0.7f && g.stun <= 0) g.alpha = std::min(0.7f, g.alpha + 0.4f * dt);

        float dx = px - g.x, dz = pz - g.z;
        float d = std::sqrt(dx * dx + dz * dz) + 0.0001f;

        if (lightOn && d < 7.5f) {
            float c = (-dx * vfx - dz * vfz) / d;
            if (c > 0.93f) {
                if (g.stun <= 0) setStatus("The light burns it! The ghost recoils!", 1.5f);
                g.stun = 1.4f;
                battery = std::max(0.0f, battery - 3.0f * dt);
            }
        }
        g.hitCooldown -= dt;

        bool playerSafe = pz > 13.5f;   // the yard near the gate is sacred ground
        if (g.stun > 0) {
            g.stun -= dt;
            g.x -= dx / d * 2.4f * dt; g.z -= dz / d * 2.4f * dt;
            g.alpha = std::max(0.22f, g.alpha - 1.5f * dt);
        }
        else if (d < 8.0f && !playerSafe) {
            float sp = g.speed * (1.0f + 0.15f * keyCount);
            g.x += dx / d * sp * dt; g.z += dz / d * sp * dt;
        }
        else {
            float wx = WAYPOINTS[g.wp][0] - g.x, wz = WAYPOINTS[g.wp][1] - g.z;
            float wd = std::sqrt(wx * wx + wz * wz) + 0.0001f;
            if (wd < 0.4f) g.wp = rand() % WP_COUNT;
            else { g.x += wx / wd * g.speed * 0.6f * dt; g.z += wz / wd * g.speed * 0.6f * dt; }
        }
        g.x = std::max(-8.0f, std::min(8.0f, g.x));
        g.z = std::max(-6.0f, std::min(12.5f, g.z));

        if (d < 0.85f && g.hitCooldown <= 0 && g.stun <= 0) {
            sanity -= 28;
            hurtFlash = 1.0f;
            g.hitCooldown = 2.0f;
            g.alpha = 0;
            int farWp = 0; float best = -1;
            for (int w = 0; w < WP_COUNT; w++) {
                float ax = WAYPOINTS[w][0] - px, az = WAYPOINTS[w][1] - pz;
                float dd = ax * ax + az * az;
                if (dd > best) { best = dd; farWp = w; }
            }
            g.x = WAYPOINTS[farWp][0]; g.z = WAYPOINTS[farWp][1]; g.wp = rand() % WP_COUNT;
            setStatus("A ghost passed THROUGH you! (-28 sanity)", 3);
        }
    }
}

void update(int)
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - lastTime) / 1000.0f; lastTime = now; if (dt > 0.05f) dt = 0.05f;
    timeSec = now / 1000.0f;

    flicker = std::max(0.15f, std::min(1.0f, 0.60f + 0.28f * std::sin(now * 0.017f) + 0.12f * std::sin(now * 0.071f)));
    if ((now / 1000) % 9 == 0 && std::sin(now * 0.05f) > 0.95f) flicker = 0.1f;   // occasional blackout
    nextLightning -= dt;
    if (nextLightning <= 0) { lightning = 1.0f; nextLightning = frand(6, 15); }
    if (lightning > 0) {
        lightning -= dt * 2.5f;
        if (lightning < 0.35f && lightning > 0.3f && rand() % 2) lightning = 0.8f;   // double flash
        if (lightning < 0) lightning = 0;
    }
    hurtFlash = std::max(0.0f, hurtFlash - dt * 1.2f);
    statusTimer -= dt;

    float shake = hurtFlash * 0.08f;
    shakeX = frand(-shake, shake); shakeY = frand(-shake, shake);

    float insideTarget = insideHouse() ? 1.0f : 0.0f;
    insideFactor += (insideTarget - insideFactor) * std::min(1.0f, dt * 2.0f);

    if (state == PLAYING) {
        playTime += dt;
        updatePlayer(dt);
        updateGhosts(dt);
    }
    if (state == MENU) {   // slow cinematic sway looking up at the mansion from beyond the gate
        yaw = -8.0f + 10.0f * std::sin(timeSec * 0.15f);
        pitch = 20.0f + 3.0f * std::sin(timeSec * 0.1f);
    }

    float target = doorOpen ? -95.0f : 0.0f;
    if (doorAngle < target) doorAngle = std::min(target, doorAngle + 120 * dt);
    if (doorAngle > target) doorAngle = std::max(target, doorAngle - 120 * dt);
    float gTarget = keyCount >= 3 ? 100.0f : 0.0f;
    if (gateAngle < gTarget) gateAngle = std::min(gTarget, gateAngle + 50 * dt);
    if (gateAngle > gTarget) gateAngle = std::max(gTarget, gateAngle - 200 * dt);

    glutPostRedisplay(); glutTimerFunc(16, update, 0);
}

// =====================================================================
//  INPUT
// =====================================================================
void setMenuCamera() { px = 2.5f; py = 1.65f; pz = 14.2f; yaw = -8; pitch = 20; }

void keyDown(unsigned char k, int, int)
{
    k = (unsigned char)std::tolower(k);
    bool firstPress = !keyDownArr[k];
    keyDownArr[k] = true;
    if (!firstPress) return;

    if (k == 27 || k == 'q') std::exit(0);

    if (k == 13) {
        if (state == MENU || state == WON || state == LOST) { resetGame(); state = PLAYING; }
        return;
    }
    if (k == 'p') {
        if (state == PLAYING) state = PAUSED;
        else if (state == PAUSED) state = PLAYING;
        return;
    }
    if (state != PLAYING) return;

    if (k == 'f') {
        if (battery <= 0) setStatus("No battery left...", 2);
        else {
            flashlight = !flashlight;
            setStatus(flashlight ? "Flashlight ON." : "Flashlight OFF. The darkness creeps in...", 2);
        }
    }
    else if (k == 'e') {
        float dx = px, dz = pz - 5.0f;
        if (dx * dx + dz * dz < 4.0f) {
            if (doorOpen && std::fabs(dz) < 0.5f && std::fabs(dx) < 1.2f) setStatus("Step out of the doorway first.", 2);
            else {
                doorOpen = !doorOpen;
                setStatus(doorOpen ? "The door creaks open..." : "The door slams shut.", 2.5f);
            }
        }
        else setStatus("Move closer to the front door to interact.", 2);
    }
    else if (k == 'h') showHelp = !showHelp;
    else if (k == 'm') showMap = !showMap;
    else if (k == 'r') { resetGame(); setStatus("Game restarted.", 2); }
}
void keyUp(unsigned char k, int, int) { keyDownArr[(unsigned char)std::tolower(k)] = false; }
void specialDown(int k, int, int) { if (k >= 0 && k < 256) arrows[k] = true; }
void specialUp(int k, int, int) { if (k >= 0 && k < 256) arrows[k] = false; }

// =====================================================================
//  INIT / MAIN
// =====================================================================
void init()
{
    glClearColor(0.12f, 0.17f, 0.18f, 1);
    glEnable(GL_DEPTH_TEST); glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    GLfloat amb[] = { 0.04f,0.05f,0.06f,1 };
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);
    glFogf(GL_FOG_DENSITY, 0.05f);
    glHint(GL_FOG_HINT, GL_NICEST);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluQuadricTexture(quad, GL_TRUE);

    srand(1234);
    buildTextures();
    buildColliders();
    buildDecorations();
    resetGame();
    setMenuCamera();
    state = MENU;
}

void cleanup() { if (quad) { gluDeleteQuadric(quad); quad = nullptr; } }

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH); glutInitWindowPosition(80, 50);
    glutCreateWindow("The Last Lantern - Blackwood Manor (OpenGL/GLUT)");
    init();
    atexit(cleanup);
    glutDisplayFunc(display); glutReshapeFunc(reshape);
    glutKeyboardFunc(keyDown); glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown); glutSpecialUpFunc(specialUp);
    lastTime = glutGet(GLUT_ELAPSED_TIME); glutTimerFunc(16, update, 0);
    glutMainLoop();
    return 0;
}
