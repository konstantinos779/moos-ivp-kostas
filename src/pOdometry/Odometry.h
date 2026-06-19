/************************************************************/
/*    NAME: konstantinos779                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: Odometry.h                                          */
/*    DATE: December 29th, 1963                             */
/************************************************************/
#include <list>
#ifndef Odometry_HEADER
#define Odometry_HEADER

#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

class Odometry : public AppCastingMOOSApp
{
 public:
   Odometry();
   ~Odometry();

 protected: // Standard MOOSApp functions to overload  
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();

 protected: // Standard AppCastingMOOSApp function to overload 
   bool buildReport();

 protected:
   void registerVariables();

 protected: //protected members
    std::list<double> m_nav_x_list;
    std::list<double> m_nav_y_list; 

 private: // Configuration variables
  bool   m_first_reading;
  double m_current_x;
  double m_current_y;
  double m_previous_x;
  double m_previous_y;
  double m_total_distance;
  double m_last_nav_time;
  double m_stale_thresh;
  std::string m_alt_unit;
  double m_alt_unit_mult;
  double m_depth_thresh;
  double m_current_depth;
  double m_dist_at_depth;

 private: // State variables
  bool m_x_updated;
  bool m_y_updated;
};

#endif 
