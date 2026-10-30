// =====================================================================
//  3D HAUNTED HOUSE - "The Last Lantern"   (OpenGL + GLUT/freeglut)
// ---------------------------------------------------------------------
//  GOAL : Find the 3 golden keys hidden in the house, then escape
//         through the iron gate behind you (south of the yard).
//  DANGER: Ghosts hunt you. Shine your flashlight on them to drive
//          them back. Darkness and ghost touches drain your SANITY.
//
//  CONTROLS
//   W A S D  : move            SPACE (hold) : sprint (uses stamina)
//   Arrows   : look around     F : flashlight      E : open/close door
//   M        : minimap         H : help            P : pause
//   ENTER    : start/restart   Q / ESC : quit
//
//  Build (Linux):   g++ haunted_house.cpp -o haunted -lGL -lGLU -lglut
//  Build (MinGW):   g++ haunted_house.cpp -o haunted -lfreeglut -lopengl32 -lglu32
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

// world fx
float flicker = 0.7f, lightning = 0.0f, nextLightning = 5.0f, gateAngle = 0.0f;
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
    {0,1}, {3.5f,3.5f}, {-3.5f,3.5f}, {-3.6f,-1.6f}, {3.6f,-1.6f},
    {0,-4}, {-3.9f,-4.2f}, {3.9f,-4.2f}, {0,7}, {-6,8}, {6,8}
};
const int WP_COUNT = sizeof(WAYPOINTS) / sizeof(WAYPOINTS[0]);

// gravestones (x, z, rotation)
const float GRAVES[][3] = { {-6,10,8}, {-4.3f,12.2f,-6}, {6.2f,10.8f,4}, {4.6f,12.8f,-10}, {-7.5f,7.5f,15} };
const int GRAVE_COUNT = 5;
const float TREES[][3] = { {-8,3,1.3f}, {8,2,1.5f}, {-8,-5,1.1f}, {8,-6,1.2f}, {-11,11,1.4f}, {11,12,1.2f} };
const int TREE_COUNT = 6;

// =====================================================================
//  WORLD SETUP
// =====================================================================
void buildColliders()
{
    walls.clear();
    // exterior walls (front wall is split for the doorway)
    walls.push_back({ -5.11f, -5.11f, -4.89f, 5.11f });
    walls.push_back({  4.89f, -5.11f,  5.11f, 5.11f });
    walls.push_back({ -5.11f, -5.11f,  5.11f, -4.89f });
    walls.push_back({ -5.11f,  4.89f, -1.0f,  5.11f });
    walls.push_back({  1.0f,   4.89f,  5.11f, 5.11f });
    // interior walls (with passage gaps in the middle)
    walls.push_back({ -5.0f, -0.375f, -1.2f, -0.225f });
    walls.push_back({  1.2f, -0.375f,  5.0f, -0.225f });
    walls.push_back({ -5.0f, -2.975f, -1.0f, -2.825f });
    walls.push_back({  1.0f, -2.975f,  5.0f, -2.825f });
    // furniture
    walls.push_back({ -3.75f, 1.6f, -2.05f, 2.6f });   // table
    walls.push_back({  2.65f, 1.6f,  3.85f, 2.4f });   // cabinet
    walls.push_back({ -3.4f, -4.2f, -2.2f, -3.4f });   // wardrobe
    walls.push_back({  2.5f, -4.1f,  3.1f, -3.5f });   // chest
    // fence line at z = 15 with a gate gap
    walls.push_back({ -18.0f, 14.85f, -1.5f, 15.15f });
    walls.push_back({   1.5f, 14.85f, 18.0f, 15.15f });
    // trees and graves
    for (int i = 0; i < TREE_COUNT; i++) {
        float s = TREES[i][2] * 0.25f;
        walls.push_back({ TREES[i][0] - s, TREES[i][1] - s, TREES[i][0] + s, TREES[i][1] + s });
    }
    for (int i = 0; i < GRAVE_COUNT; i++)
        walls.push_back({ GRAVES[i][0] - 0.45f, GRAVES[i][1] - 0.2f, GRAVES[i][0] + 0.45f, GRAVES[i][1] + 0.2f });
}

