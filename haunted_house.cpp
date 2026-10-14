#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>

// 3D HAUNTED HOUSE - one-file OpenGL/freeglut demo
// WASD: move | Arrow keys: look | F: flashlight | E: front door | Q/ESC: quit

const float PI = 3.14159265f;
float px = 0.0f, py = 1.65f, pz = 11.5f;
float yaw = 0.0f, pitch = -4.0f;
bool flashlight = true, doorOpen = false;
float doorAngle = 0.0f, ghostZ = -2.0f, ghostDir = 1.0f, ghostBob = 0.0f, flicker = 0.7f;
int lastTime = 0;
bool keys[256] = { false };
bool arrows[256] = { false };
bool showHelp = true;
const char* statusMessage = "Explore the house. Press H to toggle help.";
int statusUntil = 0;

float rad(float d) { return d * PI / 180.0f; }

void box(float x,float y,float z,float sx,float sy,float sz,float r,float g,float b)
{
    glPushMatrix();
    glTranslatef(x,y,z); glScalef(sx,sy,sz);
    glColor3f(r,g,b); glutSolidCube(1.0);
    glPopMatrix();
}
void sphere(float x,float y,float z,float s,float r,float g,float b)
{
    glPushMatrix(); glTranslatef(x,y,z); glColor3f(r,g,b);
    glutSolidSphere(s,20,16); glPopMatrix();
}

void ground()
{
    box(0,-0.12f,0,42,0.24f,42,0.08f,0.11f,0.09f);
}

void roof()
{
    glDisable(GL_LIGHTING);
    glColor3f(0.12f,0.025f,0.035f);
    glBegin(GL_QUADS);
      glVertex3f(-5.35f,4.5f,-5.3f); glVertex3f(0,7,-5.3f);
      glVertex3f(0,7,5.3f); glVertex3f(-5.35f,4.5f,5.3f);
      glVertex3f(0,7,-5.3f); glVertex3f(5.35f,4.5f,-5.3f);
      glVertex3f(5.35f,4.5f,5.3f); glVertex3f(0,7,5.3f);
    glEnd();
    glColor3f(0.20f,0.055f,0.06f);
    glBegin(GL_TRIANGLES);
      glVertex3f(-5,4.45f,5.01f); glVertex3f(5,4.45f,5.01f); glVertex3f(0,6.8f,5.01f);
      glVertex3f(-5,4.45f,-5.01f); glVertex3f(0,6.8f,-5.01f); glVertex3f(5,4.45f,-5.01f);
    glEnd();
    glEnable(GL_LIGHTING);
}

void windowFront(float x)
{
    box(x,2.45f,5.14f,1.15f,1.25f,0.16f,0.10f,0.045f,0.025f);
    box(x,2.45f,5.235f,0.91f,1.01f,0.035f,0.08f,0.13f,0.19f);
    box(x,2.45f,5.26f,0.055f,1.04f,0.04f,0.17f,0.085f,0.045f);
    box(x,2.45f,5.26f,0.98f,0.055f,0.04f,0.17f,0.085f,0.045f);
}

void door()
{
    // Door rotates around a hinge on the left side of the entrance.
    glPushMatrix();
    glTranslatef(-1.0f,0,5.08f); glRotatef(doorAngle,0,1,0);
    glTranslatef(0.95f,1.15f,0); glScalef(1.9f,2.3f,0.16f);
    glColor3f(0.12f,0.045f,0.025f); glutSolidCube(1);
    glPopMatrix();
    sphere(0.55f,1.15f,5.22f,0.09f,0.85f,0.58f,0.12f);
}

