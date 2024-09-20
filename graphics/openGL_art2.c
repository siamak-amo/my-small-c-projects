/**
 *  file: openGL_art2.c
 *  created on: 17 Sep 2024
 *
 *  OpenGL Art 2:  Snowflake
 *    Terms:
 *      RF factor    controls how the color changes
 *      D factor     controls how fast the edges get thinner
 *      N            number of clusters
 *
 *    keyboard events:
 *      d, D           increase / decrease the D factor
 *      +, -           same as d, but more accurate
 *      n, N           increase / decrease the N by 1
 *
 *      r, R           increase / decrease the RF Red factor
 *      g, G           increase / decrease the RF Green factor
 *      b, B           increase / decrease the RF Blue factor
 *      <space>        to revert to defaults
 *      q              quit
 *
 *    mouse events:
 *      Scroll UP      increade the D factor
 *      Scroll Down    decrease the D factor
 *
 *  Usage:
 *    ./glart2.out [N] [D factor]
 *
 *  Compilation:
 *    cc -ggdb -O3 -Wall -Wextra -Werror openGL_art2.c \
 *      $(pkg-config --cflags glut opengl) \
 *      $(pkg-config --libs glut opengl) -lm \
 *      -o glart2.out
 *  Options:
 *    define `-D TRI_FAC=0.2` to make lines (triangles) thicker
 *    default is 0.15
 **/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glut.h>

#define PI 3.141516
#define UNUSED(x) (void)(x)
#define GLDO(r,g,b, __DO__) do {glColor3f (r, g, b); __DO__;} while (0)

/* isosceles base factor */
#ifndef TRI_FAC
#  define TRI_FAC 0.15
#endif
/* default values */
#ifndef DEF_D
#  define DEF_D 2.73
#endif
#ifndef DEF_N
#  define DEF_N 6
#endif
#ifndef DEF_RF
#  define DEF_RF (vec3_t){0.05, 0.3, 0.3}
#endif

/* vectors 2D and 3D */
typedef struct {
    float x, y, z;
} vec3_t; /* for colors R,G,B */

typedef struct {
    float x, y;
} vec2_t;

struct snow_vertex {
  /* center of snowflake fractal */
  vec2_t center;
  vec2_t __tmp; /* helper */
  vec2_t __M; /* 2pi/n factor */

  float length;
  vec3_t color;
};

/* number of vertexes */
static int N = DEF_N;
/* RF color factor */
static vec3_t RF = DEF_RF;
#define UPDATE_RF(col, p) if (((col)+=p)>=1.0 || col <= 0){ col=0; }
/* edge's length factor */
static float D_factor = DEF_D;
#define SET_DF(val) do {                                \
    if (D_factor < 6 && D_factor > 1) D_factor += val;  \
    else D_factor = 2;                                  \
  } while (0)

/**
 *  draws the line between @p1, @p2
 *  using one (isosceles) triangle
 *  with a base of 2*TRI_FAC
 */
static inline void
glTri2f (const vec2_t *restrict p1,
         const vec2_t *restrict p2)
{
  vec2_t pt = {
    .y = (p2->x - p1->x) * TRI_FAC,
    .x = (p1->y - p2->y) * TRI_FAC,
  };
  glVertex2f (p1->x + pt.x, p1->y + pt.y);
  glVertex2f (p1->x - pt.x, p1->y - pt.y);
  glVertex2f (p2->x, p2->y);
}

/**
 *  2pi/n rotation
 *  using imaginary like numbers multiplication
 */
static inline void
sv_next_vert (struct snow_vertex *sv)
{
  vec2_t V2 = sv->__M;
  vec2_t V1 = sv->__tmp;

  /* map V1 into center */
  V1.x -= sv->center.x;
  V1.y -= sv->center.y;

  /**
   *  assume Vj = Xj + i.Yj  for j=1,2
   *  we set:  sv->__tmp = V1*V2 + sv->center
   */
  sv->__tmp.x = (V1.x * V2.x - V1.y * V2.y) +  sv->center.x;
  sv->__tmp.y = (V1.x * V2.y + V1.y * V2.x) +  sv->center.y;
}

