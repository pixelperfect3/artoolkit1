/*	Virtual Environments Project 1
	Name: Shayan Javed
	Term: Spring 2009 

	Mass-spring model ARToolkit demonstration */

#ifdef _WIN32
#  include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <iostream>

#ifndef __APPLE__
#  include <GL/glut.h>
#else
#  include <GLUT/glut.h>
#endif
#include <AR/gsub.h>
#include <AR/param.h>
#include <AR/ar.h>
#include <AR/video.h>

#include "object.h"

// the simple geometry library
#include <sg.h>

using namespace std;

#define COLLIDE_DIST 30000.0

/* Object Data */
char            *model_name = "Data/object_data2";
ObjectData_T    *object;
int             objectnum;

int             xsize, ysize;
int				thresh = 100;
int             count = 0;

/* the matrices in relation to the two markers */
double  wmat1[3][4], wmat2[3][4];

/* the mass-spring model setup */
bool first = true; // is it being detected for the first time?
bool setup = false; // have they been setup?

/* Setup the particles */
const int numParticles = 5;
sgParticle *particles[numParticles];
sgParticle *prevParticles[numParticles]; // the previous positions

// does previous exist or not?
bool prev = false;

/* The spring dampers */
const int numDampers = numParticles - 1;
sgSpringDamper *sprDampers[numDampers];

/* The spring values */
float _stiffness = 0.5;
float _damping = 0.5;
float _restLength = -1.0;

/* set up the video format globals */

#ifdef _WIN32
char			*vconf = "Data\\WDM_camera_flipV.xml";
#else
char			*vconf = "";
#endif

char           *cparam_name    = "Data/camera_para.dat";
ARParam         cparam;

static void   init(void);
static void   cleanup(void);
static void   keyEvent( unsigned char key, int x, int y);
static void   mainLoop(void);
static int draw( ObjectData_T *object, int objectnum );
static void drawTwoObjects(double gl_para1[16], double gl_para2[16]);
static int  draw_object( int obj_id, double gl_para[16] );

int main(int argc, char **argv)
{
	//initialize applications
	glutInit(&argc, argv);
    init();
	
	arVideoCapStart();

	//start the main event loop
    argMainLoop( NULL, keyEvent, mainLoop );

	return 0;
}

static void   keyEvent( unsigned char key, int x, int y)   
{
    /* quit if the ESC key is pressed */
    if( key == 0x1b ) {
        printf("*** %f (frame/sec)\n", (double)count/arUtilTimer());
        cleanup();
        exit(0);
    }
}

/* main loop */
static void mainLoop(void)
{
    ARUint8         *dataPtr;
    ARMarkerInfo    *marker_info;
    int             marker_num;
    int             i,j,k;

    /* grab a video frame */
    if( (dataPtr = (ARUint8 *)arVideoGetImage()) == NULL ) {
        arUtilSleep(2);
        return;
    }
	
    if( count == 0 ) arUtilTimerReset();  
    count++;

	/*draw the video*/
    argDrawMode2D();
    argDispImage( dataPtr, 0,0 );

	glColor3f( 1.0, 0.0, 0.0 );
	glLineWidth(6.0);

	/* detect the markers in the video frame */ 
	if(arDetectMarker(dataPtr, thresh, &marker_info, &marker_num) < 0 ) {
		cleanup(); 
		exit(0);
	}
	for( i = 0; i < marker_num; i++ ) {
		argDrawSquare(marker_info[i].vertex,0,0);
	}

	/* check for known patterns */
    for( i = 0; i < objectnum; i++ ) {
		k = -1;
		for( j = 0; j < marker_num; j++ ) {
	        if( object[i].id == marker_info[j].id) {

				/* you've found a pattern */
				//printf("Found pattern: %d ",patt_id);
				glColor3f( 0.0, 1.0, 0.0 );
				argDrawSquare(marker_info[j].vertex,0,0);

				if( k == -1 ) k = j;
		        else /* make sure you have the best pattern (highest confidence factor) */
					if( marker_info[k].cf < marker_info[j].cf ) k = j;
			}
		}
		if( k == -1 ) {
			object[i].visible = 0;
			continue;
		}
		
		/* calculate the transform for each marker */
		if( object[i].visible == 0 ) {
            arGetTransMat(&marker_info[k],
                          object[i].marker_center, object[i].marker_width,
                          object[i].trans);
        }
        else {
            arGetTransMatCont(&marker_info[k], object[i].trans,
                          object[i].marker_center, object[i].marker_width,
                          object[i].trans);
        }
        object[i].visible = 1;
	}
	
	arVideoCapNext();

	/* draw the AR graphics */
    draw( object, objectnum );

	/*swap the graphics buffers*/
	argSwapBuffers();
}

