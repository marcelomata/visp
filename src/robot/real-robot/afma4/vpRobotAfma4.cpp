/****************************************************************************
 *
 * $Id$
 *
 * Copyright (C) 1998-2010 Inria. All rights reserved.
 *
 * This software was developed at:
 * IRISA/INRIA Rennes
 * Projet Lagadic
 * Campus Universitaire de Beaulieu
 * 35042 Rennes Cedex
 * http://www.irisa.fr/lagadic
 *
 * This file is part of the ViSP toolkit.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE included in the packaging of this file.
 *
 * Licensees holding valid ViSP Professional Edition licenses may
 * use this file in accordance with the ViSP Commercial License
 * Agreement provided with the Software.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Contact visp@irisa.fr if any conditions of this licensing are
 * not clear to you.
 *
 * Description:
 * Interface for the Irisa's Afma4 robot.
 *
 * Authors:
 * Fabien Spindler
 *
 *****************************************************************************/

#include <visp/vpConfig.h>

#ifdef VISP_HAVE_AFMA4

#include <signal.h>
#include <stdlib.h>

#include <visp/vpRobotException.h>
#include <visp/vpExponentialMap.h>
#include <visp/vpDebug.h>
#include <visp/vpTwistMatrix.h>
#include <visp/vpThetaUVector.h>
#include <visp/vpRobotAfma4.h>

/* ---------------------------------------------------------------------- */
/* --- STATIC ----------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

bool vpRobotAfma4::robotAlreadyCreated = false;

/*!

  Default positioning velocity in percentage of the maximum
  velocity. This value is set to 15. The member function
  setPositioningVelocity() allows to change this value.

*/
const double vpRobotAfma4::defaultPositioningVelocity = 15.0;


/* ---------------------------------------------------------------------- */
/* --- EMERGENCY STOP --------------------------------------------------- */
/* ---------------------------------------------------------------------- */

/*!

  Emergency stops the robot if the program is interrupted by a SIGINT
  (CTRL C), SIGSEGV (segmentation fault), SIGBUS (bus error), SIGKILL
  or SIGQUIT signal.

*/
void emergencyStopAfma4(int signo)
{
  std::cout << "Stop the Afma4 application by signal (" 
	    << signo << "): " << (char)7 ;
  switch(signo)
    {
    case SIGINT:
      std::cout << "SIGINT (stop by ^C) " << std::endl ; break ;
    case SIGBUS:
      std::cout <<"SIGBUS (stop due to a bus error) " << std::endl ; break ;
    case SIGSEGV:
      std::cout <<"SIGSEGV (stop due to a segmentation fault) " << std::endl ; break ;
    case SIGKILL:
      std::cout <<"SIGKILL (stop by CTRL \\) " << std::endl ; break ;
    case SIGQUIT:
      std::cout <<"SIGQUIT " << std::endl ; break ;
    default :
      std::cout << signo << std::endl ;
    }
  // std::cout << "Emergency stop called\n";
  //  PrimitiveESTOP_Afma4();
  PrimitiveSTOP_Afma4();
  std::cout << "Robot was stopped\n";

  // Free allocated ressources
  // ShutDownConnection(); // Some times cannot exit here when Ctrl-C

  fprintf(stdout, "Application ");
  fflush(stdout);
  kill(getpid(), SIGKILL);
  exit(1) ;
}


/* ---------------------------------------------------------------------- */
/* --- CONSTRUCTOR ------------------------------------------------------ */
/* ---------------------------------------------------------------------- */

/*!

  The only available constructor.

  This contructor calls init() to initialise the connection with the MotionBox
  or low level controller, power on the robot and wait 1 sec before returning
  to be sure the initialisation is done.

  It also set the robot state to vpRobot::STATE_STOP.

*/
vpRobotAfma4::vpRobotAfma4 (void)
  :
  vpAfma4 (),
  vpRobot ()
{

  /*
    #define	SIGHUP	1	// hangup
    #define	SIGINT	2	// interrupt (rubout)
    #define	SIGQUIT	3	// quit (ASCII FS)
    #define	SIGILL	4	// illegal instruction (not reset when caught)
    #define	SIGTRAP	5	// trace trap (not reset when caught)
    #define	SIGIOT	6	// IOT instruction
    #define	SIGABRT 6	// used by abort, replace SIGIOT in the future
    #define	SIGEMT	7	// EMT instruction
    #define	SIGFPE	8	// floating point exception
    #define	SIGKILL	9	// kill (cannot be caught or ignored)
    #define	SIGBUS	10	// bus error
    #define	SIGSEGV	11	// segmentation violation
    #define	SIGSYS	12	// bad argument to system call
    #define	SIGPIPE	13	// write on a pipe with no one to read it
    #define	SIGALRM	14	// alarm clock
    #define	SIGTERM	15	// software termination signal from kill
  */

  signal(SIGINT, emergencyStopAfma4);
  signal(SIGBUS, emergencyStopAfma4) ;
  signal(SIGSEGV, emergencyStopAfma4) ;
  signal(SIGKILL, emergencyStopAfma4);
  signal(SIGQUIT, emergencyStopAfma4);

  std::cout << "Open communication with MotionBlox.\n";
  try {
    this->init();
    this->setRobotState(vpRobot::STATE_STOP) ;
  }
  catch(...) {
    //    vpERROR_TRACE("Error caught") ;
    throw ;
  }
  positioningVelocity  = defaultPositioningVelocity ;

  vpRobotAfma4::robotAlreadyCreated = true;

  return ;
}


/* ------------------------------------------------------------------------ */
/* --- INITIALISATION ----------------------------------------------------- */
/* ------------------------------------------------------------------------ */