// moves the center one layer deeper
static inline void
sv_update_center (struct snow_vertex *sv)
{
  sv->center = sv->__tmp;
  sv->__tmp.x = sv->length + sv->center.x;
  sv->__tmp.y = sv->center.y;
  
  sv->__M.y = sinf (2*PI/N);
  sv->__M.x = cosf (2*PI/N);
  
  sv->length /= D_factor;
}


#define sv_update_color(sv) do {                \
    UPDATE_RF ((sv)->color.x, RF.x);            \
    UPDATE_RF ((sv)->color.y, RF.y);            \
    UPDATE_RF ((sv)->color.z, RF.z);            \
  } while (0)

/**
 *  main logic
 *  recursive function
 */
void
sv_draw (struct snow_vertex *sv, int depth)
{
  if (N > 6 && depth < 2)
    return;
  if (depth < 1 || sv->length < 0.001)
    return;
  for (int i=0; i < N; ++i)
    {
      glBegin(GL_TRIANGLES);
      GLDO(sv->color.x, sv->color.y, sv->color.z, {
        glTri2f (&sv->center, &sv->__tmp);
        });
      glEnd();
      sv_next_vert (sv);
    }
  for (int i=0; i < N; ++i)
    {
      struct snow_vertex cpy = *sv;
      sv_update_center (&cpy);
      sv_update_color (&cpy);
      sv_draw (&cpy, depth-1);
      sv_next_vert (sv);
    }
}

void
Display ()
{
  struct snow_vertex sv = {
    .center = {0}, 
    .color = {0},
    .length = 1 - 1/D_factor 
  };
  glClearColor (0, 0, 0, 1);
  glClear (GL_COLOR_BUFFER_BIT);

  /* 7 layer recursion */
  sv_draw (&sv, 7);

  glutSwapBuffers ();
}

void
ScrollHandler (int button, int state, int, int)
{
  switch (button)
    {
    case 3:
    case 4:
      if (state != GLUT_UP)
        {
          if (button == 3)
            SET_DF (0.01);
          else
            SET_DF (-0.01);
          glutPostRedisplay();
        }
      break;

    default:
      break;
    }
}

void
KeyboardHandler (unsigned char key, int, int)
{
  switch (key)
    {
    case 'r':
      UPDATE_RF (RF.x, +0.05);
      break;
    case 'R':
      UPDATE_RF (RF.x, -0.05);
      break;
    case 'g':
      UPDATE_RF (RF.y, +0.05);
      break;
    case 'G':
      UPDATE_RF (RF.y, -0.05);
      break;
    case 'b':
      UPDATE_RF (RF.z, +0.05);
      break;
    case 'B':
      UPDATE_RF (RF.z, -0.05);
      break;

    case 'd':
      SET_DF (0.02);
      break;
    case 'D':
      SET_DF (-0.02);
      break;

    case '+':
      SET_DF (0.005);
      break;
    case '-':
      SET_DF (-0.005);
      break;

    case 'n':
      if (N < 8) N++;
      break;
    case 'N':
      if (N > 3) N--;
      break;

    case ' ':
      N = DEF_N;
      D_factor = DEF_D;
      RF = DEF_RF;
      break;

    case 'q':
    case 'x':
      exit (0);

    default:
      break;
    }

  printf ("RF{r:%f, g:%f, b:%f}, D_factor: %f, N: %d\n",
          RF.x, RF.y, RF.z, D_factor, N);
  glutPostRedisplay ();
}

int
main (int argc, char **argv)
{
  if (argc >= 2)
    N = atoi (argv[1]);
  if (argc >= 3)
    D_factor = atof (argv[2]);

  glutInit (&argc, argv);
  glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowSize (1000, 1000);
  glutInitWindowPosition (0, 0);
  glutCreateWindow ("Snowflake ~ OpenGL Art 2");

  {
    glutDisplayFunc (Display);
    glutMouseFunc (ScrollHandler);
    glutKeyboardFunc (KeyboardHandler);
    glutMainLoop ();
  }

  return 0;
}