void resetGame()
{
    px = 0; py = 1.65f; pz = 11.5f; yaw = 0; pitch = -4; bobPhase = 0;
    sanity = 100; battery = 100; stamina = 100;
    flashlight = true; doorOpen = false; doorAngle = 0; gateAngle = 0;
    keyCount = 0; playTime = 0; hurtFlash = 0;

    items.clear();
    items.push_back({ -3.9f, 0.55f, -4.3f, ITEM_KEY, false, "Key 1: 'The children hid it where the wardrobe weeps...'" });
    items.push_back({  3.25f, 1.65f, 2.0f, ITEM_KEY, false, "Key 2: Dusty, cold to the touch. Something is watching." });
    items.push_back({  4.2f, 0.45f, -1.6f, ITEM_KEY, false, "Key 3: The final key! RUN to the iron gate!" });
    items.push_back({ -2.6f, 0.95f, 2.1f, ITEM_BATTERY, false, "Battery found (+50%)." });
    items.push_back({  4.2f, 0.25f, -4.3f, ITEM_BATTERY, false, "Battery found (+50%)." });
    items.push_back({ -4.2f, 0.25f, 4.2f, ITEM_BATTERY, false, "Battery found (+50%)." });

    ghosts[0] = { 1.4f, 1.25f, -2.0f, 1.55f, 0, 0, 0.7f, 0, true };
    ghosts[1] = { -3.6f, 1.25f, -4.0f, 1.75f, 5, 0, 0.0f, 0, false };

    setStatus("Find 3 golden keys inside the house, then escape through the gate behind you.", 6);
}

// =====================================================================
//  DRAWING HELPERS
// =====================================================================
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

void ground()
{
    box(0, -0.12f, 0, 42, 0.24f, 42, 0.08f, 0.11f, 0.09f);
    // stone path from gate to door
    for (float z = 5.6f; z < 15; z += 0.9f)
        box(std::sin(z) * 0.15f, 0.01f, z, 1.4f, 0.04f, 0.7f, 0.18f, 0.17f, 0.16f);
}

void roof()
{
    glDisable(GL_LIGHTING);
    glColor3f(0.12f, 0.025f, 0.035f);
    glBegin(GL_QUADS);
      glVertex3f(-5.35f,4.5f,-5.3f); glVertex3f(0,7,-5.3f);
      glVertex3f(0,7,5.3f);          glVertex3f(-5.35f,4.5f,5.3f);
      glVertex3f(0,7,-5.3f);         glVertex3f(5.35f,4.5f,-5.3f);
      glVertex3f(5.35f,4.5f,5.3f);   glVertex3f(0,7,5.3f);
    glEnd();
    glColor3f(0.20f, 0.055f, 0.06f);
    glBegin(GL_TRIANGLES);
      glVertex3f(-5,4.45f,5.01f);  glVertex3f(5,4.45f,5.01f);  glVertex3f(0,6.8f,5.01f);
      glVertex3f(-5,4.45f,-5.01f); glVertex3f(0,6.8f,-5.01f);  glVertex3f(5,4.45f,-5.01f);
    glEnd();
    // ceiling so the inside is truly dark
    glColor3f(0.05f, 0.04f, 0.04f);
    glBegin(GL_QUADS);
      glVertex3f(-5,4.48f,-5); glVertex3f(5,4.48f,-5); glVertex3f(5,4.48f,5); glVertex3f(-5,4.48f,5);
    glEnd();
    glEnable(GL_LIGHTING);
}

void windowFront(float x)
{
    box(x,2.45f,5.14f,1.15f,1.25f,0.16f,0.10f,0.045f,0.025f);
    // window glass glows faintly from inside (brighter on lightning)
    glDisable(GL_LIGHTING);
    float g = 0.25f + 0.35f * flicker;
    box(x,2.45f,5.235f,0.91f,1.01f,0.035f,g*0.9f,g*0.6f,g*0.25f);
    glEnable(GL_LIGHTING);
    box(x,2.45f,5.26f,0.055f,1.04f,0.04f,0.17f,0.085f,0.045f);
    box(x,2.45f,5.26f,0.98f,0.055f,0.04f,0.17f,0.085f,0.045f);
}

void door()
{
    glPushMatrix();
    glTranslatef(-1.0f,0,5.08f); glRotatef(doorAngle,0,1,0);
    glPushMatrix();
      glTranslatef(0.95f,1.15f,0); glScalef(1.9f,2.3f,0.16f);
      glColor3f(0.12f,0.045f,0.025f); glutSolidCube(1);
    glPopMatrix();
    // panels + knob move with the door
    box(0.95f,1.65f,0.09f,1.4f,0.7f,0.03f,0.16f,0.06f,0.03f);
    box(0.95f,0.65f,0.09f,1.4f,0.7f,0.03f,0.16f,0.06f,0.03f);
    sphere(1.6f,1.15f,0.12f,0.07f,0.85f,0.58f,0.12f);
    glPopMatrix();
}

void candle(float x, float y, float z, float phase)
{
    box(x, y + 0.09f, z, 0.06f, 0.18f, 0.06f, 0.85f, 0.82f, 0.7f);
    glDisable(GL_LIGHTING);
    float f = 0.8f + 0.2f * std::sin(glutGet(GLUT_ELAPSED_TIME) * 0.02f + phase);
    glPushMatrix(); glTranslatef(x, y + 0.22f, z); glScalef(0.6f, 1.3f * f, 0.6f);
    glColor3f(1.0f, 0.6f * f, 0.1f); glutSolidSphere(0.035, 10, 8);
    glPopMatrix();
    glEnable(GL_LIGHTING);
}