/*!

  Initialise the connection with the MotionBox or low level
  controller, power on the
  robot and wait 1 sec before returning to be sure the initialisation
  is done.

*/
void
vpRobotAfma4::init (void)
{
  int stt;
  InitTry;

  // Initialise private variables used to compute the measured velocities
  q_prev_getvel.resize(4);
  q_prev_getvel = 0;
  time_prev_getvel = 0;
  first_time_getvel = true;

  // Initialise private variables used to compute the measured displacement
  q_prev_getdis.resize(4);
  q_prev_getdis = 0;
  first_time_getdis = true;

  // Initialize the firewire connection
  Try( stt = InitializeConnection() );

  if (stt != SUCCESS) {
    vpERROR_TRACE ("Cannot open connexion with the motionblox.");
    throw vpRobotException (vpRobotException::constructionError,
  			  "Cannot open connexion with the motionblox");
  }

  // Connect to the servoboard using the servo board GUID
  Try( stt = InitializeNode_Afma4() );

  if (stt != SUCCESS) {
    vpERROR_TRACE ("Cannot open connexion with the motionblox.");
    throw vpRobotException (vpRobotException::constructionError,
  			  "Cannot open connexion with the motionblox");
  }
  Try( PrimitiveRESET_Afma4() );

  // Look if the power is on or off
  UInt32 HIPowerStatus;
  UInt32 EStopStatus;
  Try( PrimitiveSTATUS_Afma4(NULL, NULL, &EStopStatus, NULL, NULL, NULL, 
			     &HIPowerStatus));
  CAL_Wait(0.1);

  switch(EStopStatus) {
  case ESTOP_AUTO: break;
  case ESTOP_MANUAL: break;
  case ESTOP_ACTIVATED: 
    std::cout << "Emergency stop is activated! \n"
	      << "Check the emergency stop button and push the yellow button before continuing. \n"
	      << "We quit now the application. See you soon..." << std::endl;
    // Free allocated ressources
    ShutDownConnection();
    exit(0);

    break;
  default: 
    std::cout << "Sorry there is an error on the emergency chain." << std::endl;
    std::cout << "You have to call Adept for maintenance..." << std::endl;
    // Free allocated ressources
    ShutDownConnection();
    exit(0);
  }

  if (HIPowerStatus == 0) {
    fprintf(stdout, "\nPower ON the Afma4 robot in the next 10 second...\n");
    fflush(stdout);
    Try( PrimitivePOWERON_Afma4() );
  }
  fprintf(stdout, "Afma4 power is ON. We continue...\n");
  fflush(stdout);

  // get real joint min/max from the MotionBlox
  Try( PrimitiveJOINT_MINMAX_Afma4(_joint_min, _joint_max) );
//   for (int i=0; i < njoint; i++) {
//     printf("axis %d: joint min %lf, max %lf\n", i, _joint_min[i], _joint_max[i]);
//   }

  // If an error occur in the low level controller, goto here
  // CatchPrint();
  Catch();

  // Test if an error occurs
  if (TryStt == -20001)
    printf("No connection detected. Check if the robot is powered on \n"
	   "and if the firewire link exist between the MotionBlox and this computer.\n");
  else if (TryStt == -675) 
    printf(" Timeout enabling power...\n");

  if (TryStt < 0) {
    // Power off the robot
    PrimitivePOWEROFF_Afma4();
    // Free allocated ressources
    ShutDownConnection();

    std::cout << "Cannot open connexion with the motionblox..." << std::endl;
    throw vpRobotException (vpRobotException::constructionError,
  			  "Cannot open connexion with the motionblox");
  }
  return ;
}


/* ------------------------------------------------------------------------ */
/* --- DESTRUCTOR --------------------------------------------------------- */
/* ------------------------------------------------------------------------ */

/*!

  Destructor.

  Free allocated ressources.
*/
vpRobotAfma4::~vpRobotAfma4 (void)
{
  InitTry;

  setRobotState(vpRobot::STATE_STOP) ;

  // Look if the power is on or off
  UInt32 HIPowerStatus;
  Try( PrimitiveSTATUS_Afma4(NULL, NULL, NULL, NULL, NULL, NULL,
			     &HIPowerStatus));
  CAL_Wait(0.1);

//   if (HIPowerStatus == 1) {
//     fprintf(stdout, "Power OFF the robot\n");
//     fflush(stdout);

//     Try( PrimitivePOWEROFF_Afma4() );
//   }

  // Free allocated ressources
  ShutDownConnection();

  vpRobotAfma4::robotAlreadyCreated = false;

  CatchPrint();
  return;
}





/*!

Change the robot state.

\param newState : New requested robot state.
*/
vpRobot::vpRobotStateType
vpRobotAfma4::setRobotState(vpRobot::vpRobotStateType newState)
{
  InitTry;

  switch (newState) {
  case vpRobot::STATE_STOP: {
    if (vpRobot::STATE_STOP != getRobotState ()) {
      Try( PrimitiveSTOP_Afma4() );
    }
    break;
  }
  case vpRobot::STATE_POSITION_CONTROL: {
    if (vpRobot::STATE_VELOCITY_CONTROL  == getRobotState ()) {
      std::cout << "Change the control mode from velocity to position control.\n";
      Try( PrimitiveSTOP_Afma4() );
    }
    else {
      //std::cout << "Change the control mode from stop to position control.\n";
    }
    break;
  }
  case vpRobot::STATE_VELOCITY_CONTROL: {
    if (vpRobot::STATE_VELOCITY_CONTROL != getRobotState ()) {
      std::cout << "Change the control mode from stop to velocity control.\n";
    }
    break;
  }
  default:
    break ;
  }

  CatchPrint();

  return vpRobot::setRobotState (newState);
}

/* ------------------------------------------------------------------------ */
/* --- STOP --------------------------------------------------------------- */
/* ------------------------------------------------------------------------ */

/*!

  Stop the robot and set the robot state to vpRobot::STATE_STOP.

  \exception vpRobotException::lowLevelError : If the low level
  controller returns an error during robot stopping.
*/
void
vpRobotAfma4::stopMotion(void)
{
  InitTry;
  Try( PrimitiveSTOP_Afma4() );
  setRobotState (vpRobot::STATE_STOP);

  CatchPrint();
  if (TryStt < 0) {
    vpERROR_TRACE ("Cannot stop robot motion");
    throw vpRobotException (vpRobotException::lowLevelError,
			      "Cannot stop robot motion.");
  }
}


/*!

  Power on the robot.

  \exception vpRobotException::lowLevelError : If the low level
  controller returns an error during robot power on.

  \sa powerOff(), getPowerState()
*/
void
vpRobotAfma4::powerOn(void)
{
  InitTry;

  // Look if the power is on or off
  UInt32 HIPowerStatus;
  Try( PrimitiveSTATUS_Afma4(NULL, NULL, NULL, NULL, NULL, NULL, 
			     &HIPowerStatus));
  CAL_Wait(0.1);

  if (HIPowerStatus == 0) {
    fprintf(stdout, "Power ON the Afma4 robot\n");
    fflush(stdout);

    Try( PrimitivePOWERON_Afma4() );
  }

  CatchPrint();
  if (TryStt < 0) {
    vpERROR_TRACE ("Cannot power on the robot");
    throw vpRobotException (vpRobotException::lowLevelError,
			      "Cannot power off the robot.");
  }
}

/*!

  Power off the robot.

  \exception vpRobotException::lowLevelError : If the low level
  controller returns an error during robot stopping.

  \sa powerOn(), getPowerState()
*/
void
vpRobotAfma4::powerOff(void)
{
  InitTry;

  // Look if the power is on or off
  UInt32 HIPowerStatus;
  Try( PrimitiveSTATUS_Afma4(NULL, NULL, NULL, NULL, NULL, NULL, 
			     &HIPowerStatus));
  CAL_Wait(0.1);

  if (HIPowerStatus == 1) {
    fprintf(stdout, "Power OFF the Afma4 robot\n");
    fflush(stdout);

    Try( PrimitivePOWEROFF_Afma4() );
  }

  CatchPrint();
  if (TryStt < 0) {
    vpERROR_TRACE ("Cannot power off the robot");
    throw vpRobotException (vpRobotException::lowLevelError,
			      "Cannot power off the robot.");
  }
}