static void init( void )
{
	ARParam  wparam;

    /* open the video path */
    if( arVideoOpen( vconf ) < 0 ) exit(0);
    /* find the size of the window */
    if( arVideoInqSize(&xsize, &ysize) < 0 ) exit(0);
    printf("Image size (x,y) = (%d,%d)\n", xsize, ysize);

    /* set the initial camera parameters */
    if( arParamLoad(cparam_name, 1, &wparam) < 0 ) {
        printf("Camera parameter load error !!\n");
        exit(0);
    }
    arParamChangeSize( &wparam, xsize, ysize, &cparam );
    arInitCparam( &cparam );
    printf("*** Camera Parameter ***\n");
    arParamDisp( &cparam );

	/* load in the object data - trained markers and associated bitmap files */
    if( (object=read_ObjData(model_name, &objectnum)) == NULL ) exit(0);
    printf("Objectfile num = %d\n", objectnum);

    /* open the graphics window */
    argInit( &cparam, 1.0, 0, 0, 0, 0 );

	/* initialize the particles */

}

/* cleanup function called when program exits */
static void cleanup(void)
{
	arVideoCapStop();
    arVideoClose();
    argCleanup();
}

/* draw the the AR objects */
static int draw( ObjectData_T *object, int objectnum )
{
    int     i;
    double  gl_para1[16], gl_para2[16]; // the parameters for the two markers
       
	if (!(object[0].visible && object[1].visible))
		return 0;
	else {
        arUtilMatInv(object[0].trans, wmat1);
        arUtilMatMul(wmat1, object[1].trans, wmat2);
	}

	// If detected for the first time, setup the "cloth"
	if (first) {
		// setup left first?
		sgVec3 sgv1 = {0.0, 0.0, 30.0};
		sgVec3 sgv2 = {wmat2[0][3], wmat2[1][3], wmat2[2][3]};

		particles[0] = new sgParticle(1.0, 0.0, 0.0, 30.0);
		particles[numParticles - 1] = new sgParticle(1.0, sgv2);
		particles[1] = new sgParticle(1.0, wmat2[0][3]/numParticles, wmat2[1][3]/numParticles, wmat2[2][3]/numParticles);
		particles[2] = new sgParticle(1.0, (wmat2[0][3]*2)/numParticles, (wmat2[1][3]*2)/numParticles, (wmat2[2][3]*2)/numParticles);
		particles[3] = new sgParticle(1.0, (wmat2[0][3]*3)/numParticles, (wmat2[1][3]*3)/numParticles, (wmat2[2][3]*3)/numParticles);

		// now the spring dampers
		for (int i = 0; i < numDampers; i++) 
			sprDampers[i] = new sgSpringDamper(particles[i], particles[i+1], _stiffness, _damping, _restLength);

		first = false;
		setup = true;

		cout << "Finished 1st setup " << endl;
	}
	else { // just update the position of the end two
		particles[0] = new sgParticle(1.0, 0.0, 0.0, 30.0);
		particles[numParticles-1] = new sgParticle(1.0, wmat2[0][3], wmat2[1][3], wmat2[2][3]);

		sgVec3 sgv1 = {0.0, 0.0, 30.0};
		sgVec3 sgv2 = {wmat2[0][3], wmat2[1][3], wmat2[2][3]};

		particles[0] = new sgParticle(1.0, 0.0, 0.0, 30.0);
		particles[numParticles - 1] = new sgParticle(1.0, sgv2);
		particles[1] = new sgParticle(1.0, wmat2[0][3]/numParticles, wmat2[1][3]/numParticles, wmat2[2][3]/numParticles);
		particles[2] = new sgParticle(1.0, (wmat2[0][3]*2)/numParticles, (wmat2[1][3]*2)/numParticles, (wmat2[2][3]*2)/numParticles);
		particles[3] = new sgParticle(1.0, (wmat2[0][3]*3)/numParticles, (wmat2[1][3]*3)/numParticles, (wmat2[2][3]*3)/numParticles);

		// now the spring dampers
		for (int i = 0; i < numDampers; i++) 
			sprDampers[i] = new sgSpringDamper(particles[i], particles[i+1], _stiffness, _damping, _restLength);

		cout << "Finished 2nd setup " << endl;
	}

	glClearDepth( 1.0 );
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_LIGHTING);

    /* calculate the viewing parameters - gl_para */
	argConvGlpara(object[0].trans, gl_para1);
	argConvGlpara(object[1].trans, gl_para2);

    /*for( i = 0; i < objectnum; i++ ) {
        if( object[i].visible == 0 ) continue;
        argConvGlpara(object[i].trans, gl_para);
        draw_object( object[i].id, gl_para);
    }*/

	// draw the two objects
	drawTwoObjects(gl_para1, gl_para2);
     
	glDisable( GL_LIGHTING );
    glDisable( GL_DEPTH_TEST );
	
    return(0);
}

