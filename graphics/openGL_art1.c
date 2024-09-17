/**
 *  file: openGL_art1.c
 *  created on: 14 Apr 2024
 *
 *  OpenGL Art 1:  Polygonal Animation
 *    number of sides and colors changes
 *    until it almost looks like a circle
 *
 *    keyboard events:
 *      p, space    pause
 *      r           resume
 *      [, {        increase the spped
 *      ], }        decrease the speed
 *      q           quit
 *    mouse events:
 *      <left>      pause
 *      <right>     resume
 *
 *  Compilation:
 *    cc -ggdb -O3 -Wall -Wextra -Werror openGL_art1.c \
 *      $(pkg-config --cflags glut opengl) \
 *      $(pkg-config --libs glut opengl) -lm \
 *      -o glArt1.out
 **/
#include <GL/gl.h>
#include <GL/glut.h>
#include <math.h>
#include <stdbool.h>

#define PI 3.141516
#define MAX_ROUND 6


#define GLDO(r,g,b, __DO__) do {glColor3f (r, g, b); __DO__;} while (0)
#define NORM(x, xMax) (float)(x) / (float)(xMax)
#define MAX(a, b) ((a)<(b)) ? (b) : (a)
#define ABS(x) ((x)>=0) ? (x) : (-x)
#define SGN(x) MAX(x, 0)

float d_plus=0.025f, d_minus=0.025f;
float end_deg = 2*PI;
float anim_deg = 0;
int n = 32;
float step_deg;
int rot_dir = 1;
bool pause = false;


void
display ()
{
  float r_col;
  float deg = anim_deg;

  n = 8*deg;
  if (n%2 != 0) n++;
  if (n < 3) n = 3;
  step_deg = end_deg / (float)n;
  r_col = 1 - NORM (deg, MAX_ROUND*PI);

  glClearColor (0, 0, 0, 1);
  glClear (GL_COLOR_BUFFER_BIT);

  glBegin (GL_TRIANGLES);
  for (int i = n; i != 0; --i)
    {
      GLDO (r_col, 0, 0, {
          glVertex2f (0, 0);
        });

      GLDO (0.5, (i%2!=0), (i%2==0), {
          glVertex2f (cosf (deg), sinf (deg));
        });

      deg += step_deg;
      GLDO (0, SGN ((i%2==0) - 0.1),
               SGN ((i%2!=0) - 0.3), {
          glVertex2f (cosf (deg), sinf (deg));
        });
    }
  glEnd ();

  glutSwapBuffers ();
}

void
Timer (int)
{
  if (pause)
    {
      // do nothing
    }
  else
    {
      if (rot_dir)
        {
          anim_deg += d_plus;
          if (anim_deg > 6*PI)
            {
              rot_dir = 0;
            }
        }
      else
        {
          anim_deg -= d_minus;
          if (anim_deg < 0.0f)
            {
              rot_dir = 1;
            }
        }
    }
  glutPostRedisplay();
  glutTimerFunc(40, Timer, 0);
}

void
MouseHandler (int btn, int state, int, int)
{
  if (btn == GLUT_LEFT_BUTTON)
    {
      if (state == GLUT_DOWN)
        pause = !pause;
    }
  else if (btn == GLUT_RIGHT_BUTTON)
    {
      if (state == GLUT_DOWN)
        {
          rot_dir = !rot_dir;
          if (pause) pause = false;
        }
    }
}

void
KeyboardHandler (unsigned char key, int, int)
{
  switch (key)
    {
    case 'r':
      rot_dir = !rot_dir;
      if (pause) pause = false;
      break;
    case 'p':
    case ' ':
    case 'k':
      pause = !pause;
      break;
    case '[':
      d_plus += 0.005;
      break;
    case '{':
      if (d_plus > 0.005)
        d_plus -= 0.005;
      break;
    case ']':
      d_minus += 0.005;
      break;
    case '}':
      if (d_minus > 0.005)
        d_minus -= 0.005;
      break;
    case 'q':
    case 27: // ESCAPE
      exit (0);
      break;
    }
}

int main( int argc, char** argv )
{
  glutInit (&argc, argv);
  glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowSize (900, 900);
  glutInitWindowPosition (0, 0);
  glutCreateWindow ("Polygonal animation");

  glutDisplayFunc (display);
  glutTimerFunc (40, Timer, 0);
  glutMouseFunc (MouseHandler);
  glutKeyboardFunc (KeyboardHandler);
  glutMainLoop();

  return 0;
}