/*!

  Get the robot power state indication if power is on or off.

  \return true if power is on. false if power is off

  \exception vpRobotException::lowLevelError : If the low level
  controller returns an error.

  \sa powerOn(), powerOff()
*/
bool
vpRobotAfma4::getPowerState(void)
{
  InitTry;
  bool status = false;
  // Look if the power is on or off
  UInt32 HIPowerStatus;
  Try( PrimitiveSTATUS_Afma4(NULL, NULL, NULL, NULL, NULL, NULL, 
			     &HIPowerStatus));
  CAL_Wait(0.1);

  if (HIPowerStatus == 1) {
    status = true;
  }

  CatchPrint();
  if (TryStt < 0) {
    vpERROR_TRACE ("Cannot get the power status");
    throw vpRobotException (vpRobotException::lowLevelError,
			      "Cannot get the power status.");
  }
  return status;
}

/*!

  Get the twist transformation from camera frame to end-effector
  frame.  This transformation allows to compute a velocity expressed
  in the end-effector frame into the camera frame.

  \param cVe : Twist transformation.

*/
void
vpRobotAfma4::get_cVe(vpTwistMatrix &cVe)
{
  vpHomogeneousMatrix cMe ;
  vpAfma4::get_cMe(cMe) ;

  cVe.buildFrom(cMe) ;
}

/*!

  Get the twist transformation from camera frame to the reference
  frame.  This transformation allows to compute a velocity expressed
  in the reference frame into the camera frame.

  \param cVf : Twist transformation.

*/
void
vpRobotAfma4::get_cVf(vpTwistMatrix &cVf)
{
  double position[this->njoint];

  InitTry;
  Try( PrimitiveACQ_POS_Afma4(position) );
  CatchPrint();

  vpColVector q(this->njoint);
  for (int i=0; i < njoint; i++)
    q[i] = position[i];

  try {
    vpAfma4::get_cVf(q, cVf) ;
  }
  catch(...) {
    vpERROR_TRACE("catch exception ") ;
    throw ;
  }}

/*!

  Get the geometric transformation between the camera frame and the
  end-effector frame. This transformation is constant and correspond
  to the extrinsic camera parameters estimated by calibration.

  \param cMe : Transformation between the camera frame and the
  end-effector frame.

*/
void
vpRobotAfma4::get_cMe(vpHomogeneousMatrix &cMe)
{
  vpAfma4::get_cMe(cMe) ;
}

/*!

  Get the robot jacobian expressed in the end-effector frame. To have
  acces to the analytic form of this jacobian see vpAfma4::get_eJe().

  To compute eJe, we communicate with the low level controller to get
  the articular joint position of the robot.

  \param eJe : Robot jacobian expressed in the end-effector frame.

  \sa vpAfma4::get_eJe()
*/
void
vpRobotAfma4::get_eJe(vpMatrix &eJe)
{

  double position[this->njoint];

  InitTry;
  Try( PrimitiveACQ_POS_Afma4(position) );
  CatchPrint();

  vpColVector q(this->njoint);
  for (int i=0; i < njoint; i++)
    q[i] = position[i];

  try {
    vpAfma4::get_eJe(q, eJe) ;
  }
  catch(...) {
    vpERROR_TRACE("catch exception ") ;
    throw ;
  }
}

/*!

  Get the robot jacobian expressed in the robot reference frame also
  called fix frame. To have acces to the analytic form of this
  jacobian see vpAfma4::get_fJe().

  To compute fJe, we communicate with the low level controller to get
  the articular joint position of the robot.

  \param fJe : Robot jacobian expressed in the reference frame.

  \sa vpAfma4::get_fJe()
*/

  void
  vpRobotAfma4::get_fJe(vpMatrix &fJe)
{

  double position[6];

  InitTry;
  Try( PrimitiveACQ_POS_Afma4(position) );
  CatchPrint();

  vpColVector q(6);
  for (int i=0; i < njoint; i++)
    q[i] = position[i];

  try {
    vpAfma4::get_fJe(q, fJe) ;
  }
  catch(...) {
    vpERROR_TRACE("Error caught");
    throw ;
  }
}
/*!

  Set the maximal velocity percentage to use for a positionning task.

  The default positioning velocity is defined by
  vpRobotAfma4::defaultPositioningVelocity. This method allows to
  change this default positioning velocity

  \param velocity : Percentage of the maximal velocity. Values should
  be in ]0:100].

  \code
  vpColVector q[4]);
  q = 0; // position in meter and rad

  vpRobotAfma4 robot;

  robot.setRobotState(vpRobot::STATE_POSITION_CONTROL);

  // Set the max velocity to 20%
  robot.setPositioningVelocity(20);

  // Moves the robot to the joint position [0,0,0,0]
  robot.setPosition(vpRobot::ARTICULAR_FRAME, q);
  \endcode

  \sa getPositioningVelocity()
*/
void
vpRobotAfma4::setPositioningVelocity (const double velocity)
{
  positioningVelocity = velocity;
}

/*!
  Get the maximal velocity percentage used for a positionning task.

  \sa setPositioningVelocity()
*/
double
vpRobotAfma4::getPositioningVelocity (void)
{
  return positioningVelocity;
}


/*!

  Move to an absolute position with a given percent of max velocity.
  The percent of max velocity is to set with setPositioningVelocity().

  \warning The position to reach can only be specified in joint space
  coordinates.

  This method owerloads setPosition(const
  vpRobot::vpControlFrameType, const vpColVector &).

  \warning This method is blocking. It returns only when the position
  is reached by the robot.

  \param position : The joint positions to reach. position[0]
  corresponds to the first rotation of the turret around the vertical
  axis (joint 1 with value \f$q_1\f$), while position[1] corresponds
  to the vertical translation (joint 2 with value \f$q_2\f$), while
  position[2] and position[3] correspond to the pan and tilt of the
  camera (respectively joint 4 and 5 with values \f$q_4\f$ and
  \f$q_5\f$). Rotations position[0], position[2] and position[3] are
  expressed in radians. The translation q[1] is expressed in meters.

  \param frame : Frame in which the position is expressed.

  \exception vpRobotException::lowLevelError : vpRobot::MIXT_FRAME not
  implemented.

  \exception vpRobotException::positionOutOfRangeError : The requested
  position is out of range.

  \code
  // Set positions in the joint space
  vpColVector q[4];
  double q[0] = M_PI/8; // Joint 1, in radian
  double q[1] = 0.2;    // Joint 2, in meter
  double q[2] = M_PI/4; // Joint 4, in radian
  double q[3] = M_PI/8; // Joint 5, in radian

  vpRobotAfma4 robot;

  robot.setRobotState(vpRobot::STATE_POSITION_CONTROL);

  // Set the max velocity to 20%
  robot.setPositioningVelocity(20);

  // Moves the robot in the camera frame
  robot.setPosition(vpRobot::ARTICULAR_FRAME, q);
  \endcode

  \exception vpRobotException::lowLevelError : If the requested frame
  (vpRobot::REFERENCE_FRAME, vpRobot::CAMERA_FRAME, or vpRobot::MIXT_FRAME) are
  requested since they are not implemented.

  \exception vpRobotException::positionOutOfRangeError : The requested
  position is out of range.

  To catch the exception if the position is out of range, modify the code like:

  \code
  try {
    robot.setPosition(vpRobot::CAMERA_FRAME, q);
  }
  catch (vpRobotException e) {
    if (e.getCode() == vpRobotException::positionOutOfRangeError) {
    std::cout << "The position is out of range" << std::endl;
  }
  \endcode

*/

