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

// prev positions
double prev1[3][4], prev2[3][4], prev3[3][4];

// does previous exist or not?
bool prev = false;

// screensize 
float screensize = 55;

// the type of touchscreen
char _type = 'c'; // default = capacitive

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
static void drawTwoObjects(double gl_para1[16], double gl_para2[16], double gl_para3[16]);
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
    double  gl_para1[16], gl_para2[16], gl_para3[16]; // the parameters for the two markers
       
	double      cam_trans1[3][4], cam_trans2[3][4], cam_trans3[3][4];
	/*if (!(object[0].visible && object[1].visible && object[2].visible))
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
	}*/

	// if finger and screen are visible
	if (object[0].visible && object[2].visible) {
		arUtilMatInv(object[0].trans, wmat1);

		// according to 3rd object
		arUtilMatMul(wmat1, object[2].trans, wmat3);

		cout << wmat3[0][3] << " " <<  wmat3[1][3] << " " << wmat3[2][3] << endl;
	}

	// object 2 - chooser
	if (object[1].visible) {
		arUtilMatInv(object[1].trans, wmat2);

		cout << wmat2[0][3] << " " <<  wmat2[1][3] << " " << wmat2[2][3] << endl;

		// now change the touchscreen type
		if (wmat2[2][3] >= 690) 
			_type = 'c';
		else if (wmat2[2][3] >= 630)
			_type = 'r';
		else
			_type = 'i';
	}
	
	glClearDepth( 1.0 );
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_LEQUAL);
    glEnable(GL_LIGHTING);

    /* calculate the viewing parameters - gl_para */
	argConvGlpara(object[0].trans, gl_para1);
	argConvGlpara(object[1].trans, gl_para2);
	argConvGlpara(object[2].trans, gl_para3);

    /*for( i = 0; i < objectnum; i++ ) {
        if( object[i].visible == 0 ) continue;
        argConvGlpara(object[i].trans, gl_para);
        draw_object( object[i].id, gl_para);
    }*/

	// draw the two objects
	drawTwoObjects(gl_para1, gl_para2, gl_para3);
     
	glDisable( GL_LIGHTING );
    glDisable( GL_DEPTH_TEST );
	
    return(0);
}

// draws axes
static void drawAxes() {
	float depth = 5.0;
	float len = 40.0;
	glPushMatrix();
	glBegin(GL_LINES);
		glColor3f(1.0, 0.0, 0.0); // red = x
		glVertex3f(0.0, 0.0, depth);
		glVertex3f(len, 0.0, depth);

		glColor3f(0.0, 1.0, 0.0); // green = y
		glVertex3f(0.0, 0.0, depth);
		glVertex3f(0.0, len, depth);

		glColor3f(0.0, 0.0, 1.0); // blue = z
		glVertex3f(0.0, 0.0, 0.0);
		glVertex3f(0.0, 0.0, len);
	glEnd();
	glPopMatrix();
}

/* NEW DRAW function */
static void drawTwoObjects(double gl_para1[16], double gl_para2[16], double gl_para3[16]) {
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

	// 3rd object - finger
	if (object[2].visible) {
		glDisable(GL_LIGHTING);
		glLoadIdentity();
		glLoadMatrixd( gl_para1 ); // should be para_3
		glTranslatef(wmat3[0][3],wmat3[1][3],wmat3[2][3]);
		
		float size = 10.0;
		/*glBegin(GL_QUADS); // the top-screen
			glVertex3f(-size, -size, 0.0);
			glVertex3f(size, -size, 0.0);
			glVertex3f(size, size, 0.0);
			glVertex3f(-size, size, 0.0);
		glEnd();*/
		glutSolidCube(30.0);
	}

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

	// Draw the screen and the touch-screen
	glDisable(GL_LIGHTING);
	// draw the vertices
	glPushMatrix();

	// for debugging purposes, draw an axes
	drawAxes();

	glColor3f(0.1, 0.1, 0.1);
	glPointSize(10.0);
	glBegin(GL_QUADS); // the top-screen
		glVertex3f(-screensize, -screensize, 0.0);
		glVertex3f(screensize, -screensize, 0.0);
		glVertex3f(screensize, screensize, 0.0);
		glVertex3f(-screensize, screensize, 0.0);
	glEnd();
	// the border for the screen
	float depth = 0.2;
	glColor3f(0.0, 0.0, 0.8);
	glBegin(GL_LINE_STRIP); // the top-screen
		glVertex3f(-screensize, -screensize, depth);
		glVertex3f(screensize, -screensize, depth);
		glVertex3f(screensize, screensize, depth);
		glVertex3f(-screensize, screensize, depth);

		glVertex3f(-screensize, -screensize, depth);
	glEnd();

	// Now draw the touch screen
	// first rotate and translate
	glTranslatef(0.0, -screensize - 50.0, 45.0);
	glRotatef(50, -1.0, 0.0, 0.0);
	//glTranslatef(0.0, -50.0, 0.0);
	if (_type == 'c') { // capacitive
		glBegin(GL_QUADS); // the top-screen
			glVertex3f(-screensize, -screensize, 0.0);
			glVertex3f(screensize, -screensize, 0.0);
			glVertex3f(screensize, screensize, 0.0);
			glVertex3f(-screensize, screensize, 0.0);
		glEnd();
	} else if (_type == 'r') { // resistive
	
	}
	else { // infrared
	
	}
	glPopMatrix();


	// Second object - choosing touchscreen type
	//glEnable(GL_LIGHTING);
	if (object[1].visible) {
		glLoadIdentity();
		glLoadMatrixd( gl_para2 );

		//cout << "GL_PARA2 " << gl_para2[4] << " " <<  gl_para2[8] << " " << gl_para2[2][3] << endl;

		glTranslatef(0.0, 0.0, 30.0);
		// draw a quad, and then over it the type
		glColor3f(1.0, 1.0, 1.0);
		float bgSize = 40.0;
		glBegin(GL_QUADS);
			glVertex3f(-bgSize, -bgSize, 0.0);
			glVertex3f(bgSize, -bgSize, 0.0);
			glVertex3f(bgSize, bgSize, 0.0);
			glVertex3f(-bgSize, bgSize, 0.0);
		glEnd();

		// draw character depending on the touchscreen type
		glTranslatef(0.0, 0.0, 2.0);
		glColor3f(1.0, 0.0, 0.0);
		float size = 20.0;
		if (_type == 'c') { // capacitive
			glBegin(GL_LINE_STRIP); // the top-screen
				glVertex3f(size, size, 0.0);
				glVertex3f(-size, size, 0.0);
				glVertex3f(-size, -size, 0.0);
				glVertex3f(size, -size, 0.0);
			glEnd();
		} else if (_type == 'r') { // resistive
			glBegin(GL_LINE_STRIP); // the top-screen
				glVertex3f(-size, -size, 0.0);
				glVertex3f(-size, size, 0.0);
				glVertex3f(size, size, 0.0);
				glVertex3f(size, 0, 0.0);
				glVertex3f(-size, 0, 0.0);
			glEnd();

			glBegin(GL_LINES);
				glVertex3f(size * 0.45, 0.0, 0.0);
				glVertex3f(size, -size, 0.0);
			glEnd();
		}
		else { // infrared
			glBegin(GL_LINES); // the top-screen
				glVertex3f(-size, size, 0.0);
				glVertex3f(size, size, 0.0);

				glVertex3f(0.0, size, 0.0);
				glVertex3f(0.0, -size, 0.0);

				glVertex3f(-size, -size, 0.0);
				glVertex3f(size, -size, 0.0);
			glEnd();
		}
	}
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