void house()
{
    box(0,0.035f,0,10,0.07f,10,0.20f,0.16f,0.13f);   // floor

    // exterior walls
    box(-3,2.25f,5,4,4.5f,0.22f,0.24f,0.20f,0.18f);
    box( 3,2.25f,5,4,4.5f,0.22f,0.24f,0.20f,0.18f);
    box(0,4,5,2,1,0.22f,0.24f,0.20f,0.18f);
    box(-5,2.25f,0,0.22f,4.5f,10,0.21f,0.17f,0.16f);
    box( 5,2.25f,0,0.22f,4.5f,10,0.21f,0.17f,0.16f);
    box(0,2.25f,-5,10,4.5f,0.22f,0.19f,0.15f,0.14f);

    // interior walls with passages
    box(-3.1f,2.1f,-0.3f,3.8f,4.2f,0.15f,0.15f,0.12f,0.12f);
    box( 3.1f,2.1f,-0.3f,3.8f,4.2f,0.15f,0.15f,0.12f,0.12f);
    box(-3.0f,2.0f,-2.9f,4.0f,4.0f,0.15f,0.13f,0.10f,0.10f);
    box( 3.0f,2.0f,-2.9f,4.0f,4.0f,0.15f,0.13f,0.10f,0.10f);

    windowFront(-3); windowFront(3); door(); roof();

    // table, legs and candles
    box(-2.9f,0.75f,2.1f,1.7f,0.16f,1,0.15f,0.055f,0.025f);
    box(-3.55f,0.35f,1.75f,0.12f,0.7f,0.12f,0.12f,0.045f,0.02f);
    box(-2.25f,0.35f,1.75f,0.12f,0.7f,0.12f,0.12f,0.045f,0.02f);
    box(-3.55f,0.35f,2.45f,0.12f,0.7f,0.12f,0.12f,0.045f,0.02f);
    box(-2.25f,0.35f,2.45f,0.12f,0.7f,0.12f,0.12f,0.045f,0.02f);
    candle(-3.4f, 0.83f, 2.0f, 0.0f);
    candle(-3.15f, 0.83f, 2.3f, 1.7f);

    // cabinet, wardrobe, chest
    box(3.25f,0.7f,2,1.2f,1.4f,0.75f,0.10f,0.035f,0.025f);
    box(3.25f,1.45f,2,1.3f,0.12f,0.82f,0.17f,0.055f,0.035f);
    box(-2.8f,1,-3.8f,1.2f,2,0.75f,0.10f,0.045f,0.035f);
    box(-2.8f,1,-3.39f,0.95f,1.7f,0.04f,0.17f,0.07f,0.04f);
    box(2.8f,0.45f,-3.8f,0.55f,0.9f,0.55f,0.12f,0.045f,0.025f);

    // portrait with glowing eyes in the hall
    box(0,2.6f,-0.2f,0.9f,1.1f,0.05f,0.18f,0.12f,0.05f);
    box(0,2.6f,-0.17f,0.7f,0.9f,0.03f,0.06f,0.05f,0.06f);
    glDisable(GL_LIGHTING);
    float e = 0.4f + 0.6f * flicker;
    sphere(-0.1f,2.75f,-0.15f,0.03f,e,0,0); sphere(0.1f,2.75f,-0.15f,0.03f,e,0,0);

    // hanging ceiling lamp
    glColor3f(0.1f,0.1f,0.1f);
    glBegin(GL_LINES); glVertex3f(0,4.48f,1.5f); glVertex3f(0,3.75f,1.5f); glEnd();
    sphere(0,3.7f,1.5f,0.12f,flicker,0.75f*flicker,0.35f*flicker);
    // cold blue orb in the rear room
    sphere(-2.7f,3.2f,-3.9f,0.1f,0.3f,0.45f,1.0f);
    glEnable(GL_LIGHTING);

    // cobwebs (thin lines in corners)
    glDisable(GL_LIGHTING); glColor3f(0.45f,0.45f,0.5f);
    glBegin(GL_LINES);
    for (int i = 0; i < 6; i++) {
        float t = i * 0.12f;
        glVertex3f(-4.88f,4.4f-t,4.88f); glVertex3f(-4.88f+t,4.4f,4.88f);
        glVertex3f( 4.88f,4.4f-t,-4.88f); glVertex3f( 4.88f-t,4.4f,-4.88f);
    }
    glEnd(); glEnable(GL_LIGHTING);
}

void tree(float x, float z, float s)
{
    box(x,0.9f*s,z,0.35f*s,1.8f*s,0.35f*s,0.17f,0.075f,0.035f);
    // bare twisted branches
    glPushMatrix(); glTranslatef(x,1.7f*s,z); glRotatef(40,0,0,1);
    box(0,0.4f*s,0,0.1f*s,0.9f*s,0.1f*s,0.15f,0.07f,0.03f); glPopMatrix();
    glPushMatrix(); glTranslatef(x,1.5f*s,z); glRotatef(-50,1,0,1);
    box(0,0.4f*s,0,0.1f*s,0.8f*s,0.1f*s,0.15f,0.07f,0.03f); glPopMatrix();
    sphere(x,2*s,z,0.8f*s,0.025f,0.07f,0.04f);
}