void
vpRobotAfma4::setPosition (const vpRobot::vpControlFrameType frame,
			   const vpColVector & position )
{

  if (vpRobot::STATE_POSITION_CONTROL != getRobotState ()) {
    vpERROR_TRACE ("Robot was not in position-based control\n"
		   "Modification of the robot state");
    setRobotState(vpRobot::STATE_POSITION_CONTROL) ;
  }
  
  int error = 0;
  
  switch(frame) {
  case vpRobot::REFERENCE_FRAME:
    vpERROR_TRACE ("Positionning error. Reference frame not implemented");
    throw vpRobotException (vpRobotException::lowLevelError,
			    "Positionning error: "
			    "Reference frame not implemented.");
    break ;
  case vpRobot::CAMERA_FRAME : 
    vpERROR_TRACE ("Positionning error. Camera frame not implemented");
    throw vpRobotException (vpRobotException::lowLevelError,
			    "Positionning error: "
			    "Camera frame not implemented.");
    break ;
  case vpRobot::MIXT_FRAME:
    vpERROR_TRACE ("Positionning error. Mixt frame not implemented");
    throw vpRobotException (vpRobotException::lowLevelError,
			    "Positionning error: "
			    "Mixt frame not implemented.");
    break ;
    
  case vpRobot::ARTICULAR_FRAME: {
    break ;
    
  }
  }
  if (position.getRows() != this->njoint) {
    vpERROR_TRACE ("Positionning error: bad vector dimension.");
    throw vpRobotException (vpRobotException::positionOutOfRangeError,
 			    "Positionning error: bad vector dimension."); 
  }

  InitTry;

  Try( PrimitiveMOVE_Afma4(position.data, positioningVelocity) );
  Try( WaitState_Afma4(ETAT_ATTENTE_AFMA4, 1000) );

  CatchPrint();
  if (TryStt == InvalidPosition || TryStt == -1023)
    std::cout << " : Position out of range.\n";
  else if (TryStt < 0)
    std::cout << " : Unknown error (see Fabien).\n";
  else if (error == -1)
    std::cout << "Position out of range.\n";
  
  if (TryStt < 0 || error < 0) {
    vpERROR_TRACE ("Positionning error.");
    throw vpRobotException (vpRobotException::positionOutOfRangeError,
			    "Position out of range.");
  }
  
  return ;
}


/*!
  Move to an absolute position with a given percent of max velocity.
  The percent of max velocity is to set with setPositioningVelocity().

  \warning The position to reach can only be specified in joint space
  coordinates.

  This method owerloads setPosition(const
  vpRobot::vpControlFrameType, const vpColVector &).

  \warning This method is blocking. It returns only when the position
  is reached by the robot.

  \param q1, q2, q4, q5 : The four joint positions to reach. q1 corresponds to
  the first rotation (joint 1 with value \f$q_1\f$) of the turret
  around the vertical axis, while q2 corresponds to the vertical
  translation (joint 2 with value \f$q_2\f$), while q4 and q5
  correspond to the pan and tilt of the camera (respectively joint 4
  and 5 with values \f$q_4\f$ and \f$q_5\f$). Rotations q1, q4 and
  q5 are expressed in radians. The translation q2 is expressed in
  meters.

  \param frame : Frame in which the position is expressed.

  \exception vpRobotException::lowLevelError : vpRobot::MIXT_FRAME not
  implemented.

  \exception vpRobotException::positionOutOfRangeError : The requested
  position is out of range.

  \code
  // Set positions in the camera frame
  double q1 = M_PI/8; // Joint 1, in radian
  double q2 = 0.2;    // Joint 2, in meter
  double q4 = M_PI/4; // Joint 4, in radian
  double q5 = M_PI/8; // Joint 5, in radian

  vpRobotAfma4 robot;

  robot.setRobotState(vpRobot::STATE_POSITION_CONTROL);

  // Set the max velocity to 20%
  robot.setPositioningVelocity(20);

  // Moves the robot in the camera frame
  robot.setPosition(vpRobot::ARTICULAR_FRAME, q1, q2, q4, q5);
  \endcode

  \sa setPosition()
*/
void vpRobotAfma4::setPosition (const vpRobot::vpControlFrameType frame,
				const double q1,
				const double q2,
				const double q4,
				const double q5)
{
  try {
    vpColVector position(this->njoint) ;
    position[0] = q1 ;
    position[1] = q2 ;
    position[2] = q4 ;
    position[3] = q5 ;

    setPosition(frame, position) ;
  }
  catch(...)  {
    vpERROR_TRACE("Error caught");
    throw ;
  }
}




/*!

  Move to an absolute joint position with a given percent of max
  velocity. The robot state is set to position control.  The percent
  of max velocity is to set with setPositioningVelocity(). The
  position to reach is defined in the position file.

  \param filename : Name of the position file to read. The
  readPosFile() documentation shows a typical content of such a
  position file.

  This method has the same behavior than the sample code given below;
  \code
  vpColVector q;

  robot.readPosFile("MyPositionFilename.pos", q);
  robot.setRobotState(vpRobot::STATE_POSITION_CONTROL)
  robot.setPosition(vpRobot::ARTICULAR_FRAME, q);
  \endcode

  \exception vpRobotException::lowLevelError : vpRobot::MIXT_FRAME not
  implemented.

  \exception vpRobotException::positionOutOfRangeError : The requested
  position is out of range.

  \sa setPositioningVelocity()

*/
void vpRobotAfma4::setPosition(const char *filename)
{
  vpColVector q;
  bool ret;

  ret = this->readPosFile(filename, q);

  if (ret == false) {
    vpERROR_TRACE ("Bad position in \"%s\"", filename);
    throw vpRobotException (vpRobotException::lowLevelError,
			    "Bad position in filename.");
  }
  this->setRobotState(vpRobot::STATE_POSITION_CONTROL);
  this->setPosition(vpRobot::ARTICULAR_FRAME, q);
}

