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
#include <AR/matrix.h>

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

// if something touched pos: [1,1]
int touch = 0;
int touch2 = 0;
int touch3 = 0;
int touch4 = 0;

int time = 0;

// the grids
double cap[4][4][3];

// does previous exist or not?
bool prev = false;

// screensize 
float screensize = 50;

// the type of touchscreen
char _type = 'r'; // default = resistive

float paddlePos[3];

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

/* find the position of the paddle card relative to the base and set the dropped blob position to this */
static void	findPos(float curPaddlePos[], double card_trans[3][4],double base_trans[3][4])
{
	int i,j;

	ARMat   *mat_a,  *mat_b, *mat_c;
    double  x, y, z;

	/*get card position relative to base pattern */
    mat_a = arMatrixAlloc( 4, 4 );
    mat_b = arMatrixAlloc( 4, 4 );
    mat_c = arMatrixAlloc( 4, 4 );
    for( j = 0; j < 3; j++ ) {
        for( i = 0; i < 4; i++ ) {
            mat_b->m[j*4+i] = base_trans[j][i];
        }
    }
    mat_b->m[3*4+0] = 0.0;
    mat_b->m[3*4+1] = 0.0;
    mat_b->m[3*4+2] = 0.0;
    mat_b->m[3*4+3] = 1.0;
    for( j = 0; j < 3; j++ ) {
        for( i = 0; i < 4; i++ ) {
            mat_a->m[j*4+i] = card_trans[j][i];
        }
    }
    mat_a->m[3*4+0] = 0.0;
    mat_a->m[3*4+1] = 0.0;
    mat_a->m[3*4+2] = 0.0;
    mat_a->m[3*4+3] = 1.0;
    arMatrixSelfInv( mat_b );
    arMatrixMul( mat_c, mat_b, mat_a );

	//x,y,z is card position relative to base pattern
    x = mat_c->m[0*4+3];
    y = mat_c->m[1*4+3];
    z = mat_c->m[2*4+3];
    
	curPaddlePos[0] = x;
	curPaddlePos[1] = y;
	curPaddlePos[2] = z;

    //printf("Position: %3.2f %3.2f %3.2f\n",x/(card_trans[0][3]-base_trans[0][3]),-y/card_trans[1][3],z/card_trans[2][3]);
    printf("Position: %3.2f %3.2f %3.2f\n",x,-y,z);
    arMatrixFree( mat_a );
    arMatrixFree( mat_b );
    arMatrixFree( mat_c );
}

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
	else if (key == 't')
		touch = !touch;
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


