/************************************************************/
/*    NAME: konstantinos779                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenPath.h                                          */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#ifndef GenPath_HEADER
#define GenPath_HEADER
#include "XYPoint.h"
#include "XYSegList.h"
#include <vector>
#include <string>

#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

class GenPath : public AppCastingMOOSApp
{
 public:
   GenPath();
   ~GenPath();

 protected: // Standard MOOSApp functions to overload  
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();


 protected: // Standard AppCastingMOOSApp function to overload 
   bool buildReport();

 protected:
   void registerVariables();
   double m_nav_x;
   double m_nav_y;
   bool   m_all_points_received;
   bool   m_path_generated;
   std::vector<XYPoint> m_visit_points;
   double m_visit_radius; 
   int m_total_points;

 private: // Configuration variables

 private: // State variables
};

#endif 