/*!

  Get the current position of the robot.

  \param frame : Control frame type in which to get the position, either :
  - in the camera cartesien frame,
  - joint (articular) coordinates of each axes
  - in a reference or fixed cartesien frame attached to the robot base
  - in a mixt cartesien frame (translation in reference
  frame, and rotation in camera frame)

  \param position : Measured position of the robot:
  - in camera cartesien frame, a 6 dimension vector, set to 0.

  - in articular, a 4 dimension vector corresponding to the joint position of
  each dof. position[0]
  corresponds to the first rotation of the turret around the vertical
  axis (joint 1 with value \f$q_1\f$), while position[1] corresponds
  to the vertical translation (joint 2 with value \f$q_2\f$), while
  position[2] and position[3] correspond to the pan and tilt of the
  camera (respectively joint 4 and 5 with values \f$q_4\f$ and
  \f$q_5\f$). Rotations position[0], position[2] and position[3] are
  expressed in radians. The translation q[1] is expressed in meters.

  - in reference frame, a 6 dimension vector, the first 3 values correspond to
  the translation tx, ty, tz in meters (like a vpTranslationVector), and the
  last 3 values to the rx, ry, rz rotation (like a vpRxyzVector). The code
  below show how to convert this position into a vpHomogenousMatrix:

  \code
  vpRobotAfma4 robot;
  vpColVector r;
  robot.getPosition(vpRobot::REFERENCE_FRAME, r);

  vpTranslationVector ftc; // reference frame to camera frame translations
  vpRxyzVector frc; // reference frame to camera frame rotations

  // Update the transformation between reference frame and camera frame
  for (int i=0; i < 3; i++) {
    ftc[i] = position[i];   // tx, ty, tz
    frc[i] = position[i+3]; // ry, ry, rz
  }

  // Create a rotation matrix from the Rxyz rotation angles
  vpRotationMatrix fRc(frc); // reference frame to camera frame rotation matrix

  // Create the camera to fix frame pose in terms of a homogenous matrix
  vpHomogeneousMatrix fMc(fRc, ftc);
  \endcode

  \exception vpRobotException::lowLevelError : If the position cannot
  be get from the low level controller.

  \sa setPosition(const vpRobot::vpControlFrameType frame, const
  vpColVector & r)

*/
void
vpRobotAfma4::getPosition (const vpRobot::vpControlFrameType frame,
			   vpColVector & position)
{

  InitTry;

  position.resize (this->njoint);

  switch (frame) {
  case vpRobot::CAMERA_FRAME : {
    position = 0;
    return;
  }
  case vpRobot::ARTICULAR_FRAME : {
    double _q[njoint];
    Try( PrimitiveACQ_POS_Afma4(_q) );
    for (int i=0; i < this->njoint; i ++) {
      position[i] = _q[i];
    }

    return;
  }
  case vpRobot::REFERENCE_FRAME : {
    double _q[njoint];
    Try( PrimitiveACQ_POS_Afma4(_q) );

    vpColVector q(this->njoint);
    for (int i=0; i < this->njoint; i++)
      q[i] = _q[i];

    // Compute fMc
    vpHomogeneousMatrix fMc;
    vpAfma4::get_fMc(q, fMc);

    // From fMc extract the pose
    vpRotationMatrix fRc;
    fMc.extract(fRc);
    vpRxyzVector rxyz;
    rxyz.buildFrom(fRc);

    for (int i=0; i < 3; i++) {
      position[i] = fMc[i][3]; // translation x,y,z
      position[i+3] = rxyz[i]; // Euler rotation x,y,z
    }
    break ;
  }
  case vpRobot::MIXT_FRAME: {
    vpERROR_TRACE ("Cannot get position in mixt frame: not implemented");
    throw vpRobotException (vpRobotException::lowLevelError,
			    "Cannot get position in mixt frame: "
			    "not implemented");
    break ;
  }
  }

  CatchPrint();
  if (TryStt < 0) {
    vpERROR_TRACE ("Cannot get position.");
    throw vpRobotException (vpRobotException::lowLevelError,
			    "Cannot get position.");
  }

  return;
}

