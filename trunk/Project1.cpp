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
double  wmat1[3][4], wmat2[3][4], wmat3[3][4];

/* the mass-spring model setup */
bool first = true; // is it being detected for the first time?
bool setup = false; // have they been setup?

/* Setup the particles */
const int numParticles = 5;
const int rows = 5;
const int columns = 5;
//sgParticle *particles[numParticles];
sgParticle *row1[5];
sgParticle *row2[4];
sgParticle *row3[3];
sgParticle *row4[2];
sgParticle *row5;

//sgParticle *prevParticles[numParticles]; // the previous positions

// prev positions
double prev1[3][4], prev2[3][4], prev3[3][4];

// does previous exist or not?
bool prev = false;

/* The spring dampers */
const int numDampers = numParticles - 1;
//sgSpringDamper *sprDampers[numDampers];
sgSpringDamper *sprD1[4]; // row 1
sgSpringDamper *sprD2[3]; // row 2
sgSpringDamper *sprD3[2]; // row 3
sgSpringDamper *sprD4; // row 4

sgSpringDamper *sprDC1; // column 1
sgSpringDamper *sprDC2[2]; // column 2
sgSpringDamper *sprDC3[3]; // column 3
sgSpringDamper *sprDC4[4]; // column 4

//sgSpringDamper *sprDampersX[rows][columns-1];
//sgSpringDamper *sprDampersY[rows-1][columns];