void gravestone(float x, float z, float rot)
{
    glPushMatrix(); glTranslatef(x,0,z); glRotatef(rot,0,0,1);
    box(0,0.45f,0,0.8f,0.9f,0.18f,0.28f,0.28f,0.30f);
    glPushMatrix(); glTranslatef(0,0.9f,0); glScalef(0.4f,0.2f,0.09f);
    glColor3f(0.28f,0.28f,0.30f); glutSolidSphere(1,14,10); glPopMatrix();
    box(0,0.6f,0.1f,0.08f,0.35f,0.02f,0.12f,0.12f,0.13f);
    box(0,0.66f,0.1f,0.25f,0.07f,0.02f,0.12f,0.12f,0.13f);
    glPopMatrix();
}

void fenceAndGate()
{
    for (float x = -18; x <= 18; x += 0.6f) {
        if (std::fabs(x) < 1.6f) continue;
        box(x,0.7f,15,0.07f,1.4f,0.07f,0.08f,0.08f,0.09f);
        box(x,1.45f,15,0.03f,0.12f,0.03f,0.12f,0.12f,0.12f);
    }
    box(-9.8f,0.4f,15,16.4f,0.06f,0.05f,0.08f,0.08f,0.09f);
    box( 9.8f,0.4f,15,16.4f,0.06f,0.05f,0.08f,0.08f,0.09f);
    box(-9.8f,1.1f,15,16.4f,0.06f,0.05f,0.08f,0.08f,0.09f);
    box( 9.8f,1.1f,15,16.4f,0.06f,0.05f,0.08f,0.08f,0.09f);
    // stone gate posts with lanterns
    box(-1.65f,1.1f,15,0.35f,2.2f,0.35f,0.22f,0.21f,0.2f);
    box( 1.65f,1.1f,15,0.35f,2.2f,0.35f,0.22f,0.21f,0.2f);
    glDisable(GL_LIGHTING);
    bool open = keyCount >= 3;
    float lr = open ? 0.2f : 1.0f, lg = open ? 1.0f : 0.25f, lb = open ? 0.35f : 0.1f;
    sphere(-1.65f,2.35f,15,0.12f,lr,lg,lb);
    sphere( 1.65f,2.35f,15,0.12f,lr,lg,lb);
    glEnable(GL_LIGHTING);
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

void moonAndStars()
{
    glDisable(GL_LIGHTING); glDisable(GL_FOG);
    glColor3f(0.98f,0.94f,0.72f);
    glPushMatrix(); glTranslatef(-9,10,-13); glutSolidSphere(1.25,24,18); glPopMatrix();
    glPointSize(2.5f); glColor3f(0.85f,0.88f,1);
    srand(7);
    glBegin(GL_POINTS);
    for (int i = 0; i < 90; i++) glVertex3f(frand(-30,30), frand(7,20), frand(-30,-14));
    glEnd();
    srand(glutGet(GLUT_ELAPSED_TIME));
    glEnable(GL_FOG); glEnable(GL_LIGHTING);
}

void drawKeyItem(float x, float y, float z, float rot)
{
    glPushMatrix(); glTranslatef(x,y,z); glRotatef(rot,0,1,0);
    glDisable(GL_LIGHTING);
    glColor3f(1.0f,0.82f,0.2f);
    glPushMatrix(); glTranslatef(-0.12f,0,0); glutSolidTorus(0.025,0.07,10,16); glPopMatrix();
    glPushMatrix(); glTranslatef(0.08f,0,0); glScalef(0.26f,0.035f,0.035f); glutSolidCube(1); glPopMatrix();
    glPushMatrix(); glTranslatef(0.17f,-0.04f,0); glScalef(0.035f,0.06f,0.035f); glutSolidCube(1); glPopMatrix();
    glPushMatrix(); glTranslatef(0.11f,-0.035f,0); glScalef(0.03f,0.04f,0.035f); glutSolidCube(1); glPopMatrix();
    // halo
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
    glColor4f(1.0f,0.75f,0.2f,0.18f); glutSolidSphere(0.28,16,12);
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void drawBatteryItem(float x, float y, float z, float rot)
{
    glPushMatrix(); glTranslatef(x,y,z); glRotatef(rot,0,1,0);
    glDisable(GL_LIGHTING);
    box(0,0,0,0.1f,0.22f,0.1f,0.1f,0.75f,0.25f);
    box(0,0.13f,0,0.05f,0.04f,0.05f,0.8f,0.8f,0.8f);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
    glColor4f(0.2f,1.0f,0.4f,0.15f); glutSolidSphere(0.22,14,10);
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void drawItems()
{
    float t = glutGet(GLUT_ELAPSED_TIME) * 0.001f;
    for (size_t i = 0; i < items.size(); i++) {
        const Item& it = items[i];
        if (it.taken) continue;
        float bob = 0.05f * std::sin(t * 2.5f + i);
        if (it.type == ITEM_KEY) drawKeyItem(it.x, it.y + bob, it.z, t * 90);
        else drawBatteryItem(it.x, it.y + bob, it.z, t * 60);
    }
}

void drawGhost(const Ghost& g)
{
    if (!g.active || g.alpha <= 0.01f) return;
    float t = glutGet(GLUT_ELAPSED_TIME) * 0.001f;
    float face = std::atan2(px - g.x, pz - g.z) * 180.0f / PI;
    glPushMatrix();
    glTranslatef(g.x, g.y + 0.15f * std::sin(t * 3 + g.x), g.z);
    glRotatef(face, 0, 1, 0);
    glDisable(GL_LIGHTING); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    bool angry = g.stun <= 0;
    glColor4f(0.78f, 0.88f, 1, g.alpha);
    glPushMatrix(); glTranslatef(0,0.35f,0); glScalef(0.75f,1,0.55f); glutSolidSphere(0.65,22,18); glPopMatrix();
    // wavy tail
    glColor4f(0.72f, 0.84f, 1, g.alpha * 0.85f);
    for (int i = -1; i <= 1; i++) {
        glPushMatrix();
        glTranslatef(i * 0.28f, -0.12f + 0.05f * std::sin(t * 6 + i), 0);
        glutSolidSphere(0.25, 14, 12);
        glPopMatrix();
    }
    // arms reaching forward when chasing
    if (angry) {
        glPushMatrix(); glTranslatef(-0.45f,0.3f,0.35f); glScalef(0.12f,0.12f,0.5f); glutSolidSphere(1,10,8); glPopMatrix();
        glPushMatrix(); glTranslatef( 0.45f,0.3f,0.35f); glScalef(0.12f,0.12f,0.5f); glutSolidSphere(1,10,8); glPopMatrix();
    }
    glDepthMask(GL_TRUE);
    // eyes + mouth
    glColor4f(angry ? 1.0f : 0.3f, 0.02f, angry ? 0.02f : 0.8f, std::min(1.0f, g.alpha + 0.3f));
    glPushMatrix(); glTranslatef(-0.20f,0.45f,0.49f); glutSolidSphere(0.075,12,10); glPopMatrix();
    glPushMatrix(); glTranslatef( 0.20f,0.45f,0.49f); glutSolidSphere(0.075,12,10); glPopMatrix();
    glColor4f(0.05f,0.0f,0.05f, g.alpha);
    glPushMatrix(); glTranslatef(0,0.18f,0.5f); glScalef(1,1.4f,0.4f); glutSolidSphere(0.09,12,10); glPopMatrix();
    glDisable(GL_BLEND); glEnable(GL_LIGHTING);
    glPopMatrix();
}

// =====================================================================
//  LIGHTING
// =====================================================================
void flashlightLight()   // called with identity modelview -> follows camera
{
    if (flashlight && battery > 0) {
        float strength = battery < 15 ? (0.5f + 0.5f * (std::sin(glutGet(GLUT_ELAPSED_TIME) * 0.05f) > 0)) : 1.0f;
        GLfloat fp[] = { 0,0,0,1 }, fd[] = { 0,0,-1 };
        GLfloat fcol[] = { 0.95f*strength, 0.92f*strength, 0.78f*strength, 1 };
        glLightfv(GL_LIGHT1, GL_POSITION, fp); glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, fd);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, fcol);
        glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 30);
        glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 16);
        glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0.6f);
        glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.04f);
        glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.008f);
        glEnable(GL_LIGHT1);
    } else glDisable(GL_LIGHT1);
}