/*!
  Apply a velocity to the robot.

  \param frame : Control frame in which the velocity is expressed. Velocities
  could be expressed in the joint space or in the camera frame. The reference
  frame and mixt frame are not implemented.

  \param vel : Velocity vector. Translation velocities are expressed in m/s
  while rotation velocities in rad/s. The size of this vector may be 4 (the
  number of dof) if frame is set to vpRobot::ARTICULAR_FRAME. The size of this
  vector is 2 when the control is in the camera frame (frame is than set to
  vpRobot::CAMERA_FRAME).

  - In articular, \f$ vel = [\dot{q}_1, \dot{q}_2, \dot{q}_3, \dot{q}_4]^t \f$ correspond to joint velocities.

  - In camera frame, \f$ vel = [^{c} t_x, ^{c} t_y, ^{c} t_z,^{c}
  \omega_x, ^{c} \omega_y]^t, ^{c} \omega_z]^t\f$ is expressed in the
  camera frame. Since only four dof are available, in camera frame we
  control control only the camera pan and tilt.

  \exception vpRobotException::wrongStateError : If a the robot is not
  configured to handle a velocity. The robot can handle a velocity only if the
  velocity control mode is set. For that, call setRobotState(
  vpRobot::STATE_VELOCITY_CONTROL) before setVelocity().

  \warning Velocities could be saturated if one of them exceed the
  maximal autorized speed (see vpRobot::maxTranslationVelocity and
  vpRobot::maxRotationVelocity). To change these values use
  setMaxTranslationVelocity() and setMaxRotationVelocity().

  \code
  // Set joint velocities
  vpColVector q_dot(4);
  q_dot[0] = M_PI/8; // Joint 1 velocity, in rad/s
  q_dot[1] = 0.2;    // Joint 2 velocity, in meter/s
  q_dot[2] = M_PI/4; // Joint 4 velocity, in rad/s
  q_dot[3] = M_PI/8; // Joint 5 velocity, in rad/s

  vpRobotAfma4 robot;

  robot.setRobotState(vpRobot::STATE_VELOCITY_CONTROL);

  // Moves the joint in velocity
  robot.setVelocity(vpRobot::ARTICULAR_FRAME, q_dot);
  \endcode
*/
void
vpRobotAfma4::setVelocity (const vpRobot::vpControlFrameType frame,
			   const vpColVector & vel)
{

  if (vpRobot::STATE_VELOCITY_CONTROL != getRobotState ())
    {
      vpERROR_TRACE ("Cannot send a velocity to the robot "
		     "use setRobotState(vpRobot::STATE_VELOCITY_CONTROL) first) ");
      throw vpRobotException (vpRobotException::wrongStateError,
			      "Cannot send a velocity to the robot "
			      "use setRobotState(vpRobot::STATE_VELOCITY_CONTROL) first) ");
    }

  // Check the dimension of the velocity vector to see if it is
  // compatible with the requested frame
  switch(frame) {
  case vpRobot::CAMERA_FRAME : {
    //if (vel.getRows() != 2) {
      if (vel.getRows() != 6) {
	vpERROR_TRACE ("Bad dimension of the velocity vector in camera frame");
	throw vpRobotException (vpRobotException::wrongStateError,
				"Bad dimension of the velocity vector "
				"in camera frame");
    }
    break ;
  }
  case vpRobot::ARTICULAR_FRAME : {
    if (vel.getRows() != this->njoint) {
	vpERROR_TRACE ("Bad dimension of the articular velocity vector");
	throw vpRobotException (vpRobotException::wrongStateError,
				"Bad dimension of the articular "
				"velocity vector ");
    }
    break ;
  }
  case vpRobot::REFERENCE_FRAME : {
    vpERROR_TRACE ("Cannot send a velocity to the robot "
		   "in the reference frame: "
		   "functionality not implemented");
    throw vpRobotException (vpRobotException::wrongStateError,
			    "Cannot send a velocity to the robot "
			    "in the reference frame:"
			    "functionality not implemented");
    break ;
  }
  case vpRobot::MIXT_FRAME : {
    vpERROR_TRACE ("Cannot send a velocity to the robot "
		   "in the mixt frame: "
		   "functionality not implemented");
    throw vpRobotException (vpRobotException::wrongStateError,
			    "Cannot send a velocity to the robot "
			    "in the mixt frame:"
			    "functionality not implemented");
    break ;
  }
  default: {
      vpERROR_TRACE ("Error in spec of vpRobot. "
		     "Case not taken in account.");
      throw vpRobotException (vpRobotException::wrongStateError,
			      "Cannot send a velocity to the robot ");
  }
  }


  //
  // Velocities saturation with normalization
  //
  vpColVector joint_vel(this->njoint);
  bool norm = false; // Flag to indicate when velocities need to be nomalized

  // Case of the camera frame where we control only 2 dof
  if (frame == vpRobot::CAMERA_FRAME) {
    vpColVector velocity( vel.getRows() );
    double max = getMaxRotationVelocity();
    // Determine if we need to saturate the rotation velocities
    for (int i = 0 ; i < 2; ++ i) { // rx and ry of the camera
      if (fabs (vel[i]) > max) {
	norm = true;
	max = fabs (vel[i]);
	vpERROR_TRACE ("Excess velocity %g: ROTATION "
		       "(axe nr.%d).", vel[i], i);
      }
    }
    // Rotations velocities normalisation
    if (norm == true) {
      max = getMaxRotationVelocity() / max;
      velocity = vel * max;
    }
    else {
      velocity = vel;
    }

#if 0 // ok 
    vpMatrix eJe(4,6);
    eJe = 0;
    eJe[2][4] = -1;
    eJe[3][3] =  1;

    joint_vel = eJe * velocity; // Compute the articular velocity
#endif
    vpColVector q;
    getPosition(vpRobot::ARTICULAR_FRAME, q);
    vpMatrix fJe_inverse;
    get_fJe_inverse(q, fJe_inverse);
    vpHomogeneousMatrix fMe;
    get_fMe(q, fMe);
    vpTranslationVector t;
    t=0;
    vpRotationMatrix fRe;
    fMe.extract(fRe);
    vpTwistMatrix fVe(t, fRe);
    // compute the inverse jacobian in the end-effector frame
    vpMatrix eJe_inverse = fJe_inverse * fVe;

    // Transform the velocities from camera to end-effector frame
    vpTwistMatrix eVc;
    eVc.buildFrom(this->_eMc);
    joint_vel = eJe_inverse * eVc * velocity;

//     printf("Vitesse art: %f %f %f %f\n", joint_vel[0], joint_vel[1], 
// 	   joint_vel[2], joint_vel[3]);
  }

  // Case of the joint control where we control all the joints
  else if (frame == vpRobot::ARTICULAR_FRAME) {

    // Manage the rotations: joint 0,2,3
    double max = getMaxRotationVelocity();
    // Determine if we need to saturate
    if (fabs (vel[0]) > max) {
      norm = true;
      max = fabs (vel[0]);
      vpERROR_TRACE ("Excess velocity %g: ROTATION "
		     "(axe nr.%d).", vel[0], 0);
    }

    for (int i = 2 ; i < 4; ++ i) { // joint 2 and 3
      if (fabs (vel[i]) > max) {
	norm = true;
	max = fabs (vel[i]);
	vpERROR_TRACE ("Excess velocity %g: ROTATION "
		       "(axe nr.%d).", vel[i], i);
      }
    }
    // Rotations velocities normalisation
    if (norm == true) {
      max = getMaxRotationVelocity() / max;
      joint_vel = vel * max;
     
    }
    else {
      joint_vel = vel;
    }

    // Manage the translation: joint 1
    max = getMaxTranslationVelocity();
    if (fabs (vel[1]) > max) {
      norm = true;
      max = fabs (vel[1]);
      vpERROR_TRACE ("Excess velocity %g: TRANSLATION "
		     "(axe nr.%d).", vel[1], 1);
    }

    // Translations velocities normalisation
    if (norm == true)  {
      max = getMaxTranslationVelocity() * max;
      joint_vel = vel * max;
    }
  }

  InitTry;

  // Send a joint velocity to the low level controller
  Try( PrimitiveMOVESPEED_Afma4(joint_vel.data) );

  Catch();
  if (TryStt < 0) {
    if (TryStt == VelStopOnJoint) {
      UInt32 axisInJoint[njoint];
      PrimitiveSTATUS_Afma4(NULL, NULL, NULL, NULL, NULL, axisInJoint, NULL);
      for (int i=0; i < njoint; i ++) {
	if (axisInJoint[i])
	  std::cout << "\nWarning: Velocity control stopped: axis "
		    << i+1 << " on joint limit!" <<std::endl;
      }
    }
    else {
      printf("\n%s(%d): Error %d", __FUNCTION__, TryLine, TryStt);
      if (TryString != NULL) {
	// The statement is in TryString, but we need to check the validity
	printf(" Error sentence %s\n", TryString); // Print the TryString
      }
      else {
	printf("\n");
      }
    }
  }

  return;
}

/* ------------------------------------------------------------------------ */
/* --- GET ---------------------------------------------------------------- */
/* ------------------------------------------------------------------------ */