void house()
{
    // Interior floor
    box(0,0.035f,0,10,0.07f,10,0.20f,0.16f,0.13f);

    // Exterior walls; front is split to create the doorway.
    box(-3,2.25f,5,4,4.5f,0.22f,0.24f,0.20f,0.18f);
    box( 3,2.25f,5,4,4.5f,0.22f,0.24f,0.20f,0.18f);
    box(0,4,5,2,1,0.22f,0.24f,0.20f,0.18f);
    box(-5,2.25f,0,0.22f,4.5f,10,0.21f,0.17f,0.16f);
    box( 5,2.25f,0,0.22f,4.5f,10,0.21f,0.17f,0.16f);
    box(0,2.25f,-5,10,4.5f,0.22f,0.19f,0.15f,0.14f);

    // Interior walls divide the house into dim rooms, leaving passage gaps.
    box(-3.1f,2.1f,-0.3f,3.8f,4.2f,0.15f,0.15f,0.12f,0.12f);
    box( 3.1f,2.1f,-0.3f,3.8f,4.2f,0.15f,0.15f,0.12f,0.12f);
    box(-2.55f,2.0f,-2.9f,4.9f,4.0f,0.15f,0.13f,0.10f,0.10f);
    box( 2.55f,2.0f,-2.9f,4.9f,4.0f,0.15f,0.13f,0.10f,0.10f);

    windowFront(-3); windowFront(3); door(); roof();

    // Furniture: table and legs
    box(-2.9f,0.75f,2.1f,1.7f,0.16f,1,0.15f,0.055f,0.025f);
    box(-3.55f,0.35f,1.75f,0.12f,0.7f,0.12f,0.12f,0.045f,0.02f);
    box(-2.25f,0.35f,1.75f,0.12f,0.7f,0.12f,0.12f,0.045f,0.02f);
    box(-3.55f,0.35f,2.45f,0.12f,0.7f,0.12f,0.12f,0.045f,0.02f);
    box(-2.25f,0.35f,2.45f,0.12f,0.7f,0.12f,0.12f,0.045f,0.02f);

    // Cabinet and chest
    box(3.25f,0.7f,2,1.2f,1.4f,0.75f,0.10f,0.035f,0.025f);
    box(3.25f,1.45f,2,1.3f,0.12f,0.82f,0.17f,0.055f,0.035f);
    box(-2.8f,1,-3.8f,1.2f,2,0.75f,0.10f,0.045f,0.035f);
    box(-2.8f,1,-3.39f,0.95f,1.7f,0.04f,0.17f,0.07f,0.04f);
    box(2.8f,0.45f,-3.8f,0.55f,0.9f,0.55f,0.12f,0.045f,0.025f);
}

void tree(float x,float z,float s)
{
    box(x,0.9f*s,z,0.35f*s,1.8f*s,0.35f*s,0.17f,0.075f,0.035f);
    sphere(x,2*s,z,0.9f*s,0.025f,0.09f,0.045f);
    sphere(x,2.65f*s,z,0.65f*s,0.02f,0.07f,0.035f);
}

void moonAndStars()
{
    glDisable(GL_LIGHTING);
    glColor3f(0.98f,0.94f,0.72f);
    glPushMatrix(); glTranslatef(-9,10,-13); glutSolidSphere(1.25,24,18); glPopMatrix();
    glPointSize(3); glColor3f(0.85f,0.88f,1);
    glBegin(GL_POINTS);
      glVertex3f(-12,8,-12); glVertex3f(-8,12,-15); glVertex3f(-4,9,-14);
      glVertex3f(0,12,-15); glVertex3f(4,10,-14); glVertex3f(8,12,-15);
      glVertex3f(12,9,-12); glVertex3f(10,7,-16); glVertex3f(-2,11,-18);
      glVertex3f(6,8,-18);
    glEnd();
    glEnable(GL_LIGHTING);
}

void ghost()
{
    glPushMatrix();
    glTranslatef(1.4f,1.25f+ghostBob,ghostZ);
    glDisable(GL_LIGHTING); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.78f,0.88f,1,0.72f);
    glPushMatrix(); glTranslatef(0,0.35f,0); glScalef(0.75f,1,0.55f);
    glutSolidSphere(0.65,22,18); glPopMatrix();
    glColor4f(0.72f,0.84f,1,0.65f);
    glPushMatrix(); glTranslatef(-0.28f,-0.1f,0); glutSolidSphere(0.25,14,12); glPopMatrix();
    glPushMatrix(); glTranslatef(0,-0.16f,0); glutSolidSphere(0.25,14,12); glPopMatrix();
    glPushMatrix(); glTranslatef(0.28f,-0.1f,0); glutSolidSphere(0.25,14,12); glPopMatrix();
    glColor3f(1,0.02f,0.02f);
    glPushMatrix(); glTranslatef(-0.20f,0.42f,0.49f); glutSolidSphere(0.075,12,10); glPopMatrix();
    glPushMatrix(); glTranslatef(0.20f,0.42f,0.49f); glutSolidSphere(0.075,12,10); glPopMatrix();
    glDisable(GL_BLEND); glEnable(GL_LIGHTING);
    glPopMatrix();
}