void worldLights()       // called after gluLookAt -> fixed in world
{
    GLfloat ambient[] = { 0.025f, 0.025f, 0.04f, 1 };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    // moonlight + lightning
    GLfloat mp[] = { -0.5f, 1.0f, -0.6f, 0 };
    GLfloat md[] = { 0.08f + lightning, 0.09f + lightning, 0.16f + lightning, 1 };
    glLightfv(GL_LIGHT0, GL_POSITION, mp); glLightfv(GL_LIGHT0, GL_DIFFUSE, md);

    // flickering ceiling lamp
    GLfloat p[] = { 0,3.6f,1.5f,1 };
    GLfloat d[] = { 0.95f*flicker,0.72f*flicker,0.42f*flicker,1 };
    GLfloat a[] = { 0.06f*flicker,0.04f*flicker,0.015f*flicker,1 };
    glLightfv(GL_LIGHT2, GL_POSITION, p); glLightfv(GL_LIGHT2, GL_DIFFUSE, d);
    glLightfv(GL_LIGHT2, GL_AMBIENT, a);
    glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, 0.7f);
    glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.12f);
    glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.035f);
    glEnable(GL_LIGHT2);

    // cold blue light in the rear room
    GLfloat bp[] = { -2.7f,3.2f,-3.9f,1 };
    GLfloat bd[] = { 0.12f,0.18f,0.45f,1 };
    glLightfv(GL_LIGHT3, GL_POSITION, bp); glLightfv(GL_LIGHT3, GL_DIFFUSE, bd);
    glLightf(GL_LIGHT3, GL_CONSTANT_ATTENUATION, 0.8f);
    glLightf(GL_LIGHT3, GL_LINEAR_ATTENUATION, 0.16f); glEnable(GL_LIGHT3);

    // red light near ghosts
    if (ghosts[0].active) {
        GLfloat gp[] = { ghosts[0].x, ghosts[0].y + 0.5f, ghosts[0].z, 1 };
        GLfloat gd[] = { 0.35f * ghosts[0].alpha, 0.05f, 0.08f, 1 };
        glLightfv(GL_LIGHT4, GL_POSITION, gp); glLightfv(GL_LIGHT4, GL_DIFFUSE, gd);
        glLightf(GL_LIGHT4, GL_LINEAR_ATTENUATION, 0.5f);
        glEnable(GL_LIGHT4);
    }

    float fogD = 0.055f - lightning * 0.03f;
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
    // HUD space is 1000 wide; convert pixel width to HUD units
    float w = textWidth(text, font) * 1000.0f / winW;
    drawText(500 - w / 2, y, text, font);
}
void rect(float x0, float y0, float x1, float y1, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS); glVertex2f(x0,y0); glVertex2f(x1,y0); glVertex2f(x1,y1); glVertex2f(x0,y1); glEnd();
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
    // world x[-9,9], z[-7,17] -> panel
    float mx0 = 800, my0 = 450, mw = 180, mh = 240;
    rect(mx0 - 4, my0 - 4, mx0 + mw + 4, my0 + mh + 4, 0, 0, 0, 0.6f);
    auto MX = [&](float x) { return mx0 + (x + 9) / 18.0f * mw; };
    auto MY = [&](float z) { return my0 + mh - (z + 7) / 24.0f * mh; };
    for (size_t i = 0; i < walls.size(); i++) {
        const AABB& w = walls[i];
        if (w.x1 < -9 || w.x0 > 9) continue;
        rect(MX(std::max(-9.0f, w.x0)), MY(w.z1), MX(std::min(9.0f, w.x1)), MY(w.z0), 0.6f, 0.55f, 0.5f, 0.9f);
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
    // player arrow
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
    if (lightning > 0.05f) rect(0, 0, 1000, 700, 0.8f, 0.85f, 1.0f, lightning * 0.3f);
    if (hurtFlash > 0) rect(0, 0, 1000, 700, 0.7f, 0, 0, hurtFlash * 0.5f);
    if (state == PLAYING && sanity < 40) {
        float pulse = (0.5f + 0.5f * std::sin(glutGet(GLUT_ELAPSED_TIME) * 0.006f)) * (40 - sanity) / 40.0f;
        rect(0, 0, 1000, 40, 0.6f, 0, 0, pulse * 0.6f);   rect(0, 660, 1000, 700, 0.6f, 0, 0, pulse * 0.6f);
        rect(0, 0, 40, 700, 0.6f, 0, 0, pulse * 0.6f);    rect(960, 0, 1000, 700, 0.6f, 0, 0, pulse * 0.6f);
    }

    if (state == MENU) {
        rect(0, 0, 1000, 700, 0, 0, 0, 0.75f);
        glColor3f(0.9f, 0.1f, 0.1f);
        drawTextCentered(500, "T H E   L A S T   L A N T E R N", GLUT_BITMAP_TIMES_ROMAN_24);
        glColor3f(0.85f, 0.85f, 0.9f);
        drawTextCentered(450, "Three golden keys lie hidden inside the Blackwood house.", GLUT_BITMAP_HELVETICA_18);
        drawTextCentered(420, "Find them all and escape through the iron gate before your mind breaks.", GLUT_BITMAP_HELVETICA_18);
        drawTextCentered(380, "Your flashlight repels the ghosts... but the battery will not last forever.", GLUT_BITMAP_HELVETICA_18);
        glColor3f(0.7f, 0.75f, 0.9f);
        drawTextCentered(320, "WASD move | Arrows look | SPACE sprint | F flashlight | E door | M map | P pause", GLUT_BITMAP_HELVETICA_12);
        glColor3f(1, 0.8f, 0.3f);
        if ((glutGet(GLUT_ELAPSED_TIME) / 500) % 2) drawTextCentered(250, "Press ENTER to begin", GLUT_BITMAP_TIMES_ROMAN_24);
    }
    else if (state == WON || state == LOST) {
        rect(0, 0, 1000, 700, 0, 0, 0, 0.7f);
        if (state == WON) { glColor3f(0.3f, 1, 0.4f); drawTextCentered(420, "YOU ESCAPED THE HAUNTED HOUSE!", GLUT_BITMAP_TIMES_ROMAN_24); }
        else { glColor3f(1, 0.1f, 0.1f); drawTextCentered(420, "YOUR MIND HAS SHATTERED...", GLUT_BITMAP_TIMES_ROMAN_24);
               glColor3f(0.8f, 0.6f, 0.6f); drawTextCentered(385, "You are now part of the house forever.", GLUT_BITMAP_HELVETICA_18); }
        glColor3f(0.9f, 0.9f, 0.9f);
        snprintf(buf, sizeof(buf), "Time: %d:%02d    Keys found: %d / 3    Sanity left: %d%%",
                 (int)playTime / 60, (int)playTime % 60, keyCount, (int)std::max(0.0f, sanity));
        drawTextCentered(340, buf, GLUT_BITMAP_HELVETICA_18);
        glColor3f(1, 0.8f, 0.3f);
        drawTextCentered(280, "Press ENTER to play again  |  ESC to quit", GLUT_BITMAP_HELVETICA_18);
    }
    else {
        // bars
        bar(18, 90, "SANITY", sanity, 0.8f, 0.15f, 0.2f);
        bar(18, 70, battery > 0 ? (flashlight ? "BATTERY (ON)" : "BATTERY (OFF)") : "BATTERY DEAD", battery, 0.95f, 0.85f, 0.3f);
        bar(18, 50, "STAMINA", stamina, 0.3f, 0.6f, 1.0f);

        // keys
        glColor3f(1, 0.82f, 0.25f);
        snprintf(buf, sizeof(buf), "KEYS: %d / 3", keyCount);
        drawText(18, 120, buf, GLUT_BITMAP_HELVETICA_18);
        glColor3f(0.8f, 0.8f, 0.8f);
        snprintf(buf, sizeof(buf), "TIME %d:%02d", (int)playTime / 60, (int)playTime % 60);
        drawText(18, 145, buf);

        // objective
        glColor3f(0.75f, 0.85f, 1.0f);
        drawText(18, 22, keyCount < 3 ? "Objective: Search the house for the golden keys."
                                      : "Objective: ESCAPE through the iron gate south of the house!");

        if (statusTimer > 0) {
            glColor4f(1.0f, 0.75f, 0.35f, std::min(1.0f, statusTimer));
            drawTextCentered(180, statusMessage, GLUT_BITMAP_HELVETICA_18);
        }

        // interaction prompt
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

        // crosshair
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
    float sky = lightning * 0.5f;
    glClearColor(0.008f + sky, 0.012f + sky, 0.035f + sky, 1);
    GLfloat fogC[] = { 0.01f + sky, 0.015f + sky, 0.04f + sky, 1 };
    glFogfv(GL_FOG_COLOR, fogC);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    flashlightLight();

    float yr = rad(yaw), pr = rad(pitch);
    float dx = std::sin(yr) * std::cos(pr), dy = std::sin(pr), dz = -std::cos(yr) * std::cos(pr);
    // camera shake when hurt
    float shake = hurtFlash * 0.08f;
    float sx = frand(-shake, shake), sy = frand(-shake, shake);
    gluLookAt(px + sx, py + sy, pz, px + dx + sx, py + dy + sy, pz + dz, 0, 1, 0);
    worldLights();

    ground(); house(); fenceAndGate();
    for (int i = 0; i < TREE_COUNT; i++) tree(TREES[i][0], TREES[i][1], TREES[i][2]);
    for (int i = 0; i < GRAVE_COUNT; i++) gravestone(GRAVES[i][0], GRAVES[i][1], GRAVES[i][2]);
    moonAndStars();
    drawItems();
    drawGhost(ghosts[0]); drawGhost(ghosts[1]);
    hud();
    glutSwapBuffers();
}

void reshape(int w, int h)
{
    if (h == 0) h = 1;
    winW = w; winH = h;
    glViewport(0, 0, w, h); glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(70.0, (double)w / (double)h, 0.1, 100.0);
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

    // battery
    if (flashlight && battery > 0) {
        battery = std::max(0.0f, battery - 1.1f * dt);
        if (battery <= 0) { flashlight = false; setStatus("Your flashlight died! Find a battery!", 4); }
    }

    // darkness drains sanity inside the house
    bool nearGhost = false;
    for (int i = 0; i < 2; i++) if (ghosts[i].active) {
        float gx = ghosts[i].x - px, gz = ghosts[i].z - pz;
        if (gx * gx + gz * gz < 25) nearGhost = true;
    }
    if (insideHouse() && !(flashlight && battery > 0)) sanity -= 2.2f * dt;
    else if (nearGhost) sanity -= 0.8f * dt;
    else sanity = std::min(100.0f, sanity + 1.2f * dt);

    // pickups
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
            } else {
                battery = std::min(100.0f, battery + 50);
                setStatus(it.note, 2.5f);
            }
        }
    }

    // win: walked through the open gate
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
        if (g.alpha < 0.7f && g.stun <= 0) g.alpha = std::min(0.7f, g.alpha + 0.4f * dt);   // fade in

        float dx = px - g.x, dz = pz - g.z;
        float d = std::sqrt(dx * dx + dz * dz) + 0.0001f;

        // is the ghost inside the flashlight cone?
        if (lightOn && d < 7.5f) {
            float c = (-dx * vfx - dz * vfz) / d;
            if (c > 0.93f) {
                if (g.stun <= 0) setStatus("The light burns it! The ghost recoils!", 1.5f);
                g.stun = 1.4f;
                battery = std::max(0.0f, battery - 3.0f * dt);   // repelling costs extra battery
            }
        }
        g.hitCooldown -= dt;

        bool playerSafe = pz > 13.5f;   // yard near the gate is sacred ground
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

        // touching the player
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

    // ambient effects run in every state (menu looks alive too)
    flicker = std::max(0.15f, std::min(1.0f, 0.60f + 0.28f * std::sin(now * 0.017f) + 0.12f * std::sin(now * 0.071f)));
    if ((now / 1000) % 9 == 0 && std::sin(now * 0.05f) > 0.95f) flicker = 0.1f;   // occasional blackout
    nextLightning -= dt;
    if (nextLightning <= 0) {
        lightning = 1.0f;
        nextLightning = frand(6, 15);
    }
    if (lightning > 0) {
        lightning -= dt * 2.5f;
        if (lightning < 0.35f && lightning > 0.3f && rand() % 2) lightning = 0.8f;  // double flash
        if (lightning < 0) lightning = 0;
    }
    hurtFlash = std::max(0.0f, hurtFlash - dt * 1.2f);
    statusTimer -= dt;

    if (state == PLAYING) {
        playTime += dt;
        updatePlayer(dt);
        updateGhosts(dt);
    }
    if (state == MENU) yaw += 6 * dt;   // slow cinematic pan on the title screen

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
void keyDown(unsigned char k, int, int)
{
    k = (unsigned char)std::tolower(k);
    bool firstPress = !keyDownArr[k];
    keyDownArr[k] = true;
    if (!firstPress) return;

    if (k == 27 || k == 'q') std::exit(0);

    if (k == 13) {                                  // ENTER
        if (state == MENU || state == WON || state == LOST) {
            resetGame(); state = PLAYING;
        }
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
            // don't let the door close on top of the player
            if (doorOpen && std::fabs(dz) < 0.5f && std::fabs(dx) < 1.2f) setStatus("Step out of the doorway first.", 2);
            else {
                doorOpen = !doorOpen;
                setStatus(doorOpen ? "The door creaks open..." : "The door slams shut.", 2.5f);
            }
        } else setStatus("Move closer to the front door to interact.", 2);
    }
    else if (k == 'h') showHelp = !showHelp;
    else if (k == 'm') showMap = !showMap;
    else if (k == 'r') { resetGame(); setStatus("Game restarted.", 2); }
}
void keyUp(unsigned char k, int, int) { keyDownArr[(unsigned char)std::tolower(k)] = false; }
void specialDown(int k, int, int) { if (k >= 0 && k < 256) arrows[k] = true; }
void specialUp(int k, int, int)   { if (k >= 0 && k < 256) arrows[k] = false; }

// =====================================================================
//  INIT / MAIN
// =====================================================================
void init()
{
    glClearColor(0.008f, 0.012f, 0.035f, 1);
    glEnable(GL_DEPTH_TEST); glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    GLfloat amb[] = { 0.04f,0.04f,0.07f,1 }, diff[] = { 0.08f,0.09f,0.16f,1 };
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb); glLightfv(GL_LIGHT0, GL_DIFFUSE, diff);

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);
    glFogf(GL_FOG_DENSITY, 0.055f);
    glHint(GL_FOG_HINT, GL_NICEST);

    srand(1234);
    buildColliders();
    resetGame();
    state = MENU;
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH); glutInitWindowPosition(80, 50);
    glutCreateWindow("The Last Lantern - 3D Haunted House (OpenGL/GLUT)");
    init();
    glutDisplayFunc(display); glutReshapeFunc(reshape);
    glutKeyboardFunc(keyDown); glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown); glutSpecialUpFunc(specialUp);
    glutIgnoreKeyRepeat(0);
    lastTime = glutGet(GLUT_ELAPSED_TIME); glutTimerFunc(16, update, 0);
    glutMainLoop();
    return 0;
}