/* The spring values */
float _mass = 0.5;
float _stiffness = 0.9;
float _damping = 0.3;
float _restLength = -1.0;
float timeStep = 0.4;
float downward = 0.5;
int _scene = 1; // 0: lying flat. 1: vertical 

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

	cout << "Finished init";
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
       
	double      cam_trans1[3][4], cam_trans2[3][4], cam_trans3[3][4];
	if (!(object[0].visible && object[1].visible && object[2].visible))
		return 0;
	else {
        arUtilMatInv(object[0].trans, wmat1);
		// according to 2nd object
        arUtilMatMul(wmat1, object[1].trans, wmat2);

		// according to 3rd object
		arUtilMatMul(wmat1, object[2].trans, wmat3);

		arUtilMatInv(object[0].trans, cam_trans1);
		arUtilMatInv(object[1].trans, cam_trans2);
		arUtilMatInv(object[2].trans, cam_trans3);

		cout << "detected all 3" << endl;
	}

	// If detected for the first time, setup the "cloth"
	if (first) {
		// setup left first?
		sgVec3 sgv1 = {0.0, 0.0, 0.0};
		sgVec3 sgv2 = {wmat2[0][3], wmat2[1][3], wmat2[2][3]};

		// 3rd vector (between 2nd and 3rd marker)
		float x = wmat2[0][3] - wmat3[0][3];
		float y = wmat2[1][3] - wmat3[1][3];
		float z = wmat2[2][3] - wmat3[2][3];
		
		cout << "1n" << endl;

		// diagonal first
		row1[0] = new sgParticle(_mass, 0.0, 0.0, 0.0);
		row2[0] = new sgParticle(_mass, wmat2[0][3]*1/4, wmat2[1][3]*1/4, wmat2[2][3]*1/4);
		row3[0] = new sgParticle(_mass, wmat2[0][3]*2/4, wmat2[1][3]*2/4, wmat2[2][3]*2/4);
		row4[0] = new sgParticle(_mass, wmat2[0][3]*3/4, wmat2[1][3]*3/4, wmat2[2][3]*3/4);
		row5 = new sgParticle(_mass, wmat2[0][3], wmat2[1][3], wmat2[2][3]);
		
		cout << "2n" << endl;

		// right vertical
		row1[4] = new sgParticle(_mass, wmat3[0][3], wmat3[1][3], wmat3[2][3]);
		float avg23[3] = {wmat2[0][3] - wmat3[0][3], wmat2[1][3] - wmat3[1][3], wmat2[2][3] - wmat3[2][3]};
		row2[3] = new sgParticle(_mass, wmat3[0][3] + (avg23[0]/4), wmat3[1][3] + (avg23[1]/5), wmat3[2][3] + (avg23[2]/4));
		row3[2] = new sgParticle(_mass, wmat3[0][3] + (avg23[0]*2/4), wmat3[1][3] + (avg23[1]*2/4), wmat3[2][3] + (avg23[2]*2/4));
		row4[1] = new sgParticle(_mass, wmat3[0][3] + (avg23[0]*3/4), wmat3[1][3] + (avg23[1]*3/4), wmat3[2][3] + (avg23[2]*3/4));
		
		cout << "3n" << endl;

		// first row
		row1[1] = new sgParticle(_mass, wmat3[0][3]*1/4, wmat3[1][3]*1.0/4, wmat3[2][3]*1.0/4);
		row1[2] = new sgParticle(_mass, wmat3[0][3]*2.0/4, wmat3[1][3]*2.0/4, wmat3[2][3]*2.0/4);
		row1[3] = new sgParticle(_mass, wmat3[0][3]*3.0/4, wmat3[1][3]*3.0/4, wmat3[2][3]*3.0/4);

		cout << "4n" << endl;

		// second row
		float avg2[3] = {row3[0]->getPos()[0] - row1[2]->getPos()[0],row3[0]->getPos()[1] - row1[2]->getPos()[1], row3[0]->getPos()[2] - row1[2]->getPos()[2]};
		row2[1] = new sgParticle(_mass, row1[2]->getPos()[0] + avg2[0]/2.0, row1[2]->getPos()[1] + avg2[1]/2.0, row1[2]->getPos()[2] + avg2[2]/2.0);

		cout << "5n" << endl;

		// 4th column middle
		float avg24[3] = {row4[0]->getPos()[0] - row1[3]->getPos()[0], row4[0]->getPos()[1] - row1[3]->getPos()[1], row4[0]->getPos()[2] - row1[3]->getPos()[2]};
		row2[2] = new sgParticle(_mass, row1[3]->getPos()[0] + (avg24[0]/3), row1[3]->getPos()[1] + (avg24[1]/3), row1[3]->getPos()[2] + (avg24[2]/3));
		row3[1] = new sgParticle(_mass, row1[3]->getPos()[0] + (avg24[0]*2.0/3), row1[3]->getPos()[1] + (avg24[1]*2.0/3), row1[3]->getPos()[2] + (avg24[2]*2.0/3));

		cout << "setup particles " << endl;

		// the spring dampers
		for (int i = 0; i < 4; i++) {
			sprD1[i] = new sgSpringDamper(row1[i], row1[i+1], _stiffness, _damping, _restLength);
		}
		for (int i = 0; i < 3; i++) { // 2nd row
			sprD2[i] = new sgSpringDamper(row2[i], row2[i+1], _stiffness, _damping, _restLength);
		}
		for (int i = 0; i < 2; i++) { // 3rd row
			sprD3[i] = new sgSpringDamper(row3[i], row3[i+1], _stiffness, _damping, _restLength);
		}
		sprD4 = new sgSpringDamper(row4[0], row4[1], _stiffness, _damping, _restLength); // 4th row

		// now columns
		sprDC1 = new sgSpringDamper(row1[1], row2[0], _stiffness, _damping, _restLength); // 1st

		sprDC2[0] = new sgSpringDamper(row1[2], row2[1], _stiffness, _damping, _restLength); // 2nd
		sprDC2[1] = new sgSpringDamper(row2[1], row3[0], _stiffness, _damping, _restLength); // 2nd

		sprDC3[0] = new sgSpringDamper(row1[3], row2[2], _stiffness, _damping, _restLength); // 3rd
		sprDC3[1] = new sgSpringDamper(row2[2], row3[1], _stiffness, _damping, _restLength); // 3rd
		sprDC3[2] = new sgSpringDamper(row3[1], row4[0], _stiffness, _damping, _restLength); // 3rd

		sprDC4[0] = new sgSpringDamper(row1[4], row2[3], _stiffness, _damping, _restLength); // 4th
		sprDC4[1] = new sgSpringDamper(row2[3], row3[2], _stiffness, _damping, _restLength); // 4th
		sprDC4[2] = new sgSpringDamper(row3[2], row4[1], _stiffness, _damping, _restLength); // 4th
		sprDC4[3] = new sgSpringDamper(row4[0], row5, _stiffness, _damping, _restLength); // 4th

		/*particles[0] = new sgParticle(1.0, 0.0, 0.0, 0.0);
		particles[numParticles - 1] = new sgParticle(1.0, sgv2);
		particles[1] = new sgParticle(1.0, wmat2[0][3]/numParticles, wmat2[1][3]/numParticles, wmat2[2][3]/numParticles);
		particles[2] = new sgParticle(1.0, (wmat2[0][3]*2)/numParticles, (wmat2[1][3]*2)/numParticles, (wmat2[2][3]*2)/numParticles);
		particles[3] = new sgParticle(1.0, (wmat2[0][3]*3)/numParticles, (wmat2[1][3]*3)/numParticles, (wmat2[2][3]*3)/numParticles);*/

		/*prevParticles[0] = new sgParticle(1.0, 0.0, 0.0, 0.0);
		prevParticles[numParticles - 1] = new sgParticle(1.0, sgv2);
		prevParticles[1] = new sgParticle(1.0, wmat2[0][3]/numParticles, wmat2[1][3]/numParticles, wmat2[2][3]/numParticles);
		prevParticles[2] = new sgParticle(1.0, (wmat2[0][3]*2)/numParticles, (wmat2[1][3]*2)/numParticles, (wmat2[2][3]*2)/numParticles);
		prevParticles[3] = new sgParticle(1.0, (wmat2[0][3]*3)/numParticles, (wmat2[1][3]*3)/numParticles, (wmat2[2][3]*3)/numParticles);*/

		// now the spring dampers
		//for (int i = 0; i < numDampers; i++) 
			//sprDampers[i] = new sgSpringDamper(particles[i], particles[i+1], _stiffness, _damping, _restLength);

		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++) {
				prev1[i][j] = cam_trans1[i][j];
				prev2[i][j] = cam_trans2[i][j];
				prev3[i][j] = cam_trans3[i][j];
			}

		first = false;
		setup = true;

		cout << "Finished 1st setup " << endl;
	}
	else { // just update the position of the end two
		// prev position
		//prevParticles[0]->setPos(particles[0]->getPos());
		//prevParticles[numParticles - 1]->setPos(particles[numParticles-1]->getPos());

		// new position
		//particles[0] = new sgParticle(1.0, 0.0, 0.0, 0.0);
		row1[0] = new sgParticle(_mass, 0.0, 0.0, 0.0);
		//particles[numParticles-1] = new sgParticle(1.0, wmat2[0][3], wmat2[1][3], wmat2[2][3]);
		row1[4] = new sgParticle(_mass, wmat3[0][3], wmat3[1][3], wmat3[2][3]);
		row5 = new sgParticle(_mass, wmat2[0][3], wmat2[1][3], wmat2[2][3]);

		//particles[0]->setForce(cam_trans1[0][3]-prev1[0][3], cam_trans1[1][3]-prev1[1][3], cam_trans1[2][3]-prev1[2][3]);
		//particles[numParticles-1]->setForce(cam_trans2[0][3]-prev2[0][3], cam_trans2[1][3]-prev2[1][3], cam_trans2[2][3]-prev2[2][3]);
		sgVec3 sgv1 = {0.0, 0.0, 0.0};
		sgVec3 sgv2 = {wmat2[0][3], wmat2[1][3], wmat2[2][3]};

		// update the previous
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++) {
				prev1[i][j] = cam_trans1[i][j];
				prev2[i][j] = cam_trans2[i][j];
				prev3[i][j] = cam_trans3[i][j];
			}

		/*particles[0] = new sgParticle(1.0, 0.0, 0.0, 0.0);
		particles[numParticles - 1] = new sgParticle(1.0, sgv2);
		particles[1] = new sgParticle(1.0, wmat2[0][3]/numParticles, wmat2[1][3]/numParticles, wmat2[2][3]/numParticles);
		particles[2] = new sgParticle(1.0, (wmat2[0][3]*2)/numParticles, (wmat2[1][3]*2)/numParticles, (wmat2[2][3]*2)/numParticles);
		particles[3] = new sgParticle(1.0, (wmat2[0][3]*3)/numParticles, (wmat2[1][3]*3)/numParticles, (wmat2[2][3]*3)/numParticles);

		// now the spring dampers
		for (int i = 0; i < numDampers; i++) 
			sprDampers[i] = new sgSpringDamper(particles[i], particles[i+1], _stiffness, _damping, _restLength);*/

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
	
	// Draw object in top left
	glLoadIdentity();
	glBegin(GL_QUADS);
		glVertex3f(0.0, 4.0, 0.0);
		glVertex3f(4.0, 4.0, 0.0);
		glVertex3f(4.0, 0.0, 0.0);
		glVertex3f(0.0, 0.0, 0.0);
	glEnd();

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
	for (int i = 0; i < 5; i++) {
		if (i == 0 || i == 4)
			continue;
		if (_scene == 1)
			row1[i]->setForce(0.0, downward, 0.0);
		else
			row1[i]->setForce(0.0, 0.0, -0.5);

	}
	for (int i = 0; i < 4; i++) {
		if (_scene == 1)
			row2[i]->setForce(0.0, downward, 0.0);
		else
			row2[i]->setForce(0.0, 0.0, -0.5);

	}
	for (int i = 0; i < 3; i++) {
		if (_scene == 1)
			row3[i]->setForce(0.0, downward, 0.0);
		else
			row3[i]->setForce(0.0, 0.0, -0.5);
	}
	for (int i = 0; i < 2; i++) {
		if (_scene == 1)
			row4[i]->setForce(0.0, downward, 0.0);
		else
			row4[i]->setForce(0.0, 0.0, -0.5);
	}

	// update the spring dampers
	for (int i = 0; i < 4; i++) {
		sprD1[i]->update();
		sprDC4[i]->update();
	}
	for (int i = 0; i < 3; i++) {
		sprD2[i]->update();
		sprDC3[i]->update();
	}
	for (int i = 0; i < 2; i++) {
		sprD3[i]->update();
		sprDC2[i]->update();
	}
	sprD4->update();
	sprDC1->update();

	// update the particles
	for (int i = 0; i < 5; i++) {
		if (i == 0 || i == 4)
			continue;
		row1[i]->update(timeStep);
	}
	for (int i = 0; i < 4; i++) {
		row2[i]->update(timeStep);
	}
	for (int i = 0; i < 3; i++) {
		row3[i]->update(timeStep);
	}
	for (int i = 0; i < 2; i++) {
		row4[i]->update(timeStep);
	}
	row5->update(timeStep);

	// Drawing the points
	glDisable(GL_LIGHTING);
	// draw the vertices
	glPushMatrix();
	//glScalef(20, 20, 20);
	glColor3f(1.0, 0.0, 0.0);
	glPointSize(10.0);
	glBegin(GL_POINTS);
		for (int i = 0; i < 5; i++) {
			glVertex3fv(row1[i]->getPos());
		}
		for (int i = 0; i < 4; i++) {
			glVertex3fv(row2[i]->getPos());
		}
		for (int i = 0; i < 3; i++) {
			glVertex3fv(row3[i]->getPos());
		}
		for (int i = 0; i < 2; i++) {
			glVertex3fv(row4[i]->getPos());
		}
		glVertex3fv(row5->getPos());
	glEnd();
	glPopMatrix();

	// draw the lines
	glColor3f(1.0, 1.0, 1.0);
	// the rows first
	glBegin(GL_LINE_STRIP);
		for (int i = 0; i < 5; i++) {
			glVertex3fv(row1[i]->getPos());
		}
	glEnd();
	glBegin(GL_LINE_STRIP);
		for (int i = 0; i < 4; i++) {
			glVertex3fv(row2[i]->getPos());
		}
	glEnd();
	glBegin(GL_LINE_STRIP);
		for (int i = 0; i < 3; i++) {
			glVertex3fv(row3[i]->getPos());
		}
	glEnd();
	glBegin(GL_LINE_STRIP);
		for (int i = 0; i < 2; i++) {
			glVertex3fv(row4[i]->getPos());
		}
	glEnd();
	glBegin(GL_LINE_STRIP);
		glVertex3fv(row1[0]->getPos());
		glVertex3fv(row2[0]->getPos());
		glVertex3fv(row3[0]->getPos());
		glVertex3fv(row4[0]->getPos());
		glVertex3fv(row5->getPos());
	glEnd();

	/*for (int i = 0; i < numParticles; i++)
		if (i != 0 && i != numParticles-1) {
			if (_scene == 1)
				particles[i]->setForce(0.0, -0.5, 0.0);
			else
				particles[i]->setForce(0.0, 0.0, -0.5);
		}
		else {
			float xdir = particles[i]->getPos()[0] - prevParticles[i]->getPos()[0];
			float ydir = particles[i]->getPos()[1] - prevParticles[i]->getPos()[1];
			float zdir = particles[i]->getPos()[2] - prevParticles[i]->getPos()[2];

			//cout << "New force: " << xdir << " " << ydir << " " << zdir << endl;
			//particles[i]->setForce(xdir, ydir, zdir);
			if (i == 0)
				particles[i]->setForce(-4.0, 0.0, 0.0);
			else
				particles[i]->setForce(0.0, -0.5, 0.0);

		}

	//cout << "Applied forces" << endl;

	// update the spring dampers
	for (int i = 0; i < numDampers; i++)
		sprDampers[i]->update();

	//cout << "Applied damper updates" << endl;

	// update the particles
	for (int i = 0; i < numParticles; i++)
		//if (i != 0 && i != numParticles-1)
			particles[i]->update(0.9);

	//cout << "Updated particles" << endl;

	glDisable(GL_LIGHTING);
	// draw the vertices
	glPushMatrix();
	//glScalef(20, 20, 20);
	glColor3f(1.0, 0.0, 0.0);
	glPointSize(20.0);
	glBegin(GL_POINTS);
		for (int i = 0; i < numParticles; i++) {
			glVertex3fv(particles[i]->getPos());
		}
	glEnd();
	glPopMatrix();

	// draw the lines
	glDisable(GL_LIGHTING);
	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_LINE_STRIP);
		for (int i = 0; i < numParticles-1; i++) {
			glVertex3fv(particles[i]->getPos());
			//glLoadMatrixd(gl_para2);
			glVertex3fv(particles[i+1]->getPos());
		}
	glEnd();*/

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