/*!

  Get the robot velocities.

  \param frame : Frame in wich velocities are mesured.

  \param velocity : Measured velocities. Translations are expressed in m/s
  and rotations in rad/s.

  \warning In camera frame, reference frame and mixt frame, the representation
  of the rotation is ThetaU. In that cases, \f$velocity = [\dot x, \dot y, \dot
  z, \dot {\theta U}_x, \dot {\theta U}_y, \dot {\theta U}_z]\f$.

  \warning The first time this method is called, \e velocity is set to 0. The
  first call is used to intialise the velocity computation for the next call.

  \code
  // Set joint velocities
  vpColVector q_dot(4);
  q_dot[0] = M_PI/8;  // Joint 1 velocity, in rad/s
  q_dot[1] = 0.2;     // Joint 2 velocity, in meter/s
  q_dot[2] = M_PI/4;  // Joint 4 velocity, in rad/s
  q_dot[3] = M_PI/16; // Joint 5 velocity, in rad/s

  vpRobotAfma4 robot;

  robot.setRobotState(vpRobot::STATE_VELOCITY_CONTROL);

  // Moves the joint in velocity
  robot.setVelocity(vpRobot::ARTICULAR_FRAME, q_dot);

  vpColVector q_dot_mes; // Measured velocities

  // Initialisation of the velocity measurement
  robot.getVelocity(vpRobot::ARTICULAR_FRAME, q_dot_mes); // q_dot_mes =0
  // q_dot_mes is resized to 4, the number of joint

  while (1) {
     robot.getVelocity(vpRobot::ARTICULAR_FRAME, q_dot_mes);
     vpTime::wait(40); // wait 40 ms
     // here q_dot_mes is equal to [M_PI/8, 0.2, M_PI/4, M_PI/16]
  }
  \endcode
*/
void
vpRobotAfma4::getVelocity (const vpRobot::vpControlFrameType frame,
			   vpColVector & velocity)
{

  switch (frame) {
  case vpRobot::ARTICULAR_FRAME: 
    velocity.resize (this->njoint);
  default:
    velocity.resize (6);
  }

  velocity = 0;


  double q[4];
  vpColVector q_cur(4);
  vpHomogeneousMatrix fMc_cur;
  vpHomogeneousMatrix cMc; // camera displacement


  InitTry;

  // Get the actual time
  double time_cur = vpTime::measureTimeSecond();

  // Get the current joint position
  Try( PrimitiveACQ_POS_Afma4(q) );
  for (int i=0; i < this->njoint; i ++) {
    q_cur[i] = q[i];
  }

  // Get the camera pose from the direct kinematics
  vpAfma4::get_fMc(q_cur, fMc_cur);

  if ( ! first_time_getvel ) {

    switch (frame) {
    case vpRobot::CAMERA_FRAME: {
      // Compute the displacement of the camera since the previous call
      cMc = fMc_prev_getvel.inverse() * fMc_cur;

      // Compute the velocity of the camera from this displacement
      velocity = vpExponentialMap::inverse(cMc, time_cur - time_prev_getvel);

      break ;
    }

    case vpRobot::ARTICULAR_FRAME: {
      velocity = (q_cur - q_prev_getvel)
	/ (time_cur - time_prev_getvel);
      break ;
    }

    case vpRobot::REFERENCE_FRAME: {
      // Compute the displacement of the camera since the previous call
      cMc = fMc_prev_getvel.inverse() * fMc_cur;

      // Compute the velocity of the camera from this displacement
      vpColVector v;
      v = vpExponentialMap::inverse(cMc, time_cur - time_prev_getvel);

      // Express this velocity in the reference frame
      vpTwistMatrix fVc(fMc_cur);
      velocity = fVc * v;

      break ;
    }

    case vpRobot::MIXT_FRAME: {
      vpERROR_TRACE ("Cannot get a velocity in the mixt frame: "
		     "functionality not implemented");
      throw vpRobotException (vpRobotException::wrongStateError,
			      "Cannot get a displacement in the mixt frame:"
			      "functionality not implemented");

      break ;
    }
    }
  }
  else {
    first_time_getvel = false;
  }

  // Memorize the camera pose for the next call
  fMc_prev_getvel = fMc_cur;

  // Memorize the joint position for the next call
  q_prev_getvel = q_cur;

  // Memorize the time associated to the joint position for the next call
  time_prev_getvel = time_cur;


  CatchPrint();
  if (TryStt < 0) {
    vpERROR_TRACE ("Cannot get velocity.");
    throw vpRobotException (vpRobotException::lowLevelError,
			    "Cannot get velocity.");
  }
}




/*!

  Get the robot velocities.

  \param frame : Frame in wich velocities are mesured.

  \return Measured velocities. Translations are expressed in m/s
  and rotations in rad/s.

  \code
  // Set joint velocities
  vpColVector q_dot(4);
  q_dot[0] = M_PI/8;  // Joint 1 velocity, in rad/s
  q_dot[1] = 0.2;     // Joint 2 velocity, in meter/s
  q_dot[2] = M_PI/4;  // Joint 4 velocity, in rad/s
  q_dot[3] = M_PI/16; // Joint 5 velocity, in rad/s

  vpRobotAfma4 robot;

  robot.setRobotState(vpRobot::STATE_VELOCITY_CONTROL);

  // Moves the joint in velocity
  robot.setVelocity(vpRobot::ARTICULAR_FRAME, q_dot);

  // Initialisation of the velocity measurement
  robot.getVelocity(vpRobot::ARTICULAR_FRAME, q_dot_mes); // q_dot_mes =0
  // q_dot_mes is resized to 4, the number of joint

  vpColVector q_dot_mes; // Measured velocities
  while (1) {
     q_dot_mes = robot.getVelocity(vpRobot::ARTICULAR_FRAME);
     vpTime::wait(40); // wait 40 ms
     // here q_dot_mes is equal to [M_PI/8, 0.2, M_PI/4, M_PI/16]
  }
  \endcode
*/
vpColVector
vpRobotAfma4::getVelocity (vpRobot::vpControlFrameType frame)
{
  vpColVector velocity;
  getVelocity (frame, velocity);

  return velocity;
}

/*!

Read joint positions in a specific Afma4 position file.

This position file has to start with a header. The six joint positions
are given after the "R:" keyword. The first 3 values correspond to the
joint translations X,Y,Z expressed in meters. The 3 last values
correspond to the joint rotations A,B,C expressed in degres to be more
representative for the user. Theses values are then converted in
radians in \e q. The character "#" starting a line indicates a
comment.

A typical content of such a file is given below:

\code
#AFMA4 - Position - Version 2.01
# file: "myposition.pos "
#
# R: X Y A B
# Joint position: X : rotation of the turret in degrees (joint 1)
#                 Y : vertical translation in meters (joint 2)
#                 A : pan rotation of the camera in degrees (joint 4)
#                 B : tilt rotation of the camera in degrees (joint 5)
#

R: 45 0.3 -20 30
\endcode

\param filename : Name of the position file to read.

\param q : Joint positions: X,Y,A,B. Translations Y  is
expressed in meters, while joint rotations X,A,B in radians.

\return true if the positions were successfully readen in the file. false, if
an error occurs.

The code below shows how to read a position from a file and move the robot to this position.
\code
vpRobotAfma4 robot;
vpColVector q;        // Joint position
robot.readPosFile("myposition.pos", q); // Set the joint position from the file
robot.setRobotState(vpRobot::STATE_POSITION_CONTROL);

robot.setPositioningVelocity(5); // Positioning velocity set to 5%
robot.setPosition(vpRobot::ARTICULAR_FRAME, q); // Move to the joint position
\endcode

\sa savePosFile()
*/