/* NEW DRAW function */
static void drawTwoObjects(double gl_para1[16], double gl_para2[16]) {
	if (!setup)
		return;

	GLfloat   mat_ambient[]				= {0.0, 0.0, 1.0, 1.0};
	GLfloat   mat_ambient_collide[]     = {1.0, 0.0, 0.0, 1.0};
    GLfloat   mat_flash[]				= {0.0, 0.0, 1.0, 1.0};
	GLfloat   mat_flash_collide[]       = {1.0, 0.0, 0.0, 1.0};
    GLfloat   mat_flash_shiny[] = {50.0};
    GLfloat   light_position[]  = {100.0,-200.0,200.0,0.0};
    GLfloat   ambi[]            = {0.1, 0.1, 0.1, 0.1};
    GLfloat   lightZeroColor[]  = {0.9, 0.9, 0.9, 0.1};
 
    argDrawMode3D();
    argDraw3dCamera( 0, 0 );
    glMatrixMode(GL_MODELVIEW);

	// First object
	glLoadIdentity();
    glLoadMatrixd( gl_para1 );

 	/* set the material */
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambi);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightZeroColor);

    glMaterialfv(GL_FRONT, GL_SHININESS, mat_flash_shiny);	

	glColor3f(1.0, 0.0, 0.0);

	// apply the forces
	for (int i = 0; i < numParticles; i++)
		//if (i != 0 && i != numParticles-1)
			//particles[i]->setForce(0.0, -0.1, 0.0);
		//else
			particles[i]->zeroForce();

	cout << "Applied forces" << endl;

	// update the spring dampers
	for (int i = 0; i < numDampers; i++)
		sprDampers[i]->update();

	cout << "Applied damper updates" << endl;

	// update the particles
	for (int i = 0; i < numParticles; i++)
		if (i != 0 && i != numParticles-1)
			particles[i]->update(0.05);

	cout << "Updated particles" << endl;

	// draw the lines
	glDisable(GL_LIGHTING);
	glBegin(GL_LINE_STRIP);
		for (int i = 0; i < numParticles-1; i++) {
			glVertex3fv(particles[i]->getPos());
			//glLoadMatrixd(gl_para2);
			glVertex3fv(particles[i+1]->getPos());
		}
	glEnd();

	cout << "drew lines" << endl;
	//glTranslatef(0.0, 0.0, 30.0);
	//glutSolidSphere(30,12,6);

	// Second object
	glEnable(GL_LIGHTING);
	glLoadIdentity();
    glLoadMatrixd( gl_para2 );
	glTranslatef(0.0, 0.0, 30.0);
	//glutSolidCube(50.0);


	argDrawMode2D();
}


/* draw the user object */
static int  draw_object( int obj_id, double gl_para[16])
{
    GLfloat   mat_ambient[]				= {0.0, 0.0, 1.0, 1.0};
	GLfloat   mat_ambient_collide[]     = {1.0, 0.0, 0.0, 1.0};
    GLfloat   mat_flash[]				= {0.0, 0.0, 1.0, 1.0};
	GLfloat   mat_flash_collide[]       = {1.0, 0.0, 0.0, 1.0};
    GLfloat   mat_flash_shiny[] = {50.0};
    GLfloat   light_position[]  = {100.0,-200.0,200.0,0.0};
    GLfloat   ambi[]            = {0.1, 0.1, 0.1, 0.1};
    GLfloat   lightZeroColor[]  = {0.9, 0.9, 0.9, 0.1};
 
    argDrawMode3D();
    argDraw3dCamera( 0, 0 );
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixd( gl_para );

 	/* set the material */
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambi);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightZeroColor);

    glMaterialfv(GL_FRONT, GL_SHININESS, mat_flash_shiny);	

	if(obj_id == 0){
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_flash_collide);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient_collide);
		/* draw a cube */
		glTranslatef( 0.0, 0.0, 30.0 );
		glutSolidSphere(30,12,6);
	}
	else {
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_flash);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		/* draw a cube */
		glTranslatef( 0.0, 0.0, 30.0 );
		glutSolidCube(60);
	}

    argDrawMode2D();

    return 0;
}
