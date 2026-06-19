/************************************************************/
/*    NAME: Konstantin778                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: PointAssign.h                                          */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#ifndef PointAssign_HEADER
#define PointAssign_HEADER

#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include <vector>
#include <string>
#include "XYPoint.h" // Add this at the top

class PointAssign : public AppCastingMOOSApp
{
 public:
   PointAssign();
   ~PointAssign();

 protected: // Standard MOOSApp functions to overload  
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();
   std::vector<std::string> m_vnames;
   bool m_assign_by_region;
   unsigned int m_assign_index;

 protected: // Standard AppCastingMOOSApp function to overload 
   bool buildReport();

 protected:
   void RegisterVariables();
   void postViewPoint(double x, double y, std::string label, std::string color);

 private: // Configuration variables

 private: // State variables
};

#endif 