bool
vpRobotAfma4::readPosFile(const char *filename, vpColVector &q)
{

  FILE * fd ;
  fd = fopen(filename, "r") ;
  if (fd == NULL)
    return false;

  char line[FILENAME_MAX];
  char dummy[FILENAME_MAX];
  char head[] = "R:";
  bool sortie = false;

  do {
    // Saut des lignes commencant par #
    if (fgets (line, FILENAME_MAX, fd) != NULL) {
      if ( strncmp (line, "#", 1) != 0) {
	// La ligne n'est pas un commentaire
	if ( strncmp (line, head, sizeof(head)-1) == 0) {
	  sortie = true; 	// Position robot trouvee.
	}
// 	else
// 	  return (false); // fin fichier sans position robot.
      }
    }
    else {
      return (false);		/* fin fichier 	*/
    }

  }
  while ( sortie != true );

  // Lecture des positions
  q.resize(4);
  sscanf(line, "%s %lf %lf %lf %lf",
	 dummy,
	 &q[0], &q[1], &q[2], &q[3]);

  // converts rotations from degrees into radians
  q[0] = vpMath::rad(q[0]);
  q[2] = vpMath::rad(q[2]);
  q[3] = vpMath::rad(q[3]);


  fclose(fd) ;
  return (true);
}

/*!

  Save joint (articular) positions in a specific Afma4 position file.

  This position file starts with a header on the first line. After
  convertion of the rotations in degrees, the joint position \e q is
  written on a line starting with the keyword "R: ". See readPosFile()
  documentation for an example of such a file.

  \param filename : Name of the position file to create.

  \param q : Joint positions: X,Y,A,B. Translations Y  is
  expressed in meters, while joint rotations X,A,B in radians.

  \warning The joint rotations X,A,B written in the file are converted
  in degrees to be more representative for the user.

  \return true if the positions were successfully saved in the file. false, if
  an error occurs.

  \sa readPosFile()
*/

bool
vpRobotAfma4::savePosFile(const char *filename, const vpColVector &q)
{

  FILE * fd ;
  fd = fopen(filename, "w") ;
  if (fd == NULL)
    return false;

  fprintf(fd, "\
#AFMA4 - Position - Version 2.01\n\
#\n\
# R: X Y A B\n\
# Joint position: X : rotation of the turret in degrees (joint 1)\n\
#                 Y : vertical translation in meters (joint 2)\n\
#                 A : pan rotation of the camera in degrees (joint 4)\n\
#                 B : tilt rotation of the camera in degrees (joint 5)\n\
#\n\n");

  // Save positions in mm and deg
  fprintf(fd, "R: %lf %lf %lf %lf\n",
	  vpMath::deg(q[0]),
	  q[1],
	  vpMath::deg(q[2]),
	  vpMath::deg(q[3]));

  fclose(fd) ;
  return (true);
}

/*!

  Moves the robot to the joint position specified in the filename. The
  positioning velocity is set to 10% of the robot maximal velocity.

  \param filename: File containing a joint position.

  \sa readPosFile

*/
void
vpRobotAfma4::move(const char *filename)
{
  vpColVector q;

  this->readPosFile(filename, q)  ;
  this->setRobotState(vpRobot::STATE_POSITION_CONTROL) ;
  this->setPositioningVelocity(10);
  this->setPosition ( vpRobot::ARTICULAR_FRAME,  q) ;
}

/*!

  Get the robot displacement expressed in the camera frame since the last call
  of this method.

  \param displacement : The measured displacement in camera frame. The
  dimension of \e displacement is 6 (tx, ty, ty, rx, ry,
  rz). Translations are expressed in meters, rotations in radians with
  the Euler Rxyz representation.

  \sa getDisplacement(), getArticularDisplacement()

*/
void
vpRobotAfma4::getCameraDisplacement(vpColVector &displacement)
{
  getDisplacement(vpRobot::CAMERA_FRAME, displacement);
}
/*!

  Get the robot articular displacement since the last call of this method.

  \param displacement : The measured articular displacement. The
  dimension of \e displacement is 6 (the number of axis of the
  robot). Translations are expressed in meters, rotations in radians.

  \sa getDisplacement(), getCameraDisplacement()

*/
void
vpRobotAfma4::getArticularDisplacement(vpColVector  &displacement)
{
  getDisplacement(vpRobot::ARTICULAR_FRAME, displacement);
}

/*!

  Get the robot displacement since the last call of this method.

  \warning This functionnality is not implemented for the moment in the
  cartesian space. It is only available in the joint space
  (vpRobot::ARTICULAR_FRAME).

  \param frame : The frame in which the measured displacement is expressed.

  \param displacement : The measured displacement since the last call of this
  method. The dimension of \e displacement is always 4, the number of
  joints. Translations are expressed in meters, rotations in radians.

  In camera or reference frame, rotations are expressed with the
  Euler Rxyz representation.

  \sa getArticularDisplacement(), getCameraDisplacement()

*/
void
vpRobotAfma4::getDisplacement(vpRobot::vpControlFrameType frame,
			      vpColVector &displacement)
{
  displacement.resize (6);
  displacement = 0;

  double q[6];
  vpColVector q_cur(6);

  InitTry;

  // Get the current joint position
  Try( PrimitiveACQ_POS_Afma4(q) );
  for (int i=0; i < njoint; i ++) {
    q_cur[i] = q[i];
  }

  if ( ! first_time_getdis ) {
    switch (frame) {
    case vpRobot::CAMERA_FRAME: {
      std::cout << "getDisplacement() CAMERA_FRAME not implemented\n";
      return;
      break ;
    }

    case vpRobot::ARTICULAR_FRAME: {
      displacement = q_cur - q_prev_getdis;
      break ;
    }

    case vpRobot::REFERENCE_FRAME: {
      std::cout << "getDisplacement() REFERENCE_FRAME not implemented\n";
      return;
      break ;
    }

    case vpRobot::MIXT_FRAME: {
      std::cout << "getDisplacement() MIXT_FRAME not implemented\n";
      return;
      break ;
    }
    }
  }
  else {
    first_time_getdis = false;
  }

  // Memorize the joint position for the next call
  q_prev_getdis = q_cur;

  CatchPrint();
  if (TryStt < 0) {
    vpERROR_TRACE ("Cannot get velocity.");
    throw vpRobotException (vpRobotException::lowLevelError,
			    "Cannot get velocity.");
  }
}


/*
 * Local variables:
 * c-basic-offset: 2
 * End:
 */

#endif