void lights()
{
    GLfloat ambient[]={0.025f,0.025f,0.04f,1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);

    // Flickering warm ceiling lamp
    GLfloat p[]={0,3.7f,0,1};
    GLfloat d[]={0.95f*flicker,0.72f*flicker,0.42f*flicker,1};
    GLfloat a[]={0.06f*flicker,0.04f*flicker,0.015f*flicker,1};
    glLightfv(GL_LIGHT2,GL_POSITION,p); glLightfv(GL_LIGHT2,GL_DIFFUSE,d);
    glLightfv(GL_LIGHT2,GL_AMBIENT,a);
    glLightf(GL_LIGHT2,GL_CONSTANT_ATTENUATION,0.7f);
    glLightf(GL_LIGHT2,GL_LINEAR_ATTENUATION,0.12f);
    glLightf(GL_LIGHT2,GL_QUADRATIC_ATTENUATION,0.035f);
    glEnable(GL_LIGHT2);

    // Cold light in the rear room
    GLfloat bp[]={-2.7f,3.2f,-3.5f,1};
    GLfloat bd[]={0.12f,0.18f,0.40f,1};
    glLightfv(GL_LIGHT3,GL_POSITION,bp); glLightfv(GL_LIGHT3,GL_DIFFUSE,bd);
    glLightf(GL_LIGHT3,GL_CONSTANT_ATTENUATION,0.8f);
    glLightf(GL_LIGHT3,GL_LINEAR_ATTENUATION,0.16f); glEnable(GL_LIGHT3);

    // Spotlight fixed to the camera's viewing direction
    if(flashlight) {
        GLfloat fp[]={0,0,0,1}, fd[]={0,0,-1};
        GLfloat fcol[]={0.95f,0.92f,0.78f,1};
        glLightfv(GL_LIGHT1,GL_POSITION,fp); glLightfv(GL_LIGHT1,GL_SPOT_DIRECTION,fd);
        glLightfv(GL_LIGHT1,GL_DIFFUSE,fcol);
        glLightf(GL_LIGHT1,GL_SPOT_CUTOFF,32);
        glLightf(GL_LIGHT1,GL_SPOT_EXPONENT,18);
        glLightf(GL_LIGHT1,GL_CONSTANT_ATTENUATION,0.7f);
        glLightf(GL_LIGHT1,GL_LINEAR_ATTENUATION,0.045f);
        glLightf(GL_LIGHT1,GL_QUADRATIC_ATTENUATION,0.008f);
        glEnable(GL_LIGHT1);
    } else glDisable(GL_LIGHT1);
}

void drawText(int x, int y, const char* text)
{
    glRasterPos2i(x, y);
    for (const char* c = text; *c; ++c)
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *c);
}

void hud()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 1000, 0, 700);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(0.95f, 0.92f, 0.80f);
    drawText(18, 22, "WASD: Move | Arrows: Look | F: Flashlight | E: Door | H: Help | R: Reset | Q/ESC: Quit");
    drawText(18, 42, flashlight ? "Flashlight: ON" : "Flashlight: OFF");

    int now = glutGet(GLUT_ELAPSED_TIME);
    if (now < statusUntil || statusUntil == 0)
    {
        glColor3f(1.0f, 0.75f, 0.35f);
        drawText(18, 62, statusMessage);
    }

    if (showHelp)
    {
        glColor3f(0.78f, 0.86f, 1.0f);
        drawText(18, 674, "CONTROLS");
        drawText(18, 656, "W / S: forward and backward");
        drawText(18, 638, "A / D: strafe left and right");
        drawText(18, 620, "Arrow keys: look around");
        drawText(18, 602, "F: toggle flashlight");
        drawText(18, 584, "E: open/close the front door when nearby");
        drawText(18, 566, "R: return to starting position");
        drawText(18, 548, "H: hide/show this help");
        drawText(18, 530, "Q or ESC: quit");
    }

    glColor3f(1.0f, 0.12f, 0.12f);
    glBegin(GL_LINES);
        glVertex2i(493, 350); glVertex2i(507, 350);
        glVertex2i(500, 343); glVertex2i(500, 357);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    float yr=rad(yaw), pr=rad(pitch);
    float dx=std::sin(yr)*std::cos(pr), dy=std::sin(pr), dz=-std::cos(yr)*std::cos(pr);
    gluLookAt(px,py,pz,px+dx,py+dy,pz+dz,0,1,0);
    lights();
    ground(); house();
    tree(-8,3,1.3f); tree(8,2,1.5f); tree(-8,-5,1.1f); tree(8,-6,1.2f);
    moonAndStars(); ghost(); hud();
    glutSwapBuffers();
}

void reshape(int w,int h)
{
    if(h==0) h=1;
    glViewport(0,0,w,h); glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(70.0,(double)w/(double)h,0.1,100.0);
    glMatrixMode(GL_MODELVIEW);
}

void clampPlayer()
{
    px=std::max(-18.0f,std::min(18.0f,px));
    pz=std::max(-18.0f,std::min(18.0f,pz));
    // Approximate collision against the exterior side/back walls.
    if(px < -5.25f && px > -5.8f && pz > -5.1f && pz < 5.1f) px=-5.25f;
    if(px > 5.0f && px < 5.8f && pz > -5.1f && pz < 5.1f) px=5.0f;
    if(pz < -5.0f && pz > -5.8f && px > -5.1f && px < 5.1f) pz=-5.0f;
    // Front wall collision except through the entrance.
    if(pz < 5.35f && pz > 4.75f) {
        bool doorway=(px > -1.05f && px < 1.05f);
        if(!doorway || (!doorOpen && pz < 5.15f)) pz=5.35f;
    }
}