/* initializes the grids */
static void initGrids() {

	/* /-/-/-/-/
	   /-/-/-/-/
	   /-/-/-/-/
	   /-/-/-/-/
	   /-/-/-/-/  */
	// first column

	float first = 1.0/3; float second = 2.0/3;
	cap[0][0][0] = 0; cap[0][0][1] = 0;								   cap[0][0][0] = 0;
	cap[1][0][0] = 0; cap[1][0][1] = (screensize + screensize) * first; cap[1][0][0] = 0;
	cap[2][0][0] = 0; cap[2][0][1] = (screensize + screensize) * second; cap[2][0][0] = 0;
	cap[3][0][0] = 0; cap[3][0][1] = (screensize + screensize)       ; cap[3][0][0] = 0;

	// 2nd column
	cap[0][1][0] = (screensize + screensize) * first; cap[0][1][1] = 0;								  cap[0][1][2] = 0;
	cap[1][1][0] = (screensize + screensize) * first; cap[1][1][1] = (screensize + screensize) * first; cap[1][1][2] = 0;
	cap[2][1][0] = (screensize + screensize) * first; cap[2][1][1] = (screensize + screensize) * second; cap[2][1][2] = 0;
	cap[3][1][0] = (screensize + screensize) * first; cap[3][1][1] = (screensize + screensize)       ; cap[3][1][2] = 0;

	// 3rd column
	cap[0][2][0] = (screensize + screensize) * second; cap[0][2][1] = 0;								  cap[0][2][2] = 0;
	cap[1][2][0] = (screensize + screensize) * second; cap[1][2][1] = (screensize + screensize) * first; cap[1][2][2] = 0;
	cap[2][2][0] = (screensize + screensize) * second; cap[2][2][1] = (screensize + screensize) * second; cap[2][2][2] = 0;
	cap[3][2][0] = (screensize + screensize) * second; cap[3][2][1] = (screensize + screensize)       ; cap[3][2][2] = 0;

	// 4th column
	cap[0][3][0] = (screensize + screensize); cap[0][3][1] = 0;								   cap[0][3][2] = 0;
	cap[1][3][0] = (screensize + screensize); cap[1][3][1] = (screensize + screensize) * first; cap[1][3][2] = 0;
	cap[2][3][0] = (screensize + screensize); cap[2][3][1] = (screensize + screensize) * second; cap[2][3][2] = 0;
	cap[3][3][0] = (screensize + screensize); cap[3][3][1] = (screensize + screensize)       ; cap[3][3][2] = 0;
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

	// initialize grids
	initGrids();

	// if finger and screen are visible
	if (object[0].visible && object[2].visible) {
		arUtilMatInv(object[0].trans, wmat1);
		arUtilMatInv(object[2].trans, wmat3);
		// according to 3rd object
		arUtilMatMul(wmat1, object[2].trans, wmat3);

		//cout << "FINGER: " << wmat3[0][3] << " " <<  wmat3[1][3] << " " << wmat3[2][3] << endl;
		//cout << "FINGER in relation to screen: " << (wmat3[0][3]-wmat1[0][3])/(wmat3[0][3]-wmat1[0][3]) << " " <<  (wmat3[1][3]-wmat1[1][3])/(wmat3[0][3]-wmat1[0][3]) << " " << (wmat3[2][3]-wmat1[2][3])/(wmat3[0][3]-wmat1[0][3]) << endl;
		
		cout << "FINGER: " << object[2].trans[0][3] << " " << object[2].trans[1][3] << " " << object[2].trans[2][3] << endl;

		//findPos(paddlePos, object[2].trans, object[0].trans);// object[2].trans);

		float x1,y1,z1;
		float x2,y2,z2;
		float dist;

		x1 = object[0].trans[0][3];
		y1 = object[0].trans[1][3];
		z1 = object[0].trans[2][3];

		x2 = object[2].trans[0][3];
		y2 = object[2].trans[1][3];
		z2 = object[2].trans[2][3];

		dist = (x1-x2)*(x1-x2)+(y1-y2)*(y1-y2)+(z1-z2)*(z1-z2);

		// try finding touch point - not for resistive though
		if (y2 > 109 && z2 > 620 && !(_type == 'r')) { 
			touch = true;
			touch2 = false;
		}
		else if (y2 > 50 && z2 > 660 && !(_type == 'r')) {
			touch = false; 
			touch2 = true;
		}
			
		//cout << "DISTANCE: " << dist << " " << object[0].marker_width << endl;
	}
	else {
		touch = false;
		touch2 = false;
	}
	
	// if stylus and screen are visible
	if (object[0].visible && object[3].visible) {
		arUtilMatInv(object[0].trans, wmat1);
		arUtilMatInv(object[3].trans, wmat3);
		// according to 3rd object
		arUtilMatMul(wmat1, object[3].trans, wmat3);

		//cout << "FINGER: " << wmat3[0][3] << " " <<  wmat3[1][3] << " " << wmat3[2][3] << endl;
		//cout << "FINGER in relation to screen: " << (wmat3[0][3]-wmat1[0][3])/(wmat3[0][3]-wmat1[0][3]) << " " <<  (wmat3[1][3]-wmat1[1][3])/(wmat3[0][3]-wmat1[0][3]) << " " << (wmat3[2][3]-wmat1[2][3])/(wmat3[0][3]-wmat1[0][3]) << endl;
		
		//cout << "FINGER: " << object[3].trans[0][3] << " " << object[3].trans[1][3] << " " << object[3].trans[2][3] << endl;

		//findPos(paddlePos, object[2].trans, object[0].trans);// object[2].trans);

		float x1,y1,z1;
		float x2,y2,z2;
		float dist;

		x1 = object[0].trans[0][3];
		y1 = object[0].trans[1][3];
		z1 = object[0].trans[2][3];

		x2 = object[3].trans[0][3];
		y2 = object[3].trans[1][3];
		z2 = object[3].trans[2][3];

		// try finding touch point - not for capacitive though
		if (y2 > 109 && z2 > 620 && !(_type == 'c')) { 
			touch3 = true;
			touch4 = false;
		}
		else if (y2 > 50 && z2 > 660 && !(_type == 'c')) {
			touch3 = false; 
			touch4 = true;
		}
			
		//cout << "DISTANCE: " << dist << " " << object[0].marker_width << endl;
	}
	else {
		touch3 = false;
		touch4 = false;
	}
	

	// object 2 - chooser
	if (object[1].visible) {
		arUtilMatInv(object[1].trans, wmat2);

		//cout << "CHOOSER: " << wmat2[0][3] << " " <<  wmat2[1][3] << " " << wmat2[2][3] << endl;

		// now change the touchscreen type
		if (wmat2[2][3] >= 690) 
			_type = 'r';
		else if (wmat2[2][3] >= 630)
			_type = 'c';
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
		//glTranslatef(wmat3[0][3],wmat3[1][3],wmat3[2][3]);
		glTranslatef(-paddlePos[0],-paddlePos[1],-paddlePos[2]);
		float size = 10.0;
		/*glBegin(GL_QUADS); // the top-screen
			glVertex3f(-size, -size, 0.0);
			glVertex3f(size, -size, 0.0);
			glVertex3f(size, size, 0.0);
			glVertex3f(-size, size, 0.0);
		glEnd();*/
		//glutSolidCube(30.0);
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
	//drawAxes();

	glColor3f(0.1, 0.1, 0.1);
	glPointSize(6.0);
	glBegin(GL_QUADS); // the top-screen
		glVertex3f(-screensize, -screensize, 0.0);
		glVertex3f(screensize, -screensize, 0.0);
		glVertex3f(screensize, screensize, 0.0);
		glVertex3f(-screensize, screensize, 0.0);
	glEnd();
	// the border for the screen
	float depth = 0.2;
	glColor3f(0.5, 0.5, 0.5);
	glBegin(GL_LINE_STRIP); // the top-screen
		glVertex3f(-screensize, -screensize, depth);
		glVertex3f(screensize, -screensize, depth);
		glVertex3f(screensize, screensize, depth);
		glVertex3f(-screensize, screensize, depth);

		glVertex3f(-screensize, -screensize, depth);
	glEnd();

	if (touch) { // show on the screen where it touched
		glColor3f(1.0, 1.0, 1.0);
		glBegin(GL_POINTS);
			glVertex3f(-screensize * 0.33, -screensize * 0.33, depth + 0.5);
		glEnd();
	}
	if (touch2) { // show on the screen where it touched
		glColor3f(1.0, 1.0, 1.0);
		glBegin(GL_POINTS);
			glVertex3f(-screensize * 0.33, screensize * 0.33, depth + 0.5);
		glEnd();
	}
	if (touch3) { // show on the screen where it touched
		glColor3f(1.0, 1.0, 1.0);
		glBegin(GL_POINTS);
			glVertex3f(screensize * 0.33, -screensize * 0.33, depth + 0.5);
		glEnd();
	}
	if (touch4) { // show on the screen where it touched
		glColor3f(1.0, 1.0, 1.0);
		glBegin(GL_POINTS);
			glVertex3f(screensize * 0.33, screensize * 0.33, depth + 0.5);
		glEnd();
	}

	// Now draw the touch screen
	// first rotate and translate
	glRotatef(50, -1.0, 0.0, 0.0);
	glTranslatef(-screensize, -screensize, 0.0);
	glTranslatef(-0.0, -55 - 45, 0.0);
	glTranslatef(0.0, 0.0, -40);

	if (_type == 'r') { // resistive
		/*glBegin(GL_QUADS);
			glVertex3f(-screensize, -screensize, 0.0);
			glVertex3f(screensize, -screensize, 0.0);
			glVertex3f(screensize, screensize, 0.0);
			glVertex3f(-screensize, screensize, 0.0);
		glEnd();*/
		// draw the vertices
		glColor3f(1.0, 1.0, 0.0);
		glBegin(GL_POINTS);
			for (int i = 0; i < 4; i++)
				for (int j = 0; j < 4; j++)
					glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2]);
		glEnd();
		// now draw the blue line strips representing electricity
		glColor3f(0.0, 0.7, 0.7);
		// try blending
		glEnable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);

		glColor4f(0.0, 0.7, 0.7, 0.8);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		for (int i = 0; i < 4; i++) {
			glBegin(GL_LINE_STRIP);
				for (int j = 0; j < 4; j++)
					if (j == 2 && i == 1 && touch3) {
						glColor4f(0.0, 0.3, 0.3, 0.9);
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - 10);
					}
					else if (j == 2 && i == 2 && touch4) {
						glColor4f(0.0, 0.3, 0.3, 0.9);
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - 10);
					}
					else {
						glColor4f(0.0, 0.7, 0.7, 0.8);
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2]);
					}
			glEnd();
		}
		// the other way
		for (int j = 0; j < 4; j++) {
			glBegin(GL_LINE_STRIP);
				for (int i = 0; i < 4; i++)
					if (j == 2 && i == 1 && touch3) {
						glColor4f(0.0, 0.3, 0.3, 0.9);
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - 5);
					}
					else if (j == 2 && i == 2 && touch4) {
						glColor4f(0.0, 0.3, 0.3, 0.9);
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - 10);
					}
					else {
						glColor4f(0.0, 0.7, 0.7, 0.8);
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2]);
					}
			glEnd();
		}

		// the surface? - optional?
		glColor4f(0.5, 0.0, 0.0, 0.7);
		glBegin(GL_QUADS);
			glVertex3f(cap[0][0][0],cap[0][0][1],cap[0][0][2]);
			glVertex3f(cap[3][0][0],cap[3][0][1],cap[3][0][2]);
			glVertex3f(cap[3][3][0],cap[3][3][1],cap[3][3][2]);
			glVertex3f(cap[0][3][0],cap[0][3][1],cap[0][3][2]);
		glEnd();

		// draw the lower material
		glPushMatrix();

		// try blending
		glEnable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glTranslatef(0.0, 0.0, -10.0);
		glColor4f(0.0, 0.5, 0.5, 0.4);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glBegin(GL_QUADS);
			glVertex3f(cap[0][0][0],cap[0][0][1],cap[0][0][2]);
			glVertex3f(cap[3][0][0],cap[3][0][1],cap[3][0][2]);
			glVertex3f(cap[3][3][0],cap[3][3][1],cap[3][3][2]);
			glVertex3f(cap[0][3][0],cap[0][3][1],cap[0][3][2]);
		glEnd();
		glPopMatrix();

		// reset blending
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);

	} else if (_type == 'c') { // capacitive
		// draw the vertices
		glColor3f(1.0, 1.0, 0.0);
		glBegin(GL_POINTS);
			for (int i = 0; i < 4; i++)
				for (int j = 0; j < 4; j++)
					if (j == 1 && i == 1 && touch)
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - 3);
					else if (j == 1 && i == 2 && touch2)
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - 3);
					else
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2]);
		glEnd();

		// draw the circuits at the corner
		glColor3f(1.0, 0.0, 0.0);
		glPointSize(20.0);
		glBegin(GL_POINTS);
			glVertex3f(cap[0][0][0],cap[0][0][1],cap[0][0][2]);
			glVertex3f(cap[3][0][0],cap[3][0][1],cap[3][0][2]);
			glVertex3f(cap[3][3][0],cap[3][3][1],cap[3][3][2]);
			glVertex3f(cap[0][3][0],cap[0][3][1],cap[0][3][2]);
		glEnd();

		// now draw the blue line strips representing electricity
		glColor3f(0.0, 0.7, 0.7);
		// try blending
		glEnable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);

		glColor4f(0.0, 0.7, 0.7, 0.8);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		for (int i = 0; i < 4; i++) {
			glBegin(GL_LINE_STRIP);
				for (int j = 0; j < 4; j++) { 
					int random = (rand()%3);
					if (j == 1 && i == 1 && touch) {
						glColor3f(0.3, 0.3, 0.0);
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - 3);
					}
					if (j == 1 && i == 2 && touch2) {
						glColor3f(0.3, 0.3, 0.0);
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - 3);
					}
					else {
						int random = (rand()%3);
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] + random);
					}
				}
			glEnd();
		}
		// the other way
		for (int j = 0; j < 4; j++) {
			glBegin(GL_LINE_STRIP);
				for (int i = 0; i < 4; i++) { 
					int random = (rand()%3);
					if (j == 1 && i == 1 && touch) {
						glColor3f(0.3, 0.3, 0.0);
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - 3);
					}
					else if (j == 1 && i == 2 && touch2) {
						glColor3f(0.3, 0.3, 0.0);
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - 3);
					}
					else {
						int random = (rand()%3);
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] + random);
					}
				}
			glEnd();
		}

		// reset blending
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);

		// electron effect 
		if (touch || touch2) {
			glPushMatrix();
				glLoadMatrixd(gl_para3);
				glColor3f(1.0, 1.0, 0.0);
				glPointSize(10.0);
				glBegin(GL_POINTS);
					glVertex3f(-time,0, -10.0);
					glVertex3f(-(time-7), 0, -10);
				glEnd();
				time++;
				if (time > 30)
					time = 0;
			glPopMatrix();
		}
	}
	else { // infrared
		// draw the emitters/receivers
		glPointSize(10.0);
		glColor3f(1.0, 1.0, 0.0);
		glBegin(GL_POINTS);
			for (int i = 0; i < 1; i++) // left line
				for (int j = 0; j < 4; j++)
					glVertex3f(cap[i][j][0],cap[i][j][1] - 5,cap[i][j][2]);
			for (int j = 0; j < 1; j++) // top/bottom line
				for (int i = 0; i < 4; i++)
					glVertex3f(cap[i][j][0] - 5,cap[i][j][1],cap[i][j][2]);
		glEnd();
		// now draw the red strips representing IR 
		glColor3f(1.0, 0.0, 0.0);
		for (int i = 0; i < 4; i++) {
			glBegin(GL_LINE_STRIP);
				float z = 0.0;
				for (int j = 0; j < 4; j++) {
					if (j == 1 && i == 1 && touch) {
						// stop the strip. instead, draw a blob here
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - z);
						break;
					}
					if (j % 2 == 1) // odd
						z = 2.0;
					else
						z = 0.0;
					glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - z);
				}
			glEnd();
		}
		// the other way
		for (int j = 0; j < 4; j++) {
			glBegin(GL_LINE_STRIP);
				float z = 0.0;
				for (int i = 0; i < 4; i++) {
					if (j == 1 && i == 1 && touch) {
						// stop the strip. instead, draw a blob here
						glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - z);
						break;
					}
					if (j % 2 == 1) // odd
						z = 2.0;
					else
						z = 0.0;
					glVertex3f(cap[i][j][0],cap[i][j][1],cap[i][j][2] - z);
				}
			glEnd();
		}
		if (touch) {
			glPushMatrix();
						glPointSize(15.0);
						glColor3f(0.5, 0.0, 0.0);
						glBegin(GL_POINTS);
							glVertex3f(cap[1][1][0],cap[1][1][1],cap[1][1][2]);
						glEnd();
						glPopMatrix();
		}
		
		// draw the acrylic sheets
		// draw the lower material
		glPushMatrix();

		// try blending
		glEnable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glTranslatef(0.0, 0.0, -5.0);
		glColor4f(0.4, 0.4, 0.4, 0.4);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glBegin(GL_QUADS);
			glVertex3f(cap[0][0][0],cap[0][0][1],cap[0][0][2]);
			glVertex3f(cap[3][0][0],cap[3][0][1],cap[3][0][2]);
			glVertex3f(cap[3][3][0],cap[3][3][1],cap[3][3][2]);
			glVertex3f(cap[0][3][0],cap[0][3][1],cap[0][3][2]);
		glEnd();
		glPopMatrix();

		// draw the upper material
		glPushMatrix();
		glTranslatef(0.0, 0.0, 5.0);
		glColor4f(0.4, 0.4, 0.4, 0.4);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glBegin(GL_QUADS);
			glVertex3f(cap[0][0][0],cap[0][0][1],cap[0][0][2]);
			glVertex3f(cap[3][0][0],cap[3][0][1],cap[3][0][2]);
			glVertex3f(cap[3][3][0],cap[3][3][1],cap[3][3][2]);
			glVertex3f(cap[0][3][0],cap[0][3][1],cap[0][3][2]);
		glEnd();
		glPopMatrix();

		// reset blending
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);

		// draw the camera watching it?
		//glEnable(GL_LIGHTING);
		glTranslatef(50, 60, -70.0);
		glColor3f(0.8, 0.8, 0.8);
		glutSolidSphere(10.0, 40, 40);
		glColor3f(0.5, 0.5, 0.5);
		glPointSize(12.0);
		glBegin(GL_POINTS);
			glVertex3f(0.0, 0.0, 10.0);
		glEnd();
		/*glBegin(GL_QUADS);
		
		float camsize = 10.0;
			glVertex3f(-camsize,camsize,0);
			glVertex3f(camsize,camsize,0);
			glVertex3f(camsize,-camsize,0);
			glVertex3f(-camsize,-camsize,0);
		glEnd();*/

	} // end of infrared
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
			glBegin(GL_LINE_STRIP);
				glVertex3f(size, size, 0.0);
				glVertex3f(-size, size, 0.0);
				glVertex3f(-size, -size, 0.0);
				glVertex3f(size, -size, 0.0);
			glEnd();
		} else if (_type == 'r') { // resistive
			glBegin(GL_LINE_STRIP);
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
			glBegin(GL_LINES);
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