void update(int)
{
    int now=glutGet(GLUT_ELAPSED_TIME);
    float dt=(now-lastTime)/1000.0f; lastTime=now; if(dt>0.05f) dt=0.05f;
    float y=rad(yaw), fx=std::sin(y), fz=-std::cos(y), rx=std::cos(y), rz=std::sin(y);
    float v=4.0f*dt;
    if(keys['w']||keys['W']) {px+=fx*v;pz+=fz*v;}
    if(keys['s']||keys['S']) {px-=fx*v;pz-=fz*v;}
    if(keys['a']||keys['A']) {px-=rx*v;pz-=rz*v;}
    if(keys['d']||keys['D']) {px+=rx*v;pz+=rz*v;}
    if(arrows[GLUT_KEY_LEFT]) yaw-=70*dt;
    if(arrows[GLUT_KEY_RIGHT]) yaw+=70*dt;
    if(arrows[GLUT_KEY_UP]) pitch+=55*dt;
    if(arrows[GLUT_KEY_DOWN]) pitch-=55*dt;
    pitch=std::max(-75.0f,std::min(75.0f,pitch));
    clampPlayer();

    ghostZ+=ghostDir*1.2f*dt;
    if(ghostZ>1.5f) ghostDir=-1;
    if(ghostZ< -4.0f) ghostDir=1;
    ghostBob=0.15f*std::sin(now*0.003f);
    flicker=std::max(0.18f,std::min(1.0f,0.60f+0.28f*std::sin(now*0.017f)+0.12f*std::sin(now*0.071f)));

    float target=doorOpen?-95.0f:0.0f;
    if(doorAngle<target) doorAngle=std::min(target,doorAngle+120*dt);
    if(doorAngle>target) doorAngle=std::max(target,doorAngle-120*dt);

    glutPostRedisplay(); glutTimerFunc(16,update,0);
}

void keyDown(unsigned char k, int, int)
{
    // Only process toggle keys on the initial press, not key auto-repeat.
    bool firstPress = !keys[k];
    keys[k] = true;
    if (!firstPress) return;

    if (k == 27 || k == 'q' || k == 'Q')
        std::exit(0);

    if (k == 'f' || k == 'F')
    {
        flashlight = !flashlight;
        statusMessage = flashlight ? "Flashlight switched ON." : "Flashlight switched OFF.";
        statusUntil = glutGet(GLUT_ELAPSED_TIME) + 2500;
    }
    else if (k == 'e' || k == 'E')
    {
        float dx = px;
        float dz = pz - 5.0f;
        if (dx*dx + dz*dz < 4.0f)
        {
            doorOpen = !doorOpen;
            statusMessage = doorOpen ? "You opened the front door." : "You closed the front door.";
        }
        else
        {
            statusMessage = "Move closer to the front door to interact.";
        }
        statusUntil = glutGet(GLUT_ELAPSED_TIME) + 3000;
    }
    else if (k == 'h' || k == 'H')
    {
        showHelp = !showHelp;
    }
    else if (k == 'r' || k == 'R')
    {
        px = 0.0f;
        py = 1.65f;
        pz = 11.5f;
        yaw = 0.0f;
        pitch = -4.0f;
        statusMessage = "Position reset.";
        statusUntil = glutGet(GLUT_ELAPSED_TIME) + 2500;
    }
}
void keyUp(unsigned char k, int, int)
{
    keys[k] = false;
}
void specialDown(int k,int,int) { if(k>=0&&k<256) arrows[k]=true; }
void specialUp(int k,int,int) { if(k>=0&&k<256) arrows[k]=false; }

void init()
{
    glClearColor(0.008f,0.012f,0.035f,1);
    glEnable(GL_DEPTH_TEST); glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    GLfloat amb[]={0.04f,0.04f,0.07f,1}, diff[]={0.08f,0.09f,0.16f,1};
    glLightfv(GL_LIGHT0,GL_AMBIENT,amb); glLightfv(GL_LIGHT0,GL_DIFFUSE,diff);
}

int main(int argc,char** argv)
{
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH);
    glutInitWindowSize(1100,750); glutInitWindowPosition(80,50);
    glutCreateWindow("3D Haunted House - OpenGL/freeglut");
    init();
    glutDisplayFunc(display); glutReshapeFunc(reshape);
    glutKeyboardFunc(keyDown); glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown); glutSpecialUpFunc(specialUp);
    lastTime=glutGet(GLUT_ELAPSED_TIME); glutTimerFunc(16,update,0);
    glutMainLoop();
    return 0;
}
